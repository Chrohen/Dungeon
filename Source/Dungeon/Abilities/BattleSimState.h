#pragma once

#include "CoreMinimal.h"
#include "Grid/GridModel.h"
#include "Units/UnitTypes.h"

// Юнит в снимке боя
struct FSimUnit
{
	int32 UnitId = INDEX_NONE;
	// Команду всегда задаёт сборщик снимка. Правила сравнивают её только с командой источника
	EUnitTeam Team = EUnitTeam();
	FIntPoint Position = FIntPoint::ZeroValue;
	int32 Health = 1;
	int32 MaxHealth = 1;
	bool bAlive = true;
};

// Снимок боя для правил способностей: копия модели поля (в FGridOccupant используются
// только UnitId и Team) и юниты. Не зависит от актёров и мира
struct DUNGEON_API FBattleSimState
{
	FGridModel Grid;
	TArray<FSimUnit> Units;
	int32 CollisionDamage = 1;

	FSimUnit* FindUnit(int32 UnitId);
	const FSimUnit* FindUnit(int32 UnitId) const;

	// Живой юнит на клетке или nullptr
	FSimUnit* FindUnitAt(FIntPoint Tile);
	const FSimUnit* FindUnitAt(FIntPoint Tile) const;

	// Переносит живой юнит на свободную проходимую клетку (Grid.MoveUnit) и обновляет Position
	bool MoveUnit(int32 UnitId, FIntPoint To);

	// bAlive = false и юнит снимается с клетки
	void KillUnit(int32 UnitId);

	// Здоровье остаётся в [0, MaxHealth]. OutLost / OutGained - фактическое изменение
	void ApplyDamage(int32 UnitId, int32 Amount, int32& OutLost);
	void ApplyHeal(int32 UnitId, int32 Amount, int32& OutGained);
};
