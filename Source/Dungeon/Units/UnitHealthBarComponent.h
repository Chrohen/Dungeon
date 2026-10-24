#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UnitHealthBarComponent.generated.h"

class ADungeonUnit;
class UUnitHealthBarWidget;

// Деления здоровья над моделью юнита (GDD: «Здоровье юнитов»): экранный виджет WBP_UnitHealthBar.
// Класс виджета и высоту над моделью задаёт BP_*Unit_*. Без класса виджета (тесты) ничего не делает
UCLASS(ClassGroup = (Dungeon), meta = (BlueprintSpawnableComponent))
class DUNGEON_API UUnitHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UUnitHealthBarComponent();

	// Число делений - GetMaxHealth, заполненные - GetHealth, цвет по команде. Скрыт, если юнит погиб.
	// Вызывает ADungeonUnit::RefreshVisuals
	void Refresh(const ADungeonUnit& Unit);

	// Мигание теряемых делений и череп. Вызывает UBattleOverlayComponent
	void SetPreview(int32 LostPips, bool bWillDie);

	// Вызывает UBattleOverlayComponent
	void ClearPreview();

	// Номер порядка реализации над делениями монстра; 0 - скрыт. Вызывает UBattleOverlayComponent
	void SetOrderNumber(int32 Order);

	// Цвет делений героев (GDD: зелёные, цвета временные)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar")
	FLinearColor HeroColor = FLinearColor(0.2f, 0.8f, 0.2f);

	// Цвет делений монстров (GDD: красные)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health Bar")
	FLinearColor MonsterColor = FLinearColor(0.85f, 0.1f, 0.1f);

protected:
	// Виджет может появиться после первого Refresh (создаётся при регистрации или в BeginPlay):
	// тогда на него переносится последнее состояние
	virtual void InitWidget() override;

private:
	UUnitHealthBarWidget* GetHealthBarWidget() const;

	TWeakObjectPtr<const ADungeonUnit> LastUnit;
};
