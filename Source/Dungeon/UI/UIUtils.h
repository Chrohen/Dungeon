#pragma once

#include "CoreMinimal.h"

class UBattleSelectionComponent;
class UBattleSubsystem;
class UImage;
class UTexture2D;
class UUserWidget;

// Общие функции виджетов боя
namespace DungeonUI
{
	// Подсистема боя мира виджета или nullptr
	UBattleSubsystem* GetBattle(const UUserWidget& Widget);

	// Компонент выбора контроллера игрока виджета (ABattlePlayerController::GetSelection) или nullptr
	UBattleSelectionComponent* GetSelection(const UUserWidget& Widget);

	// Показывает текстуру в UImage. Без текстуры (иконки пока временные) картинка скрыта, но место сохраняется
	void SetIcon(UImage* Image, UTexture2D* Texture);
}
