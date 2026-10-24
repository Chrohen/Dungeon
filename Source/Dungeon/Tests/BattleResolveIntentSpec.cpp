#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/ResolveIntentAction.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/MonsterUnit.h"

// Условия CanExecute. Реализацию намерений проверяют спеки фаз (задача 5.3): намерение ставят только фазы.
// Фаза ResolveIntents проходит мгновенно, поэтому действие выполняется из хука OnPhaseChanged
BEGIN_DEFINE_SPEC(FBattleResolveIntentSpec, "Dungeon.Battle.ResolveIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	bool StartBattle()
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Recorder->Reset();
		return true;
	}

	UResolveIntentAction* MakeAction(AMonsterUnit* Monster) const
	{
		UResolveIntentAction* Action = NewObject<UResolveIntentAction>();
		Action->Monster = Monster;
		return Action;
	}

	// Результат TryExecute, выполненного в фазе Phase во время EndPlayerTurn
	TOptional<EBattleActionResult> TryInPhase(EBattlePhase Phase, AMonsterUnit* Monster)
	{
		TOptional<EBattleActionResult> Result;
		Recorder->PhaseHook = [&](EBattlePhase NewPhase)
		{
			if (NewPhase == Phase)
			{
				Result = Battle->TryExecute(MakeAction(Monster));
			}
		};
		Battle->EndPlayerTurn();
		Recorder->PhaseHook = nullptr;
		return Result;
	}
END_DEFINE_SPEC(FBattleResolveIntentSpec)

void FBattleResolveIntentSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("CanExecute errors", [this]()
	{
		It("returns InvalidPhase before StartBattle", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestEqual("result", Battle->TryExecute(MakeAction(nullptr)), EBattleActionResult::InvalidPhase);
		});

		It("returns InvalidPhase in PlayerTurn", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 5), 1);
			TestEqual("result", Battle->TryExecute(MakeAction(Monster)), EBattleActionResult::InvalidPhase);
			TestEqual("InvalidPhase is checked before InvalidUnit", Battle->TryExecute(MakeAction(nullptr)), EBattleActionResult::InvalidPhase);
		});

		It("returns InvalidPhase in the other phases", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 5), 1);
			TMap<EBattlePhase, EBattleActionResult> Results;
			Recorder->PhaseHook = [&](EBattlePhase Phase)
			{
				if (Phase != EBattlePhase::ResolveIntents && Phase != EBattlePhase::PlayerTurn)
				{
					Results.Add(Phase, Battle->TryExecute(MakeAction(Monster)));
				}
			};

			Battle->EndPlayerTurn();

			TestTrue("phases checked", Results.Num() >= 3);
			for (const TPair<EBattlePhase, EBattleActionResult>& Pair : Results)
			{
				TestEqual(FString::Printf(TEXT("phase %d"), static_cast<int32>(Pair.Key)), Pair.Value, EBattleActionResult::InvalidPhase);
			}
		});

		It("returns InvalidUnit for a null monster in ResolveIntents", [this]()
		{
			if (!StartBattle()) return;
			const TOptional<EBattleActionResult> Result = TryInPhase(EBattlePhase::ResolveIntents, nullptr);
			TestTrue("checked", Result.IsSet());
			if (Result.IsSet())
			{
				TestEqual("result", Result.GetValue(), EBattleActionResult::InvalidUnit);
			}
		});

		It("returns InvalidUnit for a dead monster in ResolveIntents", [this]()
		{
			if (!StartBattle()) return;
			// KillUnit закрыт: монстр погибает от удара героя через ExecuteAbilityInternal
			UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Deadly);
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 1);
			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Battle->MakeSimState(), Hero->UnitId, Hero->GetAbilities()[0].Ability, Monster->GridPosition, Cast));
			Battle->ExecuteAbilityInternal(Hero, 0, Cast);
			TestFalse("the monster is dead", Monster->IsAlive());

			const TOptional<EBattleActionResult> Result = TryInPhase(EBattlePhase::ResolveIntents, Monster);
			TestTrue("checked", Result.IsSet());
			if (Result.IsSet())
			{
				TestEqual("result", Result.GetValue(), EBattleActionResult::InvalidUnit);
			}
		});

		It("returns NoIntent for a living monster without an intent", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 5), 1);
			TestFalse("HasIntent", Monster->HasIntent());

			const TOptional<EBattleActionResult> Result = TryInPhase(EBattlePhase::ResolveIntents, Monster);
			TestTrue("checked", Result.IsSet());
			if (Result.IsSet())
			{
				TestEqual("result", Result.GetValue(), EBattleActionResult::NoIntent);
			}
			TestEqual("no ability was used", Recorder->AbilitySources.Num(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
