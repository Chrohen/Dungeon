#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Components/CapsuleComponent.h"
#include "Data/MonsterDefinition.h"
#include "Data/UnitDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

BEGIN_DEFINE_SPEC(FBattleMoveActionSpec, "Dungeon.Battle.MoveAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	// Герой (1,2) из HeroPlacements
	ADungeonUnit* GetHero() const { return Battle->GetGrid()->GetModel().GetOccupant(FIntPoint(1, 2))->Unit.Get(); }

	UMoveAction* MakeAction(ADungeonUnit* Unit, FIntPoint Target) const
	{
		UMoveAction* Action = NewObject<UMoveAction>();
		Action->Unit = Unit;
		Action->Target = Target;
		return Action;
	}

	// Подсистема создана, бой запущен
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
END_DEFINE_SPEC(FBattleMoveActionSpec)

void FBattleMoveActionSpec::Define()
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
			TestEqual("result", Battle->TryExecute(MakeAction(nullptr, FIntPoint(1, 3))), EBattleActionResult::InvalidPhase);
		});

		It("returns InvalidUnit for a null unit", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("result", Battle->TryExecute(MakeAction(nullptr, FIntPoint(1, 3))), EBattleActionResult::InvalidUnit);
		});

		It("returns InvalidUnit for a null action", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("result", Battle->TryExecute(nullptr), EBattleActionResult::InvalidUnit);
		});

		It("returns NotPlayerUnit for a monster", [this]()
		{
			if (!StartBattle()) return;
			UUnitDefinition* MonsterDefinition = TestWorld.MakeHeroDefinition();
			MonsterDefinition->UnitClass = AMonsterUnit::StaticClass();
			ADungeonUnit* Monster = Battle->SpawnUnit(MonsterDefinition, FIntPoint(6, 6));
			TestNotNull("monster", Monster);
			TestEqual("result", Battle->TryExecute(MakeAction(Monster, FIntPoint(6, 5))), EBattleActionResult::NotPlayerUnit);
		});

		It("returns AlreadyMoved on a repeated move", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TestEqual("first move", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 3))), EBattleActionResult::Success);
			TestEqual("second move", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 4))), EBattleActionResult::AlreadyMoved);
		});

		It("returns OutOfGrid outside the field", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TestEqual("(-1,2)", Battle->TryExecute(MakeAction(Hero, FIntPoint(-1, 2))), EBattleActionResult::OutOfGrid);
			TestEqual("(8,2)", Battle->TryExecute(MakeAction(Hero, FIntPoint(8, 2))), EBattleActionResult::OutOfGrid);
			TestEqual("(1,8)", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 8))), EBattleActionResult::OutOfGrid);
		});

		It("returns Occupied for a cell with another hero", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("result", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(2, 1))), EBattleActionResult::Occupied);
		});

		It("returns Unreachable for a cell farther than the move points", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			// (1,2) -> (7,2): 6 шагов при 5 очках
			TestEqual("6 steps", Battle->TryExecute(MakeAction(Hero, FIntPoint(7, 2))), EBattleActionResult::Unreachable);
			TestEqual("far corner", Battle->TryExecute(MakeAction(Hero, FIntPoint(7, 7))), EBattleActionResult::Unreachable);
			TestTrue("the hero has not moved", Hero->CanMove());
		});

		It("returns Unreachable for a cell behind impassable tiles", [this]()
		{
			// Клетка (5,2) в 4 шагах от героя, но со всех сторон закрыта стенами
			TestWorld.BuildWalls({ FIntPoint(4, 2), FIntPoint(6, 2), FIntPoint(5, 1), FIntPoint(5, 3) });
			if (!StartBattle()) return;
			TestEqual("walled cell", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(5, 2))), EBattleActionResult::Unreachable);
			TestEqual("wall itself", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(4, 2))), EBattleActionResult::Unreachable);
		});

		It("reports the first error in the table order", [this]()
		{
			if (!StartBattle()) return;
			// Клетка героя (2,1) недоступна и как конечная, но ошибка Occupied стоит в таблице раньше Unreachable
			const UMoveAction* Action = MakeAction(GetHero(), FIntPoint(2, 1));
			TestEqual("Occupied is checked before Unreachable", Action->CanExecute(*Battle), EBattleActionResult::Occupied);

			// OutOfGrid стоит раньше Occupied, AlreadyMoved раньше OutOfGrid
			ADungeonUnit* Hero = GetHero();
			TestEqual("OutOfGrid", MakeAction(Hero, FIntPoint(-1, -1))->CanExecute(*Battle), EBattleActionResult::OutOfGrid);
			Hero->MarkMoved();
			TestEqual("AlreadyMoved is checked before OutOfGrid", MakeAction(Hero, FIntPoint(-1, -1))->CanExecute(*Battle), EBattleActionResult::AlreadyMoved);
		});
	});

	Describe("Execute", [this]()
	{
		It("moves the hero in the model, GridPosition and the world and reports OnUnitMoved", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			const FIntPoint From(1, 2);
			const FIntPoint To(3, 3);

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, To)), EBattleActionResult::Success);

			const FGridModel& Model = Battle->GetGrid()->GetModel();
			TestFalse("From is free", Model.IsOccupied(From));
			TestTrue("To is occupied", Model.IsOccupied(To));
			TestEqual("GridPosition", Hero->GridPosition, To);
			TestTrue("location", Hero->GetActorLocation().Equals(
				Battle->GetGrid()->GridToWorld(To) + FVector(0.0, 0.0, Hero->Capsule->GetScaledCapsuleHalfHeight()), 0.01));
			TestFalse("CanMove", Hero->CanMove());

			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 1);
			if (Recorder->UnitMovedCount == 1)
			{
				TestTrue("Unit", Recorder->MovedUnits[0].Get() == Hero);
				TestEqual("From", Recorder->MovedFrom[0], From);
				TestEqual("To", Recorder->MovedTo[0], To);
			}
		});

		It("reaches a cell exactly at the move points limit", [this]()
		{
			if (!StartBattle()) return;
			// (1,2) -> (6,2): 5 шагов при 5 очках
			TestEqual("result", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(6, 2))), EBattleActionResult::Success);
		});

		It("changes nothing and sends no event when the check fails", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TestEqual("result", Battle->TryExecute(MakeAction(Hero, FIntPoint(7, 7))), EBattleActionResult::Unreachable);
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 2));
			TestTrue("CanMove", Hero->CanMove());
			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 0);
		});

		It("lets the hero move again after EndPlayerTurn", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TestEqual("turn 1", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 3))), EBattleActionResult::Success);
			TestEqual("same turn", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 4))), EBattleActionResult::AlreadyMoved);

			Battle->EndPlayerTurn();
			TestEqual("turn 2", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 4))), EBattleActionResult::Success);
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 4));
		});

		It("returns InvalidPhase when it is not the player's turn", [this]()
		{
			// Ноль волн и ни одного монстра: бой заканчивается победой при входе в первый ход игрока
			TestWorld.SetWaves({}, 0);
			if (!StartBattle()) return;
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("result", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(1, 3))), EBattleActionResult::InvalidPhase);
		});
	});

	Describe("CanExecute", [this]()
	{
		It("does not change the state", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			const UMoveAction* Action = MakeAction(Hero, FIntPoint(3, 3));
			const UBattleSubsystem& ConstBattle = *Battle;

			TestEqual("result", Action->CanExecute(ConstBattle), EBattleActionResult::Success);
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 2));
			TestTrue("CanMove", Hero->CanMove());
			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 0);
		});
	});

	Describe("Dead units and phases", [this]()
	{
		It("returns InvalidUnit for a dead unit", [this]()
		{
			if (!StartBattle()) return;
			// KillUnit закрыт: монстр убивает героя ударом через ExecuteAbilityInternal
			UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) });
			AMonsterUnit* Killer = TestWorld.SpawnMonster(FIntPoint(1, 3), 1, Deadly);
			ADungeonUnit* Hero = GetHero();
			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Battle->MakeSimState(), Killer->UnitId, Killer->GetAbilities()[0].Ability, Hero->GridPosition, Cast));
			Battle->ExecuteAbilityInternal(Killer, 0, Cast);
			TestFalse("the hero is dead", Hero->IsAlive());

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 4))), EBattleActionResult::InvalidUnit);
			TestEqual("the cell is free", Battle->GetGrid()->GetModel().IsOccupied(FIntPoint(1, 4)), false);
		});

		It("checks the unit before the team and the move flag", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) });
			AMonsterUnit* Victim = TestWorld.SpawnMonster(FIntPoint(5, 5), 1);
			AMonsterUnit* Killer = TestWorld.SpawnMonster(FIntPoint(5, 6), 1, Deadly);
			FAbilityCast Cast;
			FAbilityRules::MakeCast(Battle->MakeSimState(), Killer->UnitId, Killer->GetAbilities()[0].Ability, Victim->GridPosition, Cast);
			Battle->ExecuteAbilityInternal(Killer, 0, Cast);
			Victim->MarkMoved();

			// Погибший монстр: InvalidUnit раньше NotPlayerUnit и AlreadyMoved
			TestEqual("result", MakeAction(Victim, FIntPoint(5, 4))->CanExecute(*Battle), EBattleActionResult::InvalidUnit);
		});

		It("returns InvalidPhase for a hero outside PlayerTurn and MonstersMove", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TMap<EBattlePhase, EBattleActionResult> Results;
			Recorder->PhaseHook = [&](EBattlePhase Phase)
			{
				if (Phase == EBattlePhase::ResolveIntents || Phase == EBattlePhase::WaveSpawn || Phase == EBattlePhase::MonstersIntent)
				{
					Results.Add(Phase, Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 3))));
				}
			};

			Battle->EndPlayerTurn();

			TestEqual("phases checked", Results.Num(), 3);
			for (const TPair<EBattlePhase, EBattleActionResult>& Pair : Results)
			{
				TestEqual(FString::Printf(TEXT("phase %d"), static_cast<int32>(Pair.Key)), Pair.Value, EBattleActionResult::InvalidPhase);
			}
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 2));
		});

		It("returns NotPlayerUnit for a hero in MonstersMove", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();
			TOptional<EBattleActionResult> Result;
			Recorder->PhaseHook = [&](EBattlePhase Phase)
			{
				if (Phase == EBattlePhase::MonstersMove)
				{
					Result = Battle->TryExecute(MakeAction(Hero, FIntPoint(1, 3)));
				}
			};

			Battle->EndPlayerTurn();

			TestTrue("checked", Result.IsSet());
			if (Result.IsSet())
			{
				TestEqual("result", Result.GetValue(), EBattleActionResult::NotPlayerUnit);
			}
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 2));
		});
	});

	Describe("Execute events", [this]()
	{
		It("sends OnUnitChanged and rebuilds the intent views", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = GetHero();

			TestEqual("result", Battle->TryExecute(MakeAction(Hero, FIntPoint(3, 3))), EBattleActionResult::Success);

			TestEqual("OnUnitChanged count", Recorder->ChangedUnits.Num(), 1);
			if (Recorder->ChangedUnits.Num() == 1)
			{
				TestTrue("Unit", Recorder->ChangedUnits[0].Get() == Hero);
			}
			TestEqual("OnIntentsChanged count", Recorder->IntentsChangedCount, 1);
		});

		It("sends no OnUnitChanged and no OnIntentsChanged when the check fails", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("result", Battle->TryExecute(MakeAction(GetHero(), FIntPoint(7, 7))), EBattleActionResult::Unreachable);
			TestEqual("OnUnitChanged count", Recorder->ChangedUnits.Num(), 0);
			TestEqual("OnIntentsChanged count", Recorder->IntentsChangedCount, 0);
		});
	});

	Describe("Monsters in MonstersMove", [this]()
	{
		It("moves toward the heroes through UMoveAction in MonstersMove", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestWorld.GetConfig()->MonsterSpawnArea = FGridArea{ 7, 7, 1, 1 };
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 3) }, 5);
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 1);
			if (Recorder->UnitMovedCount == 1)
			{
				TestTrue("a monster", Cast<AMonsterUnit>(Recorder->MovedUnits[0].Get()) != nullptr);
				TestEqual("From", Recorder->MovedFrom[0], FIntPoint(7, 7));
				TestEqual("the phase of the event", Recorder->MovedPhases[0], EBattlePhase::MonstersMove);
				TestTrue("within the move points", FAbilityRules::Distance(Recorder->MovedFrom[0], Recorder->MovedTo[0]) <= 3);
				TestTrue("closer to the heroes", FAbilityRules::Distance(Recorder->MovedTo[0], FIntPoint(3, 1)) < FAbilityRules::Distance(FIntPoint(7, 7), FIntPoint(3, 1)));
			}
			TestEqual("the monsters are on the field", Battle->GetLivingMonsters().Num(), 1);
			TestTrue("the move is spent", Battle->GetLivingMonsters()[0]->TurnState.bHasMoved);
			TestTrue("OnUnitChanged", Recorder->ChangedUnits.Num() > 0);
		});

		It("leaves a monster without move points in place", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestWorld.GetConfig()->MonsterSpawnArea = FGridArea{ 7, 7, 1, 1 };
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 5);
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 0);
			TestEqual("the monster stays", Battle->GetLivingMonsters()[0]->GridPosition, FIntPoint(7, 7));
		});

		It("lets the monster move again on the next turn", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestWorld.GetConfig()->MonsterSpawnArea = FGridArea{ 7, 7, 1, 1 };
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 3) }, 1);
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			const FIntPoint AfterFirstTurn = Battle->GetLivingMonsters()[0]->GridPosition;

			Battle->EndPlayerTurn();

			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 2);
			TestTrue("the monster went on", Battle->GetLivingMonsters()[0]->GridPosition != AfterFirstTurn);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
