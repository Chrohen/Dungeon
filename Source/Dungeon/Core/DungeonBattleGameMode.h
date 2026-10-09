#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DungeonBattleGameMode.generated.h"

class ADungeonGridActor;
class UBattlefieldConfig;

// Точка входа боевого уровня: строит сетку, ставит камеру и запускает бой
UCLASS(Blueprintable)
class DUNGEON_API ADungeonBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADungeonBattleGameMode();

	virtual void StartPlay() override;

	// DA_Battlefield_Default
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UBattlefieldConfig> BattlefieldConfig;

	// BP_DungeonGridActor
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle")
	TSubclassOf<ADungeonGridActor> GridActorClass;
};
