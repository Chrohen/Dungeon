#pragma once

#include "CoreMinimal.h"
#include "Abilities/AbilityTypes.h"
#include "Abilities/BattleSimState.h"
#include "AI/MonsterAITypes.h"

// Правила способностей: наведение и область действия. Чистые функции без актёров и мира.
// Стороны определяются только сравнением команд с источником
class DUNGEON_API FAbilityRules
{
public:
	// Манхэттенское расстояние
	static int32 Distance(FIntPoint A, FIntPoint B);

	// Направление, если клетки в одной строке или столбце и не совпадают, иначе None
	static EGridDirection GetDirection(FIntPoint From, FIntPoint To);

	// Шаг на одну клетку, для None - (0,0)
	static FIntPoint DirectionToOffset(EGridDirection Dir);

	// Поворот смещения шаблона, заданного для Right, по часовой стрелке (ось Y направлена вниз).
	// Для None и Right смещение не меняется
	static FIntPoint RotateOffset(FIntPoint OffsetForRight, EGridDirection Dir);

	// Форма и дальность: расстояние от MinRange до MaxRange; для Cross ещё и та же строка или столбец.
	// Поле не проверяется
	static bool IsInTargetingShape(FIntPoint Source, FIntPoint Tile, const FAbilityTargeting& T);

	// Фактическая точка цели снаряда: первый тайл с юнитом или непроходимый тайл по направлению,
	// начиная с соседнего с источником, в пределах MaxRange. Если такого нет - последний тайл
	// в пределах MaxRange и поля (при отсутствии таких - клетка источника)
	static FIntPoint TraceProjectile(const FGridModel& Grid, FIntPoint Source, EGridDirection Dir, int32 MaxRange);

	// Требование к точке цели. Источник - союзник самому себе
	static bool CheckRequirement(const FBattleSimState& S, int32 SourceId, FIntPoint Tile, ETargetRequirement R);

	// Полная проверка наведения и построение применения. Out меняется только при успехе
	static bool MakeCast(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, FIntPoint SelectedTile, FAbilityCast& Out);

	// Применение по намерению без проверки наведения и требования. false, если источника нет
	// в снимке или он мёртв. Out меняется только при успехе
	static bool MakeIntentCast(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, const FMonsterIntent& Intent, FAbilityCast& Out);

	// Все клетки поля, для которых MakeCast успешен
	static void GetSelectableTiles(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, TArray<FIntPoint>& Out);

	// Область действия от точки цели. Клетки вне поля и тайл источника (без bAffectsSource) убираются
	static void BuildArea(const FGridModel& Grid, FIntPoint SourcePos, FIntPoint Target, EGridDirection Dir, const FAbilityAreaSpec& Spec, FAffectedArea& Out);
};
