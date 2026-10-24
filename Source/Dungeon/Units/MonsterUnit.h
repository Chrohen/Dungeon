#pragma once

#include "CoreMinimal.h"
#include "AI/MonsterAITypes.h"
#include "Units/DungeonUnit.h"
#include "MonsterUnit.generated.h"

class UMonsterDefinition;

// Монстр под управлением AI
UCLASS(Blueprintable)
class DUNGEON_API AMonsterUnit : public ADungeonUnit
{
	GENERATED_BODY()

public:
	AMonsterUnit();

	// Текущее намерение. Меняет только UBattleSubsystem
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster")
	FMonsterIntent Intent;

	// Cast определения, может быть nullptr. Вызывает любой
	const UMonsterDefinition* GetMonsterDefinition() const;

	// Опыт за убийство или 0. Вызывает UBattleSubsystem
	int32 GetExperienceReward() const;

	// Профиль из определения или по умолчанию. Вызывает UBattleSubsystem
	FMonsterBehaviorProfile GetBehaviorProfile() const;

	// Вызывает UBattleSubsystem
	void SetIntent(const FMonsterIntent& InIntent);

	// Вызывает UBattleSubsystem
	void ClearIntent();

	// Вызывает любой
	bool HasIntent() const;
};
