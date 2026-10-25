#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/InputTypes.h"
#include "HeroPanelWidget.generated.h"

class ADungeonUnit;
class AHeroUnit;
class UAbilityButtonWidget;
class UBattleSelectionComponent;
class UBattleSubsystem;
class UHealthPipsWidget;
class UHorizontalBox;
class UImage;
class UTextBlock;

// Панель выбранного героя внизу экрана (GDD: «Панель героя»). Скрыта (Collapsed), пока герой не выбран.
// Раскладка задаётся в WBP_HeroPanel
UCLASS(Abstract)
class DUNGEON_API UHeroPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Показывает героя (nullptr - скрывает панель). Кнопки способностей пересоздаются, если герой сменился
	void SetHero(AHeroUnit* InHero);

	// Иконка, имена, здоровье, очки перемещения и отметки. Кнопки обновляются сами
	void Refresh();

	AHeroUnit* GetHero() const { return Hero.Get(); }

	// Кнопки способностей героя по порядку списка
	const TArray<TObjectPtr<UAbilityButtonWidget>>& GetAbilityButtons() const { return AbilityButtons; }

	// WBP_AbilityButton
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero Panel")
	TSubclassOf<UAbilityButtonWidget> AbilityButtonClass;

	// Цвет отметки, если действие доступно в этом ходу
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero Panel")
	FLinearColor MarkAvailableColor = FLinearColor::White;

	// Цвет отметки, если действие выполнено
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero Panel")
	FLinearColor MarkSpentColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void OnSelectionChanged(ESelectionState State, AHeroUnit* SelectedHero, int32 AbilityIndex);

	UFUNCTION()
	void OnUnitChanged(ADungeonUnit* Unit);

	UFUNCTION()
	void OnBattleStarted();

	void RebuildAbilityButtons();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ClassIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClassText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHealthPipsWidget> Pips;

	// «текущее/максимальное»
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MovePointsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MoveMark;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> AbilityMark;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> AbilitiesBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilityButtonWidget>> AbilityButtons;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHeroUnit> Hero;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSelectionComponent> BoundSelection;
};
