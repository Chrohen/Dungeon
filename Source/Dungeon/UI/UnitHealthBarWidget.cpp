#include "UI/UnitHealthBarWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/HealthPipsWidget.h"

void UUnitHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetOrderNumber(0);
	SkullIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UUnitHealthBarWidget::SetValues(int32 Current, int32 Max, const FLinearColor& Color)
{
	Pips->SetValues(Current, Max, Color);
}

void UUnitHealthBarWidget::SetPreview(int32 LostPips, bool bWillDie)
{
	Pips->SetPreview(LostPips, bWillDie);
	SkullIcon->SetVisibility(bWillDie ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UUnitHealthBarWidget::ClearPreview()
{
	SetPreview(0, false);
}

void UUnitHealthBarWidget::SetOrderNumber(int32 Order)
{
	if (Order <= 0)
	{
		OrderText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	OrderText->SetText(FText::AsNumber(Order));
	OrderText->SetVisibility(ESlateVisibility::HitTestInvisible);
}
