#include "Data/UnitDefinition.h"
#include "Data/AbilityDefinition.h"
#include "Misc/DataValidation.h"
#include "Units/DungeonUnit.h"

#if WITH_EDITOR
EDataValidationResult UUnitDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	if (UnitClass == nullptr)
	{
		AddError(TEXT("UnitClass is not set."));
	}
	if (MaxHealth <= 0)
	{
		AddError(FString::Printf(TEXT("MaxHealth must be greater than 0 (got %d)."), MaxHealth));
	}
	if (MovePoints < 0)
	{
		AddError(FString::Printf(TEXT("MovePoints must not be negative (got %d)."), MovePoints));
	}
	if (Level < 1)
	{
		AddError(FString::Printf(TEXT("Level must be at least 1 (got %d)."), Level));
	}

	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FUnitAbilityEntry& Entry = Abilities[Index];
		if (Entry.Ability == nullptr)
		{
			AddError(FString::Printf(TEXT("Abilities[%d]: Ability is not set."), Index));
			continue;
		}

		for (int32 OverrideIndex = 0; OverrideIndex < Entry.Overrides.Num(); ++OverrideIndex)
		{
			const FAbilityParamKey& Key = Entry.Overrides[OverrideIndex].Key;
			if (!Entry.Ability->HasParam(Key))
			{
				AddError(FString::Printf(TEXT("Abilities[%d].Overrides[%d]: %s does not exist in %s (EffectIndex %d)."),
					Index, OverrideIndex, *UEnum::GetValueAsString(Key.Param), *Entry.Ability->GetName(), Key.EffectIndex));
			}
		}

		const int32 MaxLevel = Entry.Ability->GetMaxLevel();
		if (Entry.Level < 1 || Entry.Level > MaxLevel)
		{
			AddError(FString::Printf(TEXT("Abilities[%d]: Level %d is outside 1..%d of %s."),
				Index, Entry.Level, MaxLevel, *Entry.Ability->GetName()));
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
