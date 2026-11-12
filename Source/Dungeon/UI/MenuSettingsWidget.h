#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuSettingsWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuSettingsClosed);

// Set the layout and future controls in WBP_MenuSettings.
UCLASS(Abstract, Blueprintable)
class DUNGEON_API UMenuSettingsWidget : public UUserWidget
{

	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FOnMenuSettingsClosed OnClosed;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void FocusBackButton();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void CloseSettings();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Menu")
	TObjectPtr<UButton> BackButton;
};
