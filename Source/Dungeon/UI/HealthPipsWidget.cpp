#include "UI/HealthPipsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"

namespace
{
	// Наименьшая непрозрачность теряемого деления при обычном и при смертельном уроне
	constexpr float BlinkMinAlpha = 0.35f;
	constexpr float LethalBlinkMinAlpha = 0.f;
}

void UHealthPipsWidget::SetValues(int32 Current, int32 Max, const FLinearColor& Color)
{
	Max = FMath::Max(Max, 0);
	CurrentValue = FMath::Clamp(Current, 0, Max);
	FilledColor = Color;
	if (PipImages.Num() != Max)
	{
		RebuildPips(Max);
	}
	UpdatePips();
}

void UHealthPipsWidget::SetPreview(int32 Lost, bool bWillDie)
{
	LostPips = FMath::Max(Lost, 0);
	bPreviewLethal = bWillDie;
	BlinkTime = 0.f;
	UpdatePips();
}

void UHealthPipsWidget::ClearPreview()
{
	SetPreview(0, false);
}

void UHealthPipsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (LostPips > 0)
	{
		BlinkTime += InDeltaTime;
		UpdatePips();
	}
}

void UHealthPipsWidget::RebuildPips(int32 Max)
{
	if (PipsBox == nullptr)
	{
		return;
	}

	PipsBox->ClearChildren();
	PipImages.Reset();
	for (int32 Index = 0; Index < Max; ++Index)
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush = FSlateColorBrush(FLinearColor::White);
		Brush.ImageSize = FVector2f(PipSize);
		Pip->SetBrush(Brush);

		UHorizontalBoxSlot* BoxSlot = PipsBox->AddChildToHorizontalBox(Pip);
		BoxSlot->SetPadding(FMargin(PipSpacing * 0.5f, 0.f));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
		PipImages.Add(Pip);
	}
}

void UHealthPipsWidget::UpdatePips()
{
	// Теряемые деления - последние из заполненных
	const int32 FirstLost = FMath::Max(CurrentValue - LostPips, 0);
	const float MinAlpha = bPreviewLethal ? LethalBlinkMinAlpha : BlinkMinAlpha;
	const float Phase = 0.5f + 0.5f * FMath::Sin(BlinkTime * BlinkFrequency * UE_TWO_PI);
	const float BlinkAlpha = FMath::Lerp(MinAlpha, 1.f, Phase);

	for (int32 Index = 0; Index < PipImages.Num(); ++Index)
	{
		const bool bFilled = Index < CurrentValue;
		FLinearColor Color = bFilled ? FilledColor : EmptyColor;
		if (bFilled && Index >= FirstLost && LostPips > 0)
		{
			Color.A *= BlinkAlpha;
		}
		PipImages[Index]->SetColorAndOpacity(Color);
	}
}
