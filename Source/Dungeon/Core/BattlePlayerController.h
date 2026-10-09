#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Input/InputTypes.h"
#include "BattlePlayerController.generated.h"

class UBattleHUDWidget;
class UBattleSelectionComponent;
class UInputAction;
class UInputMappingContext;

// Контроллер боя: настраивает ввод и HUD, определяет цель под курсором и передаёт ввод
// компоненту выбора. Игровых правил не содержит
UCLASS(Blueprintable)
class DUNGEON_API ABattlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABattlePlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	// Что под курсором (TDD 8.2). Без боя или сетки возвращает пустую цель
	FCursorTarget ResolveCursorTarget() const;

	// IMC_Battle
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputContext;

	// IA_Select
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SelectAction;

	// IA_ConfirmMove
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ConfirmAction;

	// IA_Cancel
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CancelAction;

	// WBP_BattleHUD
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UBattleHUDWidget> HUDWidgetClass;

	// Стейт-машина выбора
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UBattleSelectionComponent> Selection;

	// Цель прошлого PlayerTick, чтобы обрабатывать наведение только при смене клетки
	FCursorTarget LastHoverTarget;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnSelectTriggered(const FInputActionValue& Value);
	void OnConfirmTriggered(const FInputActionValue& Value);
	void OnCancelTriggered(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDWidget> HUD;
};
