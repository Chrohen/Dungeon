#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitHealthBarWidget.generated.h"

class UHealthPipsWidget;
class UImage;
class UTextBlock;

// Виджет UUnitHealthBarComponent: деления здоровья, номер порядка реализации (монстры) и
// череп предпросмотра смерти. Раскладка задаётся в WBP_UnitHealthBar
UCLASS(Abstract)
class DUNGEON_API UUnitHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetValues(int32 Current, int32 Max, const FLinearColor& Color);

	// Мигание теряемых делений; череп показывается, если юнит погибнет
	void SetPreview(int32 LostPips, bool bWillDie);

	void ClearPreview();

	// Номер порядка над делениями. 0 и меньше - номер скрыт
	void SetOrderNumber(int32 Order);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHealthPipsWidget> Pips;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OrderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkullIcon;
};
