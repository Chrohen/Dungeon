#include "UI/HeroPanelWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/UnitDefinition.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/AbilityButtonWidget.h"
#include "UI/HealthPipsWidget.h"
#include "UI/UIUtils.h"
#include "Units/HeroUnit.h"
#include "Units/UnitHealthBarComponent.h"

void UHeroPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBattleSubsystem* Battle = DungeonUI::GetBattle(*this);
	if (Battle != nullptr)
	{
		BoundBattle = Battle;
		Battle->OnUnitChanged.AddDynamic(this, &UHeroPanelWidget::OnUnitChanged);
		Battle->OnBattleStarted.AddDynamic(this, &UHeroPanelWidget::OnBattleStarted);
	}
	else
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UHeroPanelWidget::NativeConstruct: UBattleSubsystem is not available."));
	}

	UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this);
	if (Selection != nullptr)
	{
		BoundSelection = Selection;
		Selection->OnSelectionChanged.AddDynamic(this, &UHeroPanelWidget::OnSelectionChanged);
	}

	// Герой мог быть выбран раньше, чем создана панель
	SetHero(Selection != nullptr ? Selection->GetSelectedHero() : nullptr);
}

void UHeroPanelWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnUnitChanged.RemoveDynamic(this, &UHeroPanelWidget::OnUnitChanged);
		Battle->OnBattleStarted.RemoveDynamic(this, &UHeroPanelWidget::OnBattleStarted);
	}
	if (UBattleSelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged.RemoveDynamic(this, &UHeroPanelWidget::OnSelectionChanged);
	}
	BoundBattle.Reset();
	BoundSelection.Reset();

	Super::NativeDestruct();
}

void UHeroPanelWidget::SetHero(AHeroUnit* InHero)
{
	if (InHero == nullptr)
	{
		Hero.Reset();
		AbilityButtons.Reset();
		AbilitiesBox->ClearChildren();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bHeroChanged = Hero.Get() != InHero;
	Hero = InHero;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (bHeroChanged)
	{
		RebuildAbilityButtons();
	}
	Refresh();
}

void UHeroPanelWidget::Refresh()
{
	const AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr)
	{
		return;
	}

	DungeonUI::SetIcon(ClassIcon, Unit->Definition != nullptr ? Unit->Definition->Icon.Get() : nullptr);
	NameText->SetText(Unit->GetDisplayName());
	ClassText->SetText(Unit->Definition != nullptr ? Unit->Definition->DisplayName : FText::GetEmpty());
	LevelText->SetText(FText::Format(NSLOCTEXT("HeroPanel", "Level", "Уровень {0}"), FText::AsNumber(Unit->Level)));

	Pips->SetValues(Unit->GetHealth(), Unit->GetMaxHealth(), Unit->HealthBar != nullptr ? Unit->HealthBar->HeroColor : FLinearColor::Green);
	HealthText->SetText(FText::Format(NSLOCTEXT("HeroPanel", "Health", "{0}/{1}"), FText::AsNumber(Unit->GetHealth()), FText::AsNumber(Unit->GetMaxHealth())));

	// Очки перемещения героя из определения (GetMovePoints)
	MovePointsText->SetText(FText::Format(NSLOCTEXT("HeroPanel", "MovePoints", "Очки перемещения: {0}"), FText::AsNumber(Unit->GetMovePoints())));

	// Отметки, как на карточке отряда
	MoveMark->SetColorAndOpacity(Unit->CanMove() ? MarkAvailableColor : MarkSpentColor);
	AbilityMark->SetColorAndOpacity(Unit->CanAct() ? MarkAvailableColor : MarkSpentColor);
}

void UHeroPanelWidget::OnSelectionChanged(ESelectionState State, AHeroUnit* SelectedHero, int32 AbilityIndex)
{
	SetHero(SelectedHero);
}

void UHeroPanelWidget::OnUnitChanged(ADungeonUnit* Unit)
{
	if (Unit != nullptr && Unit == Hero.Get())
	{
		Refresh();
	}
}

void UHeroPanelWidget::OnBattleStarted()
{
	SetHero(nullptr);
}

void UHeroPanelWidget::RebuildAbilityButtons()
{
	AbilityButtons.Reset();
	AbilitiesBox->ClearChildren();

	AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr || AbilityButtonClass == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < Unit->GetAbilities().Num(); ++Index)
	{
		UAbilityButtonWidget* Button = CreateWidget<UAbilityButtonWidget>(this, AbilityButtonClass);
		if (Button == nullptr)
		{
			continue;
		}
		AbilitiesBox->AddChildToHorizontalBox(Button);
		Button->SetAbility(Unit, Index);
		AbilityButtons.Add(Button);
	}
}
