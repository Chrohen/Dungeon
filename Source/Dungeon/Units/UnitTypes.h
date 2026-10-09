#pragma once

#include "CoreMinimal.h"
#include "UnitTypes.generated.h"

UENUM(BlueprintType)
enum class EUnitTeam : uint8
{
	Hero,
	Monster
};

// Что юнит уже сделал за текущий ход
USTRUCT(BlueprintType)
struct FUnitTurnState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMoved = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasActed = false;
};
