#include "Input/BattleSelectionComponent.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Battle/UseAbilityAction.h"
#include "Engine/World.h"
#include "Grid/BattleOverlayComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridHighlightComponent.h"
#include "Grid/GridPathfinder.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

struct UBattleSelectionComponent::FNotifyScope
{
	explicit FNotifyScope(UBattleSelectionComponent& InOwner)
		: Owner(InOwner)
	{
		++Owner.NotifyDepth;
	}

	~FNotifyScope()
	{
		if (--Owner.NotifyDepth == 0)
		{
			Owner.NotifyIfChanged();
		}
	}

	UBattleSelectionComponent& Owner;
};

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
	Battle->OnUnitDied.AddDynamic(this, &UBattleSelectionComponent::OnUnitDied);
	Battle->OnIntentsChanged.AddDynamic(this, &UBattleSelectionComponent::OnIntentsChanged);
	Battle->OnBattleStarted.AddDynamic(this, &UBattleSelectionComponent::OnBattleStarted);

	// Бой мог начаться раньше, чем создан компонент
	if (Battle->IsPlayerTurn())
	{
		FNotifyScope Scope(*this);
		State = ESelectionState::Idle;
	}
}

void UBattleSelectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBattleSubsystem* Battle = GetBattle())
	{
		Battle->OnPhaseChanged.RemoveDynamic(this, &UBattleSelectionComponent::OnPhaseChanged);
		Battle->OnUnitMoved.RemoveDynamic(this, &UBattleSelectionComponent::OnUnitMoved);
		Battle->OnUnitDied.RemoveDynamic(this, &UBattleSelectionComponent::OnUnitDied);
		Battle->OnIntentsChanged.RemoveDynamic(this, &UBattleSelectionComponent::OnIntentsChanged);
		Battle->OnBattleStarted.RemoveDynamic(this, &UBattleSelectionComponent::OnBattleStarted);
	}
	BoundBattle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBattleSelectionComponent::HandleSelect(const FCursorTarget& Target)
{
	FNotifyScope Scope(*this);
	if (State == ESelectionState::Locked)
	{
		return;
	}
	// Если выбранный герой исчез, состояние становится Idle, и ввод обрабатывается как в Idle
	ValidateSelection();

	ADungeonUnit* Unit = Target.Unit.Get();
	if (AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
	{
		SelectHeroByInput(Hero);
	}
	else if (Unit != nullptr && State == ESelectionState::HeroSelected)
	{
		// Монстр: выбор другого объекта снимает выбор героя. В Aiming это промах, прицеливание остаётся
		ClearSelection();
	}
	// Клетка без юнита или вне поля ничего не меняет
}

void UBattleSelectionComponent::HandleConfirm(const FCursorTarget& Target)
{
	FNotifyScope Scope(*this);
	if ((State != ESelectionState::HeroSelected && State != ESelectionState::Aiming) || !Target.bOnGrid || !ValidateSelection())
	{
		return;
	}

	UBattleSubsystem* Battle = GetBattle();
	if (Battle == nullptr)
	{
		return;
	}

	if (State == ESelectionState::HeroSelected)
	{
		UMoveAction* Action = NewObject<UMoveAction>(GetTransientPackage());
		Action->Unit = SelectedHero.Get();
		Action->Target = Target.Tile;

		// При Success подсветку снимает OnUnitMoved. При ошибке ничего не меняется
		Battle->TryExecute(Action);
		return;
	}

	// Aiming: только клетка из CachedTargetable, остальные клики - промах
	if (!CachedTargetable.Contains(Target.Tile))
	{
		return;
	}
	UUseAbilityAction* Action = NewObject<UUseAbilityAction>(GetTransientPackage());
	Action->Unit = SelectedHero.Get();
	Action->AbilityIndex = SelectedAbility;
	Action->SelectedTile = Target.Tile;

	const EBattleActionResult Result = Battle->TryExecute(Action);

	// Способность могла завершить бой (OnPhaseChanged -> Locked) или убить героя (OnUnitDied -> Idle):
	// тогда прежних переходов уже нет
	if (State != ESelectionState::Aiming || Result != EBattleActionResult::Success)
	{
		return;
	}
	// GDD: если герой не ходил, снова видны клетки перемещения (ExitAiming вызывает RefreshReachable)
	ExitAiming();
}

void UBattleSelectionComponent::HandleCancel()
{
	FNotifyScope Scope(*this);
	if (!ValidateSelection())
	{
		return;
	}
	if (State == ESelectionState::Aiming)
	{
		ExitAiming();
	}
	else if (State == ESelectionState::HeroSelected)
	{
		ClearSelection();
	}
}

void UBattleSelectionComponent::HandleHover(const FCursorTarget& Target)
{
	FNotifyScope Scope(*this);
	ValidateSelection();

	// 1. Слой Hover - во всех состояниях
	UGridHighlightComponent* Highlight = GetHighlight();
	if (Highlight != nullptr)
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

	// 2. Сменился юнит под курсором
	const bool bUnitChanged = LastHover.Unit != Target.Unit;
	LastHover = Target;
	if (bUnitChanged)
	{
		OnHoveredUnitChanged.Broadcast(Target.Unit.Get());
	}

	UBattleSubsystem* Battle = GetBattle();
	UBattleOverlayComponent* Overlay = GetOverlay();
	if (Overlay == nullptr || Battle == nullptr)
	{
		return;
	}

	// 3. Прицеливание: предпросмотр на допустимой клетке. Фокус намерений не меняется (TD 13, п. 8)
	if (State == ESelectionState::Aiming)
	{
		AHeroUnit* Hero = SelectedHero.Get();
		FAbilityOutcome Outcome;
		if (Hero != nullptr && Target.bOnGrid && CachedTargetable.Contains(Target.Tile)
			&& Battle->PreviewAbility(Hero, SelectedAbility, Target.Tile, Outcome))
		{
			const bool bProjectile = Hero->GetAbilities().IsValidIndex(SelectedAbility)
				&& Hero->GetAbilities()[SelectedAbility].Ability.Targeting.Trajectory == ETrajectory::Projectile;
			Overlay->ShowAbilityPreview(Outcome, bProjectile);
		}
		else
		{
			Overlay->ClearAbilityPreview();
		}
		return;
	}

	// 4. Idle и HeroSelected в PlayerTurn: фокус намерения. 5. Locked: только пункты 1-2
	if ((State == ESelectionState::Idle || State == ESelectionState::HeroSelected) && Battle->IsPlayerTurn())
	{
		AMonsterUnit* Monster = Cast<AMonsterUnit>(Target.Unit.Get());
		if (Monster != nullptr && Monster->IsAlive() && Monster->HasIntent())
		{
			Overlay->SetFocus(Monster, {});
			return;
		}
		if (Target.bOnGrid)
		{
			const bool bUnderThreat = Battle->GetIntentViews().ContainsByPredicate([&Target](const FIntentView& View)
			{
				return View.Cast.Area.Contains(Target.Tile, EAreaPart::All);
			});
			if (bUnderThreat)
			{
				Overlay->SetFocus(nullptr, Target.Tile);
				return;
			}
		}
		Overlay->SetFocus(nullptr, {});
	}
}

void UBattleSelectionComponent::SelectAbility(int32 Index)
{
	FNotifyScope Scope(*this);
	if ((State != ESelectionState::HeroSelected && State != ESelectionState::Aiming) || !ValidateSelection())
	{
		return;
	}

	// Кнопка выбранной способности отменяет прицеливание
	if (State == ESelectionState::Aiming && Index == SelectedAbility)
	{
		ExitAiming();
		return;
	}
	if (!IsAbilityAvailable(Index))
	{
		return;
	}

	if (State == ESelectionState::Aiming)
	{
		LeaveAiming();
	}
	EnterAiming(Index);
}

void UBattleSelectionComponent::SelectHeroFromCard(AHeroUnit* Hero)
{
	FNotifyScope Scope(*this);
	if (State == ESelectionState::Locked || !IsValid(Hero) || !Hero->IsAlive())
	{
		return;
	}
	ValidateSelection();
	SelectHeroByInput(Hero);
}

void UBattleSelectionComponent::SelectHero(AHeroUnit* Hero)
{
	FNotifyScope Scope(*this);
	if (State == ESelectionState::Locked || !IsValid(Hero))
	{
		return;
	}
	if (State == ESelectionState::Aiming)
	{
		LeaveAiming();
		ClearSelection();
	}

	SelectedHero = Hero;
	State = ESelectionState::HeroSelected;
	Hero->SetSelected(true);
	RefreshReachable();
}

void UBattleSelectionComponent::ClearSelection()
{
	FNotifyScope Scope(*this);
	if (State == ESelectionState::Aiming)
	{
		LeaveAiming();
	}
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

void UBattleSelectionComponent::EnterAiming(int32 Index)
{
	FNotifyScope Scope(*this);
	UBattleSubsystem* Battle = GetBattle();
	AHeroUnit* Hero = SelectedHero.Get();
	if (State != ESelectionState::HeroSelected || Battle == nullptr || Hero == nullptr)
	{
		return;
	}
	TArray<FIntPoint> Targetable;
	if (!Battle->GetSelectableTiles(Hero, Index, Targetable))
	{
		return;
	}

	State = ESelectionState::Aiming;
	SelectedAbility = Index;
	CachedReachable = FReachableArea();
	CachedTargetable = MoveTemp(Targetable);

	// GDD: подсветка клеток для перемещения скрывается
	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		Highlight->ClearReachable();
		Highlight->SetLayerTiles(EGridHighlightLayer::Targetable, CachedTargetable);
	}
	RefreshHover();
}

void UBattleSelectionComponent::ExitAiming()
{
	FNotifyScope Scope(*this);
	if (State != ESelectionState::Aiming)
	{
		return;
	}
	LeaveAiming();
	RefreshReachable();
	RefreshHover();
}

void UBattleSelectionComponent::LeaveAiming()
{
	FNotifyScope Scope(*this);
	if (State != ESelectionState::Aiming)
	{
		return;
	}
	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		Highlight->ClearLayer(EGridHighlightLayer::Targetable);
	}
	if (UBattleOverlayComponent* Overlay = GetOverlay())
	{
		Overlay->ClearAbilityPreview();
	}
	CachedTargetable.Reset();
	SelectedAbility = INDEX_NONE;
	State = ESelectionState::HeroSelected;
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

	if (Hero->IsAlive() && Hero->CanMove() && Grid != nullptr)
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

void UBattleSelectionComponent::RefreshHover()
{
	HandleHover(LastHover);
}

void UBattleSelectionComponent::OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	FNotifyScope Scope(*this);
	if (NewPhase == EBattlePhase::PlayerTurn)
	{
		if (State == ESelectionState::Locked)
		{
			State = ESelectionState::Idle;
		}
		return;
	}

	// Прицеливание и выбор сбрасываются (GDD)
	LeaveAiming();
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

void UBattleSelectionComponent::OnUnitDied(ADungeonUnit* Unit, ADungeonUnit* Killer)
{
	FNotifyScope Scope(*this);
	if ((State == ESelectionState::HeroSelected || State == ESelectionState::Aiming) && Unit != nullptr && Unit == SelectedHero.Get())
	{
		LeaveAiming();
		ClearSelection();
	}
}

void UBattleSelectionComponent::OnIntentsChanged()
{
	RefreshHover();
}

void UBattleSelectionComponent::OnBattleStarted()
{
	FNotifyScope Scope(*this);
	LeaveAiming();
	ClearSelection();
	State = ESelectionState::Locked;
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

UBattleOverlayComponent* UBattleSelectionComponent::GetOverlay() const
{
	const UBattleSubsystem* Battle = GetBattle();
	const ADungeonGridActor* Grid = Battle != nullptr ? Battle->GetGrid() : nullptr;
	return Grid != nullptr ? Grid->GetOverlay() : nullptr;
}

bool UBattleSelectionComponent::ValidateSelection()
{
	if ((State == ESelectionState::HeroSelected || State == ESelectionState::Aiming) && !SelectedHero.IsValid())
	{
		FNotifyScope Scope(*this);
		LeaveAiming();
		ClearSelection();
		return false;
	}
	return true;
}

void UBattleSelectionComponent::SelectHeroByInput(AHeroUnit* Hero)
{
	if (Hero == nullptr || !Hero->IsAlive())
	{
		return;
	}
	switch (State)
	{
	case ESelectionState::Idle:
		SelectHero(Hero);
		break;
	case ESelectionState::HeroSelected:
	case ESelectionState::Aiming:
		if (Hero != SelectedHero.Get())
		{
			// В Aiming ClearSelection сначала выходит из прицеливания
			ClearSelection();
			SelectHero(Hero);
		}
		break;
	default:
		break;
	}
}

bool UBattleSelectionComponent::IsAbilityAvailable(int32 Index) const
{
	const UBattleSubsystem* Battle = GetBattle();
	const AHeroUnit* Hero = SelectedHero.Get();
	if (Battle == nullptr || !Battle->IsPlayerTurn() || Hero == nullptr || !Hero->IsAlive())
	{
		return false;
	}
	const TArray<FUnitAbilityState>& Abilities = Hero->GetAbilities();
	return Abilities.IsValidIndex(Index) && Hero->CanAct() && Abilities[Index].IsReady();
}

void UBattleSelectionComponent::NotifyIfChanged()
{
	if (State == NotifiedState && SelectedHero == NotifiedHero && SelectedAbility == NotifiedAbility)
	{
		return;
	}
	NotifiedState = State;
	NotifiedHero = SelectedHero;
	NotifiedAbility = SelectedAbility;
	OnSelectionChanged.Broadcast(State, SelectedHero.Get(), SelectedAbility);
}
