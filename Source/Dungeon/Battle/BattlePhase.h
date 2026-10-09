#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Battle/BattleTypes.h"
#include "BattlePhase.generated.h"

class UBattleSubsystem;

// Фаза боя. Объекты фаз создаёт UBattleSubsystem::StartBattle
UCLASS(Abstract)
class DUNGEON_API UBattlePhase : public UObject
{
	GENERATED_BODY()

public:
	// Какую фазу реализует класс
	virtual EBattlePhase GetPhaseType() const { return EBattlePhase::None; }

	// Следующая фаза в цикле
	virtual EBattlePhase GetNextPhase() const { return EBattlePhase::None; }

	virtual void Enter(UBattleSubsystem& Battle) {}

	virtual void Exit(UBattleSubsystem& Battle) {}
};
