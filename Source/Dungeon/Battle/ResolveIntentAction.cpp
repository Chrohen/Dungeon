#include "Battle/ResolveIntentAction.h"
#include "Battle/BattleSubsystem.h"
#include "Units/MonsterUnit.h"

EBattleActionResult UResolveIntentAction::CanExecute(const UBattleSubsystem& Battle) const
{
	if (Battle.GetGrid() == nullptr || Battle.GetCurrentPhase() != EBattlePhase::ResolveIntents)
	{
		return EBattleActionResult::InvalidPhase;
	}
	if (!IsValid(Monster) || !Monster->IsAlive())
	{
		return EBattleActionResult::InvalidUnit;
	}
	if (!Monster->HasIntent())
	{
		return EBattleActionResult::NoIntent;
	}
	return EBattleActionResult::Success;
}

void UResolveIntentAction::Execute(UBattleSubsystem& Battle)
{
	const int32 AbilityIndex = Monster->Intent.AbilityIndex;
	FAbilityCast Cast;
	if (Battle.GetIntentCast(Monster, Cast))
	{
		Battle.ExecuteAbilityInternal(Monster, AbilityIndex, Cast);
	}
	else
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UResolveIntentAction::Execute: the intent of %s cannot be built, it is dropped."), *GetNameSafe(Monster));
	}
	Battle.ClearIntent(Monster);
}
