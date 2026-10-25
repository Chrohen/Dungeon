#include "UI/BattleHUDWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/BattleResultWidget.h"
#include "UI/HeroPanelWidget.h"
#include "UI/SquadPanelWidget.h"
#include "UI/UIUtils.h"
#include "UI/UnitTooltipWidget.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"

void UBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBattleSubsystem* Battle = DungeonUI::GetBattle(*this);
	if (Battle == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UBattleHUDWidget::NativeConstruct: UBattleSubsystem is not available."));
		return;
	}

	BoundBattle = Battle;
	EndTurnButton->OnClicked.AddUniqueDynamic(this, &UBattleHUDWidget::OnEndTurnClicked);
	Battle->OnBattleStarted.AddDynamic(this, &UBattleHUDWidget::OnBattleStarted);
	Battle->OnPhaseChanged.AddDynamic(this, &UBattleHUDWidget::OnPhaseChanged);
	Battle->OnTurnStarted.AddDynamic(this, &UBattleHUDWidget::OnTurnStarted);
	Battle->OnWaveChanged.AddDynamic(this, &UBattleHUDWidget::OnWaveChanged);
	Battle->OnBattleEnded.AddDynamic(this, &UBattleHUDWidget::OnBattleEnded);

	UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this);
	if (Selection != nullptr)
	{
		BoundSelection = Selection;
		Selection->OnSelectionChanged.AddDynamic(this, &UBattleHUDWidget::OnSelectionChanged);
		Selection->OnHoveredUnitChanged.AddDynamic(this, &UBattleHUDWidget::OnHoveredUnitChanged);
	}
	else
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("UBattleHUDWidget::NativeConstruct: the owning player is not ABattlePlayerController, panels and tooltip are not updated by the selection."));
	}

	// Бой мог начаться раньше, чем создан HUD
	UpdatePhase(Battle->GetCurrentPhase());
	UpdateTurn(Battle->GetTurnNumber());
	UpdateWave(Battle->GetCurrentWave(), Battle->GetTotalWaves());
	UnitTooltip->SetUnit(nullptr);
	ResultWindow->ShowResult(Battle->GetResult());
	HeroPanel->SetHero(Selection != nullptr ? Selection->GetSelectedHero() : nullptr);
}

void UBattleHUDWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnBattleStarted.RemoveDynamic(this, &UBattleHUDWidget::OnBattleStarted);
		Battle->OnPhaseChanged.RemoveDynamic(this, &UBattleHUDWidget::OnPhaseChanged);
		Battle->OnTurnStarted.RemoveDynamic(this, &UBattleHUDWidget::OnTurnStarted);
		Battle->OnWaveChanged.RemoveDynamic(this, &UBattleHUDWidget::OnWaveChanged);
		Battle->OnBattleEnded.RemoveDynamic(this, &UBattleHUDWidget::OnBattleEnded);
	}
	if (UBattleSelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged.RemoveDynamic(this, &UBattleHUDWidget::OnSelectionChanged);
		Selection->OnHoveredUnitChanged.RemoveDynamic(this, &UBattleHUDWidget::OnHoveredUnitChanged);
	}
	BoundBattle.Reset();
	BoundSelection.Reset();

	if (EndTurnButton != nullptr)
	{
		EndTurnButton->OnClicked.RemoveDynamic(this, &UBattleHUDWidget::OnEndTurnClicked);
	}

	Super::NativeDestruct();
}

void UBattleHUDWidget::OnEndTurnClicked()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->EndPlayerTurn();
	}
}

void UBattleHUDWidget::OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	UpdatePhase(NewPhase);
}

void UBattleHUDWidget::OnTurnStarted(int32 TurnNumber)
{
	UpdateTurn(TurnNumber);
}

void UBattleHUDWidget::OnWaveChanged(int32 CurrentWave, int32 TotalWaves)
{
	UpdateWave(CurrentWave, TotalWaves);
}

void UBattleHUDWidget::OnBattleEnded(EBattleResult Result)
{
	ResultWindow->ShowResult(Result);
}

void UBattleHUDWidget::OnBattleStarted()
{
	// Новый бой: окно скрыто, панели и подсказка очищены (герои придут по OnUnitSpawned)
	ResultWindow->Hide();
	SquadPanel->Clear();
	HeroPanel->SetHero(nullptr);
	UnitTooltip->SetUnit(nullptr);

	if (const UBattleSubsystem* Battle = BoundBattle.Get())
	{
		UpdateTurn(Battle->GetTurnNumber());
		UpdateWave(Battle->GetCurrentWave(), Battle->GetTotalWaves());
	}
}

void UBattleHUDWidget::OnSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex)
{
	// Панели подписаны сами; здесь им передаётся выбранный герой, кнопки способностей ловят выбор способности сами
	HeroPanel->SetHero(Hero);
	SquadPanel->SetSelectedHero(Hero);
}

void UBattleHUDWidget::OnHoveredUnitChanged(ADungeonUnit* Unit)
{
	UnitTooltip->SetUnit(Unit);
}

void UBattleHUDWidget::UpdatePhase(EBattlePhase Phase)
{
	EndTurnButton->SetIsEnabled(Phase == EBattlePhase::PlayerTurn);

	FText Text;
	switch (Phase)
	{
	case EBattlePhase::PlayerTurn:
		Text = NSLOCTEXT("BattleHUD", "PlayerTurn", "Ваш ход");
		break;
	case EBattlePhase::None:
	case EBattlePhase::Finished:
		break;
	default:
		Text = NSLOCTEXT("BattleHUD", "EnemyTurn", "Ход противника");
		break;
	}
	PhaseText->SetText(Text);
}

void UBattleHUDWidget::UpdateTurn(int32 TurnNumber)
{
	TurnText->SetText(FText::Format(NSLOCTEXT("BattleHUD", "Turn", "Ход {0}"), FText::AsNumber(TurnNumber)));
}

void UBattleHUDWidget::UpdateWave(int32 CurrentWave, int32 TotalWaves)
{
	WaveText->SetText(FText::Format(NSLOCTEXT("BattleHUD", "Wave", "Волна {0}/{1}"), FText::AsNumber(CurrentWave), FText::AsNumber(TotalWaves)));
}
