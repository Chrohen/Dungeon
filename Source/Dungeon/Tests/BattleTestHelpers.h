#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "Battle/BattleTypes.h"
#include "Input/InputTypes.h"
#include "UI/TileLabelWidget.h"
#include "BattleTestHelpers.generated.h"

class ADungeonGridActor;
class ADungeonUnit;
class AHeroUnit;
class AMonsterUnit;
class UAbilityDefinition;
class UBattleRulesConfig;
class UBattleSelectionComponent;
class UBattleSubsystem;
class UBattlefieldConfig;
class UMonsterDefinition;
class UTileTypeDefinition;
class UUnitDefinition;
class UWorld;

// Записывает события UBattleSubsystem. Для динамических делегатов нужен UObject с UFUNCTION,
// а UHT не читает .cpp, поэтому класс лежит в заголовке
UCLASS()
class UBattleEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	void Subscribe(UBattleSubsystem& Battle);

	// Подсистема, на которую подписана запись
	TWeakObjectPtr<UBattleSubsystem> Observed;

	UFUNCTION()
	void HandleBattleStarted();

	UFUNCTION()
	void HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void HandleTurnStarted(int32 TurnNumber);

	UFUNCTION()
	void HandleWaveChanged(int32 CurrentWave, int32 TotalWaves);

	UFUNCTION()
	void HandleUnitSpawned(ADungeonUnit* Unit);

	UFUNCTION()
	void HandleUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To);

	UFUNCTION()
	void HandleUnitChanged(ADungeonUnit* Unit);

	UFUNCTION()
	void HandleUnitDied(ADungeonUnit* Unit, ADungeonUnit* Killer);

	UFUNCTION()
	void HandleAbilityUsed(ADungeonUnit* Source, int32 AbilityIndex, FIntPoint TargetPoint);

	UFUNCTION()
	void HandleIntentsChanged();

	UFUNCTION()
	void HandleBattleEnded(EBattleResult Result);

	// Вызывается из HandlePhaseChanged с новой фазой, когда CurrentPhase уже равна ей. Так тест выполняет
	// действие в нужной фазе (например, ResolveIntents), которая проходит мгновенно
	TFunction<void(EBattlePhase)> PhaseHook;

	// Сколько раз пришёл OnBattleStarted и сколько OnPhaseChanged к моменту первого прихода
	int32 BattleStartedCount = 0;
	int32 PhaseChangesAtBattleStarted = INDEX_NONE;

	// Новые фазы в порядке прихода OnPhaseChanged
	TArray<EBattlePhase> PhaseChanges;
	TArray<int32> StartedTurns;
	TArray<int32> WaveChangeCurrent;
	TArray<int32> WaveChangeTotal;
	TArray<TWeakObjectPtr<ADungeonUnit>> SpawnedUnits;
	int32 UnitMovedCount = 0;
	TArray<TWeakObjectPtr<ADungeonUnit>> MovedUnits;
	TArray<FIntPoint> MovedFrom;
	TArray<FIntPoint> MovedTo;

	// Фаза подсистемы в момент каждого OnUnitMoved
	TArray<EBattlePhase> MovedPhases;
	TArray<TWeakObjectPtr<ADungeonUnit>> ChangedUnits;
	TArray<TWeakObjectPtr<ADungeonUnit>> DiedUnits;
	TArray<TWeakObjectPtr<ADungeonUnit>> Killers;
	TArray<TWeakObjectPtr<ADungeonUnit>> AbilitySources;
	TArray<int32> AbilityIndices;
	TArray<FIntPoint> AbilityTargetPoints;
	int32 IntentsChangedCount = 0;
	TArray<EBattleResult> EndedResults;

	// Сколько OnPhaseChanged пришло к моменту первого OnBattleEnded
	int32 PhaseChangesAtBattleEnd = INDEX_NONE;

	void Reset();
};

// Записывает делегаты UBattleSelectionComponent (OnSelectionChanged, OnHoveredUnitChanged)
UCLASS()
class USelectionEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	void Subscribe(UBattleSelectionComponent& Selection);

	UFUNCTION()
	void HandleSelectionChanged(ESelectionState State, AHeroUnit* Hero, int32 AbilityIndex);

	UFUNCTION()
	void HandleHoveredUnitChanged(ADungeonUnit* Unit);

	// Параметры каждого OnSelectionChanged по порядку прихода
	TArray<ESelectionState> States;
	TArray<TWeakObjectPtr<AHeroUnit>> Heroes;
	TArray<int32> Abilities;

	// Юниты каждого OnHoveredUnitChanged (nullptr - юнита нет)
	TArray<TWeakObjectPtr<ADungeonUnit>> HoveredUnits;

	void Reset();
};

// Неабстрактный наследник UTileLabelWidget без дерева виджетов: тестам пула подписей
// UBattleOverlayComponent нужен класс, который можно задать в TileLabelClass
UCLASS()
class UTestTileLabelWidget : public UTileLabelWidget
{
	GENERATED_BODY()
};

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Data/UnitDefinition.h"
#include "Engine/World.h"

// Временный мир типа Game для тестов боя: мир с подсистемами, ADungeonGridActor,
// построенный по тестовому UBattlefieldConfig (8x8, тайлы без мешей, три героя), и определения героев
// с UnitClass = AHeroUnit. Используется спеками Dungeon.Battle.*
class FBattleTestWorld
{
public:
	~FBattleTestWorld();

	// Создаёт мир, поле и конфиг. Подсистема боя создаётся вместе с миром, StartBattle не вызывается
	void Create();

	// Уничтожает мир (DestroyWorldContext, DestroyWorld). Безопасно вызывать повторно
	void Destroy();

	UWorld* GetWorld() const { return World.Get(); }
	UBattleSubsystem* GetBattle() const;
	ADungeonGridActor* GetGrid() const { return Grid; }
	UBattlefieldConfig* GetConfig() const { return Config.Get(); }

	// Определение героя с UnitClass = AHeroUnit, MovePoints = 5, заданным здоровьем и способностями
	UUnitDefinition* MakeHeroDefinition(int32 MaxHealth = 3, const TArray<UAbilityDefinition*>& Abilities = {});

	// Определение монстра с UnitClass = AMonsterUnit
	UMonsterDefinition* MakeMonsterDefinition(int32 MaxHealth, int32 MovePoints, const TArray<UAbilityDefinition*>& Abilities = {});

	// Способность: ромб MinRange..MaxRange, мгновенная, любой тайл, область "тайл", перезарядка Cooldown
	// и перечисленные эффекты. Остальные поля можно поменять у результата
	UAbilityDefinition* MakeAbility(const TArray<FAbilityEffect>& Effects, int32 MinRange = 1, int32 MaxRange = 1, int32 Cooldown = 0);

	// Герой или монстр на клетке через UBattleSubsystem::SpawnUnit. Вызывать после StartBattle.
	// nullptr, если клетка занята. Монстр по умолчанию стоит на месте (0 очков перемещения)
	ADungeonUnit* SpawnHero(FIntPoint Cell, UAbilityDefinition* Ability = nullptr, int32 MaxHealth = 3);
	AMonsterUnit* SpawnMonster(FIntPoint Cell, int32 MaxHealth, UAbilityDefinition* Ability = nullptr, int32 ExperienceReward = 0, int32 MovePoints = 0);

	// Состав волны и число волн. Вызывать до StartBattle. Без вызова состав пуст, а волн 100 -
	// бесконечный цикл ходов, как в спринте 1
	void SetWaves(const TArray<UMonsterDefinition*>& Composition, int32 Count);

	// Один герой вместо расстановки по умолчанию. Вызывать до StartBattle
	void SetSingleHero(UUnitDefinition* Hero, FIntPoint Position);

	// Здоровье всех героев из HeroPlacements (MaxHealth определений). Вызывать до StartBattle
	void SetHeroHealth(int32 Health);

	// Зерно случайности боя (UBattlefieldConfig::RandomSeed). Вызывать до StartBattle
	void SetSeed(int32 Seed);

	// Правила боя в Config->Rules. Вызывать до StartBattle
	UBattleRulesConfig* SetRules(int32 CollisionDamage, float MonsterActionDelay);

	// Ставит непроходимые тайлы (TileOverrides) и перестраивает поле. Вызывать до StartBattle
	void BuildWalls(const TArray<FIntPoint>& Cells);

	// Двигает время мира на Seconds секунд кадрами по 0.1 с, чтобы сработали таймеры фаз. Менеджер таймеров
	// тикает не чаще раза за кадр (GFrameCounter), поэтому счётчик кадров увеличивается вручную. Таймер,
	// поставленный между кадрами, начинает отсчёт со следующего кадра: для паузы P нужно не меньше P + 0.2 с
	void AdvanceTime(float Seconds);

	// Количество актёров юнитов в мире
	int32 CountUnitActors() const;

private:
	TStrongObjectPtr<UWorld> World;
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UTileTypeDefinition> Wall;
	TStrongObjectPtr<UBattlefieldConfig> Config;
	TArray<TStrongObjectPtr<UUnitDefinition>> Definitions;
	TArray<TStrongObjectPtr<UAbilityDefinition>> AbilityDefinitions;
	ADungeonGridActor* Grid = nullptr;
};

#endif // WITH_DEV_AUTOMATION_TESTS
