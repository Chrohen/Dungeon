#pragma once

#include "CoreMinimal.h"
#include "AI/MonsterAITypes.h"
#include "Data/UnitDefinition.h"
#include "MonsterDefinition.generated.h"

// Описание типа монстра
UCLASS(BlueprintType)
class DUNGEON_API UMonsterDefinition : public UUnitDefinition
{
	GENERATED_BODY()

public:
	// Опыт за убийство
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ExperienceReward = 0;

	// Профиль поведения AI
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FMonsterBehaviorProfile BehaviorProfile;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
