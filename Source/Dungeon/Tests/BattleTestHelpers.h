#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "Battle/BattleTypes.h"
#include "BattleTestHelpers.generated.h"

class ADungeonGridActor;
class ADungeonUnit;
class UBattleSubsystem;
class UBattlefieldConfig;
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

	UFUNCTION()
	void HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void HandleTurnStarted(int32 TurnNumber);

	UFUNCTION()
	void HandleUnitSpawned(ADungeonUnit* Unit);

	UFUNCTION()
	void HandleUnitMoved(ADungeonUnit* Unit, FIntPoint From, FIntPoint To);

	UFUNCTION()
	void HandleBattleEnded(EBattleResult Result);

	// Новые фазы в порядке прихода OnPhaseChanged
	TArray<EBattlePhase> PhaseChanges;
	TArray<int32> StartedTurns;
	TArray<TWeakObjectPtr<ADungeonUnit>> SpawnedUnits;
	int32 UnitMovedCount = 0;
	TArray<TWeakObjectPtr<ADungeonUnit>> MovedUnits;
	TArray<FIntPoint> MovedFrom;
	TArray<FIntPoint> MovedTo;
	TArray<EBattleResult> EndedResults;

	void Reset();
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

	// Определение героя с UnitClass = AHeroUnit и MovePoints = 5
	UUnitDefinition* MakeHeroDefinition();

	// Ставит непроходимые тайлы (TileOverrides) и перестраивает поле. Вызывать до StartBattle
	void BuildWalls(const TArray<FIntPoint>& Cells);

	// Количество актёров юнитов в мире
	int32 CountUnitActors() const;

private:
	TStrongObjectPtr<UWorld> World;
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UTileTypeDefinition> Wall;
	TStrongObjectPtr<UBattlefieldConfig> Config;
	TArray<TStrongObjectPtr<UUnitDefinition>> Definitions;
	ADungeonGridActor* Grid = nullptr;
};

#endif // WITH_DEV_AUTOMATION_TESTS
