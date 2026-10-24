#pragma once

#include "CoreMinimal.h"
#include "Abilities/AbilityTypes.h"
#include "Abilities/BattleSimState.h"
#include "AI/MonsterAITypes.h"

// AI монстров: выбор варианта действия одного монстра по снимку боя. Чистые функции без актёров и мира
class DUNGEON_API FMonsterAI
{
public:
	// Выбор варианта действия монстра MonsterId (TD 6.5). Снимок S не меняется.
	// bAllowMove - рассматриваются ли тайлы, доступные по очкам MovePoints; иначе только текущий.
	// Атакующий вариант (AbilityIndex задан) становится намерением, вариант без способности -
	// движением к героям или ожиданием. Если монстра нет в снимке или он мёртв, возвращается
	// пустой вариант. Random расходуется только при полном равенстве лучших вариантов
	static FAIVariant ChooseVariant(const FBattleSimState& S, int32 MonsterId, int32 MovePoints,
		const TArray<FUnitAbilityState>& Abilities, const FMonsterBehaviorProfile& Profile, bool bAllowMove,
		const FMonsterAIContext& Context, FRandomStream& Random);

	// Добавляет выбор монстра в контекст для следующих монстров: тайлы области действия попадают
	// в DangerTiles, урон по героям вычитается из ExpectedHealth (не ниже 0). Неатакующие
	// варианты контекст не меняют
	static void RegisterChoice(const FBattleSimState& S, const FAIVariant& Variant, FMonsterAIContext& Context);

	// Пустой контекст: ExpectedHealth - текущее здоровье живых героев
	static FMonsterAIContext MakeContext(const FBattleSimState& S);
};
