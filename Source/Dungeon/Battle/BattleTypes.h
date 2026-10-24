#pragma once

#include "CoreMinimal.h"
#include "Abilities/AbilityTypes.h"
#include "Grid/GridTypes.h"
#include "BattleTypes.generated.h"

class AMonsterUnit;
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
	Occupied,
	AlreadyActed,
	InvalidAbility,
	OnCooldown,
	InvalidTarget,
	NoIntent
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

	// //CURRENT-STAGE: пустое имя - показывается название класса
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Name;
};

// Урон одного намерения по клетке для отображения числа
USTRUCT(BlueprintType)
struct FIntentTileDamage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Tile = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Damage = 0;
};

// Стрелка смещения на клетке
USTRUCT(BlueprintType)
struct FIntentPushMark
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Tile = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGridDirection Direction = EGridDirection::None;
};

// Готовое к показу намерение монстра: область, урон по клеткам, стрелки, порядок
// и результат для предпросмотра при наведении
USTRUCT(BlueprintType)
struct FIntentView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AMonsterUnit> Monster;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityCast Cast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntentTileDamage> TileDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntentPushMark> PushMarks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityOutcome Preview;
};
