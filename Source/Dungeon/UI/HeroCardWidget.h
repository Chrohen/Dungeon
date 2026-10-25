#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HeroCardWidget.generated.h"

class AHeroUnit;
class UButton;
class UHealthPipsWidget;
class UImage;
class UTextBlock;

// Карточка героя в панели отряда (GDD: «Панель отряда»). Сама ни на что не подписана: обновляет её USquadPanelWidget.
// Раскладка задаётся в WBP_HeroCard
UCLASS(Abstract)
class DUNGEON_API UHeroCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязывает карточку к герою и обновляет её
	void SetHero(AHeroUnit* InHero);

	AHeroUnit* GetHero() const { return Hero.Get(); }

	// Иконка класса, имя, деления, отметки, затемнение погибшего
	void Refresh();

	// Рамка выбранного героя
	void SetSelected(bool bSelected);

	// Цвет отметки, если действие доступно в этом ходу
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero Card")
	FLinearColor MarkAvailableColor = FLinearColor::White;

	// Цвет отметки, если действие выполнено
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero Card")
	FLinearColor MarkSpentColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void OnSelectClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ClassIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHealthPipsWidget> Pips;

	// Значок сапога
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MoveMark;

	// Значок меча
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> AbilityMark;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectionFrame;

	// Затемнение и перечёркивание погибшего
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> DeadOverlay;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHeroUnit> Hero;
};
