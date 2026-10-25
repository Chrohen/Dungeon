#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleTypes.h"
#include "UI/UITypes.h"
#include "BattleResultWidget.generated.h"

class UButton;
class UTextBlock;

// Окно завершения боя (TD 7.6): награды и переход после победы, «Отряд погиб» после поражения.
// //CURRENT-STAGE: все кнопки перезапускают бой через ADungeonBattleGameMode::RestartBattle.
// Раскладка задаётся в WBP_BattleResult
UCLASS(Abstract)
class DUNGEON_API UBattleResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Открывает экран Rewards (победа) или Defeat (поражение). None скрывает окно
	void ShowResult(EBattleResult Result);

	// Скрывает окно
	void Hide();

	EBattleResultScreen GetScreen() const { return Screen; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void OnPrimaryClicked();

	UFUNCTION()
	void OnSecondaryClicked();

	void ShowScreen(EBattleResultScreen NewScreen);

	// Решение 13: GetWorld()->GetAuthGameMode<ADungeonBattleGameMode>()->RestartBattle(). Без GameMode - предупреждение
	void RestartBattle();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RewardsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrimaryButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PrimaryText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SecondaryButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SecondaryText;

	UPROPERTY(Transient)
	EBattleResultScreen Screen = EBattleResultScreen::None;
};
