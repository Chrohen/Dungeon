#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Battle/BattleTypes.h"
#include "Grid/GridTypes.h"
#include "BattlefieldConfig.generated.h"

class UTileTypeDefinition;

// Параметры поля боя
UCLASS(BlueprintType)
class DUNGEON_API UBattlefieldConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TileSize = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTileTypeDefinition> DefaultTileType = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGridArea HeroSpawnArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGridArea MonsterSpawnArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 WaveCount = 0;

	//CURRENT-STAGE
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FHeroPlacement> HeroPlacements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTileOverride> TileOverrides;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
