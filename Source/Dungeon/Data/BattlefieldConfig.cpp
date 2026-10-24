#include "Data/BattlefieldConfig.h"
#include "Data/BattleRulesConfig.h"
#include "Data/MonsterDefinition.h"
#include "Data/TileTypeDefinition.h"
#include "Data/UnitDefinition.h"
#include "Misc/DataValidation.h"
#include "Units/HeroUnit.h"

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

	const bool bAreasValid = HeroSpawnArea.Width > 0 && HeroSpawnArea.Height > 0
		&& MonsterSpawnArea.Width > 0 && MonsterSpawnArea.Height > 0;
	if (bAreasValid
		&& HeroSpawnArea.Left < MonsterSpawnArea.Left + MonsterSpawnArea.Width
		&& MonsterSpawnArea.Left < HeroSpawnArea.Left + HeroSpawnArea.Width
		&& HeroSpawnArea.Top < MonsterSpawnArea.Top + MonsterSpawnArea.Height
		&& MonsterSpawnArea.Top < HeroSpawnArea.Top + HeroSpawnArea.Height)
	{
		AddError(TEXT("HeroSpawnArea and MonsterSpawnArea overlap."));
	}

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

		const UUnitDefinition* Hero = HeroPlacements[Index].Hero;
		if (Hero == nullptr)
		{
			AddError(FString::Printf(TEXT("HeroPlacements[%d]: Hero is not set."), Index));
		}
		else if (Hero->UnitClass == nullptr || !Hero->UnitClass->IsChildOf(AHeroUnit::StaticClass()))
		{
			AddError(FString::Printf(TEXT("HeroPlacements[%d]: %s UnitClass is not a subclass of AHeroUnit."), Index, *Hero->GetName()));
		}

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

	if (WaveCount < 1)
	{
		AddError(FString::Printf(TEXT("WaveCount must be at least 1 (got %d)."), WaveCount));
	}

	int32 MonstersPerWave = 0;
	for (int32 Index = 0; Index < WaveComposition.Num(); ++Index)
	{
		if (WaveComposition[Index] == nullptr)
		{
			AddError(FString::Printf(TEXT("WaveComposition[%d] is not set."), Index));
		}
		else
		{
			++MonstersPerWave;
		}
	}

	if (Rules == nullptr)
	{
		AddError(TEXT("Rules is not set."));
	}

	// Клетки появления монстров: переопределение типа, если задано, иначе DefaultTileType
	if (bFieldValid && DefaultTileType != nullptr && MonstersPerWave > 0
		&& MonsterSpawnArea.Width > 0 && MonsterSpawnArea.Height > 0)
	{
		int32 PassableTiles = 0;
		for (int32 Y = MonsterSpawnArea.Top; Y < MonsterSpawnArea.Top + MonsterSpawnArea.Height; ++Y)
		{
			for (int32 X = MonsterSpawnArea.Left; X < MonsterSpawnArea.Left + MonsterSpawnArea.Width; ++X)
			{
				if (X < 0 || X >= Width || Y < 0 || Y >= Height)
				{
					continue;
				}
				const UTileTypeDefinition* const* Override = OverrideTypes.Find(FIntPoint(X, Y));
				const UTileTypeDefinition* TileType = Override != nullptr ? *Override : DefaultTileType.Get();
				if (TileType->bPassable)
				{
					++PassableTiles;
				}
			}
		}
		if (PassableTiles < MonstersPerWave)
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("MonsterSpawnArea has %d passable tiles, but a wave has %d monsters."), PassableTiles, MonstersPerWave)));
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
