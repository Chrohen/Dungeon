#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/UseAbilityAction.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Конец боя: победа, поражение, порядок делегатов, таймер после конца, перезапуск
BEGIN_DEFINE_SPEC(FBattleEndSpec, "Dungeon.Battle.End",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;
	AHeroUnit* Hero = nullptr;

	// Единственный герой на (1,2) с заданным здоровьем и способностью; бой запущен, запись событий очищена
	bool StartBattle(int32 HeroHealth, UAbilityDefinition* HeroAbility = nullptr)
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		TArray<UAbilityDefinition*> Abilities;
		if (HeroAbility != nullptr)
		{
			Abilities.Add(HeroAbility);
		}
		TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(HeroHealth, Abilities), FIntPoint(1, 2));
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Hero = Battle->GetHeroes().Num() > 0 ? Battle->GetHeroes()[0] : nullptr;
		Recorder->Reset();
		return Hero != nullptr;
	}

	// Способность героя по клетке. Результат TryExecute
	EBattleActionResult UseHeroAbility(FIntPoint Target)
	{
		UUseAbilityAction* Action = NewObject<UUseAbilityAction>();
		Action->Unit = Hero;
		Action->AbilityIndex = 0;
		Action->SelectedTile = Target;
		return Battle->TryExecute(Action);
	}

	AMonsterUnit* FirstMonster() const
	{
		const TArray<AMonsterUnit*> Monsters = Battle->GetLivingMonsters();
		return Monsters.Num() > 0 ? Monsters[0] : nullptr;
	}

	UAbilityDefinition* Deadly(int32 MinRange = 1, int32 MaxRange = 14)
	{
		return TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, MinRange, MaxRange);
	}

	void TickUntil(EBattlePhase Phase)
	{
		for (int32 Index = 0; Index < 40 && Battle->GetCurrentPhase() != Phase; ++Index)
		{
			TestWorld.AdvanceTime(0.7f);
		}
	}
END_DEFINE_SPEC(FBattleEndSpec)

void FBattleEndSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Hero = nullptr;
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Hero = nullptr;
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("Victory", [this]()
	{
		It("comes right after the hero action that kills the last monster of the last wave", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle(5, Deadly())) return;
			AMonsterUnit* Monster = FirstMonster();
			TestNotNull("the first wave", Monster);
			if (!Monster) return;
			TestEqual("not over yet", Battle->GetResult(), EBattleResult::None);

			TestEqual("the action", UseHeroAbility(Monster->GridPosition), EBattleActionResult::Success);

			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
			if (Recorder->EndedResults.Num() == 1)
			{
				TestEqual("OnBattleEnded result", Recorder->EndedResults[0], EBattleResult::Victory);
			}
			TestEqual("OnPhaseChanged", Recorder->PhaseChanges.Num(), 1);
			TestEqual("it was Finished", Recorder->PhaseChanges.Last(), EBattlePhase::Finished);
			TestEqual("OnBattleEnded came after OnPhaseChanged", Recorder->PhaseChangesAtBattleEnd, 1);

			Battle->EndPlayerTurn();
			TestEqual("the phase stays", Battle->GetCurrentPhase(), EBattlePhase::Finished);
		});

		It("does not come while waves remain", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 2);
			if (!StartBattle(5, Deadly())) return;

			TestEqual("kill wave 1", UseHeroAbility(FirstMonster()->GridPosition), EBattleActionResult::Success);
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestEqual("no monsters", Battle->GetLivingMonsters().Num(), 0);

			Battle->EndPlayerTurn();
			TestEqual("wave 2 appeared", Battle->GetLivingMonsters().Num(), 1);
			TestEqual("CurrentWave", Battle->GetCurrentWave(), 2);
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);

			TestEqual("kill wave 2", UseHeroAbility(FirstMonster()->GridPosition), EBattleActionResult::Success);
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
		});

		It("comes on entering the player's turn when no monsters are left", [this]()
		{
			TestWorld.SetWaves({}, 0);
			if (!Battle) return;
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			const TArray<EBattlePhase> Expected = {
				EBattlePhase::Preparation, EBattlePhase::MonstersMove, EBattlePhase::MonstersIntent,
				EBattlePhase::PlayerTurn, EBattlePhase::Finished
			};
			TestTrue("the order of OnPhaseChanged is not broken", Recorder->PhaseChanges == Expected);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
			TestEqual("OnBattleEnded came after OnPhaseChanged(Finished)", Recorder->PhaseChangesAtBattleEnd, 5);
			TestEqual("OnTurnStarted was sent for the last turn", Recorder->StartedTurns.Num(), 1);
		});
	});

	Describe("Defeat", [this]()
	{
		It("comes after the intent that kills the last hero", [this]()
		{
			if (!StartBattle(2)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) }));
			Battle->EndPlayerTurn();
			TestTrue("HasIntent", Monster->HasIntent());
			Recorder->Reset();

			Battle->EndPlayerTurn();

			TestFalse("the hero is dead", Hero->IsAlive());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Defeat);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			const TArray<EBattlePhase> Expected = { EBattlePhase::ResolveIntents, EBattlePhase::Finished };
			TestTrue("OnPhaseChanged: ResolveIntents, then Finished", Recorder->PhaseChanges == Expected);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
			TestEqual("OnBattleEnded came after OnPhaseChanged(Finished)", Recorder->PhaseChangesAtBattleEnd, 2);
			TestFalse("the intents are cleared", Monster->HasIntent());
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 0);
		});

		It("comes from the hero's own ability", [this]()
		{
			UAbilityDefinition* Blast = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			Blast->Area.Type = EAreaType::Diamond;
			Blast->Area.Size = 3;
			Blast->Area.bAffectsSource = true;
			if (!StartBattle(1, Blast)) return;

			TestEqual("the action", UseHeroAbility(FIntPoint(1, 3)), EBattleActionResult::Success);

			TestFalse("the hero is dead", Hero->IsAlive());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Defeat);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
		});

		It("wins over Victory when the last hero and the last monster die together", [this]()
		{
			UAbilityDefinition* Blast = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) }, 1, 14);
			Blast->Area.Type = EAreaType::Diamond;
			Blast->Area.Size = 14;
			Blast->Area.bAffectsSource = true;
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle(1, Blast)) return;
			AMonsterUnit* Monster = FirstMonster();
			if (!Monster) return;

			TestEqual("the action", UseHeroAbility(Monster->GridPosition), EBattleActionResult::Success);

			TestFalse("the hero is dead", Hero->IsAlive());
			TestFalse("the monster is dead", Monster->IsAlive());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Defeat);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
		});
	});

	Describe("After the end", [this]()
	{
		It("the next monsters of the phase do not act and the timer does not move the battle", [this]()
		{
			TestWorld.SetRules(1, 0.4f);
			if (!StartBattle(2)) return;
			UAbilityDefinition* Lethal = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			AMonsterUnit* First = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Lethal);
			AMonsterUnit* Second = TestWorld.SpawnMonster(FIntPoint(1, 3), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }));
			Battle->EndPlayerTurn();
			TickUntil(EBattlePhase::PlayerTurn);
			TestTrue("both have intents", First->HasIntent() && Second->HasIntent());
			Recorder->Reset();

			Battle->EndPlayerTurn();
			TestEqual("Result", Battle->GetResult(), EBattleResult::Defeat);
			TestEqual("the first monster acted", Recorder->AbilitySources.Num(), 1);

			TestWorld.AdvanceTime(4.f);
			TestEqual("the second monster did not act", Recorder->AbilitySources.Num(), 1);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
			TestEqual("OnPhaseChanged count", Recorder->PhaseChanges.Num(), 2);
		});

		It("StartBattle starts the battle anew", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle(5, Deadly())) return;
			TestEqual("the action", UseHeroAbility(FirstMonster()->GridPosition), EBattleActionResult::Success);
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);

			Recorder->Reset();
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestEqual("turn", Battle->GetTurnNumber(), 1);
			TestEqual("CurrentWave", Battle->GetCurrentWave(), 1);
			TestEqual("a new monster", Battle->GetLivingMonsters().Num(), 1);
			TestEqual("a new hero", Battle->GetHeroes().Num(), 1);
			TestTrue("the hero is alive", Battle->GetHeroes()[0]->IsAlive());
			TestEqual("OnBattleStarted count", Recorder->BattleStartedCount, 1);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
