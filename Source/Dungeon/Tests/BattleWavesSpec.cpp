#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Data/MonsterDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Волны монстров: первая волна в Preparation, новые в WaveSpawn, места появления.
// Монстры с 0 очков перемещения стоят там, где появились
BEGIN_DEFINE_SPEC(FBattleWavesSpec, "Dungeon.Battle.Waves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	// Задаёт состав волн (MonstersPerWave одинаковых монстров) и запускает бой без сброса записи событий
	bool StartBattle(int32 MonstersPerWave, int32 WaveCount)
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		UMonsterDefinition* Monster = TestWorld.MakeMonsterDefinition(1, 0);
		TArray<UMonsterDefinition*> Composition;
		for (int32 Index = 0; Index < MonstersPerWave; ++Index)
		{
			Composition.Add(Monster);
		}
		TestWorld.SetWaves(Composition, WaveCount);
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		return true;
	}

	TArray<FIntPoint> MonsterCells() const
	{
		TArray<FIntPoint> Cells;
		for (const AMonsterUnit* Monster : Battle->GetLivingMonsters())
		{
			Cells.Add(Monster->GridPosition);
		}
		return Cells;
	}

	// Клетки области монстров 4..7 x 1..6, кроме перечисленных, - стены
	TArray<FIntPoint> WallsExcept(const TArray<FIntPoint>& Free) const
	{
		TArray<FIntPoint> Walls;
		for (int32 Y = 1; Y <= 6; ++Y)
		{
			for (int32 X = 4; X <= 7; ++X)
			{
				if (!Free.Contains(FIntPoint(X, Y)))
				{
					Walls.Add(FIntPoint(X, Y));
				}
			}
		}
		return Walls;
	}
END_DEFINE_SPEC(FBattleWavesSpec)

void FBattleWavesSpec::Define()
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

	Describe("The first wave", [this]()
	{
		It("appears in Preparation before the heroes and sets the counter to 1", [this]()
		{
			if (!StartBattle(3, 5)) return;

			TestEqual("living monsters", Battle->GetLivingMonsters().Num(), 3);
			TestEqual("heroes", Battle->GetHeroes().Num(), 3);
			TestEqual("CurrentWave", Battle->GetCurrentWave(), 1);
			TestEqual("TotalWaves", Battle->GetTotalWaves(), 5);
			TestEqual("OnWaveChanged count", Recorder->WaveChangeCurrent.Num(), 1);
			if (Recorder->WaveChangeCurrent.Num() == 1)
			{
				TestEqual("OnWaveChanged current", Recorder->WaveChangeCurrent[0], 1);
				TestEqual("OnWaveChanged total", Recorder->WaveChangeTotal[0], 5);
			}

			TestEqual("OnUnitSpawned", Recorder->SpawnedUnits.Num(), 6);
			for (int32 Index = 0; Index < Recorder->SpawnedUnits.Num(); ++Index)
			{
				const bool bMonster = Cast<AMonsterUnit>(Recorder->SpawnedUnits[Index].Get()) != nullptr;
				TestEqual(FString::Printf(TEXT("unit %d is a monster"), Index), bMonster, Index < 3);
			}
		});

		It("stands only on distinct passable cells of the monster area", [this]()
		{
			if (!StartBattle(6, 5)) return;

			const TArray<FIntPoint> Cells = MonsterCells();
			TestEqual("monsters", Cells.Num(), 6);
			for (int32 Index = 0; Index < Cells.Num(); ++Index)
			{
				TestTrue("inside MonsterSpawnArea", TestWorld.GetConfig()->MonsterSpawnArea.Contains(Cells[Index]));
				TestEqual("one monster on a cell", Cells.Find(Cells[Index]), Index);
			}
		});

		It("is empty when the composition is empty", [this]()
		{
			if (!StartBattle(0, 5)) return;
			TestEqual("living monsters", Battle->GetLivingMonsters().Num(), 0);
			TestEqual("CurrentWave", Battle->GetCurrentWave(), 1);
		});

		It("gives the heroes their names from HeroPlacements", [this]()
		{
			TestWorld.GetConfig()->HeroPlacements[1].Name = FText::FromString(TEXT("Artur"));
			if (!StartBattle(0, 5)) return;

			const TArray<AHeroUnit*> Heroes = Battle->GetHeroes();
			TestEqual("Heroes", Heroes.Num(), 3);
			if (Heroes.Num() == 3)
			{
				TestEqual("named hero", Heroes[1]->GetDisplayName().ToString(), FString(TEXT("Artur")));
				TestTrue("the others have no name", Heroes[0]->HeroName.IsEmpty());
			}
		});
	});

	Describe("The next waves", [this]()
	{
		It("appear in every WaveSpawn while the counter is below TotalWaves, then the phase is skipped", [this]()
		{
			if (!StartBattle(2, 3)) return;
			Recorder->Reset();

			Battle->EndPlayerTurn();
			TestEqual("wave 2", Battle->GetCurrentWave(), 2);
			TestEqual("monsters after wave 2", Battle->GetLivingMonsters().Num(), 4);
			Battle->EndPlayerTurn();
			TestEqual("wave 3", Battle->GetCurrentWave(), 3);
			TestEqual("monsters after wave 3", Battle->GetLivingMonsters().Num(), 6);

			Battle->EndPlayerTurn();
			TestEqual("no wave 4", Battle->GetCurrentWave(), 3);
			TestEqual("monsters stay", Battle->GetLivingMonsters().Num(), 6);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);

			TestEqual("OnWaveChanged count", Recorder->WaveChangeCurrent.Num(), 2);
			if (Recorder->WaveChangeCurrent.Num() == 2)
			{
				TestEqual("first", Recorder->WaveChangeCurrent[0], 2);
				TestEqual("second", Recorder->WaveChangeCurrent[1], 3);
				TestEqual("total", Recorder->WaveChangeTotal[1], 3);
			}
		});

		It("appear on free cells only", [this]()
		{
			if (!StartBattle(4, 3)) return;
			const TArray<FIntPoint> Before = MonsterCells();
			Battle->EndPlayerTurn();

			const TArray<FIntPoint> After = MonsterCells();
			TestEqual("monsters", After.Num(), 8);
			for (int32 Index = 0; Index < After.Num(); ++Index)
			{
				TestEqual("one monster on a cell", After.Find(After[Index]), Index);
			}
			for (const FIntPoint& Cell : Before)
			{
				TestTrue("the first wave stays", After.Contains(Cell));
			}
		});
	});

	Describe("Not enough cells", [this]()
	{
		It("warns, leaves the monster out and still counts the wave", [this]()
		{
			// Свободны две клетки, а в волне три монстра
			TestWorld.BuildWalls(WallsExcept({ FIntPoint(4, 1), FIntPoint(7, 6) }));
			AddExpectedMessage(TEXT("no free passable tile"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			if (!StartBattle(3, 5)) return;

			TestEqual("two monsters appear", Battle->GetLivingMonsters().Num(), 2);
			const TArray<FIntPoint> Cells = MonsterCells();
			TestTrue("on the free cells", Cells.Contains(FIntPoint(4, 1)) && Cells.Contains(FIntPoint(7, 6)));
			TestEqual("the wave is counted", Battle->GetCurrentWave(), 1);
			TestEqual("the battle goes on", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		});

		It("does not stop the next waves when the area is full", [this]()
		{
			TestWorld.BuildWalls(WallsExcept({ FIntPoint(4, 1), FIntPoint(5, 1), FIntPoint(6, 1) }));
			if (!StartBattle(3, 3)) return;
			TestEqual("the area is full", Battle->GetLivingMonsters().Num(), 3);

			AddExpectedMessage(TEXT("no free passable tile"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
			Battle->EndPlayerTurn();

			TestEqual("no new monster", Battle->GetLivingMonsters().Num(), 3);
			TestEqual("the wave is counted", Battle->GetCurrentWave(), 2);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		});
	});

	Describe("Random places", [this]()
	{
		It("repeat with a fixed seed", [this]()
		{
			TestWorld.SetSeed(777);
			if (!StartBattle(4, 3)) return;
			const TArray<FIntPoint> First = MonsterCells();
			Battle->EndPlayerTurn();
			const TArray<FIntPoint> FirstAfterWave = MonsterCells();

			if (!StartBattle(4, 3)) return;
			const TArray<FIntPoint> Second = MonsterCells();
			Battle->EndPlayerTurn();
			const TArray<FIntPoint> SecondAfterWave = MonsterCells();

			TestEqual("monsters", First.Num(), 4);
			TestTrue("the first wave", First == Second);
			TestTrue("the second wave", FirstAfterWave == SecondAfterWave);
		});

		It("differ between fixed seeds", [this]()
		{
			TArray<TArray<FIntPoint>> Places;
			for (int32 Seed = 1; Seed <= 6; ++Seed)
			{
				TestWorld.SetSeed(Seed);
				if (!StartBattle(4, 3)) return;
				Places.Add(MonsterCells());
			}
			bool bDifferent = false;
			for (int32 Index = 1; Index < Places.Num(); ++Index)
			{
				bDifferent = bDifferent || Places[Index] != Places[0];
			}
			TestTrue("the seed matters", bDifferent);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
