#include "Data/MonsterDefinition.h"
#include "Misc/DataValidation.h"
#include "Units/MonsterUnit.h"

#if WITH_EDITOR
EDataValidationResult UMonsterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	// Пустой UnitClass уже отмечен родительской проверкой
	if (UnitClass != nullptr && !UnitClass->IsChildOf(AMonsterUnit::StaticClass()))
	{
		AddError(FString::Printf(TEXT("UnitClass %s is not a subclass of AMonsterUnit."), *UnitClass->GetName()));
	}
	if (ExperienceReward < 0)
	{
		AddError(FString::Printf(TEXT("ExperienceReward must not be negative (got %d)."), ExperienceReward));
	}
	if (BehaviorProfile.PreferredDistance < 0)
	{
		AddError(FString::Printf(TEXT("BehaviorProfile.PreferredDistance must not be negative (got %d)."), BehaviorProfile.PreferredDistance));
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
