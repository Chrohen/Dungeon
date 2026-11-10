#include "Battle/BattleSubsystem.h"
#include "Save/DungeonSaveGame.h"
#include "Data/AbilityDefinition.h"
#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Data/UnitDefinition.h"
#include "Engine/World.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridModel.h"
#include "Kismet/GameplayStatics.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	bool RejectSave(FString& Error, const TCHAR* Message)
	{
		Error = Message;
		return false;
	}

	template <typename EnumType>
	bool IsSaveEnumValid(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		const int64 RawValue = static_cast<int64>(Value);
		return Enum->IsValidEnumValue(RawValue) && !Enum->GetNameStringByValue(RawValue).EndsWith(TEXT("_MAX"));
	}

	void ApplySavedUnitState(ADungeonUnit& Unit, const FDungeonUnitSaveState& State)
	{
		Unit.Definition = State.Definition.Get();
		Unit.UnitId = State.UnitId;
		Unit.GridPosition = State.GridPosition;
		Unit.Team = State.Team;
		Unit.Health = State.Health;
		Unit.bAlive = State.bAlive;
		Unit.Level = State.Level;
		Unit.TurnState = State.TurnState;
		Unit.Abilities = State.Abilities;
		if (AHeroUnit* Hero = Cast<AHeroUnit>(&Unit))
		{
			Hero->HeroName = State.HeroName;
			Hero->Experience = State.Experience;
			Hero->UnspentPoints = State.UnspentPoints;
		}
		if (AMonsterUnit* Monster = Cast<AMonsterUnit>(&Unit))
		{
			Monster->Intent = State.Intent;
		}
	}

	bool ValidateSavedAbility(const FUnitAbilityState& State, int32 Turn, FString& Error)
	{
		const FResolvedAbility& Ability = State.Ability;
		if (!IsValid(Ability.Definition) || State.Level < 1 || State.Level > Ability.Definition->GetMaxLevel()
			|| Ability.Cooldown < 0 || State.RemainingCooldown < 0 || State.RemainingCooldown > Ability.Cooldown
			|| State.UsedOnTurn < INDEX_NONE || State.UsedOnTurn > Turn)
		{
			return RejectSave(Error, TEXT("The save contains an invalid ability or cooldown."));
		}
		if (!IsSaveEnumValid(Ability.Targeting.Shape) || !IsSaveEnumValid(Ability.Targeting.Trajectory)
			|| !IsSaveEnumValid(Ability.Targeting.Requirement) || !IsSaveEnumValid(Ability.Area.Type)
			|| Ability.Targeting.MinRange < 0 || Ability.Targeting.MaxRange < Ability.Targeting.MinRange
			|| Ability.Area.Size < 0 || Ability.Targeting.MaxRange == MAX_int32 || Ability.Area.Size == MAX_int32)
		{
			return RejectSave(Error, TEXT("The save contains invalid ability targeting."));
		}
		const bool bHasDirection = Ability.Targeting.Shape == ETargetingShape::Cross && Ability.Targeting.MinRange >= 1;
		if ((Ability.Targeting.Trajectory == ETrajectory::Projectile && Ability.Targeting.Shape != ETargetingShape::Cross)
			|| ((Ability.Area.Type == EAreaType::Line || Ability.Area.Type == EAreaType::CrossLine
				|| Ability.Area.Type == EAreaType::Path) && !bHasDirection)
			|| (Ability.Area.Type == EAreaType::Line && Ability.Area.Size < 1))
		{
			return RejectSave(Error, TEXT("The save contains an invalid ability direction."));
		}
		for (const FAbilityEffect& Effect : Ability.Effects)
		{
			if (!IsSaveEnumValid(Effect.Type) || !IsSaveEnumValid(Effect.AreaPart) || !IsSaveEnumValid(Effect.Filter)
				|| !IsSaveEnumValid(Effect.PushDirection) || !IsSaveEnumValid(Effect.MoveMode)
				|| Effect.Amount < 0 || Effect.PushDistance < 0 || Effect.Duration < 0
				|| (Effect.Status != nullptr && !IsValid(Effect.Status)))
			{
				return RejectSave(Error, TEXT("The save contains an invalid ability effect."));
			}
			if ((Effect.Type == EAbilityEffectType::Push && (Effect.PushDistance < 1
				|| (Effect.PushDirection == EPushDirection::AbilityDirection && !bHasDirection)))
				|| (Effect.Type == EAbilityEffectType::MoveSource && Effect.MoveMode == ESourceMoveMode::Dash && !bHasDirection))
			{
				return RejectSave(Error, TEXT("The save contains an invalid movement effect."));
			}
		}
		return true;
	}
}

bool UBattleSubsystem::CaptureSave(UDungeonSaveGame& Out, FString& Error) const
{
	Error.Reset();
	if (bIsTransitioning || bApplyingAction || bRestoringSave || (CurrentPhase != EBattlePhase::PlayerTurn && CurrentPhase != EBattlePhase::Finished)
		|| !IsValid(Config) || !IsValid(Grid) || GetWorld() == nullptr)
	{
		return RejectSave(Error, TEXT("The battle is not at a save point."));
	}
	Out.Version = UDungeonSaveGame::CurrentVersion;
	Out.MapPackage = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
	Out.Battlefield = Config.Get();
	Out.Phase = CurrentPhase;
	Out.Result = Result;
	Out.TurnNumber = TurnNumber;
	Out.CurrentWave = CurrentWave;
	Out.TotalWaves = TotalWaves;
	Out.NextUnitId = NextUnitId;
	Out.Random = Random;
	Out.Units.Reset(Units.Num());
	for (const ADungeonUnit* Unit : Units)
	{
		if (!IsValid(Unit))
		{
			return RejectSave(Error, TEXT("A battle unit is no longer valid."));
		}
		FDungeonUnitSaveState& State = Out.Units.AddDefaulted_GetRef();
		State.Definition = Unit->Definition.Get();
		State.Team = Unit->Team;
		State.UnitId = Unit->UnitId;
		State.GridPosition = Unit->GridPosition;
		State.Health = Unit->Health;
		State.bAlive = Unit->bAlive;
		State.Level = Unit->Level;
		State.TurnState = Unit->TurnState;
		State.Abilities = Unit->Abilities;
		if (const AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
		{
			State.HeroName = Hero->HeroName;
			State.Experience = Hero->Experience;
			State.UnspentPoints = Hero->UnspentPoints;
		}
		if (const AMonsterUnit* Monster = Cast<AMonsterUnit>(Unit))
		{
			State.Intent = Monster->Intent;
		}
	}
	return ValidateSave(Out, Error);
}

bool UBattleSubsystem::ValidateSave(const UDungeonSaveGame& Save, FString& Error)
{
	Error.Reset();
	if (Save.Version != UDungeonSaveGame::CurrentVersion || Save.Sequence < 0)
	{
		return RejectSave(Error, TEXT("The save version or sequence is not supported."));
	}
	if (Save.MapPackage.IsEmpty() || Save.MapPackage.Contains(TEXT("/")) || Save.MapPackage.Contains(TEXT("\\")))
	{
		return RejectSave(Error, TEXT("The save has no valid map name."));
	}
	if ((Save.Phase != EBattlePhase::PlayerTurn && Save.Phase != EBattlePhase::Finished)
		|| !IsSaveEnumValid(Save.Result) || (Save.Phase == EBattlePhase::PlayerTurn) != (Save.Result == EBattleResult::None)
		|| Save.TurnNumber < 0 || Save.TurnNumber == MAX_int32 || (Save.Phase == EBattlePhase::PlayerTurn && Save.TurnNumber == 0)
		|| Save.CurrentWave < 0 || Save.TotalWaves < 0 || Save.CurrentWave > Save.TotalWaves
		|| Save.NextUnitId < 0 || Save.NextUnitId == MAX_int32)
	{
		return RejectSave(Error, TEXT("The save contains invalid battle counters or phase."));
	}
	UBattlefieldConfig* SavedConfig = Save.Battlefield.LoadSynchronous();
	if (!IsValid(SavedConfig) || SavedConfig->Width <= 0 || SavedConfig->Height <= 0
		|| static_cast<int64>(SavedConfig->Width) * SavedConfig->Height > 1048576
		|| !FMath::IsFinite(SavedConfig->TileSize) || SavedConfig->TileSize <= 0.f
		|| !IsValid(SavedConfig->DefaultTileType) || Save.TotalWaves != SavedConfig->WaveCount)
	{
		return RejectSave(Error, TEXT("The saved battlefield is missing or invalid."));
	}
	FGridModel Model;
	Model.Initialize(SavedConfig->Width, SavedConfig->Height, SavedConfig->DefaultTileType);
	for (const FTileOverride& Override : SavedConfig->TileOverrides)
	{
		if (!Model.IsInside(Override.Coord))
		{
			return RejectSave(Error, TEXT("The battlefield contains an invalid tile override."));
		}
		if (Override.Type != nullptr)
		{
			Model.SetTileType(Override.Coord, Override.Type);
		}
	}
	TSet<int32> Ids;
	TSet<int32> IntentOrders;
	int32 LivingHeroes = 0;
	int32 LivingMonsters = 0;
	for (const FDungeonUnitSaveState& State : Save.Units)
	{
		UUnitDefinition* Definition = State.Definition.LoadSynchronous();
		if (!IsValid(Definition) || Definition->UnitClass == nullptr
			|| Definition->UnitClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| Definition->MaxHealth < 1 || Definition->MovePoints < 0
			|| !IsSaveEnumValid(State.Team) || State.UnitId < 0 || State.UnitId >= Save.NextUnitId || Ids.Contains(State.UnitId)
			|| State.Level < 1 || State.Health < 0 || State.Health > Definition->MaxHealth
			|| State.bAlive != (State.Health > 0) || State.Experience < 0 || State.UnspentPoints < 0
			|| !Model.IsInside(State.GridPosition))
		{
			return RejectSave(Error, TEXT("The save contains an invalid unit."));
		}
		if ((State.Team == EUnitTeam::Hero && !Definition->UnitClass->IsChildOf(AHeroUnit::StaticClass()))
			|| (State.Team == EUnitTeam::Monster && !Definition->UnitClass->IsChildOf(AMonsterUnit::StaticClass())))
		{
			return RejectSave(Error, TEXT("A saved unit class does not match its team."));
		}
		Ids.Add(State.UnitId);
		if (State.bAlive)
		{
			FGridOccupant Occupant;
			Occupant.UnitId = State.UnitId;
			Occupant.Team = State.Team;
			if (!Model.PlaceUnit(Occupant, State.GridPosition))
			{
				return RejectSave(Error, TEXT("Saved units overlap or occupy a blocked tile."));
			}
			State.Team == EUnitTeam::Hero ? ++LivingHeroes : ++LivingMonsters;
		}
		for (const FUnitAbilityState& Ability : State.Abilities)
		{
			if (!ValidateSavedAbility(Ability, Save.TurnNumber, Error))
			{
				return false;
			}
		}
		if (!IsSaveEnumValid(State.Intent.Direction) || State.Intent.Order < 0
			|| State.Intent.AbilityIndex < INDEX_NONE
			|| (State.Intent.AbilityIndex != INDEX_NONE && !State.Abilities.IsValidIndex(State.Intent.AbilityIndex)))
		{
			return RejectSave(Error, TEXT("The save contains an invalid monster intent."));
		}
		if (State.Intent.bActive)
		{
			if (State.Team != EUnitTeam::Monster || !State.bAlive || Save.Phase == EBattlePhase::Finished
				|| !State.Abilities.IsValidIndex(State.Intent.AbilityIndex) || IntentOrders.Contains(State.Intent.Order)
				|| FMath::Abs(static_cast<int64>(State.Intent.SelectedOffset.X)) >= SavedConfig->Width
				|| FMath::Abs(static_cast<int64>(State.Intent.SelectedOffset.Y)) >= SavedConfig->Height)
			{
				return RejectSave(Error, TEXT("The save contains an invalid active intent."));
			}
			IntentOrders.Add(State.Intent.Order);
		}
	}
	if ((Save.Phase == EBattlePhase::PlayerTurn && (LivingHeroes == 0 || (LivingMonsters == 0 && Save.CurrentWave >= Save.TotalWaves)))
		|| (Save.Result == EBattleResult::Defeat && LivingHeroes != 0)
		|| (Save.Result == EBattleResult::Victory && (LivingHeroes == 0 || LivingMonsters != 0 || Save.CurrentWave < Save.TotalWaves)))
	{
		return RejectSave(Error, TEXT("The saved result does not match the surviving units."));
	}
	return true;
}

bool UBattleSubsystem::RestoreSave(const UDungeonSaveGame& Save, ADungeonGridActor* InGrid, FString& Error)
{
	Error.Reset();
	if (bIsTransitioning || bApplyingAction || bRestoringSave || !IsValid(InGrid) || InGrid->GetWorld() != GetWorld()
		|| GetWorld() == nullptr || Save.MapPackage != UGameplayStatics::GetCurrentLevelName(GetWorld(), true))
	{
		return RejectSave(Error, TEXT("The save cannot be restored in this world or battle phase."));
	}
	if (!ValidateSave(Save, Error))
	{
		return false;
	}
	TGuardValue<bool> RestoreGuard(bRestoringSave, true);
	TStrongObjectPtr<UBattlefieldConfig> SavedConfig(Save.Battlefield.Get());
	TArray<ADungeonUnit*> StagedUnits;
	StagedUnits.Reserve(Save.Units.Num());
	for (const FDungeonUnitSaveState& State : Save.Units)
	{
		UUnitDefinition* Definition = State.Definition.LoadSynchronous();
		ADungeonUnit* Unit = GetWorld()->SpawnActorDeferred<ADungeonUnit>(Definition->UnitClass, FTransform::Identity,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Unit != nullptr)
		{
			StagedUnits.Add(Unit);
			Unit->SetActorHiddenInGame(true);
			Unit->SetActorEnableCollision(false);
			ApplySavedUnitState(*Unit, State);
			Unit->FinishSpawning(FTransform::Identity);
		}
		if (!IsValid(Unit))
		{
			for (ADungeonUnit* Staged : StagedUnits)
			{
				if (IsValid(Staged)) Staged->Destroy();
			}
			return RejectSave(Error, TEXT("A saved unit could not be created. The current battle is unchanged."));
		}
		// Construction scripts must not replace the saved values.
		ApplySavedUnitState(*Unit, State);
		Unit->SetActorHiddenInGame(true);
		Unit->SetActorEnableCollision(false);
	}

	// All records and actors are ready. Replace the current battle now.
	ResetBattleState(SavedConfig.Get(), InGrid);
	InGrid->BuildFromConfig(SavedConfig.Get());
	TurnNumber = Save.TurnNumber;
	CurrentWave = Save.CurrentWave;
	TotalWaves = Save.TotalWaves;
	NextUnitId = Save.NextUnitId;
	Random = Save.Random;
	Result = Save.Result;
	CurrentPhase = Save.Phase;
	for (ADungeonUnit* Unit : StagedUnits)
	{
		Units.Add(Unit);
		Unit->SetGridPosition(Unit->GridPosition, InGrid->GridToWorld(Unit->GridPosition));
		if (Unit->bAlive)
		{
			FGridOccupant Occupant;
			Occupant.Unit = Unit;
			Occupant.UnitId = Unit->UnitId;
			Occupant.Team = Unit->Team;
			InGrid->GetMutableModel().PlaceUnit(Occupant, Unit->GridPosition);
			Unit->SetActorHiddenInGame(false);
			Unit->SetActorEnableCollision(true);
			Unit->RefreshVisuals();
		}
		else
		{
			Unit->MarkDead();
		}
	}

	// Send notifications only. Phase entry would reset actions and cooldowns.
	OnBattleStarted.Broadcast();
	for (ADungeonUnit* Unit : Units)
	{
		OnUnitSpawned.Broadcast(Unit);
	}
	RebuildIntentViews();
	OnWaveChanged.Broadcast(CurrentWave, TotalWaves);
	OnPhaseChanged.Broadcast(EBattlePhase::None, CurrentPhase);
	if (CurrentPhase == EBattlePhase::PlayerTurn)
	{
		OnTurnStarted.Broadcast(TurnNumber);
	}
	else
	{
		OnBattleEnded.Broadcast(Result);
	}
	return true;
}
