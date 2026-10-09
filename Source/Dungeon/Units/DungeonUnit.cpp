#include "Units/DungeonUnit.h"
#include "Core/DungeonCollision.h"
#include "Data/UnitDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// Значение стенсила для контура выбранного юнита (TDD §10)
	constexpr int32 SelectionStencilValue = 1;
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
}

void ADungeonUnit::InitializeUnit(UUnitDefinition* Def, FIntPoint Coord)
{
	Definition = Def;
	GridPosition = Coord;
}

int32 ADungeonUnit::GetMovePoints() const
{
	return Definition != nullptr ? Definition->MovePoints : 0;
}

bool ADungeonUnit::CanMove() const
{
	return !TurnState.bHasMoved;
}

void ADungeonUnit::ResetTurnState()
{
	TurnState = FUnitTurnState();
}

void ADungeonUnit::MarkMoved()
{
	TurnState.bHasMoved = true;
}

void ADungeonUnit::SetGridPosition(FIntPoint Coord, const FVector& World)
{
	GridPosition = Coord;
	SetActorLocation(World + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight()));
}

void ADungeonUnit::SetSelected(bool bSelected)
{
	Mesh->SetCustomDepthStencilValue(SelectionStencilValue);
	Mesh->SetRenderCustomDepth(bSelected);
}
