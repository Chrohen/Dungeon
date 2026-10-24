#include "Battle/MoveAction.h"
#include "Battle/BattleSubsystem.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridPathfinder.h"
#include "Units/DungeonUnit.h"

EBattleActionResult UMoveAction::CanExecute(const UBattleSubsystem& Battle) const
{
	const ADungeonGridActor* Grid = Battle.GetGrid();
	const EBattlePhase Phase = Battle.GetCurrentPhase();
	if (Grid == nullptr || (Phase != EBattlePhase::PlayerTurn && Phase != EBattlePhase::MonstersMove))
	{
		return EBattleActionResult::InvalidPhase;
	}
	if (!IsValid(Unit) || !Unit->IsAlive())
	{
		return EBattleActionResult::InvalidUnit;
	}
	// В свой ход ходит своя сторона: героям - PlayerTurn, монстрам - MonstersMove
	const EUnitTeam PhaseTeam = Phase == EBattlePhase::PlayerTurn ? EUnitTeam::Hero : EUnitTeam::Monster;
	if (Unit->Team != PhaseTeam)
	{
		return EBattleActionResult::NotPlayerUnit;
	}
	if (!Unit->CanMove())
	{
		return EBattleActionResult::AlreadyMoved;
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
	Battle.OnUnitChanged.Broadcast(Unit);
	// Перемещение могло перекрыть снаряд или сдвинуть цель намерения
	Battle.RebuildIntentViews();
}
