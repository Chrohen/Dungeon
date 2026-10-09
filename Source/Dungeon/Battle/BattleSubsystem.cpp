#include "Battle/BattleSubsystem.h"
#include "Battle/BattleAction.h"
#include "Battle/BattlePhases.h"
#include "Data/BattlefieldConfig.h"
#include "Data/UnitDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogDungeonBattle);

bool UBattleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBattleSubsystem::StartBattle(UBattlefieldConfig* InConfig, ADungeonGridActor* InGrid)
{
	if (InConfig == nullptr || InGrid == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("StartBattle: Config and Grid must be set."));
		return;
	}

	// Убираем юнитов предыдущего боя
	for (ADungeonUnit* Unit : Units)
	{
		if (Unit != nullptr)
		{
			if (Grid != nullptr)
			{
				Grid->GetMutableModel().RemoveUnit(Unit->GridPosition);
			}
			Unit->Destroy();
		}
	}
	Units.Reset();
	Phases.Reset();

	Config = InConfig;
	Grid = InGrid;
	CurrentPhase = EBattlePhase::None;
	TurnNumber = 0;
	CurrentWave = 0;
	TotalWaves = InConfig->WaveCount;
	Result = EBattleResult::None;
	bIsTransitioning = false;
	bTransitionPending = false;

	const TSubclassOf<UBattlePhase> PhaseClasses[] = {
		UPreparationPhase::StaticClass(),
		UMonstersMovePhase::StaticClass(),
		UMonstersIntentPhase::StaticClass(),
		UPlayerTurnPhase::StaticClass(),
		UResolveIntentsPhase::StaticClass(),
		UWaveSpawnPhase::StaticClass(),
	};
	for (const TSubclassOf<UBattlePhase>& PhaseClass : PhaseClasses)
	{
		UBattlePhase* Phase = NewObject<UBattlePhase>(this, PhaseClass);
		Phases.Add(Phase->GetPhaseType(), Phase);
	}

	TransitionTo(EBattlePhase::Preparation);
}

EBattleActionResult UBattleSubsystem::TryExecute(UBattleAction* Action)
{
	if (Action == nullptr)
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("TryExecute: Action is null."));
		return EBattleActionResult::InvalidUnit;
	}

	const EBattleActionResult CheckResult = Action->CanExecute(*this);
	if (CheckResult == EBattleActionResult::Success)
	{
		Action->Execute(*this);
	}
	return CheckResult;
}

void UBattleSubsystem::EndPlayerTurn()
{
	if (IsPlayerTurn())
	{
		AdvancePhase();
	}
}

void UBattleSubsystem::AdvancePhase()
{
	if (bIsTransitioning)
	{
		bTransitionPending = true; // вызов из Enter(): флаг подхватит цикл в TransitionTo
		return;
	}

	const TObjectPtr<UBattlePhase>* Current = Phases.Find(CurrentPhase);
	if (Current == nullptr || *Current == nullptr)
	{
		return; // у фаз None и Finished нет объекта
	}
	TransitionTo((*Current)->GetNextPhase());
}

void UBattleSubsystem::TransitionTo(EBattlePhase Next)
{
	bIsTransitioning = true;
	SetPhase(Next);
	while (bTransitionPending)
	{
		bTransitionPending = false;
		const TObjectPtr<UBattlePhase>* Current = Phases.Find(CurrentPhase);
		if (Current == nullptr || *Current == nullptr)
		{
			break;
		}
		SetPhase((*Current)->GetNextPhase());
	}
	bIsTransitioning = false;
}

void UBattleSubsystem::SetPhase(EBattlePhase Next)
{
	const EBattlePhase OldPhase = CurrentPhase;

	if (const TObjectPtr<UBattlePhase>* Old = Phases.Find(OldPhase))
	{
		if (*Old != nullptr)
		{
			(*Old)->Exit(*this);
		}
	}

	CurrentPhase = Next;

	if (const TObjectPtr<UBattlePhase>* New = Phases.Find(Next))
	{
		if (*New != nullptr)
		{
			(*New)->Enter(*this);
		}
	}

	OnPhaseChanged.Broadcast(OldPhase, Next);
}

void UBattleSubsystem::FinishBattle(EBattleResult InResult)
{
	Result = InResult;
	bTransitionPending = false;
	SetPhase(EBattlePhase::Finished);
	OnBattleEnded.Broadcast(Result);
}

ADungeonUnit* UBattleSubsystem::SpawnUnit(UUnitDefinition* Def, FIntPoint Coord)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Grid == nullptr || Def == nullptr || Def->UnitClass == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADungeonUnit* Unit = World->SpawnActor<ADungeonUnit>(Def->UnitClass, FTransform(Grid->GridToWorld(Coord)), SpawnParams);
	if (Unit == nullptr)
	{
		return nullptr;
	}

	Unit->InitializeUnit(Def, Coord);

	FGridOccupant Occupant;
	Occupant.Unit = Unit;
	Occupant.Team = Unit->Team;
	if (!Grid->GetMutableModel().PlaceUnit(Occupant, Coord))
	{
		Unit->Destroy();
		return nullptr;
	}

	Unit->SetGridPosition(Coord, Grid->GridToWorld(Coord));
	Units.Add(Unit);
	OnUnitSpawned.Broadcast(Unit);
	return Unit;
}

bool UBattleSubsystem::MoveUnitInternal(ADungeonUnit* Unit, FIntPoint To)
{
	if (Unit == nullptr || Grid == nullptr)
	{
		return false;
	}

	if (!Grid->GetMutableModel().MoveUnit(Unit->GridPosition, To))
	{
		return false;
	}
	Unit->SetGridPosition(To, Grid->GridToWorld(To));
	return true;
}

TArray<AHeroUnit*> UBattleSubsystem::GetHeroes() const
{
	TArray<AHeroUnit*> Heroes;
	for (ADungeonUnit* Unit : Units)
	{
		if (AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
		{
			Heroes.Add(Hero);
		}
	}
	return Heroes;
}
