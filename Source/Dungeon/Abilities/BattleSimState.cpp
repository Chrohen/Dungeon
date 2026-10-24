#include "Abilities/BattleSimState.h"

FSimUnit* FBattleSimState::FindUnit(int32 UnitId)
{
	return Units.FindByPredicate([UnitId](const FSimUnit& Unit) { return Unit.UnitId == UnitId; });
}

const FSimUnit* FBattleSimState::FindUnit(int32 UnitId) const
{
	return Units.FindByPredicate([UnitId](const FSimUnit& Unit) { return Unit.UnitId == UnitId; });
}

FSimUnit* FBattleSimState::FindUnitAt(FIntPoint Tile)
{
	const FGridOccupant* Occupant = Grid.GetOccupant(Tile);
	if (Occupant == nullptr)
	{
		return nullptr;
	}
	FSimUnit* Unit = FindUnit(Occupant->UnitId);
	return Unit != nullptr && Unit->bAlive ? Unit : nullptr;
}

const FSimUnit* FBattleSimState::FindUnitAt(FIntPoint Tile) const
{
	return const_cast<FBattleSimState*>(this)->FindUnitAt(Tile);
}

bool FBattleSimState::MoveUnit(int32 UnitId, FIntPoint To)
{
	FSimUnit* Unit = FindUnit(UnitId);
	if (Unit == nullptr || !Unit->bAlive || !Grid.MoveUnit(Unit->Position, To))
	{
		return false;
	}
	Unit->Position = To;
	return true;
}

void FBattleSimState::KillUnit(int32 UnitId)
{
	FSimUnit* Unit = FindUnit(UnitId);
	if (Unit == nullptr)
	{
		return;
	}
	if (Unit->bAlive)
	{
		Grid.RemoveUnit(Unit->Position);
	}
	Unit->bAlive = false;
}

void FBattleSimState::ApplyDamage(int32 UnitId, int32 Amount, int32& OutLost)
{
	OutLost = 0;
	FSimUnit* Unit = FindUnit(UnitId);
	if (Unit == nullptr || !Unit->bAlive)
	{
		return;
	}
	OutLost = FMath::Clamp(Amount, 0, Unit->Health);
	Unit->Health -= OutLost;
}

void FBattleSimState::ApplyHeal(int32 UnitId, int32 Amount, int32& OutGained)
{
	OutGained = 0;
	FSimUnit* Unit = FindUnit(UnitId);
	if (Unit == nullptr || !Unit->bAlive)
	{
		return;
	}
	OutGained = FMath::Clamp(Amount, 0, FMath::Max(Unit->MaxHealth - Unit->Health, 0));
	Unit->Health += OutGained;
}
