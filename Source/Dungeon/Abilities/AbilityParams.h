#pragma once

#include "CoreMinimal.h"
#include "Abilities/AbilityTypes.h"

class UAbilityDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonAbilities, Log, All);

// Итоговые значения параметров способности (GDD: «Итоговые значения параметров»)
struct DUNGEON_API FAbilityParams
{
	// 1) копия значений из Def; 2) переопределения заменяют значения; 3) изменения уровней
	// 2..Level прибавляются по порядку. Level ограничивается диапазоном [1, GetMaxLevel()].
	// Итог ограничивается: MinRange >= 0, MaxRange >= MinRange, Cooldown >= 0, Amount >= 0,
	// PushDistance >= 0, Size >= 0. Неизвестный ключ пропускается с предупреждением
	static FResolvedAbility Resolve(const UAbilityDefinition* Def, const TArray<FAbilityParamValue>& Overrides, int32 Level);

	// Поле по ключу или nullptr, если для параметра эффекта EffectIndex вне Effects
	static int32* FindParam(FResolvedAbility& Ability, const FAbilityParamKey& Key);
};
