#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TileLabelWidget.generated.h"

class UImage;
class UTextBlock;

// Подпись на клетке поля (GDD: «Намерение монстров», «Предпросмотр результата»): число урона
// или значок удара с числом. Подписи лежат в пуле UBattleOverlayComponent. Раскладка задаётся в WBP_TileLabel
UCLASS(Abstract)
class DUNGEON_API UTileLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Красное число урона, значок удара скрыт
	void ShowDamage(int32 Damage);

	// Значок удара и число урона столкновения
	void ShowCollision(int32 Damage);

	// Цвет числа урона (GDD: красное, цвета временные)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Label")
	FLinearColor DamageColor = FLinearColor(0.9f, 0.1f, 0.1f);

	// Цвет числа урона столкновения
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Label")
	FLinearColor CollisionColor = FLinearColor(1.f, 0.85f, 0.2f);

private:
	void SetValue(int32 Value, const FLinearColor& Color, bool bShowImpact);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ImpactIcon;
};
