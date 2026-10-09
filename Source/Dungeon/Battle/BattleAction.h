#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Battle/BattleTypes.h"
#include "BattleAction.generated.h"

class UBattleSubsystem;

// Действие, меняющее состояние боя. Объект действия создаёт инициатор (NewObject, transient)
// и передаёт в UBattleSubsystem::TryExecute. После выполнения ссылка на объект не хранится
UCLASS(Abstract)
class DUNGEON_API UBattleAction : public UObject
{
	GENERATED_BODY()

public:
	// Проверка без изменения состояния
	virtual EBattleActionResult CanExecute(const UBattleSubsystem& Battle) const { return EBattleActionResult::Success; }

	// Применение. Вызывается только после успешного CanExecute
	virtual void Execute(UBattleSubsystem& Battle) {}
};
