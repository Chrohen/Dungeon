#include "Input/BattleSelectionComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Engine/World.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridHighlightComponent.h"
#include "Grid/GridPathfinder.h"
#include "Units/HeroUnit.h"

UBattleSelectionComponent::UBattleSelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AHeroUnit* UBattleSelectionComponent::GetSelectedHero() const
{
	return SelectedHero.Get();
}

void UBattleSelectionComponent::BeginPlay()
{
	Super::BeginPlay();

	UBattleSubsystem* Battle = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBattleSubsystem>() : nullptr;
	if (Battle == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UBattleSelectionComponent::BeginPlay: UBattleSubsystem is not available."));
		return;
	}

	BoundBattle = Battle;
	Battle->OnPhaseChanged.AddDynamic(this, &UBattleSelectionComponent::OnPhaseChanged);
	Battle->OnUnitMoved.AddDynamic(this, &UBattleSelectionComponent::OnUnitMoved);

	// Бой мог начаться раньше, чем создан компонент
	if (Battle->IsPlayerTurn())
	{
		State = ESelectionState::Idle;
	}
}

void UBattleSelectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBattleSubsystem* Battle = GetBattle())
	{
		Battle->OnPhaseChanged.RemoveDynamic(this, &UBattleSelectionComponent::OnPhaseChanged);
		Battle->OnUnitMoved.RemoveDynamic(this, &UBattleSelectionComponent::OnUnitMoved);
	}
	BoundBattle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBattleSelectionComponent::HandleSelect(const FCursorTarget& Target)
{
	if (State == ESelectionState::Locked)
	{
		return;
	}
	// Если выбранный герой исчез, состояние становится Idle, и ввод обрабатывается как в Idle
	ValidateSelection();

	ADungeonUnit* Unit = Target.Unit.Get();
	if (AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
	{
		if (State == ESelectionState::HeroSelected)
		{
			if (Hero == SelectedHero.Get())
			{
				return;
			}
			ClearSelection();
		}
		SelectHero(Hero);
	}
	else if (Unit != nullptr && State == ESelectionState::HeroSelected)
	{
		// Монстр: выбор другого объекта снимает выбор героя
		ClearSelection();
	}
	// Клетка без юнита или вне поля ничего не меняет
}

void UBattleSelectionComponent::HandleConfirm(const FCursorTarget& Target)
{
	if (State != ESelectionState::HeroSelected || !Target.bOnGrid || !ValidateSelection())
	{
		return;
	}

	UBattleSubsystem* Battle = GetBattle();
	if (Battle == nullptr)
	{
		return;
	}

	UMoveAction* Action = NewObject<UMoveAction>(GetTransientPackage());
	Action->Unit = SelectedHero.Get();
	Action->Target = Target.Tile;

	// При Success подсветку снимает OnUnitMoved. При ошибке ничего не меняется
	Battle->TryExecute(Action);
}

void UBattleSelectionComponent::HandleCancel()
{
	if (State == ESelectionState::HeroSelected)
	{
		ClearSelection();
	}
}

void UBattleSelectionComponent::HandleHover(const FCursorTarget& Target)
{
	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		if (Target.bOnGrid)
		{
			Highlight->SetHoveredTile(Target.Tile);
		}
		else
		{
			Highlight->SetHoveredTile(TOptional<FIntPoint>());
		}
	}
}

void UBattleSelectionComponent::SelectHero(AHeroUnit* Hero)
{
	if (State == ESelectionState::Locked || !IsValid(Hero))
	{
		return;
	}

	SelectedHero = Hero;
	State = ESelectionState::HeroSelected;
	Hero->SetSelected(true);
	RefreshReachable();
}

void UBattleSelectionComponent::ClearSelection()
{
	if (AHeroUnit* Hero = SelectedHero.Get())
	{
		Hero->SetSelected(false);
	}
	SelectedHero.Reset();
	CachedReachable = FReachableArea();

	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		Highlight->ClearReachable();
	}

	if (State == ESelectionState::HeroSelected)
	{
		State = ESelectionState::Idle;
	}
}

void UBattleSelectionComponent::RefreshReachable()
{
	if (State != ESelectionState::HeroSelected || !ValidateSelection())
	{
		return;
	}

	AHeroUnit* Hero = SelectedHero.Get();
	UBattleSubsystem* Battle = GetBattle();
	const ADungeonGridActor* Grid = Battle != nullptr ? Battle->GetGrid() : nullptr;
	UGridHighlightComponent* Highlight = GetHighlight();

	if (Hero->CanMove() && Grid != nullptr)
	{
		FPathQuery Query;
		Query.Origin = Hero->GridPosition;
		Query.MovePoints = Hero->GetMovePoints();
		Query.Team = Hero->Team;
		CachedReachable = FGridPathfinder::ComputeReachable(Grid->GetModel(), Query);
		if (Highlight != nullptr)
		{
			Highlight->ShowReachable(CachedReachable.GetDestinations());
		}
	}
	else
	{
		CachedReachable = FReachableArea();
		if (Highlight != nullptr)
		{
			Highlight->ClearReachable();
		}
	}
}

void UBattleSelectionComponent::OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	if (NewPhase == EBattlePhase::PlayerTurn)
	{
		if (State == ESelectionState::Locked)
		{
			State = ESelectionState::Idle;
		}
		return;
	}

	ClearSelection();
	State = ESelectionState::Locked;
}

void UBattleSelectionComponent::OnUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To)
{
	if (State == ESelectionState::HeroSelected)
	{
		RefreshReachable();
	}
}

UBattleSubsystem* UBattleSelectionComponent::GetBattle() const
{
	return BoundBattle.Get();
}

UGridHighlightComponent* UBattleSelectionComponent::GetHighlight() const
{
	const UBattleSubsystem* Battle = GetBattle();
	const ADungeonGridActor* Grid = Battle != nullptr ? Battle->GetGrid() : nullptr;
	return Grid != nullptr ? Grid->GetHighlight() : nullptr;
}

bool UBattleSelectionComponent::ValidateSelection()
{
	if (State == ESelectionState::HeroSelected && !SelectedHero.IsValid())
	{
		ClearSelection();
		return false;
	}
	return true;
}
