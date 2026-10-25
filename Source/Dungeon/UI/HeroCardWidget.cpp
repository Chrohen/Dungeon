#include "UI/HeroCardWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/UnitDefinition.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/HealthPipsWidget.h"
#include "UI/UIUtils.h"
#include "Units/HeroUnit.h"
#include "Units/UnitHealthBarComponent.h"

void UHeroCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SelectButton->OnClicked.AddUniqueDynamic(this, &UHeroCardWidget::OnSelectClicked);
	SelectionFrame->SetVisibility(ESlateVisibility::Hidden);
	DeadOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

void UHeroCardWidget::SetHero(AHeroUnit* InHero)
{
	Hero = InHero;
	Refresh();
}

void UHeroCardWidget::Refresh()
{
	const AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr)
	{
		return;
	}

	DungeonUI::SetIcon(ClassIcon, Unit->Definition != nullptr ? Unit->Definition->Icon.Get() : nullptr);
	NameText->SetText(Unit->GetDisplayName());
	Pips->SetValues(Unit->GetHealth(), Unit->GetMaxHealth(), Unit->HealthBar != nullptr ? Unit->HealthBar->HeroColor : FLinearColor::Green);

	// Отметки яркие, если действие доступно в этом ходу, и серые, если выполнено (у погибшего оба недоступны)
	MoveMark->SetColorAndOpacity(Unit->CanMove() ? MarkAvailableColor : MarkSpentColor);
	AbilityMark->SetColorAndOpacity(Unit->CanAct() ? MarkAvailableColor : MarkSpentColor);

	// Погибший затемнён и перечёркнут, кнопка неактивна
	DeadOverlay->SetVisibility(Unit->IsAlive() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SelectButton->SetIsEnabled(Unit->IsAlive());
}

void UHeroCardWidget::SetSelected(bool bSelected)
{
	SelectionFrame->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UHeroCardWidget::OnSelectClicked()
{
	AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr || !Unit->IsAlive())
	{
		return;
	}
	if (UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this))
	{
		Selection->SelectHeroFromCard(Unit);
	}
}
