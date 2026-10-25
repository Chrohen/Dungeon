#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "UnitTooltipWidget.generated.h"

class ADungeonUnit;
class AMonsterUnit;
class UBattleSubsystem;
class UHealthPipsWidget;
class UTextBlock;

// Подсказка юнита под курсором (GDD: «Подсказка юнита»): имя героя или тип монстра, деления и «текущее/максимальное»,
// для монстра - намерение. Следует за курсором. Раскладка задаётся в WBP_UnitTooltip: виджет лежит в Canvas Panel
// HUD с якорем в левом верхнем углу и выравниванием (0, 0)
UCLASS(Abstract)
class DUNGEON_API UUnitTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Показывает подсказку юнита, nullptr (или погибший юнит) скрывает её (Collapsed)
	void SetUnit(ADungeonUnit* InUnit);

	// Имя, деления и намерение по текущему состоянию юнита
	void Refresh();

	ADungeonUnit* GetUnit() const;

	// Наибольшее TileDamage намерения (решение 12): для удара по клетке это урон удара, для способности
	// по площади - урон по центру. 0, если урона нет
	static int32 GetIntentDamage(const FIntentView& View);

	// «<способность>, урон N, порядок K» или «Нет намерения», если у монстра нет намерения среди Views
	static FText MakeIntentText(const AMonsterUnit& Monster, const TArray<FIntentView>& Views);

	// Отступ подсказки от курсора
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tooltip")
	FVector2D CursorOffset = FVector2D(16.0, 16.0);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void OnUnitChanged(ADungeonUnit* Unit);

	UFUNCTION()
	void OnIntentsChanged();

	// Ставит подсказку у курсора, не выпуская за край окна
	void UpdatePosition();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHealthPipsWidget> Pips;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> IntentText;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADungeonUnit> Unit;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;
};
