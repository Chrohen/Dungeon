#pragma once

#include "CoreMinimal.h"
#include "Abilities/AbilityTypes.h"
#include "MonsterAITypes.generated.h"

// Профиль поведения монстра: веса оценки вариантов действия
USTRUCT(BlueprintType)
struct FMonsterBehaviorProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DamageWeight = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float KillWeight = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredDistance = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceWeight = 1.f;
};

// Намерение монстра: способность, направление, положение выбранной клетки
// относительно монстра и номер порядка реализации
USTRUCT(BlueprintType)
struct FMonsterIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AbilityIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGridDirection Direction = EGridDirection::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint SelectedOffset = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;
};

// Вариант действия. AbilityIndex = INDEX_NONE - вариант без способности
// (движение к героям или ожидание)
struct FAIVariant
{
	FIntPoint MoveTile = FIntPoint::ZeroValue;
	int32 PathCost = 0;
	int32 AbilityIndex = INDEX_NONE;
	FIntPoint SelectedTile = FIntPoint::ZeroValue;
	FAbilityOutcome Outcome;
	float Score = 0.f;
	bool bAttacking = false;
	bool bSafe = false;
};

// Что выбрали предыдущие монстры в этой фазе: тайлы их областей действия
// и ожидаемое здоровье героев по UnitId
struct FMonsterAIContext
{
	TSet<FIntPoint> DangerTiles;
	TMap<int32, int32> ExpectedHealth;
};
