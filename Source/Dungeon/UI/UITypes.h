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
