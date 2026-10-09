#pragma once

#include "CoreMinimal.h"
#include "Units/DungeonUnit.h"
#include "MonsterUnit.generated.h"

// Монстр. В спринте 1 только объявлен, на поле не появляется
UCLASS(Blueprintable)
class DUNGEON_API AMonsterUnit : public ADungeonUnit
{
	GENERATED_BODY()

public:
	AMonsterUnit();
};
