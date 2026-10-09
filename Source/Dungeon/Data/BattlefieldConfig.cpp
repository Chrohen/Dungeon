#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Misc/DataValidation.h"

#if WITH_EDITOR
EDataValidationResult UBattlefieldConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	const bool bFieldValid = Width > 0 && Height > 0;
	if (!bFieldValid)
	{
		AddError(FString::Printf(TEXT("Width and Height must be greater than 0 (got %d x %d)."), Width, Height));
	}

	if (DefaultTileType == nullptr)
	{
		AddError(TEXT("DefaultTileType is not set."));
	}

	// Область должна иметь размер и целиком лежать внутри поля
	auto CheckArea = [&](const TCHAR* Name, const FGridArea& Area)
	{
		if (Area.Width <= 0 || Area.Height <= 0)
		{
			AddError(FString::Printf(TEXT("%s must have positive Width and Height (got %d x %d)."), Name, Area.Width, Area.Height));
		}
		else if (bFieldValid
			&& (Area.Left < 0 || Area.Top < 0 || Area.Left + Area.Width > Width || Area.Top + Area.Height > Height))
		{
			AddError(FString::Printf(TEXT("%s (Left %d, Top %d, Width %d, Height %d) is outside the %d x %d field."),
				Name, Area.Left, Area.Top, Area.Width, Area.Height, Width, Height));
		}
	};
	CheckArea(TEXT("HeroSpawnArea"), HeroSpawnArea);
	CheckArea(TEXT("MonsterSpawnArea"), MonsterSpawnArea);

	// Если для клетки несколько переопределений, действует последнее
	TMap<FIntPoint, UTileTypeDefinition*> OverrideTypes;
	for (int32 Index = 0; Index < TileOverrides.Num(); ++Index)
	{
		const FTileOverride& TileOverride = TileOverrides[Index];
		if (bFieldValid
			&& (TileOverride.Coord.X < 0 || TileOverride.Coord.X >= Width || TileOverride.Coord.Y < 0 || TileOverride.Coord.Y >= Height))
		{
			AddError(FString::Printf(TEXT("TileOverrides[%d]: (%d, %d) is outside the %d x %d field."),
				Index, TileOverride.Coord.X, TileOverride.Coord.Y, Width, Height));
		}
		if (TileOverride.Type != nullptr)
		{
			OverrideTypes.Add(TileOverride.Coord, TileOverride.Type);
		}
	}

	TSet<FIntPoint> UsedPositions;
	for (int32 Index = 0; Index < HeroPlacements.Num(); ++Index)
	{
		const FIntPoint& Position = HeroPlacements[Index].Position;

		if (!HeroSpawnArea.Contains(Position))
		{
			AddError(FString::Printf(TEXT("HeroPlacements[%d]: (%d, %d) is outside HeroSpawnArea."), Index, Position.X, Position.Y));
		}

		bool bAlreadyInSet = false;
		UsedPositions.Add(Position, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			AddError(FString::Printf(TEXT("HeroPlacements[%d]: (%d, %d) is used by another hero."), Index, Position.X, Position.Y));
		}

		// Фактический тип клетки: переопределение, если задано, иначе DefaultTileType
		const UTileTypeDefinition* const* Override = OverrideTypes.Find(Position);
		const UTileTypeDefinition* EffectiveType = Override != nullptr ? *Override : DefaultTileType.Get();
		if (EffectiveType != nullptr && !EffectiveType->bPassable)
		{
			AddError(FString::Printf(TEXT("HeroPlacements[%d]: tile (%d, %d) is impassable."), Index, Position.X, Position.Y));
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
