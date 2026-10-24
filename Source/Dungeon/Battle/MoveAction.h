#pragma once

#include "CoreMinimal.h"
#include "Battle/BattleAction.h"
#include "MoveAction.generated.h"

class ADungeonUnit;

// Перемещение героя в PlayerTurn или монстра в MonstersMove на клетку. //CURRENT-STAGE: юнит телепортируется
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
	// InvalidPhase (нет боя или фаза не PlayerTurn и не MonstersMove), InvalidUnit (нет юнита или он погиб),
	// NotPlayerUnit (сторона юнита не ходит в этой фазе), AlreadyMoved, OutOfGrid, Occupied, Unreachable.
	// Поиск доступных клеток выполняется заново, кэшу UI действие не доверяет
	virtual EBattleActionResult CanExecute(const UBattleSubsystem& Battle) const override;

	// MoveUnitInternal, затем MarkMoved, OnUnitMoved, OnUnitChanged и RebuildIntentViews
	virtual void Execute(UBattleSubsystem& Battle) override;
};
