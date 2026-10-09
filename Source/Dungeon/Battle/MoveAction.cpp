#include "Battle/MoveAction.h"
#include "Battle/BattleSubsystem.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridPathfinder.h"
#include "Units/DungeonUnit.h"

EBattleActionResult UMoveAction::CanExecute(const UBattleSubsystem& Battle) const
{
	if (!Battle.IsPlayerTurn())
	{
		return EBattleActionResult::InvalidPhase;
	}
	if (!IsValid(Unit))
	{
		return EBattleActionResult::InvalidUnit;
	}
	if (Unit->Team != EUnitTeam::Hero)
	{
		return EBattleActionResult::NotPlayerUnit;
	}
	if (!Unit->CanMove())
	{
		return EBattleActionResult::AlreadyMoved;
	}

	const ADungeonGridActor* Grid = Battle.GetGrid();
	if (Grid == nullptr)
	{
		return EBattleActionResult::InvalidPhase;
	}
	const FGridModel& Model = Grid->GetModel();
	if (!Model.IsInside(Target))
	{
		return EBattleActionResult::OutOfGrid;
	}
	if (Model.IsOccupied(Target))
	{
		return EBattleActionResult::Occupied;
	}

	FPathQuery Query;
	Query.Origin = Unit->GridPosition;
	Query.MovePoints = Unit->GetMovePoints();
	Query.Team = Unit->Team;
	if (!FGridPathfinder::ComputeReachable(Model, Query).IsDestination(Target))
	{
		return EBattleActionResult::Unreachable;
	}
	return EBattleActionResult::Success;
}

void UMoveAction::Execute(UBattleSubsystem& Battle)
{
	const FIntPoint From = Unit->GridPosition;
	if (!Battle.MoveUnitInternal(Unit, Target))
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UMoveAction::Execute: MoveUnitInternal failed after a successful CanExecute."));
		return;
	}
	Unit->MarkMoved();
	Battle.OnUnitMoved.Broadcast(Unit, From, Target);
}
