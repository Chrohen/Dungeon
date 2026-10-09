#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "BattleHUDWidget.generated.h"

class UBattleSubsystem;
class UButton;
class UTextBlock;

// HUD боя: кнопка завершения хода и отладочные тексты. Раскладка задаётся в WBP_BattleHUD
UCLASS(Abstract)
class DUNGEON_API UBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void OnEndTurnClicked();

	UFUNCTION()
	void OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void OnTurnStarted(int32 TurnNumber);

	void UpdatePhase(EBattlePhase Phase);
	void UpdateTurn(int32 TurnNumber);

	// Кнопка завершения хода
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EndTurnButton;

	// «Ход N», для отладки
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	// Название фазы, для отладки
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;
};
