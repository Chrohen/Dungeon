#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Battle/BattleTypes.h"
#include "Grid/GridTypes.h"
#include "Input/InputTypes.h"
#include "BattleSelectionComponent.generated.h"

class ADungeonUnit;
class AHeroUnit;
class UBattleOverlayComponent;
class UBattleSubsystem;
class UGridHighlightComponent;

// Для HUD: при любой смене состояния, выбранного героя или способности
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSelectionChanged, ESelectionState, State, AHeroUnit*, Hero, int32, AbilityIndex);

// Для HUD: сменился юнит под курсором (nullptr - юнита нет)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHoveredUnitChanged, ADungeonUnit*, Unit);

// Стейт-машина выбора героя и прицеливания (TD 7.2). Живёт на ABattlePlayerController.
// Состояние боя меняет только через UBattleSubsystem::TryExecute. От классов UI не зависит
UCLASS(ClassGroup = (Dungeon))
class DUNGEON_API UBattleSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleSelectionComponent();

	// ЛКМ. Решение принимается по Target.Unit
	void HandleSelect(const FCursorTarget& Target);

	// ПКМ, только при Target.bOnGrid. HeroSelected: TryExecute(UMoveAction), при ошибке ничего не меняется.
	// Aiming: если Target.Tile в CachedTargetable, TryExecute(UUseAbilityAction); при Success - ExitAiming
	void HandleConfirm(const FCursorTarget& Target);

	// Esc. Пошагово: Aiming - HeroSelected, HeroSelected - Idle
	void HandleCancel();

	// Наведение (TD 6.7): подсветка клетки, смена юнита под курсором, предпросмотр в Aiming,
	// фокус намерений в Idle и HeroSelected
	void HandleHover(const FCursorTarget& Target);

	// ЛКМ по кнопке способности (из UI). Работает в HeroSelected и Aiming. Доступность проверяет сам компонент
	// (TD, решение 10): фаза PlayerTurn, герой выбран и жив, индекс в пределах списка, CanAct(), IsReady().
	// В Aiming та же способность - ExitAiming, другая доступная - смена способности
	void SelectAbility(int32 Index);

	// ЛКМ по карточке героя (из UI): как ЛКМ по этому герою. Погибший герой игнорируется
	void SelectHeroFromCard(AHeroUnit* Hero);

	// Вход в HeroSelected: SetSelected(true), RefreshReachable
	void SelectHero(AHeroUnit* Hero);

	// Выход из HeroSelected: SetSelected(false), ClearReachable. Из Locked и Idle ничего не меняет
	void ClearSelection();

	// Вход в Aiming: ClearReachable, CachedTargetable, слой Targetable, RefreshHover. Только из HeroSelected
	void EnterAiming(int32 Index);

	// Выход из Aiming в HeroSelected: слой Targetable, предпросмотр и SelectedAbility очищаются, клетки
	// перемещения показываются снова (GDD: подсветка перемещения скрыта только во время прицеливания)
	void ExitAiming();

	// Показывает доступные клетки выбранного героя. Если он уже ходил или идёт прицеливание, снимает подсветку
	void RefreshReachable();

	// HandleHover(LastHover): предпросмотр и фокус после изменения поля
	void RefreshHover();

	ESelectionState GetState() const { return State; }
	AHeroUnit* GetSelectedHero() const;
	int32 GetSelectedAbility() const { return SelectedAbility; }
	const TArray<FIntPoint>& GetCachedTargetable() const { return CachedTargetable; }

	UPROPERTY(BlueprintAssignable)
	FOnSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable)
	FOnHoveredUnitChanged OnHoveredUnitChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Откладывает OnSelectionChanged до конца внешней операции и посылает его, только если что-то изменилось:
	// составные переходы (смена героя, смена способности) дают одно событие
	struct FNotifyScope;
	friend struct FNotifyScope;

	UFUNCTION()
	void OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void OnUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To);

	UFUNCTION()
	void OnUnitDied(ADungeonUnit* Unit, ADungeonUnit* Killer);

	UFUNCTION()
	void OnIntentsChanged();

	UFUNCTION()
	void OnBattleStarted();

	UBattleSubsystem* GetBattle() const;

	// Подсветка и отображение поля боя, nullptr пока сетки нет
	UGridHighlightComponent* GetHighlight() const;
	UBattleOverlayComponent* GetOverlay() const;

	// Если выбранный герой исчез, ведёт себя как после ClearSelection. Возвращает true, если выбор актуален
	bool ValidateSelection();

	// ЛКМ по герою (с поля или с карточки)
	void SelectHeroByInput(AHeroUnit* Hero);

	// Способность доступна для прицеливания (TD, решение 10)
	bool IsAbilityAvailable(int32 Index) const;

	// Выход из Aiming без показа клеток перемещения: для переходов, после которых герой не остаётся выбранным
	// или уже выбран заново
	void LeaveAiming();

	void NotifyIfChanged();

	UPROPERTY(VisibleAnywhere, Category = "Selection", meta = (AllowPrivateAccess = "true"))
	ESelectionState State = ESelectionState::Locked;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHeroUnit> SelectedHero;

	// Индекс способности в прицеливании или INDEX_NONE
	UPROPERTY(Transient)
	int32 SelectedAbility = INDEX_NONE;

	// Доступные клетки для подсветки
	UPROPERTY(Transient)
	FReachableArea CachedReachable;

	// Клетки, допустимые точкой цели в Aiming
	UPROPERTY(Transient)
	TArray<FIntPoint> CachedTargetable;

	// Последняя цель наведения
	UPROPERTY(Transient)
	FCursorTarget LastHover;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	// Что было послано в OnSelectionChanged в последний раз
	ESelectionState NotifiedState = ESelectionState::Locked;
	TWeakObjectPtr<AHeroUnit> NotifiedHero;
	int32 NotifiedAbility = INDEX_NONE;
	int32 NotifyDepth = 0;
};
