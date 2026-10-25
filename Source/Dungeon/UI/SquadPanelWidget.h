#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/InputTypes.h"
#include "SquadPanelWidget.generated.h"

class ADungeonUnit;
class AHeroUnit;
class UBattleSelectionComponent;
class UBattleSubsystem;
class UHeroCardWidget;
class UVerticalBox;

// Вертикальный список карточек героев слева (GDD: «Панель отряда»). Раскладка задаётся в WBP_SquadPanel
UCLASS(Abstract)
class DUNGEON_API USquadPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// WBP_HeroCard
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad")
	TSubclassOf<UHeroCardWidget> CardClass;

	// Убирает все карточки (начало нового боя)
	void Clear();

	// Рамка у карточки выбранного героя, у остальных снимается
	void SetSelectedHero(const AHeroUnit* SelectedHero);

	// Карточки по порядку появления героев
	const TArray<TObjectPtr<UHeroCardWidget>>& GetCards() const { return Cards; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void OnBattleStarted();

	UFUNCTION()
	void OnUnitSpawned(ADungeonUnit* Unit);

	UFUNCTION()
	void OnUnitChanged(ADungeonUnit* Unit);

	UFUNCTION()
	void OnSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex);

	// Пересоздаёт карточки всех героев боя, включая погибших
	void Rebuild();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> CardsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHeroCardWidget>> Cards;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSelectionComponent> BoundSelection;
};
