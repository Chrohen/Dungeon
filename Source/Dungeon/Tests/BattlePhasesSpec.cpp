#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Data/UnitDefinition.h"

BEGIN_DEFINE_SPEC(FBattlePhasesSpec, "Dungeon.Battle.Phases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;
END_DEFINE_SPEC(FBattlePhasesSpec)

void FBattlePhasesSpec::Define()
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

	Describe("Subsystem", [this]()
	{
		It("is created in a Game world", [this]()
		{
			TestNotNull("UBattleSubsystem", Battle);
			TestEqual("phase before StartBattle", Battle ? Battle->GetCurrentPhase() : EBattlePhase::Finished, EBattlePhase::None);
		});

		It("supports only Game and PIE worlds", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestTrue("Game", Battle->DoesSupportWorldType(EWorldType::Game));
			TestTrue("PIE", Battle->DoesSupportWorldType(EWorldType::PIE));
			TestFalse("Editor", Battle->DoesSupportWorldType(EWorldType::Editor));
			TestFalse("EditorPreview", Battle->DoesSupportWorldType(EWorldType::EditorPreview));
			TestFalse("Inactive", Battle->DoesSupportWorldType(EWorldType::Inactive));
		});
	});

	Describe("Before StartBattle", [this]()
	{
		It("ignores EndPlayerTurn", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->EndPlayerTurn();
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::None);
			TestEqual("turn", Battle->GetTurnNumber(), 0);
			TestEqual("OnPhaseChanged", Recorder->PhaseChanges.Num(), 0);
			TestEqual("OnTurnStarted", Recorder->StartedTurns.Num(), 0);
		});

		It("ignores AdvancePhase", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->AdvancePhase();
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::None);
			TestEqual("OnPhaseChanged", Recorder->PhaseChanges.Num(), 0);
		});
	});

	Describe("StartBattle", [this]()
	{
		It("stops in PlayerTurn on turn 1 and starts the turn once", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestTrue("IsPlayerTurn", Battle->IsPlayerTurn());
			TestEqual("turn", Battle->GetTurnNumber(), 1);
			TestEqual("TotalWaves", Battle->GetTotalWaves(), 5);
			TestEqual("CurrentWave", Battle->GetCurrentWave(), 0);
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("OnTurnStarted count", Recorder->StartedTurns.Num(), 1);
			if (Recorder->StartedTurns.Num() == 1)
			{
				TestEqual("OnTurnStarted value", Recorder->StartedTurns[0], 1);
			}
		});

		It("passes the phases in order", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			const TArray<EBattlePhase> Expected = {
				EBattlePhase::Preparation, EBattlePhase::MonstersMove, EBattlePhase::MonstersIntent, EBattlePhase::PlayerTurn
			};
			TestTrue("OnPhaseChanged order", Recorder->PhaseChanges == Expected);
		});

		It("spawns the heroes from HeroPlacements", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestEqual("Units", Battle->GetUnits().Num(), 3);
			TestEqual("Heroes", Battle->GetHeroes().Num(), 3);
			TestEqual("OnUnitSpawned", Recorder->SpawnedUnits.Num(), 3);
			TestEqual("unit actors in the world", TestWorld.CountUnitActors(), 3);

			const FGridModel& Model = TestWorld.GetGrid()->GetModel();
			const FIntPoint Positions[] = { FIntPoint(1, 2), FIntPoint(2, 1), FIntPoint(3, 1) };
			for (const FIntPoint& Position : Positions)
			{
				const FGridOccupant* Occupant = Model.GetOccupant(Position);
				TestNotNull(FString::Printf(TEXT("occupant at (%d,%d)"), Position.X, Position.Y), Occupant);
				if (Occupant)
				{
					TestEqual("Team", Occupant->Team, EUnitTeam::Hero);
				}
			}
			for (const AHeroUnit* Hero : Battle->GetHeroes())
			{
				TestTrue("hero stands on its cell", Hero->GetActorLocation().Equals(
					TestWorld.GetGrid()->GridToWorld(Hero->GridPosition) + FVector(0.0, 0.0, Hero->Capsule->GetScaledCapsuleHalfHeight()), 0.01));
			}
		});
	});

	Describe("EndPlayerTurn", [this]()
	{
		It("passes ResolveIntents, WaveSpawn, MonstersMove, MonstersIntent, PlayerTurn", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			Recorder->Reset();

			Battle->EndPlayerTurn();

			const TArray<EBattlePhase> Expected = {
				EBattlePhase::ResolveIntents, EBattlePhase::WaveSpawn, EBattlePhase::MonstersMove,
				EBattlePhase::MonstersIntent, EBattlePhase::PlayerTurn
			};
			TestTrue("OnPhaseChanged order", Recorder->PhaseChanges == Expected);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestEqual("turn", Battle->GetTurnNumber(), 2);
			TestEqual("OnTurnStarted count", Recorder->StartedTurns.Num(), 1);
			if (Recorder->StartedTurns.Num() == 1)
			{
				TestEqual("OnTurnStarted value", Recorder->StartedTurns[0], 2);
			}
		});

		It("reaches turn 11 after 10 calls in a row", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Battle->EndPlayerTurn();
			}
			TestEqual("turn", Battle->GetTurnNumber(), 11);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		});

		It("lets the heroes move again on the new turn", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			AHeroUnit* Hero = Battle->GetHeroes()[0];
			Hero->MarkMoved();
			TestFalse("CanMove after MarkMoved", Hero->CanMove());

			Battle->EndPlayerTurn();
			TestTrue("CanMove on the next turn", Hero->CanMove());
		});
	});

	Describe("SpawnUnit", [this]()
	{
		It("returns nullptr and removes the actor for an occupied cell", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			TestNull("occupied (1,2)", Battle->SpawnUnit(TestWorld.MakeHeroDefinition(), FIntPoint(1, 2)));
			TestNull("outside the field", Battle->SpawnUnit(TestWorld.MakeHeroDefinition(), FIntPoint(8, 8)));
			TestEqual("Units", Battle->GetUnits().Num(), 3);
			TestEqual("OnUnitSpawned", Recorder->SpawnedUnits.Num(), 3);
			TestEqual("unit actors in the world", TestWorld.CountUnitActors(), 3);
		});

		It("returns nullptr without a definition or UnitClass", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());

			UUnitDefinition* NoClass = TestWorld.MakeHeroDefinition();
			NoClass->UnitClass = nullptr;
			TestNull("null definition", Battle->SpawnUnit(nullptr, FIntPoint(5, 5)));
			TestNull("no UnitClass", Battle->SpawnUnit(NoClass, FIntPoint(5, 5)));
		});
	});

	Describe("MoveUnitInternal", [this]()
	{
		It("moves the unit in the model and in the world without OnUnitMoved", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			ADungeonUnit* Unit = Battle->GetUnits()[0];
			const FIntPoint From = Unit->GridPosition;
			const FIntPoint To(6, 6);

			TestTrue("MoveUnitInternal", Battle->MoveUnitInternal(Unit, To));
			TestEqual("GridPosition", Unit->GridPosition, To);
			TestFalse("From is free", TestWorld.GetGrid()->GetModel().IsOccupied(From));
			TestTrue("To is occupied", TestWorld.GetGrid()->GetModel().IsOccupied(To));
			TestEqual("OnUnitMoved is the action's job", Recorder->UnitMovedCount, 0);
		});

		It("returns false for an occupied cell", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			ADungeonUnit* Unit = Battle->GetUnits()[0];
			const FIntPoint From = Unit->GridPosition;

			TestFalse("MoveUnitInternal", Battle->MoveUnitInternal(Unit, Battle->GetUnits()[1]->GridPosition));
			TestEqual("GridPosition", Unit->GridPosition, From);
			TestFalse("null unit", Battle->MoveUnitInternal(nullptr, FIntPoint(6, 6)));
		});
	});

	Describe("FinishBattle", [this]()
	{
		It("moves to Finished, reports the result once and stops the cycle", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			Recorder->Reset();

			Battle->FinishBattle(EBattleResult::Victory);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);
			TestEqual("OnPhaseChanged count", Recorder->PhaseChanges.Num(), 1);

			Battle->AdvancePhase();
			Battle->EndPlayerTurn();
			TestEqual("phase stays", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("OnPhaseChanged count stays", Recorder->PhaseChanges.Num(), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
