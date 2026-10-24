#include "Data/AbilityDefinition.h"
#include "Misc/DataValidation.h"

bool UAbilityDefinition::HasParam(const FAbilityParamKey& Key) const
{
	switch (Key.Param)
	{
	case EAbilityParam::EffectAmount:
	case EAbilityParam::PushDistance:
	case EAbilityParam::StatusDuration:
		return Effects.IsValidIndex(Key.EffectIndex);
	default:
		return true;
	}
}

#if WITH_EDITOR
EDataValidationResult UAbilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	if (Targeting.MinRange < 0 || Targeting.MinRange > Targeting.MaxRange)
	{
		AddError(FString::Printf(TEXT("Targeting must satisfy 0 <= MinRange <= MaxRange (got %d..%d)."),
			Targeting.MinRange, Targeting.MaxRange));
	}
	if (Cooldown < 0)
	{
		AddError(FString::Printf(TEXT("Cooldown must not be negative (got %d)."), Cooldown));
	}
	if (Targeting.Trajectory == ETrajectory::Projectile && Targeting.Shape != ETargetingShape::Cross)
	{
		AddError(TEXT("Projectile trajectory requires the Cross shape."));
	}

	// Направление определено только для формы "Крест" и минимальной дальности от 1
	const bool bHasDirection = Targeting.Shape == ETargetingShape::Cross && Targeting.MinRange >= 1;
	const TCHAR* DirectionRule = TEXT("requires a direction: Cross shape and MinRange >= 1");

	if ((Area.Type == EAreaType::Line || Area.Type == EAreaType::CrossLine || Area.Type == EAreaType::Path) && !bHasDirection)
	{
		AddError(FString::Printf(TEXT("Area %s %s."), *UEnum::GetValueAsString(Area.Type), DirectionRule));
	}
	if (Area.Size < 0)
	{
		AddError(FString::Printf(TEXT("Area.Size must not be negative (got %d)."), Area.Size));
	}
	else if (Area.Type == EAreaType::Line && Area.Size < 1)
	{
		AddError(FString::Printf(TEXT("Area.Size of a Line must be at least 1 (got %d)."), Area.Size));
	}

	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		const FAbilityEffect& Effect = Effects[Index];
		switch (Effect.Type)
		{
		case EAbilityEffectType::Damage:
		case EAbilityEffectType::Heal:
			if (Effect.Amount < 0)
			{
				AddError(FString::Printf(TEXT("Effects[%d]: Amount must not be negative (got %d)."), Index, Effect.Amount));
			}
			break;
		case EAbilityEffectType::Push:
			if (Effect.PushDistance < 1)
			{
				AddError(FString::Printf(TEXT("Effects[%d]: PushDistance must be at least 1 (got %d)."), Index, Effect.PushDistance));
			}
			if (Effect.PushDirection == EPushDirection::AbilityDirection && !bHasDirection)
			{
				AddError(FString::Printf(TEXT("Effects[%d]: Push along AbilityDirection %s."), Index, DirectionRule));
			}
			break;
		case EAbilityEffectType::MoveSource:
			if (Effect.MoveMode == ESourceMoveMode::Dash && !bHasDirection)
			{
				AddError(FString::Printf(TEXT("Effects[%d]: Dash %s."), Index, DirectionRule));
			}
			break;
		case EAbilityEffectType::UnitStatus:
		case EAbilityEffectType::TileStatus:
			//CURRENT-STAGE: состояния не используются, правила их пропускают
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("Effects[%d]: %s effects are not used yet, the rules skip them."), Index, *UEnum::GetValueAsString(Effect.Type))));
			break;
		}
	}

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		const TArray<FAbilityParamValue>& Changes = Levels[LevelIndex].Changes;
		for (int32 ChangeIndex = 0; ChangeIndex < Changes.Num(); ++ChangeIndex)
		{
			const FAbilityParamKey& Key = Changes[ChangeIndex].Key;
			if (!HasParam(Key))
			{
				AddError(FString::Printf(TEXT("Levels[%d].Changes[%d]: %s does not exist (EffectIndex %d)."),
					LevelIndex, ChangeIndex, *UEnum::GetValueAsString(Key.Param), Key.EffectIndex));
			}
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
