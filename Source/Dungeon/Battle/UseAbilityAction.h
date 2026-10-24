#pragma once

#include "CoreMinimal.h"
#include "Battle/BattleAction.h"
#include "UseAbilityAction.generated.h"

class ADungeonUnit;

// Способность героя в фазу хода игрока
UCLASS()
class DUNGEON_API UUseAbilityAction : public UBattleAction
{
	GENERATED_BODY()

public:
	// Источник
	UPROPERTY()
	TObjectPtr<ADungeonUnit> Unit;

	// Индекс в Unit->GetAbilities()
	UPROPERTY()
	int32 AbilityIndex = INDEX_NONE;

	// Клетка, выбранная игроком
	UPROPERTY()
	FIntPoint SelectedTile = FIntPoint::ZeroValue;

	// Условия проверяются по порядку, возвращается первая ошибка:
	// InvalidPhase (нет боя или не PlayerTurn), InvalidUnit (нет юнита или он погиб), NotPlayerUnit (не герой),
	// AlreadyActed, InvalidAbility (индекс вне списка), OnCooldown, InvalidTarget (MakeCast не удался).
	// MakeCast считается заново на текущем снимке боя, кэшу UI действие не доверяет
	virtual EBattleActionResult CanExecute(const UBattleSubsystem& Battle) const override;

	// MakeCast заново, затем ExecuteAbilityInternal
	virtual void Execute(UBattleSubsystem& Battle) override;
};
