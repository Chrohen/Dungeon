#include "Battle/BattlePhases.h"
#include "Battle/BattleSubsystem.h"
#include "Data/BattlefieldConfig.h"
#include "Data/UnitDefinition.h"
#include "Units/HeroUnit.h"

void UPreparationPhase::Enter(UBattleSubsystem& Battle)
{
	const UBattlefieldConfig* Config = Battle.GetConfig();
	if (Config != nullptr)
	{
		for (const FHeroPlacement& Placement : Config->HeroPlacements)
		{
			if (Placement.Hero == nullptr)
			{
				UE_LOG(LogDungeonBattle, Warning, TEXT("HeroPlacements: hero definition is not set for (%d, %d)."), Placement.Position.X, Placement.Position.Y);
				continue;
			}
			if (Battle.SpawnUnit(Placement.Hero, Placement.Position) == nullptr)
			{
				UE_LOG(LogDungeonBattle, Warning, TEXT("Could not spawn %s at (%d, %d)."), *GetNameSafe(Placement.Hero), Placement.Position.X, Placement.Position.Y);
			}
		}
	}
	Battle.AdvancePhase();
}

void UMonstersMovePhase::Enter(UBattleSubsystem& Battle)
{
	++Battle.TurnNumber;
	Battle.AdvancePhase();
}

void UMonstersIntentPhase::Enter(UBattleSubsystem& Battle)
{
	Battle.AdvancePhase();
}

void UPlayerTurnPhase::Enter(UBattleSubsystem& Battle)
{
	for (AHeroUnit* Hero : Battle.GetHeroes())
	{
		Hero->ResetTurnState();
	}
	Battle.OnTurnStarted.Broadcast(Battle.GetTurnNumber());
}

void UResolveIntentsPhase::Enter(UBattleSubsystem& Battle)
{
	Battle.AdvancePhase();
}

void UWaveSpawnPhase::Enter(UBattleSubsystem& Battle)
{
	Battle.AdvancePhase();
}
