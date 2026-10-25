#include "UI/SquadPanelWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/VerticalBox.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/HeroCardWidget.h"
#include "UI/UIUtils.h"
#include "Units/HeroUnit.h"

void USquadPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBattleSubsystem* Battle = DungeonUI::GetBattle(*this);
	if (Battle == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("USquadPanelWidget::NativeConstruct: UBattleSubsystem is not available."));
		return;
	}

	BoundBattle = Battle;
	Battle->OnBattleStarted.AddDynamic(this, &USquadPanelWidget::OnBattleStarted);
	Battle->OnUnitSpawned.AddDynamic(this, &USquadPanelWidget::OnUnitSpawned);
	Battle->OnUnitChanged.AddDynamic(this, &USquadPanelWidget::OnUnitChanged);

	if (UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this))
	{
		BoundSelection = Selection;
		Selection->OnSelectionChanged.AddDynamic(this, &USquadPanelWidget::OnSelectionChanged);
	}

	// Бой мог начаться раньше, чем создана панель
	Rebuild();
}

void USquadPanelWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnBattleStarted.RemoveDynamic(this, &USquadPanelWidget::OnBattleStarted);
		Battle->OnUnitSpawned.RemoveDynamic(this, &USquadPanelWidget::OnUnitSpawned);
		Battle->OnUnitChanged.RemoveDynamic(this, &USquadPanelWidget::OnUnitChanged);
	}
	if (UBattleSelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged.RemoveDynamic(this, &USquadPanelWidget::OnSelectionChanged);
	}
	BoundBattle.Reset();
	BoundSelection.Reset();

	Super::NativeDestruct();
}

void USquadPanelWidget::OnBattleStarted()
{
	// Герои прошлого боя уничтожены, новые придут по OnUnitSpawned
	Clear();
}

void USquadPanelWidget::Clear()
{
	CardsBox->ClearChildren();
	Cards.Reset();
}

void USquadPanelWidget::OnUnitSpawned(ADungeonUnit* Unit)
{
	if (Cast<AHeroUnit>(Unit) != nullptr)
	{
		Rebuild();
	}
}

void USquadPanelWidget::OnUnitChanged(ADungeonUnit* Unit)
{
	for (UHeroCardWidget* Card : Cards)
	{
		if (Card != nullptr && Card->GetHero() == Unit)
		{
			Card->Refresh();
		}
	}
}

void USquadPanelWidget::OnSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex)
{
	SetSelectedHero(Hero);
}

void USquadPanelWidget::Rebuild()
{
	CardsBox->ClearChildren();
	Cards.Reset();

	const UBattleSubsystem* Battle = BoundBattle.Get();
	if (Battle == nullptr || CardClass == nullptr)
	{
		return;
	}

	for (AHeroUnit* Hero : Battle->GetHeroes())
	{
		UHeroCardWidget* Card = CreateWidget<UHeroCardWidget>(this, CardClass);
		if (Card == nullptr)
		{
			continue;
		}
		CardsBox->AddChildToVerticalBox(Card);
		Card->SetHero(Hero);
		Cards.Add(Card);
	}

	const UBattleSelectionComponent* Selection = BoundSelection.Get();
	SetSelectedHero(Selection != nullptr ? Selection->GetSelectedHero() : nullptr);
}

void USquadPanelWidget::SetSelectedHero(const AHeroUnit* SelectedHero)
{
	for (UHeroCardWidget* Card : Cards)
	{
		if (Card != nullptr)
		{
			Card->SetSelected(SelectedHero != nullptr && Card->GetHero() == SelectedHero);
		}
	}
}
