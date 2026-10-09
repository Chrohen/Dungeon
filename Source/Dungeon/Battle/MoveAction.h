#pragma once

#include "CoreMinimal.h"
#include "Battle/BattleAction.h"
#include "MoveAction.generated.h"

class ADungeonUnit;

// Перемещение героя на клетку. //CURRENT-STAGE: герой телепортируется
UCLASS()
class DUNGEON_API UMoveAction : public UBattleAction
{
	GENERATED_BODY()

public:
	// Кто идёт
	UPROPERTY()
	TObjectPtr<ADungeonUnit> Unit;

	// Куда идёт
	UPROPERTY()
	FIntPoint Target = FIntPoint::ZeroValue;

	// Условия проверяются по порядку, возвращается первая ошибка:
	// InvalidPhase, InvalidUnit, NotPlayerUnit, AlreadyMoved, OutOfGrid, Occupied, Unreachable.
	// Поиск доступных клеток выполняется заново, кэшу UI действие не доверяет
	virtual EBattleActionResult CanExecute(const UBattleSubsystem& Battle) const override;

	// MoveUnitInternal, затем MarkMoved, затем OnUnitMoved
	virtual void Execute(UBattleSubsystem& Battle) override;
};
