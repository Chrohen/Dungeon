#pragma once

#include "CoreMinimal.h"
#include "InputTypes.generated.h"

class ADungeonUnit;

UENUM(BlueprintType)
enum class ESelectionState : uint8
{
	Locked,
	Idle,
	HeroSelected
};

// Что под курсором. Tile - клетка плоскости поля, модели юнитов её не закрывают.
// Unit - юнит, на модель которого наведён курсор, иначе юнит на Tile
USTRUCT(BlueprintType)
struct FCursorTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOnGrid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Tile = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<ADungeonUnit> Unit;
};
