#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleRulesConfig.generated.h"

// Общие правила боя
UCLASS(BlueprintType)
class DUNGEON_API UBattleRulesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Урон столкновения при смещении
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CollisionDamage = 1;

	// Пауза в секундах после действия монстра и появления волны. 0 - ход монстров синхронный
	//CURRENT-STAGE
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MonsterActionDelay = 0.f;
};
