#include "UI/TileLabelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UTileLabelWidget::ShowDamage(int32 Damage)
{
	SetValue(Damage, DamageColor, /*bShowImpact=*/false);
}

void UTileLabelWidget::ShowCollision(int32 Damage)
{
	SetValue(Damage, CollisionColor, /*bShowImpact=*/true);
}

void UTileLabelWidget::SetValue(int32 Value, const FLinearColor& Color, bool bShowImpact)
{
	// Элементы обязательны в WBP_TileLabel, но тесты создают наследника без дерева виджетов
	if (ValueText != nullptr)
	{
		ValueText->SetText(FText::AsNumber(Value));
		ValueText->SetColorAndOpacity(FSlateColor(Color));
	}
	if (ImpactIcon != nullptr)
	{
		ImpactIcon->SetVisibility(bShowImpact ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
