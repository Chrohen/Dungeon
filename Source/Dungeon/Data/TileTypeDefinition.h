#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TileTypeDefinition.generated.h"

class UStaticMesh;

// Описание типа тайла
UCLASS(BlueprintType)
class DUNGEON_API UTileTypeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// Можно ли пройти по тайлу
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPassable = true;

	// Сколько очков перемещения стоит зайти на тайл. Для непроходимого тайла не используется
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MoveCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> Mesh = nullptr;
};
