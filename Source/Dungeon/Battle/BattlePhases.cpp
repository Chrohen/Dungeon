#include "Battle/BattlePhases.h"
#include "Abilities/AbilityRules.h"
#include "AI/MonsterAI.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Battle/ResolveIntentAction.h"
#include "Data/BattleRulesConfig.h"
#include "Data/BattlefieldConfig.h"
#include "Data/UnitDefinition.h"
#include "Engine/World.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

namespace
{
	FTimerManager* GetTimerManager(const UBattleSubsystem* Battle)
	{
		UWorld* World = Battle != nullptr ? Battle->GetWorld() : nullptr;
		return World != nullptr ? &World->GetTimerManager() : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// UMonsterStepPhase

void UMonsterStepPhase::Enter(UBattleSubsystem& Battle)
{
	OwnerBattle = &Battle;
	Queue.Reset();
	BuildQueue(Battle);
	NextIndex = 0;
	RunSteps();
}

void UMonsterStepPhase::Exit(UBattleSubsystem& Battle)
{
	if (FTimerManager* TimerManager = GetTimerManager(&Battle))
	{
		TimerManager->ClearTimer(StepTimer);
	}
	Queue.Reset();
	NextIndex = 0;
}

void UMonsterStepPhase::RunSteps()
{
	UBattleSubsystem* Battle = OwnerBattle.Get();
	if (Battle == nullptr)
	{
		return;
	}

	while (Queue.IsValidIndex(NextIndex))
	{
		AMonsterUnit* Monster = Queue[NextIndex++].Get();
		if (Monster == nullptr || !Monster->IsAlive())
		{
			continue;
		}

		const bool bVisibleAction = ProcessMonster(*Battle, Monster);
		if (Battle->IsBattleOver())
		{
			return; // бой уже завершён, следующая фаза не нужна
		}
		if (bVisibleAction && Battle->GetRules().MonsterActionDelay > 0.f)
		{
			if (FTimerManager* TimerManager = GetTimerManager(Battle))
			{
				TimerManager->SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UMonsterStepPhase::RunSteps),
					Battle->GetRules().MonsterActionDelay, false);
				return;
			}
		}
	}

	OnQueueFinished(*Battle);
	Battle->AdvancePhase();
}

// ---------------------------------------------------------------------------------------------------------------------
// UPreparationPhase

void UPreparationPhase::Enter(UBattleSubsystem& Battle)
{
	// Первая волна появляется до расстановки героев, чтобы игрок видел противников
	Battle.SpawnWave();
	Battle.CurrentWave = 1;
	Battle.OnWaveChanged.Broadcast(Battle.GetCurrentWave(), Battle.GetTotalWaves());

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
			ADungeonUnit* Unit = Battle.SpawnUnit(Placement.Hero, Placement.Position);
			if (Unit == nullptr)
			{
				UE_LOG(LogDungeonBattle, Warning, TEXT("Could not spawn %s at (%d, %d)."), *GetNameSafe(Placement.Hero), Placement.Position.X, Placement.Position.Y);
				continue;
			}
			if (AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
			{
				Hero->SetHeroName(Placement.Name);
			}
		}
	}
	Battle.AdvancePhase();
}

// ---------------------------------------------------------------------------------------------------------------------
// UMonstersMovePhase

void UMonstersMovePhase::BuildQueue(UBattleSubsystem& Battle)
{
	++Battle.TurnNumber;

	for (AMonsterUnit* Monster : Battle.GetLivingMonsters())
	{
		Monster->ResetTurnState();
		Queue.Add(Monster);
	}
	Context = FMonsterAI::MakeContext(Battle.MakeSimState());
}

bool UMonstersMovePhase::ProcessMonster(UBattleSubsystem& Battle, AMonsterUnit* Monster)
{
	// Снимок строится заново: предыдущие монстры уже переместились
	const FBattleSimState Sim = Battle.MakeSimState();
	const FAIVariant Variant = FMonsterAI::ChooseVariant(Sim, Monster->UnitId, Monster->GetMovePoints(), Monster->GetAbilities(),
		Monster->GetBehaviorProfile(), /*bAllowMove=*/true, Context, Battle.GetRandom());
	FMonsterAI::RegisterChoice(Sim, Variant, Context);

	if (Variant.MoveTile == Monster->GridPosition)
	{
		return false;
	}
	UMoveAction* Action = NewObject<UMoveAction>();
	Action->Unit = Monster;
	Action->Target = Variant.MoveTile;
	return Battle.TryExecute(Action) == EBattleActionResult::Success;
}

// ---------------------------------------------------------------------------------------------------------------------
// UMonstersIntentPhase

void UMonstersIntentPhase::Enter(UBattleSubsystem& Battle)
{
	Battle.ClearAllIntents();
	FMonsterAIContext Context = FMonsterAI::MakeContext(Battle.MakeSimState());

	int32 Order = 0;
	for (AMonsterUnit* Monster : Battle.GetLivingMonsters())
	{
		// Снимок строится заново: выбор учитывает позиции после перемещения всех монстров
		const FBattleSimState Sim = Battle.MakeSimState();
		const FAIVariant Variant = FMonsterAI::ChooseVariant(Sim, Monster->UnitId, Monster->GetMovePoints(), Monster->GetAbilities(),
			Monster->GetBehaviorProfile(), /*bAllowMove=*/false, Context, Battle.GetRandom());
		if (!Variant.bAttacking || Variant.AbilityIndex == INDEX_NONE)
		{
			continue; // монстр без атакующего варианта остаётся без намерения
		}

		FMonsterIntent Intent;
		Intent.bActive = true;
		Intent.AbilityIndex = Variant.AbilityIndex;
		Intent.Direction = FAbilityRules::GetDirection(Monster->GridPosition, Variant.SelectedTile);
		Intent.SelectedOffset = Variant.SelectedTile - Monster->GridPosition;
		Intent.Order = ++Order;
		Battle.SetIntent(Monster, Intent);
		FMonsterAI::RegisterChoice(Sim, Variant, Context);
	}

	Battle.RebuildIntentViews();
	Battle.AdvancePhase();
}

// ---------------------------------------------------------------------------------------------------------------------
// UPlayerTurnPhase

void UPlayerTurnPhase::Enter(UBattleSubsystem& Battle)
{
	for (AHeroUnit* Hero : Battle.GetHeroes())
	{
		if (Hero->IsAlive())
		{
			Hero->ResetTurnState();
			Battle.OnUnitChanged.Broadcast(Hero);
		}
	}
	Battle.OnTurnStarted.Broadcast(Battle.GetTurnNumber());
	Battle.RebuildIntentViews();
	Battle.CheckBattleEnd();
}

// ---------------------------------------------------------------------------------------------------------------------
// UResolveIntentsPhase

void UResolveIntentsPhase::BuildQueue(UBattleSubsystem& Battle)
{
	// Номер порядка совпадает с порядком появления; сортировка устойчива
	for (AMonsterUnit* Monster : Battle.GetLivingMonsters())
	{
		if (Monster->HasIntent())
		{
			Queue.Add(Monster);
		}
	}
	Queue.StableSort([](const TWeakObjectPtr<AMonsterUnit>& A, const TWeakObjectPtr<AMonsterUnit>& B)
	{
		return A->Intent.Order < B->Intent.Order;
	});
}

bool UResolveIntentsPhase::ProcessMonster(UBattleSubsystem& Battle, AMonsterUnit* Monster)
{
	UResolveIntentAction* Action = NewObject<UResolveIntentAction>();
	Action->Monster = Monster;
	return Battle.TryExecute(Action) == EBattleActionResult::Success;
}

void UResolveIntentsPhase::OnQueueFinished(UBattleSubsystem& Battle)
{
	Battle.ClearAllIntents();
}

// ---------------------------------------------------------------------------------------------------------------------
// UWaveSpawnPhase

void UWaveSpawnPhase::Enter(UBattleSubsystem& Battle)
{
	bool bSpawned = false;
	if (Battle.GetCurrentWave() < Battle.GetTotalWaves())
	{
		Battle.SpawnWave();
		++Battle.CurrentWave;
		Battle.OnWaveChanged.Broadcast(Battle.GetCurrentWave(), Battle.GetTotalWaves());
		bSpawned = true;
	}

	// Перезарядка уменьшается в конце хода, после появления волны
	Battle.TickCooldowns();

	const float Delay = Battle.GetRules().MonsterActionDelay;
	FTimerManager* TimerManager = GetTimerManager(&Battle);
	if (bSpawned && Delay > 0.f && TimerManager != nullptr)
	{
		OwnerBattle = &Battle;
		TimerManager->SetTimer(PauseTimer, FTimerDelegate::CreateUObject(this, &UWaveSpawnPhase::AdvanceAfterPause), Delay, false);
		return;
	}
	Battle.AdvancePhase();
}

void UWaveSpawnPhase::Exit(UBattleSubsystem& Battle)
{
	if (FTimerManager* TimerManager = GetTimerManager(&Battle))
	{
		TimerManager->ClearTimer(PauseTimer);
	}
}

void UWaveSpawnPhase::AdvanceAfterPause()
{
	if (UBattleSubsystem* Battle = OwnerBattle.Get())
	{
		Battle->AdvancePhase();
	}
}
