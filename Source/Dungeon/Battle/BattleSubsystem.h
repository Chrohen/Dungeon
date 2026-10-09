#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/BattleTypes.h"
#include "BattleSubsystem.generated.h"

class ADungeonGridActor;
class ADungeonUnit;
class AHeroUnit;
class UBattleAction;
class UBattlePhase;
class UBattlefieldConfig;
class UUnitDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonBattle, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBattlePhaseChanged, EBattlePhase, OldPhase, EBattlePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleTurnStarted, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleUnitSpawned, ADungeonUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBattleUnitMoved, ADungeonUnit*, Unit, FIntPoint, From, FIntPoint, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleEnded, EBattleResult, Result);

// Центр логики боя. Создаётся только в игровых мирах (Game и PIE)
UCLASS()
class DUNGEON_API UBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// Сбрасывает состояние, создаёт фазы и запускает Preparation
	void StartBattle(UBattlefieldConfig* InConfig, ADungeonGridActor* InGrid);

	// Вызывает CanExecute, затем Execute, если проверка вернула Success. Возвращает результат проверки.
	// Ссылку на действие не хранит. Для пустого действия возвращает InvalidUnit
	EBattleActionResult TryExecute(UBattleAction* Action);

	// Работает только в PlayerTurn: вызывает AdvancePhase(). В других фазах ничего не делает
	void EndPlayerTurn();

	// Запрос перехода к GetNextPhase() текущей фазы. Если цикл переходов уже идёт (вызов из Enter),
	// только ставит bTransitionPending. Если у текущей фазы нет объекта (None, Finished), ничего не делает
	void AdvancePhase();

	// Переход в Finished и OnBattleEnded. В спринте 1 не вызывается
	void FinishBattle(EBattleResult InResult);

	// Спавнит актёр, ставит его в модель сетки и добавляет в Units.
	// Если клетка занята, непроходима или вне поля, уничтожает актёр и возвращает nullptr
	ADungeonUnit* SpawnUnit(UUnitDefinition* Def, FIntPoint Coord);

	// FGridModel::MoveUnit и ADungeonUnit::SetGridPosition. OnUnitMoved не вызывает:
	// его вызывает действие, из которого вызван метод
	bool MoveUnitInternal(ADungeonUnit* Unit, FIntPoint To);

	EBattlePhase GetCurrentPhase() const { return CurrentPhase; }
	bool IsPlayerTurn() const { return CurrentPhase == EBattlePhase::PlayerTurn; }
	int32 GetTurnNumber() const { return TurnNumber; }
	int32 GetCurrentWave() const { return CurrentWave; }
	int32 GetTotalWaves() const { return TotalWaves; }
	EBattleResult GetResult() const { return Result; }

	ADungeonGridActor* GetGrid() const { return Grid; }
	const TArray<TObjectPtr<ADungeonUnit>>& GetUnits() const { return Units; }
	TArray<AHeroUnit*> GetHeroes() const;

	// Нужен фазам
	const UBattlefieldConfig* GetConfig() const { return Config; }

	UPROPERTY(BlueprintAssignable)
	FOnBattlePhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable)
	FOnBattleTurnStarted OnTurnStarted;

	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitSpawned OnUnitSpawned;

	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitMoved OnUnitMoved;

	UPROPERTY(BlueprintAssignable)
	FOnBattleEnded OnBattleEnded;

private:
	// TurnNumber увеличивает только фаза монстров
	friend class UMonstersMovePhase;

	// Цикл переходов без рекурсии: SetPhase(Next), затем, пока стоит bTransitionPending,
	// сбрасывает флаг и делает SetPhase для следующей фазы
	void TransitionTo(EBattlePhase Next);

	// Один переход: Exit текущей фазы, смена CurrentPhase, Enter новой, OnPhaseChanged
	void SetPhase(EBattlePhase Next);

	UPROPERTY()
	TObjectPtr<ADungeonGridActor> Grid;

	UPROPERTY()
	TObjectPtr<UBattlefieldConfig> Config;

	UPROPERTY()
	TArray<TObjectPtr<ADungeonUnit>> Units;

	UPROPERTY()
	TMap<EBattlePhase, TObjectPtr<UBattlePhase>> Phases;

	EBattlePhase CurrentPhase = EBattlePhase::None;

	// Номер текущего хода. StartBattle ставит 0, UMonstersMovePhase увеличивает его
	// перед каждым ходом игрока, поэтому первый ход игрока - 1
	int32 TurnNumber = 0;

	int32 CurrentWave = 0;
	int32 TotalWaves = 0;
	EBattleResult Result = EBattleResult::None;

	// Идёт цикл переходов между фазами
	bool bIsTransitioning = false;

	// Фаза запросила переход во время цикла
	bool bTransitionPending = false;
};
