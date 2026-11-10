#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Abilities/BattleSimState.h"
#include "AI/MonsterAITypes.h"
#include "Battle/BattleTypes.h"
#include "BattleSubsystem.generated.h"

class ADungeonGridActor;
class ADungeonUnit;
class AHeroUnit;
class AMonsterUnit;
class UBattleAction;
class UBattlePhase;
class UBattleRulesConfig;
class UBattlefieldConfig;
class UUnitDefinition;
class UDungeonSaveGame;

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonBattle, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBattlePhaseChanged, EBattlePhase, OldPhase, EBattlePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleTurnStarted, int32, TurnNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBattleWaveChanged, int32, CurrentWave, int32, TotalWaves);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleUnitSpawned, ADungeonUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBattleUnitMoved, ADungeonUnit*, Unit, FIntPoint, From, FIntPoint, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleUnitChanged, ADungeonUnit*, Unit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBattleUnitDied, ADungeonUnit*, Unit, ADungeonUnit*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBattleAbilityUsed, ADungeonUnit*, Source, int32, AbilityIndex, FIntPoint, TargetPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleIntentsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleEnded, EBattleResult, Result);

// Центр логики боя. Создаётся только в игровых мирах (Game и PIE)
UCLASS()
class DUNGEON_API UBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// Сбрасывает состояние, создаёт фазы, вызывает OnBattleStarted и запускает Preparation.
	// При повторном вызове уничтожает юнитов прошлого боя и освобождает их клетки
	void StartBattle(UBattlefieldConfig* InConfig, ADungeonGridActor* InGrid);

	// Enable disk writes for this world. Test worlds do not save by default.
	void ConfigureAutoSave(const FString& SlotName, bool bEnabled);

	bool CaptureSave(UDungeonSaveGame& Out, FString& Error) const;
	static bool ValidateSave(const UDungeonSaveGame& Save, FString& Error);
	bool RestoreSave(const UDungeonSaveGame& Save, ADungeonGridActor* InGrid, FString& Error);

	// Save only a complete player turn state or a finished battle.
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveBattle();

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadBattle(ADungeonGridActor* InGrid);

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString LastSaveError;

	// Вызывает CanExecute, затем Execute, если проверка вернула Success, и после Execute - CheckBattleEnd().
	// Возвращает результат проверки. Ссылку на действие не хранит. Для пустого действия возвращает InvalidUnit
	EBattleActionResult TryExecute(UBattleAction* Action);

	// Работает только в PlayerTurn: вызывает AdvancePhase(). В других фазах ничего не делает
	void EndPlayerTurn();

	// Запрос перехода к GetNextPhase() текущей фазы. Если бой завершён, ничего не делает. Если цикл переходов
	// уже идёт (вызов из Enter), только ставит bTransitionPending. Если у текущей фазы нет объекта
	// (None, Finished), ничего не делает
	void AdvancePhase();

	// Проверка конца боя: нет живых героев - Defeat (проверяется первым), нет живых монстров и
	// CurrentWave >= TotalWaves - Victory. В фазах None и Finished ничего не меняет.
	// Возвращает true, если бой завершён или завершается. Вызывает TryExecute после каждого успешного действия
	bool CheckBattleEnd();

	bool IsBattleOver() const { return Result != EBattleResult::None; }

	// Спавнит актёр, присваивает ему следующий UnitId, ставит его в модель сетки и добавляет в Units.
	// Если клетка занята, непроходима или вне поля, уничтожает актёр и возвращает nullptr
	ADungeonUnit* SpawnUnit(UUnitDefinition* Def, FIntPoint Coord);

	// FGridModel::MoveUnit и ADungeonUnit::SetGridPosition. OnUnitMoved не вызывает:
	// его вызывает действие, из которого вызван метод
	bool MoveUnitInternal(ADungeonUnit* Unit, FIntPoint To);

	// Снимок боя для правил способностей: копия модели поля, живые юниты, Rules->CollisionDamage
	FBattleSimState MakeSimState() const;

	// FAbilityRules::GetSelectableTiles для способности юнита. Out заменяется. false (Out пуст), если юнита
	// нет, он мёртв или индекс способности вне списка; иначе true, даже если клеток нет
	bool GetSelectableTiles(const ADungeonUnit* Unit, int32 AbilityIndex, TArray<FIntPoint>& Out) const;

	// MakeCast и Preview на снимке, состояние боя не меняется. false, если клетка недопустима
	// (или юнита / способности нет), Out при этом пуст
	bool PreviewAbility(const ADungeonUnit* Unit, int32 AbilityIndex, FIntPoint SelectedTile, FAbilityOutcome& Out) const;

	// Применение способности: Simulate на снимке, перезарядка, MarkActed для героя, ApplyOutcome, делегаты,
	// RebuildIntentViews. Проверок не делает: вызывается только из UUseAbilityAction и UResolveIntentAction.
	// Конец боя проверяет TryExecute после Execute действия
	void ExecuteAbilityInternal(ADungeonUnit* Source, int32 AbilityIndex, const FAbilityCast& Cast);

	// Применение намерения монстра на текущем снимке (FAbilityRules::MakeIntentCast). false, если монстра нет,
	// он мёртв, у него нет намерения или способность вне списка
	bool GetIntentCast(const AMonsterUnit* Monster, FAbilityCast& Out) const;

	// Случайность боя и правила для фаз и AI
	FRandomStream& GetRandom() { return Random; }
	const UBattleRulesConfig& GetRules() const;

	EBattlePhase GetCurrentPhase() const { return CurrentPhase; }
	bool IsPlayerTurn() const { return CurrentPhase == EBattlePhase::PlayerTurn; }
	int32 GetTurnNumber() const { return TurnNumber; }
	int32 GetCurrentWave() const { return CurrentWave; }
	int32 GetTotalWaves() const { return TotalWaves; }
	EBattleResult GetResult() const { return Result; }

	ADungeonGridActor* GetGrid() const { return Grid; }
	const TArray<TObjectPtr<ADungeonUnit>>& GetUnits() const { return Units; }

	// Все герои боя, включая погибших (они нужны панели отряда)
	TArray<AHeroUnit*> GetHeroes() const;

	// Живые монстры в порядке появления
	TArray<AMonsterUnit*> GetLivingMonsters() const;

	bool HasLivingHeroes() const;

	// Юнит по UnitId, в том числе погибший, или nullptr
	ADungeonUnit* FindUnitById(int32 UnitId) const;

	// Намерения, готовые к показу, по порядку Order
	const TArray<FIntentView>& GetIntentViews() const { return IntentViews; }

	// Нужен фазам
	const UBattlefieldConfig* GetConfig() const { return Config; }

	// В StartBattle после сброса состояния, до Preparation. Подписчики очищают своё состояние
	UPROPERTY(BlueprintAssignable)
	FOnBattleStarted OnBattleStarted;

	UPROPERTY(BlueprintAssignable)
	FOnBattlePhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable)
	FOnBattleTurnStarted OnTurnStarted;

	// После появления волны в Preparation и WaveSpawn
	UPROPERTY(BlueprintAssignable)
	FOnBattleWaveChanged OnWaveChanged;

	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitSpawned OnUnitSpawned;

	// После перемещения, смещения и перемещения источника
	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitMoved OnUnitMoved;

	// После изменения здоровья, флагов хода, перезарядки, опыта, смерти
	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitChanged OnUnitChanged;

	// Killer - источник способности или nullptr
	UPROPERTY(BlueprintAssignable)
	FOnBattleUnitDied OnUnitDied;

	UPROPERTY(BlueprintAssignable)
	FOnBattleAbilityUsed OnAbilityUsed;

	UPROPERTY(BlueprintAssignable)
	FOnBattleIntentsChanged OnIntentsChanged;

	UPROPERTY(BlueprintAssignable)
	FOnBattleEnded OnBattleEnded;

private:
	void ResetBattleState(UBattlefieldConfig* InConfig, ADungeonGridActor* InGrid);
	void AutoSaveCheckpoint();

	FString AutoSaveSlot;
	bool bAutoSaveEnabled = false;
	bool bRestoringSave = false;
	bool bApplyingAction = false;

	// Закрытые методы и поля меняют служебное состояние боя: их вызывают только фазы и действия
	friend class UPreparationPhase;
	friend class UMonsterStepPhase;
	friend class UMonstersMovePhase;
	friend class UMonstersIntentPhase;
	friend class UPlayerTurnPhase;
	friend class UResolveIntentsPhase;
	friend class UWaveSpawnPhase;
	friend class UMoveAction;
	friend class UUseAbilityAction;
	friend class UResolveIntentAction;

	// Цикл переходов без рекурсии: SetPhase(Next), затем, пока стоит bTransitionPending или bFinishPending,
	// переходит к следующей фазе или завершает бой
	void TransitionTo(EBattlePhase Next);

	// Один переход: Exit текущей фазы, смена CurrentPhase, Enter новой, OnPhaseChanged
	void SetPhase(EBattlePhase Next);

	// Завершение боя: Result, ClearAllIntents, Finished и OnBattleEnded. Внутри цикла переходов
	// завершение откладывается флагом bFinishPending. Вызывается только из CheckBattleEnd
	void FinishBattle(EBattleResult InResult);

	// Для каждого определения из WaveComposition по порядку: случайная свободная проходимая клетка
	// MonsterSpawnArea через Random, затем SpawnUnit. Без свободной клетки монстр не появляется
	// (предупреждение в лог). Счётчик волн не меняет
	void SpawnWave();

	// Применяет события результата к бою по порядку (Damage, Heal, Push, SourceMove, Death)
	void ApplyOutcome(const FAbilityOutcome& Outcome);

	// Смерть юнита: снятие с поля, MarkDead, отмена намерения, опыт убийце-герою, делегаты.
	// Killer - источник способности или nullptr
	void KillUnit(ADungeonUnit* Unit, ADungeonUnit* Killer);

	// Намерения монстров. SetIntent выставляет bActive. ClearAllIntents очищает и IntentViews (без делегата)
	void SetIntent(AMonsterUnit* Monster, const FMonsterIntent& Intent);
	void ClearIntent(AMonsterUnit* Monster);
	void ClearAllIntents();

	// Пересчитывает IntentViews по Order и вызывает OnIntentsChanged
	void RebuildIntentViews();

	// TickCooldowns(TurnNumber) у всех живых юнитов. OnUnitChanged - для тех, у кого перезарядка изменилась
	void TickCooldowns();

	UPROPERTY()
	TObjectPtr<ADungeonGridActor> Grid;

	UPROPERTY()
	TObjectPtr<UBattlefieldConfig> Config;

	// Config->Rules или CDO UBattleRulesConfig
	UPROPERTY()
	TObjectPtr<UBattleRulesConfig> Rules;

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

	// Бой завершился во время цикла переходов
	bool bFinishPending = false;

	// Следующий UnitId. StartBattle ставит 0
	int32 NextUnitId = 0;

	// Места появления монстров и выбор AI при равных оценках
	FRandomStream Random;

	UPROPERTY()
	TArray<FIntentView> IntentViews;
};
