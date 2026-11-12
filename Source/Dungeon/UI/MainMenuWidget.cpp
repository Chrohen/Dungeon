#include "UI/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/DungeonMenuPlayerController.h"
#include "InputCoreTypes.h"
#include "UI/MenuSettingsWidget.h"

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	NewGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnNewGameClicked);
	ContinueButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnContinueClicked);
	SettingsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::ShowSettings);
	QuitButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnQuitClicked);
	ConfirmNewGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnConfirmNewGameClicked);
	CancelNewGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnCancelNewGameClicked);
	SettingsPanel->OnClosed.AddUniqueDynamic(this, &UMainMenuWidget::OnSettingsClosed);
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ShowMainMenu();
}

void UMainMenuWidget::RefreshMenu()
{
	ADungeonMenuPlayerController* Controller = GetOwningPlayer<ADungeonMenuPlayerController>();
	const bool bCanContinue = Controller != nullptr && Controller->CanContinueGame();
	ContinueButton->SetIsEnabled(bCanContinue);
	const FText Error = Controller != nullptr ? Controller->LastMenuError : FText::GetEmpty();
	StatusText->SetText(!Error.IsEmpty() ? Error : (bCanContinue ? FText::GetEmpty()
		: NSLOCTEXT("MainMenu", "NoSave", "Нет доступного сохранения")));
	StatusText->SetVisibility(StatusText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMainMenuWidget::FocusDefaultButton()
{
	if (SettingsPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		SettingsPanel->FocusBackButton();
	}
	else if (NewGameConfirmPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		CancelNewGameButton->SetKeyboardFocus();
	}
	else
	{
		(ContinueButton->GetIsEnabled() ? ContinueButton : NewGameButton)->SetKeyboardFocus();
	}
}

void UMainMenuWidget::ShowSettings()
{
	MainPanel->SetVisibility(ESlateVisibility::Collapsed);
	NewGameConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	SettingsPanel->SetVisibility(ESlateVisibility::Visible);
	SettingsPanel->FocusBackButton();
}

void UMainMenuWidget::ShowMainMenu()
{
	MainPanel->SetVisibility(ESlateVisibility::Visible);
	SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	NewGameConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	RefreshMenu();
	FocusDefaultButton();
}

void UMainMenuWidget::OnSettingsClosed()
{
	ShowMainMenu();
	SettingsButton->SetKeyboardFocus();
}

void UMainMenuWidget::OnNewGameClicked()
{
	ADungeonMenuPlayerController* Controller = GetOwningPlayer<ADungeonMenuPlayerController>();
	if (Controller == nullptr) return;
	if (Controller->HasExistingSave())
	{
		MainPanel->SetVisibility(ESlateVisibility::Collapsed);
		NewGameConfirmPanel->SetVisibility(ESlateVisibility::Visible);
		CancelNewGameButton->SetKeyboardFocus();
		return;
	}
	Controller->StartNewGame();
}

void UMainMenuWidget::OnContinueClicked()
{
	if (ADungeonMenuPlayerController* Controller = GetOwningPlayer<ADungeonMenuPlayerController>())
	{
		Controller->ContinueGame();
	}
}

void UMainMenuWidget::OnQuitClicked()
{
	if (ADungeonMenuPlayerController* Controller = GetOwningPlayer<ADungeonMenuPlayerController>())
	{
		Controller->QuitGame();
	}
}

void UMainMenuWidget::OnConfirmNewGameClicked()
{
	if (ADungeonMenuPlayerController* Controller = GetOwningPlayer<ADungeonMenuPlayerController>())
	{
		Controller->StartNewGame();
		// Restore the main panel if travel fails before it starts.
		ShowMainMenu();
	}
}

void UMainMenuWidget::OnCancelNewGameClicked()
{
	ShowMainMenu();
	NewGameButton->SetKeyboardFocus();
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (NewGameConfirmPanel->GetVisibility() == ESlateVisibility::Visible)
		{
			OnCancelNewGameClicked();
			return FReply::Handled();
		}
		if (SettingsPanel->GetVisibility() == ESlateVisibility::Visible)
		{
			OnSettingsClosed();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}
