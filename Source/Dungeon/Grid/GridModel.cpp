#include "Grid/GridModel.h"
#include "Data/TileTypeDefinition.h"

void FGridModel::Initialize(int32 InWidth, int32 InHeight, UTileTypeDefinition* DefaultType)
{
	Width = FMath::Max(InWidth, 0);
	Height = FMath::Max(InHeight, 0);

	FTileData DefaultTile;
	DefaultTile.Type = DefaultType;
	Tiles.Init(DefaultTile, Width * Height);
	Occupants.Reset();
}

bool FGridModel::IsInside(FIntPoint Coord) const
{
	return Coord.X >= 0 && Coord.X < Width && Coord.Y >= 0 && Coord.Y < Height;
}

const FTileData* FGridModel::GetTile(FIntPoint Coord) const
{
	return IsInside(Coord) ? &Tiles[Coord.Y * Width + Coord.X] : nullptr;
}

void FGridModel::SetTileType(FIntPoint Coord, UTileTypeDefinition* Type)
{
	if (IsInside(Coord))
	{
		Tiles[Coord.Y * Width + Coord.X].Type = Type;
	}
}

bool FGridModel::IsPassable(FIntPoint Coord) const
{
	const FTileData* Tile = GetTile(Coord);
	return Tile != nullptr && Tile->Type != nullptr && Tile->Type->bPassable;
}

int32 FGridModel::GetMoveCost(FIntPoint Coord) const
{
	return IsPassable(Coord) ? GetTile(Coord)->Type->MoveCost : 0;
}

const FGridOccupant* FGridModel::GetOccupant(FIntPoint Coord) const
{
	return Occupants.Find(Coord);
}

bool FGridModel::IsOccupied(FIntPoint Coord) const
{
	return Occupants.Contains(Coord);
}

bool FGridModel::PlaceUnit(const FGridOccupant& Occupant, FIntPoint Coord)
{
	if (!IsPassable(Coord) || IsOccupied(Coord))
	{
		return false;
	}
	Occupants.Add(Coord, Occupant);
	return true;
}

bool FGridModel::MoveUnit(FIntPoint From, FIntPoint To)
{
	if (!IsOccupied(From) || !IsPassable(To) || IsOccupied(To))
	{
		return false;
	}
	FGridOccupant Occupant;
	Occupants.RemoveAndCopyValue(From, Occupant);
	Occupants.Add(To, Occupant);
	return true;
}

void FGridModel::RemoveUnit(FIntPoint Coord)
{
	Occupants.Remove(Coord);
}

void FGridModel::GetNeighbors4(FIntPoint Coord, TArray<FIntPoint>& Out) const
{
	static const FIntPoint Offsets[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };

	Out.Reset();
	for (const FIntPoint& Offset : Offsets)
	{
		const FIntPoint Neighbor = Coord + Offset;
		if (IsInside(Neighbor))
		{
			Out.Add(Neighbor);
		}
	}
}
