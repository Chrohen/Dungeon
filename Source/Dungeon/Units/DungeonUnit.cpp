#include "Units/DungeonUnit.h"
#include "Abilities/AbilityParams.h"
#include "Core/DungeonCollision.h"
#include "Data/AbilityDefinition.h"
#include "Data/UnitDefinition.h"
#include "Units/UnitHealthBarComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// Значения стенсила для контура юнита (TD §10): 1 - выбранный герой, 2 - выделенный монстр
	constexpr int32 SelectionStencilValue = 1;
	constexpr int32 FocusStencilValue = 2;
}

ADungeonUnit::ADungeonUnit()
{
	PrimaryActorTick.bCanEverTick = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(35.f, 90.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Unit, ECR_Block);
	Capsule->SetGenerateOverlapEvents(false);
	Capsule->SetCanEverAffectNavigation(false);
	SetRootComponent(Capsule);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	// Ноги модели стоят на дне капсулы
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
	Mesh->SetCustomDepthStencilValue(SelectionStencilValue);
	Mesh->SetRenderCustomDepth(false);

	HealthBar = CreateDefaultSubobject<UUnitHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(Capsule);
	// Заглушка: чуть выше макушки модели высотой с капсулу. Точную высоту задаёт BP_*Unit_*
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, Capsule->GetUnscaledCapsuleHalfHeight() + 15.f));
}

void ADungeonUnit::InitializeUnit(UUnitDefinition* Def, FIntPoint Coord, int32 InUnitId)
{
	Definition = Def;
	GridPosition = Coord;
	UnitId = InUnitId;

	Health = GetMaxHealth();
	bAlive = true;
	Level = Def != nullptr ? Def->Level : 1;

	Abilities.Reset();
	if (Def != nullptr)
	{
		for (const FUnitAbilityEntry& Entry : Def->Abilities)
		{
			FUnitAbilityState State;
			State.Ability = FAbilityParams::Resolve(Entry.Ability, Entry.Overrides, Entry.Level);
			State.Level = Entry.Ability != nullptr ? FMath::Clamp(Entry.Level, 1, Entry.Ability->GetMaxLevel()) : Entry.Level;
			State.RemainingCooldown = 0;
			State.UsedOnTurn = INDEX_NONE;
			Abilities.Add(State);
		}
	}
	RefreshVisuals();
}

int32 ADungeonUnit::GetMovePoints() const
{
	return Definition != nullptr ? Definition->MovePoints : 0;
}

int32 ADungeonUnit::GetMaxHealth() const
{
	return Definition != nullptr ? Definition->MaxHealth : 1;
}

bool ADungeonUnit::CanMove() const
{
	return IsAlive() && !TurnState.bHasMoved;
}

bool ADungeonUnit::CanAct() const
{
	return IsAlive() && !TurnState.bHasActed;
}

void ADungeonUnit::ResetTurnState()
{
	TurnState = FUnitTurnState();
	RefreshVisuals();
}

void ADungeonUnit::MarkMoved()
{
	TurnState.bHasMoved = true;
	RefreshVisuals();
}

void ADungeonUnit::MarkActed()
{
	TurnState.bHasActed = true;
	RefreshVisuals();
}

void ADungeonUnit::SetGridPosition(FIntPoint Coord, const FVector& World)
{
	GridPosition = Coord;
	SetActorLocation(World + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight()));
}

void ADungeonUnit::SetHealth(int32 NewHealth)
{
	Health = FMath::Clamp(NewHealth, 0, GetMaxHealth());
	RefreshVisuals();
}

void ADungeonUnit::MarkDead()
{
	bAlive = false;
	SetActorHiddenInGame(true);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshVisuals();
}

void ADungeonUnit::StartCooldown(int32 Index, int32 TurnNumber)
{
	if (!Abilities.IsValidIndex(Index))
	{
		return;
	}
	FUnitAbilityState& State = Abilities[Index];
	State.RemainingCooldown = State.Ability.Cooldown;
	State.UsedOnTurn = TurnNumber;
	RefreshVisuals();
}

void ADungeonUnit::TickCooldowns(int32 TurnNumber)
{
	for (FUnitAbilityState& State : Abilities)
	{
		if (State.RemainingCooldown > 0 && State.UsedOnTurn != TurnNumber)
		{
			--State.RemainingCooldown;
		}
	}
	RefreshVisuals();
}

void ADungeonUnit::SetSelected(bool bSelected)
{
	Mesh->SetCustomDepthStencilValue(SelectionStencilValue);
	Mesh->SetRenderCustomDepth(bSelected);
}

void ADungeonUnit::SetFocused(bool bFocused)
{
	Mesh->SetCustomDepthStencilValue(FocusStencilValue);
	Mesh->SetRenderCustomDepth(bFocused);
}

void ADungeonUnit::RefreshVisuals()
{
	if (HealthBar != nullptr)
	{
		HealthBar->Refresh(*this);
	}
}
