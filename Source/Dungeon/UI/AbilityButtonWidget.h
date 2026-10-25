#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Abilities/AbilityTypes.h"
#include "Battle/BattleTypes.h"
#include "Input/InputTypes.h"
#include "UI/UITypes.h"
#include "AbilityButtonWidget.generated.h"

class ADungeonUnit;
class AHeroUnit;
class UBattleSelectionComponent;
class UBattleSubsystem;
class UButton;
class UImage;
class UTextBlock;

// Кнопка способности в панели героя (GDD: «Кнопки способностей»). Раскладка задаётся в WBP_AbilityButton
UCLASS(Abstract)
class DUNGEON_API UAbilityButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Состояние кнопки по таблице TD 7.5, первая подходящая проверка. Индекс вне списка способностей - Unavailable.
	// bIsPlayerTurn - UBattleSubsystem::IsPlayerTurn() (решение 11: функция статическая и фазу не знает)
	static EAbilityButtonState GetButtonState(const AHeroUnit& Hero, int32 Index, int32 SelectedIndex, bool bIsPlayerTurn);

	// Подсказка: название, описание, урон каждого эффекта Damage с частью области, дальность «Min–Max», перезарядка
	static FText MakeTooltipText(const FResolvedAbility& Ability);

	// Привязывает кнопку к способности героя и обновляет её
	void SetAbility(AHeroUnit* InHero, int32 InIndex);

	// Пересчитывает состояние, вид и подсказку
	void Refresh();

	EAbilityButtonState GetState() const { return State; }
	AHeroUnit* GetHero() const;
	int32 GetAbilityIndex() const { return AbilityIndex; }

	// Цвет иконки затемнённой кнопки
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Button")
	FLinearColor DimmedColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void OnButtonClicked();

	UFUNCTION()
	void OnUnitChanged(ADungeonUnit* Unit);

	UFUNCTION()
	void OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void OnSelectionChanged(ESelectionState SelectionState, AHeroUnit* SelectedHero, int32 SelectedAbility);

	// Индекс способности, которой герой целится сейчас, или INDEX_NONE
	int32 GetSelectedIndex() const;

	// Состояние по текущим данным боя и компонента выбора
	EAbilityButtonState ComputeState() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Icon;

	// Число оставшихся ходов перезарядки
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;

	// Подсвеченная рамка выбранной способности
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedFrame;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHeroUnit> Hero;

	UPROPERTY(Transient)
	int32 AbilityIndex = INDEX_NONE;

	UPROPERTY(Transient)
	EAbilityButtonState State = EAbilityButtonState::Unavailable;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSelectionComponent> BoundSelection;
};
