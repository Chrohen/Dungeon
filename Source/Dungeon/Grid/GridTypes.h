#pragma once

#include "CoreMinimal.h"
#include "Units/UnitTypes.h"
#include "GridTypes.generated.h"

class ADungeonUnit;
class UTileTypeDefinition;

// Прямоугольник области появления: отступ слева, отступ сверху, ширина, высота
USTRUCT(BlueprintType)
struct FGridArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Left = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Top = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Height = 0;

	bool Contains(const FIntPoint& Tile) const
	{
		return Tile.X >= Left && Tile.X < Left + Width
			&& Tile.Y >= Top && Tile.Y < Top + Height;
	}
};

// Состояние клетки
USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTileTypeDefinition> Type = nullptr;
};

// Юнит на клетке. Команда хранится отдельно, чтобы тестам не нужны были актёры
USTRUCT(BlueprintType)
struct FGridOccupant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<ADungeonUnit> Unit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitTeam Team = EUnitTeam::Hero;
};

// Входные данные для поиска доступных клеток
USTRUCT(BlueprintType)
struct FPathQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Origin = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MovePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitTeam Team = EUnitTeam::Hero;
};

// Узел результата поиска: потраченные очки и предыдущая клетка
USTRUCT(BlueprintType)
struct FPathNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Parent = FIntPoint::ZeroValue;

	// Можно ли закончить на клетке ход. Ложно для клеток союзников, через которые можно только пройти
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanStop = false;
};

// Результат поиска. Nodes включает клетки союзников, через которые можно пройти, но IsDestination для них ложно
USTRUCT(BlueprintType)
struct FReachableArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Origin = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FIntPoint, FPathNode> Nodes;

	bool IsDestination(const FIntPoint& Tile) const
	{
		const FPathNode* Node = Nodes.Find(Tile);
		return Node != nullptr && Node->bCanStop;
	}

	TArray<FIntPoint> GetDestinations() const
	{
		TArray<FIntPoint> Result;
		for (const TPair<FIntPoint, FPathNode>& Pair : Nodes)
		{
			if (Pair.Value.bCanStop)
			{
				Result.Add(Pair.Key);
			}
		}
		return Result;
	}
};

// Тип тайла в конкретной клетке, если он отличается от DefaultTileType
USTRUCT(BlueprintType)
struct FTileOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTileTypeDefinition> Type = nullptr;
};
