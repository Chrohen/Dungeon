#include "Battle/UseAbilityAction.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Units/DungeonUnit.h"

EBattleActionResult UUseAbilityAction::CanExecute(const UBattleSubsystem& Battle) const
{
	if (Battle.GetGrid() == nullptr || !Battle.IsPlayerTurn())
	{
		return EBattleActionResult::InvalidPhase;
	}
	if (!IsValid(Unit) || !Unit->IsAlive())
	{
		return EBattleActionResult::InvalidUnit;
	}
	if (Unit->Team != EUnitTeam::Hero)
	{
		return EBattleActionResult::NotPlayerUnit;
	}
	if (!Unit->CanAct())
	{
		return EBattleActionResult::AlreadyActed;
	}
	if (!Unit->GetAbilities().IsValidIndex(AbilityIndex))
	{
		return EBattleActionResult::InvalidAbility;
	}
	const FUnitAbilityState& State = Unit->GetAbilities()[AbilityIndex];
	if (!State.IsReady())
	{
		return EBattleActionResult::OnCooldown;
	}

	FAbilityCast Cast;
	if (!FAbilityRules::MakeCast(Battle.MakeSimState(), Unit->UnitId, State.Ability, SelectedTile, Cast))
	{
		return EBattleActionResult::InvalidTarget;
	}
	return EBattleActionResult::Success;
}

void UUseAbilityAction::Execute(UBattleSubsystem& Battle)
{
	FAbilityCast Cast;
	if (!FAbilityRules::MakeCast(Battle.MakeSimState(), Unit->UnitId, Unit->GetAbilities()[AbilityIndex].Ability, SelectedTile, Cast))
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UUseAbilityAction::Execute: MakeCast failed after a successful CanExecute."));
		return;
	}
	Battle.ExecuteAbilityInternal(Unit, AbilityIndex, Cast);
}
