#pragma once

#include "CoreMinimal.h"
#include "Units/DungeonUnit.h"
#include "HeroUnit.generated.h"

// Герой игрока. В спринте 1 других отличий от базового класса нет
UCLASS(Blueprintable)
class DUNGEON_API AHeroUnit : public ADungeonUnit
{
	GENERATED_BODY()

public:
	AHeroUnit();
};
