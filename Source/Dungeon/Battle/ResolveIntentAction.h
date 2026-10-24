#pragma once

#include "CoreMinimal.h"
#include "Battle/BattleAction.h"
#include "ResolveIntentAction.generated.h"

class AMonsterUnit;

// Реализация намерения монстра в фазу реализации намерений
UCLASS()
class DUNGEON_API UResolveIntentAction : public UBattleAction
{
	GENERATED_BODY()

public:
	// Чьё намерение
	UPROPERTY()
	TObjectPtr<AMonsterUnit> Monster;

	// Условия проверяются по порядку, возвращается первая ошибка:
	// InvalidPhase (нет боя или не ResolveIntents), InvalidUnit (нет монстра или он погиб), NoIntent.
	// Наведение, требование к точке цели и перезарядка не проверяются: они проверены при выборе намерения
	virtual EBattleActionResult CanExecute(const UBattleSubsystem& Battle) const override;

	// GetIntentCast, ExecuteAbilityInternal (здесь запускается перезарядка), ClearIntent
	virtual void Execute(UBattleSubsystem& Battle) override;
};
