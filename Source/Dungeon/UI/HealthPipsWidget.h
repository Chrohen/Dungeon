#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthPipsWidget.generated.h"

class UHorizontalBox;
class UImage;

// Ряд делений здоровья (GDD: «Здоровье юнитов»). Одно деление - UImage с однотонной кистью
// размера PipSize. Заполненное деление рисуется цветом Color, пустое - EmptyColor.
// Используется всеми виджетами, которые показывают здоровье. Раскладка задаётся в WBP_HealthPips
UCLASS(Abstract)
class DUNGEON_API UHealthPipsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Деления создаются по числу Max, заполнены первые Current. Предпросмотр не сбрасывается
	void SetValues(int32 Current, int32 Max, const FLinearColor& Color);

	// Теряемые деления (последние заполненные, Lost штук) мигают. bWillDie - юнит погибнет:
	// мигание полное, до прозрачности
	void SetPreview(int32 Lost, bool bWillDie);

	void ClearPreview();

	int32 GetMax() const { return PipImages.Num(); }

	int32 GetCurrent() const { return CurrentValue; }

	// Размер одного деления
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pips")
	FVector2D PipSize = FVector2D(9.0, 14.0);

	// Промежуток между соседними делениями
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pips")
	float PipSpacing = 2.f;

	// Цвет пустого деления
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pips")
	FLinearColor EmptyColor = FLinearColor(0.08f, 0.08f, 0.08f, 0.8f);

	// Частота мигания теряемых делений, раз в секунду
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pips")
	float BlinkFrequency = 2.f;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildPips(int32 Max);

	// Красит деления по текущему значению, предпросмотру и фазе мигания
	void UpdatePips();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PipsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PipImages;

	FLinearColor FilledColor = FLinearColor::Green;
	int32 CurrentValue = 0;
	int32 LostPips = 0;
	bool bPreviewLethal = false;
	float BlinkTime = 0.f;
};
