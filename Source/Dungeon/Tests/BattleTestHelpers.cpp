#include "Tests/BattleTestHelpers.h"
#include "Battle/BattleSubsystem.h"

void UBattleEventRecorder::Subscribe(UBattleSubsystem& Battle)
{
	Battle.OnPhaseChanged.AddDynamic(this, &UBattleEventRecorder::HandlePhaseChanged);
	Battle.OnTurnStarted.AddDynamic(this, &UBattleEventRecorder::HandleTurnStarted);
	Battle.OnUnitSpawned.AddDynamic(this, &UBattleEventRecorder::HandleUnitSpawned);
	Battle.OnUnitMoved.AddDynamic(this, &UBattleEventRecorder::HandleUnitMoved);
	Battle.OnBattleEnded.AddDynamic(this, &UBattleEventRecorder::HandleBattleEnded);
}

void UBattleEventRecorder::HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	PhaseChanges.Add(NewPhase);
}

void UBattleEventRecorder::HandleTurnStarted(int32 TurnNumber)
{
	StartedTurns.Add(TurnNumber);
}

void UBattleEventRecorder::HandleUnitSpawned(ADungeonUnit* Unit)
{
	SpawnedUnits.Add(Unit);
}

void UBattleEventRecorder::HandleUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To)
{
	++UnitMovedCount;
	MovedUnits.Add(Unit);
	MovedFrom.Add(From);
	MovedTo.Add(To);
}

void UBattleEventRecorder::HandleBattleEnded(EBattleResult Result)
{
	EndedResults.Add(Result);
}

void UBattleEventRecorder::Reset()
{
	PhaseChanges.Reset();
	StartedTurns.Reset();
	SpawnedUnits.Reset();
	UnitMovedCount = 0;
	MovedUnits.Reset();
	MovedFrom.Reset();
	MovedTo.Reset();
	EndedResults.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Data/UnitDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/DungeonGridActor.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"

FBattleTestWorld::~FBattleTestWorld()
{
	Destroy();
}

void FBattleTestWorld::Create()
{
	Destroy();

	UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	World.Reset(NewWorld);

	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(NewWorld);
	NewWorld->InitializeActorsForPlay(FURL());
	NewWorld->BeginPlay();

	Floor.Reset(NewObject<UTileTypeDefinition>());
	Floor->bPassable = true;
	Floor->MoveCost = 1;

	Wall.Reset(NewObject<UTileTypeDefinition>());
	Wall->bPassable = false;

	Config.Reset(NewObject<UBattlefieldConfig>());
	Config->Width = 8;
	Config->Height = 8;
	Config->TileSize = 100.f;
	Config->DefaultTileType = Floor.Get();
	Config->HeroSpawnArea = FGridArea{ 1, 1, 3, 6 };
	Config->MonsterSpawnArea = FGridArea{ 4, 1, 4, 6 };
	Config->WaveCount = 5;

	const FIntPoint Positions[] = { FIntPoint(1, 2), FIntPoint(2, 1), FIntPoint(3, 1) };
	for (const FIntPoint& Position : Positions)
	{
		FHeroPlacement Placement;
		Placement.Hero = MakeHeroDefinition();
		Placement.Position = Position;
		Config->HeroPlacements.Add(Placement);
	}

	Grid = NewWorld->SpawnActor<ADungeonGridActor>();
	Grid->BuildFromConfig(Config.Get());
}

void FBattleTestWorld::Destroy()
{
	if (World.IsValid())
	{
		UWorld* OldWorld = World.Get();
		GEngine->DestroyWorldContext(OldWorld);
		OldWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}
	Grid = nullptr;
	World.Reset();
	Config.Reset();
	Floor.Reset();
	Wall.Reset();
	Definitions.Reset();
}

void FBattleTestWorld::BuildWalls(const TArray<FIntPoint>& Cells)
{
	Config->TileOverrides.Reset();
	for (const FIntPoint& Cell : Cells)
	{
		FTileOverride TileOverride;
		TileOverride.Coord = Cell;
		TileOverride.Type = Wall.Get();
		Config->TileOverrides.Add(TileOverride);
	}
	Grid->BuildFromConfig(Config.Get());
}

UBattleSubsystem* FBattleTestWorld::GetBattle() const
{
	return World.IsValid() ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
}

UUnitDefinition* FBattleTestWorld::MakeHeroDefinition()
{
	UUnitDefinition* Definition = NewObject<UUnitDefinition>();
	Definition->UnitClass = AHeroUnit::StaticClass();
	Definition->MovePoints = 5;
	Definitions.Emplace(Definition);
	return Definition;
}

int32 FBattleTestWorld::CountUnitActors() const
{
	int32 Count = 0;
	for (TActorIterator<ADungeonUnit> It(World.Get()); It; ++It)
	{
		if (IsValid(*It))
		{
			++Count;
		}
	}
	return Count;
}

#endif // WITH_DEV_AUTOMATION_TESTS
