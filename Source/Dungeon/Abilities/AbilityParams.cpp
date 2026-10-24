#include "Abilities/AbilityParams.h"
#include "Data/AbilityDefinition.h"

DEFINE_LOG_CATEGORY(LogDungeonAbilities);

int32* FAbilityParams::FindParam(FResolvedAbility& Ability, const FAbilityParamKey& Key)
{
	switch (Key.Param)
	{
	case EAbilityParam::Cooldown:
		return &Ability.Cooldown;
	case EAbilityParam::MinRange:
		return &Ability.Targeting.MinRange;
	case EAbilityParam::MaxRange:
		return &Ability.Targeting.MaxRange;
	case EAbilityParam::AreaSize:
		return &Ability.Area.Size;
	case EAbilityParam::EffectAmount:
		return Ability.Effects.IsValidIndex(Key.EffectIndex) ? &Ability.Effects[Key.EffectIndex].Amount : nullptr;
	case EAbilityParam::PushDistance:
		return Ability.Effects.IsValidIndex(Key.EffectIndex) ? &Ability.Effects[Key.EffectIndex].PushDistance : nullptr;
	case EAbilityParam::StatusDuration:
		return Ability.Effects.IsValidIndex(Key.EffectIndex) ? &Ability.Effects[Key.EffectIndex].Duration : nullptr;
	}
	return nullptr;
}

FResolvedAbility FAbilityParams::Resolve(const UAbilityDefinition* Def, const TArray<FAbilityParamValue>& Overrides, int32 Level)
{
	FResolvedAbility Result;
	if (Def == nullptr)
	{
		UE_LOG(LogDungeonAbilities, Warning, TEXT("Resolve: ability definition is null."));
		return Result;
	}

	// 1) значения способности
	Result.Definition = const_cast<UAbilityDefinition*>(Def);
	Result.Targeting = Def->Targeting;
	Result.Area = Def->Area;
	Result.Effects = Def->Effects;
	Result.Cooldown = Def->Cooldown;

	// Ключ, которого нет у способности, пропускается
	auto FindKnownParam = [&](const FAbilityParamKey& Key, const TCHAR* Source) -> int32*
	{
		int32* Param = Def->HasParam(Key) ? FindParam(Result, Key) : nullptr;
		if (Param == nullptr)
		{
			UE_LOG(LogDungeonAbilities, Warning, TEXT("Resolve: %s of %s has unknown parameter %s (EffectIndex %d), skipped."),
				Source, *Def->GetName(), *UEnum::GetValueAsString(Key.Param), Key.EffectIndex);
		}
		return Param;
	};

	// 2) переопределения юнита заменяют значения
	for (const FAbilityParamValue& Override : Overrides)
	{
		if (int32* Param = FindKnownParam(Override.Key, TEXT("override")))
		{
			*Param = Override.Value;
		}
	}

	// 3) изменения уровней 2..Level прибавляются по порядку
	const int32 ClampedLevel = FMath::Clamp(Level, 1, Def->GetMaxLevel());
	for (int32 LevelNumber = 2; LevelNumber <= ClampedLevel; ++LevelNumber)
	{
		for (const FAbilityParamValue& Change : Def->Levels[LevelNumber - 2].Changes)
		{
			if (int32* Param = FindKnownParam(Change.Key, TEXT("level change")))
			{
				*Param += Change.Value;
			}
		}
	}

	Result.Targeting.MinRange = FMath::Max(Result.Targeting.MinRange, 0);
	Result.Targeting.MaxRange = FMath::Max(Result.Targeting.MaxRange, Result.Targeting.MinRange);
	Result.Cooldown = FMath::Max(Result.Cooldown, 0);
	Result.Area.Size = FMath::Max(Result.Area.Size, 0);
	for (FAbilityEffect& Effect : Result.Effects)
	{
		Effect.Amount = FMath::Max(Effect.Amount, 0);
		Effect.PushDistance = FMath::Max(Effect.PushDistance, 0);
	}
	return Result;
}
