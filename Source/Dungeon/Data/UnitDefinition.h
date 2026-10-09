#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnitDefinition.generated.h"

class ADungeonUnit;

// Описание типа юнита
UCLASS(BlueprintType)
class DUNGEON_API UUnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// Какой Blueprint спавнить (BP_HeroUnit_*)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ADungeonUnit> UnitClass;

	// Очки перемещения
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MovePoints = 5;
};
