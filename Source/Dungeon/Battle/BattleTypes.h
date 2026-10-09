#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

class UUnitDefinition;

UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
	None,
	Preparation,
	MonstersMove,
	MonstersIntent,
	PlayerTurn,
	ResolveIntents,
	WaveSpawn,
	Finished
};

UENUM(BlueprintType)
enum class EBattleResult : uint8
{
	None,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class EBattleActionResult : uint8
{
	Success,
	InvalidPhase,
	InvalidUnit,
	NotPlayerUnit,
	AlreadyMoved,
	OutOfGrid,
	Unreachable,
	Occupied
};

// Стартовая позиция героя (//CURRENT-STAGE)
USTRUCT(BlueprintType)
struct FHeroPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UUnitDefinition> Hero = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Position = FIntPoint::ZeroValue;
};
