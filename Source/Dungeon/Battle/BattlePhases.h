#pragma once

#include "CoreMinimal.h"
#include "AI/MonsterAITypes.h"
#include "Battle/BattlePhase.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "BattlePhases.generated.h"

class AMonsterUnit;

// Общая основа фаз, в которых монстры действуют по одному с паузой Rules.MonsterActionDelay между
// видимыми действиями. При паузе 0 все шаги выполняются внутри Enter
UCLASS(Abstract)
class DUNGEON_API UMonsterStepPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	// BuildQueue, NextIndex = 0, RunSteps()
	virtual void Enter(UBattleSubsystem& Battle) override;

	// Сбрасывает таймер и очередь: если бой завершился или был перезапущен во время паузы, продолжения не будет
	virtual void Exit(UBattleSubsystem& Battle) override;

protected:
	// Порядок монстров фазы (определяет наследник). Вызывается из Enter
	virtual void BuildQueue(UBattleSubsystem& Battle) PURE_VIRTUAL(UMonsterStepPhase::BuildQueue, );

	// Действие одного живого монстра. true, если было видимое действие (после него нужна пауза)
	virtual bool ProcessMonster(UBattleSubsystem& Battle, AMonsterUnit* Monster) PURE_VIRTUAL(UMonsterStepPhase::ProcessMonster, return false;);

	// Очередь обработана, бой не завершён. Вызывается перед AdvancePhase()
	virtual void OnQueueFinished(UBattleSubsystem& Battle) {}

	// Очередь монстров фазы и следующий необработанный
	TArray<TWeakObjectPtr<AMonsterUnit>> Queue;
	int32 NextIndex = 0;

private:
	// Обрабатывает очередь: погибшие пропускаются; после IsBattleOver() выход без AdvancePhase;
	// после видимого действия при паузе больше 0 - таймер и выход; пустая очередь - AdvancePhase()
	void RunSteps();

	TWeakObjectPtr<UBattleSubsystem> OwnerBattle;
	FTimerHandle StepTimer;
};

// Первая волна монстров (счётчик волн 1), затем герои по Config->HeroPlacements с именами и сразу AdvancePhase().
// //CURRENT-STAGE: расстановки игроком пока нет
UCLASS()
class DUNGEON_API UPreparationPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::Preparation; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersMove; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// TurnNumber++, сброс флагов хода у живых монстров, затем монстры по порядку появления выбирают вариант
// (FMonsterAI, с перемещением) и перемещаются через UMoveAction
UCLASS()
class DUNGEON_API UMonstersMovePhase : public UMonsterStepPhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::MonstersMove; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersIntent; }

protected:
	virtual void BuildQueue(UBattleSubsystem& Battle) override;
	virtual bool ProcessMonster(UBattleSubsystem& Battle, AMonsterUnit* Monster) override;

private:
	// Выбранные предыдущими монстрами варианты в этой фазе
	FMonsterAIContext Context;
};

// Намерения: каждый живой монстр по порядку появления повторно выбирает вариант без перемещения;
// атакующий вариант становится намерением. Затем RebuildIntentViews() и сразу AdvancePhase()
UCLASS()
class DUNGEON_API UMonstersIntentPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::MonstersIntent; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::PlayerTurn; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// ResetTurnState() у живых героев, OnTurnStarted, RebuildIntentViews(), CheckBattleEnd(). Ждёт EndPlayerTurn()
UCLASS()
class DUNGEON_API UPlayerTurnPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::PlayerTurn; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::ResolveIntents; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// Живые монстры с намерением по номеру Order реализуют его через UResolveIntentAction. После очереди ClearAllIntents()
UCLASS()
class DUNGEON_API UResolveIntentsPhase : public UMonsterStepPhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::ResolveIntents; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::WaveSpawn; }

protected:
	virtual void BuildQueue(UBattleSubsystem& Battle) override;
	virtual bool ProcessMonster(UBattleSubsystem& Battle, AMonsterUnit* Monster) override;
	virtual void OnQueueFinished(UBattleSubsystem& Battle) override;
};

// Если CurrentWave < TotalWaves - новая волна, CurrentWave++ и OnWaveChanged, иначе фаза пропускается.
// Затем TickCooldowns(). Если волна появилась и пауза больше 0, AdvancePhase() вызывает таймер, иначе сразу
UCLASS()
class DUNGEON_API UWaveSpawnPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::WaveSpawn; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersMove; }
	virtual void Enter(UBattleSubsystem& Battle) override;

	// Сбрасывает таймер паузы
	virtual void Exit(UBattleSubsystem& Battle) override;

private:
	void AdvanceAfterPause();

	TWeakObjectPtr<UBattleSubsystem> OwnerBattle;
	FTimerHandle PauseTimer;
};
