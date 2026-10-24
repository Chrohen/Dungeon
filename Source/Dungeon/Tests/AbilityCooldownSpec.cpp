#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/UseAbilityAction.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Перезарядка на уровне боя: способность с перезарядкой N, использованная на ходу T, доступна на ходу T + N + 1
BEGIN_DEFINE_SPEC(FAbilityCooldownSpec, "Dungeon.Abilities.Cooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	EBattleActionResult UseHeroAbility(ADungeonUnit* Hero, FIntPoint Target)
	{
		UUseAbilityAction* Action = NewObject<UUseAbilityAction>();
		Action->Unit = Hero;
		Action->AbilityIndex = 0;
		Action->SelectedTile = Target;
		return Battle->TryExecute(Action);
	}

	// Герой использует способность с перезарядкой Cooldown на ходу 1, потом проверяется, на каком ходу она снова доступна
	void VerifyHeroCooldown(int32 Cooldown)
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return;
		}
		UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 1, Cooldown);
		TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(50, { Ability }), FIntPoint(1, 2));
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		ADungeonUnit* Hero = Battle->GetHeroes()[0];
		TestWorld.SpawnMonster(FIntPoint(2, 2), 99);

		TestEqual("turn", Battle->GetTurnNumber(), 1);
		TestTrue("ready at the start", Hero->GetAbilities()[0].IsReady());
		TestEqual("used on turn 1", UseHeroAbility(Hero, FIntPoint(2, 2)), EBattleActionResult::Success);
		TestEqual("RemainingCooldown after use", Hero->GetAbilities()[0].RemainingCooldown, Cooldown);

		int32 FirstAvailableTurn = INDEX_NONE;
		for (int32 Step = 0; Step < Cooldown + 3 && FirstAvailableTurn == INDEX_NONE; ++Step)
		{
			Battle->EndPlayerTurn();
			const EBattleActionResult Result = UseHeroAbility(Hero, FIntPoint(2, 2));
			if (Result == EBattleActionResult::Success)
			{
				FirstAvailableTurn = Battle->GetTurnNumber();
			}
			else
			{
				TestEqual(FString::Printf(TEXT("turn %d: not ready"), Battle->GetTurnNumber()), Result, EBattleActionResult::OnCooldown);
			}
		}
		TestEqual("the first turn the ability is available again", FirstAvailableTurn, 1 + Cooldown + 1);
	}
END_DEFINE_SPEC(FAbilityCooldownSpec)

void FAbilityCooldownSpec::Define()
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

	Describe("Hero", [this]()
	{
		It("with cooldown 0 can use the ability every turn", [this]()
		{
			VerifyHeroCooldown(0);
		});

		It("with cooldown 1 used on turn 1 can use it again on turn 3", [this]()
		{
			VerifyHeroCooldown(1);
		});

		It("with cooldown 2 used on turn 1 can use it again on turn 4", [this]()
		{
			VerifyHeroCooldown(2);
		});

		It("with cooldown 3 used on turn 1 can use it again on turn 5", [this]()
		{
			VerifyHeroCooldown(3);
		});
	});

	Describe("Monster", [this]()
	{
		It("with cooldown 1 attacks on turn 1, skips turn 2 and attacks again on turn 3", [this]()
		{
			// Дальняя способность: монстр бьёт героя, где бы он ни появился
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 14, 1);
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(9, 0, { Ability }) }, 1);
			TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(50), FIntPoint(1, 2));
			if (!Battle) return;
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			AMonsterUnit* Monster = Battle->GetLivingMonsters()[0];
			AHeroUnit* Hero = Battle->GetHeroes()[0];

			TestEqual("the cooldown is 0 at the start", Monster->GetAbilities()[0].RemainingCooldown, 0);
			TestEqual("UsedOnTurn at the start", Monster->GetAbilities()[0].UsedOnTurn, static_cast<int32>(INDEX_NONE));
			TestEqual("turn", Battle->GetTurnNumber(), 1);
			TestTrue("turn 1: intent", Monster->HasIntent());

			Recorder->Reset();
			Battle->EndPlayerTurn();
			TestEqual("the monster attacked on turn 1", Recorder->AbilitySources.Num(), 1);
			TestEqual("the hero took the hit", Hero->GetHealth(), 49);
			TestEqual("UsedOnTurn", Monster->GetAbilities()[0].UsedOnTurn, 1);
			TestEqual("the turn of use does not lower the cooldown", Monster->GetAbilities()[0].RemainingCooldown, 1);
			TestEqual("turn", Battle->GetTurnNumber(), 2);
			TestFalse("turn 2: no intent while the cooldown lasts", Monster->HasIntent());

			Recorder->Reset();
			Battle->EndPlayerTurn();
			TestEqual("the monster did not attack on turn 2", Recorder->AbilitySources.Num(), 0);
			TestEqual("the cooldown ended after turn 2", Monster->GetAbilities()[0].RemainingCooldown, 0);
			TestEqual("turn", Battle->GetTurnNumber(), 3);
			TestTrue("turn 3: intent", Monster->HasIntent());
			TestEqual("the hero took no more damage", Hero->GetHealth(), 49);
		});

		It("starts every ability of a monster and a hero with cooldown 0", [this]()
		{
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 1, 4);
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(9, 0, { Ability }) }, 1);
			TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(5, { Ability }), FIntPoint(1, 2));
			if (!Battle) return;
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			for (const ADungeonUnit* Unit : Battle->GetUnits())
			{
				TestEqual("abilities", Unit->GetAbilities().Num(), 1);
				TestEqual("RemainingCooldown", Unit->GetAbilities()[0].RemainingCooldown, 0);
				TestEqual("UsedOnTurn", Unit->GetAbilities()[0].UsedOnTurn, static_cast<int32>(INDEX_NONE));
				TestTrue("IsReady", Unit->GetAbilities()[0].IsReady());
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
