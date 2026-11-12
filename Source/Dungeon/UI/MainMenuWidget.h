#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UMenuSettingsWidget;
class UWidget;

// The Widget Blueprint owns the layout, text, colors, and button styles.
UCLASS(Abstract, Blueprintable)
class DUNGEON_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RefreshMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void FocusDefaultButton();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMainMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UWidget> MainPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UMenuSettingsWidget> SettingsPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UWidget> NewGameConfirmPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> ConfirmNewGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> CancelNewGameButton;

private:
	UFUNCTION()
	void OnSettingsClosed();

	UFUNCTION()
	void OnNewGameClicked();

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnConfirmNewGameClicked();

	UFUNCTION()
	void OnCancelNewGameClicked();
};
