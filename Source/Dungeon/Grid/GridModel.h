#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "GridModel.generated.h"

class UTileTypeDefinition;

// Состояние поля без зависимостей от актёров, мира и UI.
// Инвариант: на клетке не больше одного юнита.
USTRUCT()
struct FGridModel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Width = 0;

	UPROPERTY()
	int32 Height = 0;

	// Индекс клетки: Y * Width + X
	UPROPERTY()
	TArray<FTileData> Tiles;

	UPROPERTY()
	TMap<FIntPoint, FGridOccupant> Occupants;

	// Заполняет поле тайлами DefaultType и убирает всех юнитов.
	// Неположительные Width и Height дают пустое поле
	void Initialize(int32 InWidth, int32 InHeight, UTileTypeDefinition* DefaultType);

	bool IsInside(FIntPoint Coord) const;

	// Данные тайла или nullptr вне поля
	const FTileData* GetTile(FIntPoint Coord) const;

	// Меняет тип тайла. Вне поля ничего не делает
	void SetTileType(FIntPoint Coord, UTileTypeDefinition* Type);

	// Клетка внутри поля, у неё задан тип, и тип проходим
	bool IsPassable(FIntPoint Coord) const;

	// Стоимость захода на клетку. Для клетки вне поля и непроходимой возвращает 0:
	// значение не имеет смысла, поэтому вызывать метод нужно только для проходимых клеток
	int32 GetMoveCost(FIntPoint Coord) const;

	const FGridOccupant* GetOccupant(FIntPoint Coord) const;

	bool IsOccupied(FIntPoint Coord) const;

	// false, если клетка занята, непроходима или вне поля
	bool PlaceUnit(const FGridOccupant& Occupant, FIntPoint Coord);

	// Переносит юнит. false, если на From нет юнита или To вне поля, непроходима или занята.
	// Правила хода (очки, команда, фаза) не проверяются
	bool MoveUnit(FIntPoint From, FIntPoint To);

	// Убирает юнит с клетки. Если юнита нет, ничего не делает
	void RemoveUnit(FIntPoint Coord);

	// Очищает Out и записывает соседей по общей грани, лежащих внутри поля
	void GetNeighbors4(FIntPoint Coord, TArray<FIntPoint>& Out) const;
};
