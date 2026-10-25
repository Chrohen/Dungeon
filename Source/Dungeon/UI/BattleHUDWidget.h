#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "Input/InputTypes.h"
#include "BattleHUDWidget.generated.h"

class AHeroUnit;
class ADungeonUnit;
class UBattleResultWidget;
class UBattleSelectionComponent;
class UBattleSubsystem;
class UButton;
class UHeroPanelWidget;
class USquadPanelWidget;
class UTextBlock;
class UUnitTooltipWidget;

// HUD боя (GDD: «Интерфейс»): счётчики, кнопка завершения хода, панели отряда и героя, подсказка юнита и окно
// завершения боя. Раскладка задаётся в WBP_BattleHUD
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

	UFUNCTION()
	void OnWaveChanged(int32 CurrentWave, int32 TotalWaves);

	UFUNCTION()
	void OnBattleEnded(EBattleResult Result);

	UFUNCTION()
	void OnBattleStarted();

	UFUNCTION()
	void OnSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex);

	UFUNCTION()
	void OnHoveredUnitChanged(ADungeonUnit* Unit);

	void UpdatePhase(EBattlePhase Phase);
	void UpdateTurn(int32 TurnNumber);
	void UpdateWave(int32 CurrentWave, int32 TotalWaves);

	// Кнопка завершения хода
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> EndTurnButton;

	// «Ход N»
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TurnText;

	// «Волна N/M»
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WaveText;

	// «Ваш ход» в PlayerTurn, «Ход противника» в остальных фазах боя, пусто в None и Finished
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USquadPanelWidget> SquadPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHeroPanelWidget> HeroPanel;

	// По умолчанию скрыта
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUnitTooltipWidget> UnitTooltip;

	// По умолчанию скрыто
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBattleResultWidget> ResultWindow;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBattleSelectionComponent> BoundSelection;
};
