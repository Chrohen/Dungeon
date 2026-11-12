#include "UI/MenuSettingsWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"

void UMenuSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BackButton->OnClicked.AddUniqueDynamic(this, &UMenuSettingsWidget::CloseSettings);
}

void UMenuSettingsWidget::FocusBackButton()
{
	BackButton->SetKeyboardFocus();
}

void UMenuSettingsWidget::CloseSettings()
{
	OnClosed.Broadcast();
}

FReply UMenuSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		CloseSettings();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}
