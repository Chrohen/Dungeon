#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Units/UnitTypes.h"
#include "DungeonUnit.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UUnitDefinition;

// Юнит на поле. Обычный актёр, не ACharacter: без контроллера и компонентов движения.
// Работает без меша (в тестах боя UnitClass = AHeroUnit без ассетов)
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

	// Задаёт тип и стартовую позицию. Вызывает UBattleSubsystem
	void InitializeUnit(UUnitDefinition* Def, FIntPoint Coord);

	// Сейчас Definition->MovePoints (0 без Definition). Позже будет учитывать эффекты. Вызывает любой
	int32 GetMovePoints() const;

	// !TurnState.bHasMoved. Вызывает любой
	bool CanMove() const;

	// Сбрасывает флаги хода. Вызывает UPlayerTurnPhase
	void ResetTurnState();

	// bHasMoved = true. Вызывает UMoveAction
	void MarkMoved();

	// Обновляет копию позиции и телепортирует актёр так, чтобы низ капсулы стоял в World.
	// Вызывает UBattleSubsystem. //CURRENT-STAGE: позже появится анимация перемещения
	void SetGridPosition(FIntPoint Coord, const FVector& World);

	// Включает или выключает Custom Depth на Mesh (зелёный контур, стенсил 1).
	// Вызывает UBattleSelectionComponent
	void SetSelected(bool bSelected);
};
