#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/AbilityTypes.h"
#include "Units/UnitTypes.h"
#include "DungeonUnit.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UUnitDefinition;
class UUnitHealthBarComponent;

// Юнит на поле. Обычный актёр, не ACharacter: без контроллера и компонентов движения.
// Работает без меша и без класса виджета (в тестах боя UnitClass = AHeroUnit без ассетов)
UCLASS(Abstract, Blueprintable)
class DUNGEON_API ADungeonUnit : public AActor
{
	GENERATED_BODY()

public:
	ADungeonUnit();

	// Корневой компонент. Блокирует только канал ECC_Unit, чтобы героя можно было выбрать по модели
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<UCapsuleComponent> Capsule;

	// Модель KayKit. Без коллизии
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	// Деления здоровья над моделью. Высоту задаёт BP_*Unit_*
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<UUnitHealthBarComponent> HealthBar;

	// Тип юнита
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TObjectPtr<UUnitDefinition> Definition;

	// Задаётся в наследнике
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	EUnitTeam Team = EUnitTeam::Hero;

	// Копия позиции из FGridModel
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	FIntPoint GridPosition = FIntPoint::ZeroValue;

	// Что сделано за ход
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	FUnitTurnState TurnState;

	// Номер юнита в бою, по порядку появления. Он же порядок обработки монстров AI
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	int32 UnitId = INDEX_NONE;

	// Текущее здоровье
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	int32 Health = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	bool bAlive = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	int32 Level = 1;

	// Способности в бою: итоговые значения и перезарядка
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
	TArray<FUnitAbilityState> Abilities;

	// Задаёт тип, стартовую позицию и UnitId. Health = MaxHealth, bAlive = true, Level из определения,
	// способности через FAbilityParams::Resolve, перезарядки 0. TurnState не сбрасывает.
	// Вызывает UBattleSubsystem
	void InitializeUnit(UUnitDefinition* Def, FIntPoint Coord, int32 InUnitId);

	// Сейчас Definition->MovePoints (0 без Definition). Позже будет учитывать эффекты. Вызывает любой
	int32 GetMovePoints() const;

	// Definition->MaxHealth, 1 без Definition. Вызывает любой
	int32 GetMaxHealth() const;

	int32 GetHealth() const { return Health; }

	bool IsAlive() const { return bAlive; }

	// IsAlive() && !TurnState.bHasMoved. Вызывает любой
	bool CanMove() const;

	// IsAlive() && !TurnState.bHasActed. Вызывает любой
	bool CanAct() const;

	// Сбрасывает флаги хода. Вызывает UPlayerTurnPhase и UMonstersMovePhase
	void ResetTurnState();

	// bHasMoved = true. Вызывает UMoveAction
	void MarkMoved();

	// bHasActed = true. Вызывает UBattleSubsystem::ExecuteAbilityInternal
	void MarkActed();

	// Обновляет копию позиции и телепортирует актёр так, чтобы низ капсулы стоял в World.
	// Вызывает UBattleSubsystem. //CURRENT-STAGE: позже появится анимация перемещения
	void SetGridPosition(FIntPoint Coord, const FVector& World);

	// Здоровье в пределах [0, GetMaxHealth()]. Смерть не запускает: её обрабатывает подсистема.
	// Вызывает UBattleSubsystem
	void SetHealth(int32 NewHealth);

	// bAlive = false, актёр скрыт, коллизия капсулы выключена, деления скрыты. Актёр остаётся
	// в Units подсистемы до следующего StartBattle. Вызывает UBattleSubsystem
	void MarkDead();

	// Вызывает любой
	const TArray<FUnitAbilityState>& GetAbilities() const { return Abilities; }

	// RemainingCooldown = Cooldown способности, UsedOnTurn = TurnNumber (TD 7.4). Вызывает UBattleSubsystem
	void StartCooldown(int32 Index, int32 TurnNumber);

	// Уменьшает на 1 оставшуюся перезарядку каждой способности, у которой она больше 0
	// и UsedOnTurn != TurnNumber. Вызывает UBattleSubsystem
	void TickCooldowns(int32 TurnNumber);

	// Включает или выключает Custom Depth на Mesh (зелёный контур, стенсил 1).
	// Значение стенсила выставляется при каждом вызове. Вызывает UBattleSelectionComponent
	void SetSelected(bool bSelected);

	// Красный контур монстра: Custom Depth со стенсилом 2. Вызывает UBattleOverlayComponent
	void SetFocused(bool bFocused);

	// Обновляет деления здоровья (наследники добавляют своё, например обесцвечивание).
	// Вызывается из всех методов, меняющих состояние
	virtual void RefreshVisuals();
};
