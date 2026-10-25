#pragma once

#include "CoreMinimal.h"
#include "UITypes.generated.h"

// Состояние кнопки способности в панели героя
UENUM(BlueprintType)
enum class EAbilityButtonState : uint8
{
	Available,
	Selected,
	Cooldown,
	Unavailable
};

// Экран окна завершения боя (TD 7.6)
UENUM(BlueprintType)
enum class EBattleResultScreen : uint8
{
	None,
	Rewards,
	Transition,
	Defeat
};
