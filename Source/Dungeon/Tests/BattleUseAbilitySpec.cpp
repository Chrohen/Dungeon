#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/UseAbilityAction.h"
#include "Data/AbilityDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

BEGIN_DEFINE_SPEC(FBattleUseAbilitySpec, "Dungeon.Battle.UseAbility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	// Подсистема создана, бой запущен (PlayerTurn, герои на (1,2), (2,1), (3,1))
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

	UUseAbilityAction* MakeAction(ADungeonUnit* Unit, int32 AbilityIndex, FIntPoint Tile) const
	{
		UUseAbilityAction* Action = NewObject<UUseAbilityAction>();
		Action->Unit = Unit;
		Action->AbilityIndex = AbilityIndex;
		Action->SelectedTile = Tile;
		return Action;
	}

	// Монстр убивает юнита ударом через ExecuteAbilityInternal (KillUnit закрыт)
	void KillUnit(ADungeonUnit* Victim)
	{
		UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, 1, 8);
		AMonsterUnit* Killer = TestWorld.SpawnMonster(FIntPoint(7, 7), 1, Deadly);
		FAbilityCast Cast;
		if (!FAbilityRules::MakeCast(Battle->MakeSimState(), Killer->UnitId, Killer->GetAbilities()[0].Ability, Victim->GridPosition, Cast))
		{
			AddError("KillUnit: MakeCast failed");
			return;
		}
		Battle->ExecuteAbilityInternal(Killer, 0, Cast);
	}
END_DEFINE_SPEC(FBattleUseAbilitySpec)

void FBattleUseAbilitySpec::Define()
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
			TestEqual("result", Battle->TryExecute(MakeAction(nullptr, 0, FIntPoint(1, 3))), EBattleActionResult::InvalidPhase);
		});

		It("returns InvalidPhase outside PlayerTurn", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			TMap<EBattlePhase, EBattleActionResult> Results;
			Recorder->PhaseHook = [&](EBattlePhase Phase)
			{
				if (Phase == EBattlePhase::ResolveIntents || Phase == EBattlePhase::MonstersMove || Phase == EBattlePhase::WaveSpawn)
				{
					Results.Add(Phase, Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))));
				}
			};

			Battle->EndPlayerTurn();

			TestEqual("phases checked", Results.Num(), 3);
			for (const TPair<EBattlePhase, EBattleActionResult>& Pair : Results)
			{
				TestEqual(FString::Printf(TEXT("phase %d"), static_cast<int32>(Pair.Key)), Pair.Value, EBattleActionResult::InvalidPhase);
			}
			TestFalse("bHasActed", Hero->TurnState.bHasActed);
		});

		It("returns InvalidUnit for a null unit", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("result", Battle->TryExecute(MakeAction(nullptr, 0, FIntPoint(1, 3))), EBattleActionResult::InvalidUnit);
		});

		It("returns InvalidUnit for a dead unit", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			KillUnit(Hero);
			TestFalse("the hero is dead", Hero->IsAlive());

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::InvalidUnit);
		});

		It("returns NotPlayerUnit for a monster", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 5), 1, Ability);

			TestEqual("result", Battle->TryExecute(MakeAction(Monster, 0, FIntPoint(5, 6))), EBattleActionResult::NotPlayerUnit);
		});

		It("returns AlreadyActed after a use", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			TestEqual("first use", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
			TestEqual("second use", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::AlreadyActed);
			TestEqual("AlreadyActed comes before InvalidAbility", Battle->TryExecute(MakeAction(Hero, 7, FIntPoint(5, 6))), EBattleActionResult::AlreadyActed);
		});

		It("returns InvalidAbility for an index outside the list", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);

			TestEqual("index 1", Battle->TryExecute(MakeAction(Hero, 1, FIntPoint(5, 6))), EBattleActionResult::InvalidAbility);
			TestEqual("index -1", Battle->TryExecute(MakeAction(Hero, -1, FIntPoint(5, 6))), EBattleActionResult::InvalidAbility);
			TestEqual("a hero without abilities", Battle->TryExecute(MakeAction(Battle->GetUnits()[0], 0, FIntPoint(1, 3))), EBattleActionResult::InvalidAbility);
		});

		It("returns OnCooldown while the cooldown lasts", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 1, 2);
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			TestEqual("first use", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
			TestEqual("RemainingCooldown", Hero->GetAbilities()[0].RemainingCooldown, 2);

			// Новый ход: флаги сброшены, перезарядка ещё идёт (ход использования не уменьшает её)
			Hero->ResetTurnState();
			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::OnCooldown);
			TestEqual("OnCooldown comes before InvalidTarget", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(0, 0))), EBattleActionResult::OnCooldown);
		});

		It("returns InvalidTarget for a cell the targeting does not allow", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			Ability->Targeting.Requirement = ETargetRequirement::Enemy;
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			TestEqual("out of range", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 7))), EBattleActionResult::InvalidTarget);
			TestEqual("the source cell", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 5))), EBattleActionResult::InvalidTarget);
			TestEqual("no enemy on the cell", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(4, 5))), EBattleActionResult::InvalidTarget);
			TestEqual("outside the field", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(-1, 5))), EBattleActionResult::InvalidTarget);
			TestFalse("bHasActed", Hero->TurnState.bHasActed);
			TestEqual("a valid cell", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
		});

		It("recomputes the cast on the current board instead of trusting the UI", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			Ability->Targeting.Requirement = ETargetRequirement::Enemy;
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			TArray<FIntPoint> Tiles;
			TestTrue("GetSelectableTiles", Battle->GetSelectableTiles(Hero, 0, Tiles));
			TestTrue("(5,6) was selectable", Tiles.Contains(FIntPoint(5, 6)));

			// Монстр ушёл с клетки после того, как UI получил список
			TestTrue("MoveUnitInternal", Battle->MoveUnitInternal(Monster, FIntPoint(7, 6)));
			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::InvalidTarget);
		});

		It("does not change the state in CanExecute", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }, 1, 1, 1);
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 3);
			const UBattleSubsystem& ConstBattle = *Battle;
			Recorder->Reset();

			TestEqual("result", MakeAction(Hero, 0, FIntPoint(5, 6))->CanExecute(ConstBattle), EBattleActionResult::Success);
			TestEqual("health", Monster->GetHealth(), 3);
			TestFalse("bHasActed", Hero->TurnState.bHasActed);
			TestEqual("cooldown", Hero->GetAbilities()[0].RemainingCooldown, 0);
			TestEqual("OnAbilityUsed count", Recorder->AbilitySources.Num(), 0);
			TestEqual("OnUnitChanged count", Recorder->ChangedUnits.Num(), 0);
		});
	});

	Describe("Execute", [this]()
	{
		It("gives the same health and positions as PreviewAbility", [this]()
		{
			if (!StartBattle()) return;
			// Удар щитом: урон 2 и смещение от источника на 1, противники
			UAbilityDefinition* Ability = TestWorld.MakeAbility({
				AbilityTest::MakeDamage(2, EAreaPart::All, ETargetFilter::Enemies),
				AbilityTest::MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::All, ETargetFilter::Enemies) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 5);

			FAbilityOutcome Preview;
			TestTrue("PreviewAbility", Battle->PreviewAbility(Hero, 0, FIntPoint(5, 6), Preview));
			FIntPoint ExpectedPosition = Monster->GridPosition;
			TestTrue("the preview moves the monster", Preview.GetFinalPosition(Monster->UnitId, ExpectedPosition));
			const int32 ExpectedHealth = Monster->GetHealth() - Preview.GetDamageTo(Monster->UnitId);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);

			TestEqual("health", Monster->GetHealth(), ExpectedHealth);
			TestEqual("health value", Monster->GetHealth(), 3);
			TestEqual("position", Monster->GridPosition, ExpectedPosition);
			TestEqual("position value", Monster->GridPosition, FIntPoint(5, 7));
			TestEqual("the model", Battle->GetGrid()->GetModel().GetOccupant(FIntPoint(5, 7))->UnitId, Monster->UnitId);
			TestEqual("the hero stays", Hero->GridPosition, FIntPoint(5, 5));
		});

		It("marks the hero as acted and starts the cooldown", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 1, 3);
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			TestWorld.SpawnMonster(FIntPoint(5, 6), 9);
			Recorder->Reset();

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);

			TestTrue("bHasActed", Hero->TurnState.bHasActed);
			TestFalse("bHasMoved", Hero->TurnState.bHasMoved);
			TestTrue("CanMove", Hero->CanMove());
			TestEqual("RemainingCooldown", Hero->GetAbilities()[0].RemainingCooldown, 3);
			TestEqual("OnAbilityUsed count", Recorder->AbilitySources.Num(), 1);
		});

		It("does not set bHasMoved with a push of the target or with a dash of the source", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Push = TestWorld.MakeAbility({ AbilityTest::MakePush(EPushDirection::AbilityDirection, 1) });
			ADungeonUnit* Pusher = TestWorld.SpawnHero(FIntPoint(5, 5), Push);
			AMonsterUnit* Pushed = TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			// Выпад: рывок источника и урон по пути
			UAbilityDefinition* Lunge = TestWorld.MakeAbility({
				AbilityTest::MakeMoveSource(ESourceMoveMode::Dash),
				AbilityTest::MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) }, 1, 3);
			Lunge->Targeting.Shape = ETargetingShape::Cross;
			Lunge->Area.Type = EAreaType::Path;
			ADungeonUnit* Dasher = TestWorld.SpawnHero(FIntPoint(1, 6), Lunge);
			AMonsterUnit* Target = TestWorld.SpawnMonster(FIntPoint(4, 6), 9);

			TestEqual("push", Battle->TryExecute(MakeAction(Pusher, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
			TestEqual("the target is pushed", Pushed->GridPosition, FIntPoint(5, 7));
			TestTrue("the pusher has acted", Pusher->TurnState.bHasActed);
			TestFalse("the pusher has not moved", Pusher->TurnState.bHasMoved);
			TestFalse("the target has not moved", Pushed->TurnState.bHasMoved);

			FAbilityOutcome Preview;
			TestTrue("PreviewAbility", Battle->PreviewAbility(Dasher, 0, FIntPoint(4, 6), Preview));
			FIntPoint Expected = Dasher->GridPosition;
			TestTrue("the preview moves the source", Preview.GetFinalPosition(Dasher->UnitId, Expected));
			TestEqual("dash", Battle->TryExecute(MakeAction(Dasher, 0, FIntPoint(4, 6))), EBattleActionResult::Success);
			TestEqual("the source dashed as in the preview", Dasher->GridPosition, Expected);
			TestTrue("and it is not where it started", Dasher->GridPosition != FIntPoint(1, 6));
			TestTrue("the dasher has acted", Dasher->TurnState.bHasActed);
			TestFalse("the dasher has not moved", Dasher->TurnState.bHasMoved);
			TestTrue("CanMove", Dasher->CanMove());
			TestEqual("the dash target took damage", Target->GetHealth(), 8);
		});

		It("gives the hero the experience for a lethal hit", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			AHeroUnit* Hero = Cast<AHeroUnit>(TestWorld.SpawnHero(FIntPoint(5, 5), Ability));
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 2, nullptr, 3);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);

			TestFalse("the monster is dead", Monster->IsAlive());
			TestEqual("Experience", Hero->Experience, 3);
		});

		It("gives the hero the experience for a lethal collision", [this]()
		{
			TestWorld.BuildWalls({ FIntPoint(5, 7) });
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakePush(EPushDirection::AbilityDirection, 1) });
			AHeroUnit* Hero = Cast<AHeroUnit>(TestWorld.SpawnHero(FIntPoint(5, 5), Ability));
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 1, nullptr, 2);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);

			TestFalse("the monster died in the collision", Monster->IsAlive());
			TestEqual("Experience", Hero->Experience, 2);
		});

		It("gives no experience when the hit does not kill", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			AHeroUnit* Hero = Cast<AHeroUnit>(TestWorld.SpawnHero(FIntPoint(5, 5), Ability));
			TestWorld.SpawnMonster(FIntPoint(5, 6), 2, nullptr, 3);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
			TestEqual("Experience", Hero->Experience, 0);
		});

		It("takes a dead monster off the field", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			ADungeonUnit* Hero = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 6), 1);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, 0, FIntPoint(5, 6))), EBattleActionResult::Success);

			TestFalse("IsAlive", Monster->IsAlive());
			TestTrue("hidden", Monster->IsHidden());
			TestFalse("HasIntent", Monster->HasIntent());
			TestFalse("the cell is free", Battle->GetGrid()->GetModel().IsOccupied(FIntPoint(5, 6)));
			TestFalse("not in the snapshot", Battle->MakeSimState().FindUnit(Monster->UnitId) != nullptr);
			TestEqual("OnUnitDied count", Recorder->DiedUnits.Num(), 1);
		});

		It("lets another hero act in the same turn", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) });
			ADungeonUnit* First = TestWorld.SpawnHero(FIntPoint(5, 5), Ability);
			ADungeonUnit* Second = TestWorld.SpawnHero(FIntPoint(6, 6), Ability);
			TestWorld.SpawnMonster(FIntPoint(5, 6), 9);

			TestEqual("first hero", Battle->TryExecute(MakeAction(First, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
			TestEqual("second hero", Battle->TryExecute(MakeAction(Second, 0, FIntPoint(5, 6))), EBattleActionResult::Success);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
