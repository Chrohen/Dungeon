#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Math/RandomStream.h"
#include "Abilities/AbilityTypes.h"
#include "AI/MonsterAITypes.h"
#include "Battle/BattleTypes.h"
#include "DungeonSaveGame.generated.h"

class UBattlefieldConfig;
class UUnitDefinition;

USTRUCT()
struct FDungeonUnitSaveState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UUnitDefinition> Definition;

	UPROPERTY(SaveGame)
	EUnitTeam Team = EUnitTeam::Hero;

	UPROPERTY(SaveGame)
	int32 UnitId = INDEX_NONE;

	UPROPERTY(SaveGame)
	FIntPoint GridPosition = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame)
	int32 Health = 0;

	UPROPERTY(SaveGame)
	bool bAlive = true;

	UPROPERTY(SaveGame)
	int32 Level = 1;

	UPROPERTY(SaveGame)
	FUnitTurnState TurnState;

	// Keep the resolved values. Do not apply the definition a second time.
	UPROPERTY(SaveGame)
	TArray<FUnitAbilityState> Abilities;

	UPROPERTY(SaveGame)
	FText HeroName;

	UPROPERTY(SaveGame)
	int32 Experience = 0;

	UPROPERTY(SaveGame)
	int32 UnspentPoints = 0;

	UPROPERTY(SaveGame)
	FMonsterIntent Intent;
};

UCLASS()
class DUNGEON_API UDungeonSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame)
	int32 Version = CurrentVersion;

	UPROPERTY(SaveGame)
	int64 Sequence = 0;

	// Short map name, without the PIE prefix.
	UPROPERTY(SaveGame)
	FString MapPackage;

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UBattlefieldConfig> Battlefield;

	UPROPERTY(SaveGame)
	EBattlePhase Phase = EBattlePhase::None;

	UPROPERTY(SaveGame)
	EBattleResult Result = EBattleResult::None;

	UPROPERTY(SaveGame)
	int32 TurnNumber = 0;

	UPROPERTY(SaveGame)
	int32 CurrentWave = 0;

	UPROPERTY(SaveGame)
	int32 TotalWaves = 0;

	UPROPERTY(SaveGame)
	int32 NextUnitId = 0;

	UPROPERTY(SaveGame)
	FRandomStream Random;

	UPROPERTY(SaveGame)
	TArray<FDungeonUnitSaveState> Units;
};
