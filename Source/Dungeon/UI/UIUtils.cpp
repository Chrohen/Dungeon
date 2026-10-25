#include "UI/UIUtils.h"
#include "Blueprint/UserWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/Image.h"
#include "Core/BattlePlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

UBattleSubsystem* DungeonUI::GetBattle(const UUserWidget& Widget)
{
	const UWorld* World = Widget.GetWorld();
	return World != nullptr ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
}

UBattleSelectionComponent* DungeonUI::GetSelection(const UUserWidget& Widget)
{
	const ABattlePlayerController* Controller = Widget.GetOwningPlayer<ABattlePlayerController>();
	return Controller != nullptr ? Controller->GetSelection() : nullptr;
}

void DungeonUI::SetIcon(UImage* Image, UTexture2D* Texture)
{
	if (Image == nullptr)
	{
		return;
	}
	if (Texture != nullptr)
	{
		Image->SetBrushFromTexture(Texture);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Hidden);
	}
}
