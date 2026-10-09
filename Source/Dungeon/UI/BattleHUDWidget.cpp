#include "UI/BattleHUDWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBattleSubsystem* Battle = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBattleSubsystem>() : nullptr;
	if (Battle == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UBattleHUDWidget::NativeConstruct: UBattleSubsystem is not available."));
		return;
	}

	BoundBattle = Battle;
	EndTurnButton->OnClicked.AddUniqueDynamic(this, &UBattleHUDWidget::OnEndTurnClicked);
	Battle->OnPhaseChanged.AddDynamic(this, &UBattleHUDWidget::OnPhaseChanged);
	Battle->OnTurnStarted.AddDynamic(this, &UBattleHUDWidget::OnTurnStarted);

	// Бой мог начаться раньше, чем создан HUD
	UpdatePhase(Battle->GetCurrentPhase());
	UpdateTurn(Battle->GetTurnNumber());
}

void UBattleHUDWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnPhaseChanged.RemoveDynamic(this, &UBattleHUDWidget::OnPhaseChanged);
		Battle->OnTurnStarted.RemoveDynamic(this, &UBattleHUDWidget::OnTurnStarted);
	}
	BoundBattle.Reset();

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

void UBattleHUDWidget::UpdatePhase(EBattlePhase Phase)
{
	EndTurnButton->SetIsEnabled(Phase == EBattlePhase::PlayerTurn);
	if (PhaseText != nullptr)
	{
		PhaseText->SetText(UEnum::GetDisplayValueAsText(Phase));
	}
}

void UBattleHUDWidget::UpdateTurn(int32 TurnNumber)
{
	if (TurnText != nullptr)
	{
		TurnText->SetText(FText::Format(NSLOCTEXT("BattleHUD", "Turn", "Ход {0}"), FText::AsNumber(TurnNumber)));
	}
}
