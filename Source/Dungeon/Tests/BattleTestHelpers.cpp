#include "Tests/BattleTestHelpers.h"
#include "Battle/BattleSubsystem.h"
#include "Input/BattleSelectionComponent.h"

void UBattleEventRecorder::Subscribe(UBattleSubsystem& Battle)
{
	Observed = &Battle;
	Battle.OnBattleStarted.AddDynamic(this, &UBattleEventRecorder::HandleBattleStarted);
	Battle.OnPhaseChanged.AddDynamic(this, &UBattleEventRecorder::HandlePhaseChanged);
	Battle.OnTurnStarted.AddDynamic(this, &UBattleEventRecorder::HandleTurnStarted);
	Battle.OnWaveChanged.AddDynamic(this, &UBattleEventRecorder::HandleWaveChanged);
	Battle.OnUnitSpawned.AddDynamic(this, &UBattleEventRecorder::HandleUnitSpawned);
	Battle.OnUnitMoved.AddDynamic(this, &UBattleEventRecorder::HandleUnitMoved);
	Battle.OnUnitChanged.AddDynamic(this, &UBattleEventRecorder::HandleUnitChanged);
	Battle.OnUnitDied.AddDynamic(this, &UBattleEventRecorder::HandleUnitDied);
	Battle.OnAbilityUsed.AddDynamic(this, &UBattleEventRecorder::HandleAbilityUsed);
	Battle.OnIntentsChanged.AddDynamic(this, &UBattleEventRecorder::HandleIntentsChanged);
	Battle.OnBattleEnded.AddDynamic(this, &UBattleEventRecorder::HandleBattleEnded);
}

void UBattleEventRecorder::HandleBattleStarted()
{
	if (BattleStartedCount == 0)
	{
		PhaseChangesAtBattleStarted = PhaseChanges.Num();
	}
	++BattleStartedCount;
}

void UBattleEventRecorder::HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	PhaseChanges.Add(NewPhase);
	if (PhaseHook)
	{
		PhaseHook(NewPhase);
	}
}

void UBattleEventRecorder::HandleTurnStarted(int32 TurnNumber)
{
	StartedTurns.Add(TurnNumber);
}

void UBattleEventRecorder::HandleWaveChanged(int32 CurrentWave, int32 TotalWaves)
{
	WaveChangeCurrent.Add(CurrentWave);
	WaveChangeTotal.Add(TotalWaves);
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
	MovedPhases.Add(Observed.IsValid() ? Observed->GetCurrentPhase() : EBattlePhase::None);
}

void UBattleEventRecorder::HandleUnitChanged(ADungeonUnit* Unit)
{
	ChangedUnits.Add(Unit);
}

void UBattleEventRecorder::HandleUnitDied(ADungeonUnit* Unit, ADungeonUnit* Killer)
{
	DiedUnits.Add(Unit);
	Killers.Add(Killer);
}

void UBattleEventRecorder::HandleAbilityUsed(ADungeonUnit* Source, int32 AbilityIndex, FIntPoint TargetPoint)
{
	AbilitySources.Add(Source);
	AbilityIndices.Add(AbilityIndex);
	AbilityTargetPoints.Add(TargetPoint);
}

void UBattleEventRecorder::HandleIntentsChanged()
{
	++IntentsChangedCount;
}

void UBattleEventRecorder::HandleBattleEnded(EBattleResult Result)
{
	if (EndedResults.Num() == 0)
	{
		PhaseChangesAtBattleEnd = PhaseChanges.Num();
	}
	EndedResults.Add(Result);
}

void UBattleEventRecorder::Reset()
{
	BattleStartedCount = 0;
	PhaseChangesAtBattleStarted = INDEX_NONE;
	PhaseChanges.Reset();
	StartedTurns.Reset();
	WaveChangeCurrent.Reset();
	WaveChangeTotal.Reset();
	SpawnedUnits.Reset();
	UnitMovedCount = 0;
	MovedUnits.Reset();
	MovedFrom.Reset();
	MovedTo.Reset();
	MovedPhases.Reset();
	ChangedUnits.Reset();
	DiedUnits.Reset();
	Killers.Reset();
	AbilitySources.Reset();
	AbilityIndices.Reset();
	AbilityTargetPoints.Reset();
	IntentsChangedCount = 0;
	EndedResults.Reset();
	PhaseChangesAtBattleEnd = INDEX_NONE;
}

void USelectionEventRecorder::Subscribe(UBattleSelectionComponent& Selection)
{
	Selection.OnSelectionChanged.AddDynamic(this, &USelectionEventRecorder::HandleSelectionChanged);
	Selection.OnHoveredUnitChanged.AddDynamic(this, &USelectionEventRecorder::HandleHoveredUnitChanged);
}

void USelectionEventRecorder::HandleSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex)
{
	States.Add(State);
	Heroes.Add(Hero);
	Abilities.Add(AbilityIndex);
}

void USelectionEventRecorder::HandleHoveredUnitChanged(ADungeonUnit* Unit)
{
	HoveredUnits.Add(Unit);
}

void USelectionEventRecorder::Reset()
{
	States.Reset();
	Heroes.Reset();
	Abilities.Reset();
	HoveredUnits.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/AbilityDefinition.h"
#include "Data/BattleRulesConfig.h"
#include "Data/BattlefieldConfig.h"
#include "Data/MonsterDefinition.h"
#include "Data/TileTypeDefinition.h"
#include "Data/UnitDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/DungeonGridActor.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

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
	// Волн много, а состав пуст: цикл ходов остаётся бесконечным, как в спринте 1
	// (иначе без монстров победа наступила бы сразу)
	Config->WaveCount = 100;

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
	AbilityDefinitions.Reset();
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

namespace
{
	void AssignAbilities(UUnitDefinition& Definition, const TArray<UAbilityDefinition*>& Abilities)
	{
		for (UAbilityDefinition* Ability : Abilities)
		{
			FUnitAbilityEntry Entry;
			Entry.Ability = Ability;
			Definition.Abilities.Add(Entry);
		}
	}
}

UUnitDefinition* FBattleTestWorld::MakeHeroDefinition(int32 MaxHealth, const TArray<UAbilityDefinition*>& Abilities)
{
	UUnitDefinition* Definition = NewObject<UUnitDefinition>();
	Definition->UnitClass = AHeroUnit::StaticClass();
	Definition->MovePoints = 5;
	Definition->MaxHealth = MaxHealth;
	AssignAbilities(*Definition, Abilities);
	Definitions.Emplace(Definition);
	return Definition;
}

UMonsterDefinition* FBattleTestWorld::MakeMonsterDefinition(int32 MaxHealth, int32 MovePoints, const TArray<UAbilityDefinition*>& Abilities)
{
	UMonsterDefinition* Definition = NewObject<UMonsterDefinition>();
	Definition->UnitClass = AMonsterUnit::StaticClass();
	Definition->MovePoints = MovePoints;
	Definition->MaxHealth = MaxHealth;
	AssignAbilities(*Definition, Abilities);
	Definitions.Emplace(Definition);
	return Definition;
}

UAbilityDefinition* FBattleTestWorld::MakeAbility(const TArray<FAbilityEffect>& Effects, int32 MinRange, int32 MaxRange, int32 Cooldown)
{
	UAbilityDefinition* Ability = NewObject<UAbilityDefinition>();
	Ability->Targeting.Shape = ETargetingShape::Diamond;
	Ability->Targeting.MinRange = MinRange;
	Ability->Targeting.MaxRange = MaxRange;
	Ability->Targeting.Trajectory = ETrajectory::Instant;
	Ability->Targeting.Requirement = ETargetRequirement::AnyTile;
	Ability->Area.Type = EAreaType::Tile;
	Ability->Effects = Effects;
	Ability->Cooldown = Cooldown;
	AbilityDefinitions.Emplace(Ability);
	return Ability;
}

ADungeonUnit* FBattleTestWorld::SpawnHero(FIntPoint Cell, UAbilityDefinition* Ability, int32 MaxHealth)
{
	TArray<UAbilityDefinition*> Abilities;
	if (Ability != nullptr)
	{
		Abilities.Add(Ability);
	}
	return GetBattle()->SpawnUnit(MakeHeroDefinition(MaxHealth, Abilities), Cell);
}

AMonsterUnit* FBattleTestWorld::SpawnMonster(FIntPoint Cell, int32 MaxHealth, UAbilityDefinition* Ability, int32 ExperienceReward, int32 MovePoints)
{
	TArray<UAbilityDefinition*> Abilities;
	if (Ability != nullptr)
	{
		Abilities.Add(Ability);
	}
	UMonsterDefinition* Definition = MakeMonsterDefinition(MaxHealth, MovePoints, Abilities);
	Definition->ExperienceReward = ExperienceReward;
	return Cast<AMonsterUnit>(GetBattle()->SpawnUnit(Definition, Cell));
}

void FBattleTestWorld::SetWaves(const TArray<UMonsterDefinition*>& Composition, int32 Count)
{
	Config->WaveComposition.Reset();
	for (UMonsterDefinition* Monster : Composition)
	{
		Config->WaveComposition.Add(Monster);
	}
	Config->WaveCount = Count;
}

void FBattleTestWorld::SetSingleHero(UUnitDefinition* Hero, FIntPoint Position)
{
	FHeroPlacement Placement;
	Placement.Hero = Hero;
	Placement.Position = Position;
	Config->HeroPlacements.Reset();
	Config->HeroPlacements.Add(Placement);
}

void FBattleTestWorld::SetHeroHealth(int32 Health)
{
	for (const FHeroPlacement& Placement : Config->HeroPlacements)
	{
		if (Placement.Hero != nullptr)
		{
			Placement.Hero->MaxHealth = Health;
		}
	}
}

void FBattleTestWorld::SetSeed(int32 Seed)
{
	Config->RandomSeed = Seed;
}

UBattleRulesConfig* FBattleTestWorld::SetRules(int32 CollisionDamage, float MonsterActionDelay)
{
	// Config держит правила через UPROPERTY, отдельная ссылка не нужна
	UBattleRulesConfig* Rules = NewObject<UBattleRulesConfig>();
	Rules->CollisionDamage = CollisionDamage;
	Rules->MonsterActionDelay = MonsterActionDelay;
	Config->Rules = Rules;
	return Rules;
}

void FBattleTestWorld::AdvanceTime(float Seconds)
{
	constexpr float Step = 0.1f;
	for (float Elapsed = 0.f; Elapsed < Seconds - KINDA_SMALL_NUMBER; Elapsed += Step)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, Step);
	}
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
