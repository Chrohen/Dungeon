#include "UI/BattleResultWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/DungeonBattleGameMode.h"
#include "Engine/World.h"

void UBattleResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	PrimaryButton->OnClicked.AddUniqueDynamic(this, &UBattleResultWidget::OnPrimaryClicked);
	SecondaryButton->OnClicked.AddUniqueDynamic(this, &UBattleResultWidget::OnSecondaryClicked);
	Hide();
}

void UBattleResultWidget::ShowResult(EBattleResult Result)
{
	switch (Result)
	{
	case EBattleResult::Victory:
		ShowScreen(EBattleResultScreen::Rewards);
		break;
	case EBattleResult::Defeat:
		ShowScreen(EBattleResultScreen::Defeat);
		break;
	default:
		Hide();
		break;
	}
}

void UBattleResultWidget::Hide()
{
	ShowScreen(EBattleResultScreen::None);
}

void UBattleResultWidget::ShowScreen(EBattleResultScreen NewScreen)
{
	Screen = NewScreen;
	if (Screen == EBattleResultScreen::None)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::Visible);

	const bool bRewards = Screen == EBattleResultScreen::Rewards;
	const bool bTransition = Screen == EBattleResultScreen::Transition;

	// Заголовок есть на экранах Rewards и Defeat, список наград - только на Rewards
	TitleText->SetVisibility(bTransition ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	TitleText->SetText(bRewards ? NSLOCTEXT("BattleResult", "Rewards", "Этаж зачищен") : NSLOCTEXT("BattleResult", "Defeat", "Отряд погиб"));
	RewardsText->SetVisibility(bRewards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// //CURRENT-STAGE: список наград пуст
	RewardsText->SetText(NSLOCTEXT("BattleResult", "NoRewards", "Награды отсутствуют"));

	if (bRewards)
	{
		PrimaryText->SetText(NSLOCTEXT("BattleResult", "Next", "Далее"));
	}
	else if (bTransition)
	{
		PrimaryText->SetText(NSLOCTEXT("BattleResult", "NextFloor", "Следующий этаж"));
	}
	else
	{
		PrimaryText->SetText(NSLOCTEXT("BattleResult", "ReturnToTown", "Вернуться в город"));
	}

	SecondaryText->SetText(NSLOCTEXT("BattleResult", "ReturnToTown", "Вернуться в город"));
	SecondaryButton->SetVisibility(bTransition ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UBattleResultWidget::OnPrimaryClicked()
{
	switch (Screen)
	{
	case EBattleResultScreen::Rewards:
		ShowScreen(EBattleResultScreen::Transition);
		break;
	case EBattleResultScreen::Transition:
	case EBattleResultScreen::Defeat:
		RestartBattle();
		break;
	default:
		break;
	}
}

void UBattleResultWidget::OnSecondaryClicked()
{
	if (Screen == EBattleResultScreen::Transition)
	{
		RestartBattle();
	}
}

void UBattleResultWidget::RestartBattle()
{
	ADungeonBattleGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ADungeonBattleGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("UBattleResultWidget::RestartBattle: there is no ADungeonBattleGameMode, the button does nothing."));
		return;
	}
	GameMode->RestartBattle();
}
