#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "Grid/GridTypes.h"
#include "Input/InputTypes.h"
#include "BattleSelectionComponent.generated.h"

class ADungeonUnit;
class AHeroUnit;
class UBattleSubsystem;
class UGridHighlightComponent;

// Стейт-машина выбора героя (TDD 7.2). Живёт на ABattlePlayerController.
// Состояние боя меняет только через UBattleSubsystem::TryExecute
UCLASS(ClassGroup = (Dungeon))
class DUNGEON_API UBattleSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleSelectionComponent();

	// ЛКМ. Решение принимается по Target.Unit
	void HandleSelect(const FCursorTarget& Target);

	// ПКМ. Работает только в HeroSelected и при Target.bOnGrid: TryExecute(UMoveAction).
	// При ошибке состояние не меняется
	void HandleConfirm(const FCursorTarget& Target);

	// Esc
	void HandleCancel();

	// Подсвечивает Target.Tile, если курсор над полем, иначе снимает подсветку. Во всех состояниях
	void HandleHover(const FCursorTarget& Target);

	// Вход в HeroSelected: SetSelected(true), RefreshReachable
	void SelectHero(AHeroUnit* Hero);

	// Выход из HeroSelected: SetSelected(false), ClearReachable. Из Locked и Idle ничего не меняет
	void ClearSelection();

	// Показывает доступные клетки выбранного героя. Если он уже ходил, снимает подсветку
	void RefreshReachable();

	ESelectionState GetState() const { return State; }
	AHeroUnit* GetSelectedHero() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void OnUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To);

	UBattleSubsystem* GetBattle() const;

	// Подсветка поля боя, nullptr пока сетки нет
	UGridHighlightComponent* GetHighlight() const;

	// Если выбранный герой исчез, ведёт себя как после ClearSelection. Возвращает true, если выбор актуален
	bool ValidateSelection();

	UPROPERTY(VisibleAnywhere, Category = "Selection", meta = (AllowPrivateAccess = "true"))
	ESelectionState State = ESelectionState::Locked;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHeroUnit> SelectedHero;

	// Доступные клетки для подсветки
	UPROPERTY(Transient)
	FReachableArea CachedReachable;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;
};
