#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridModel.h"
#include "DungeonGridActor.generated.h"

class UBattlefieldConfig;
class UBoxComponent;
class UGridHighlightComponent;
class UInstancedStaticMeshComponent;
class UTileTypeDefinition;

// Показывает сетку на уровне и хранит FGridModel
UCLASS(Blueprintable)
class DUNGEON_API ADungeonGridActor : public AActor
{
	GENERATED_BODY()

public:
	ADungeonGridActor();

	// Создаёт модель, применяет TileOverrides через SetTileType, расставляет инстансы тайлов,
	// задаёт размер PickPlane. Повторный вызов перестраивает поле
	void BuildFromConfig(UBattlefieldConfig* InConfig);

	// Мировая позиция центра верхней грани клетки (Z = 0 в локальных координатах актёра).
	// Границы поля не проверяются
	FVector GridToWorld(FIntPoint Coord) const;

	// Клетка по мировой точке (высота точки не учитывается). false вне поля
	bool WorldToGrid(const FVector& World, FIntPoint& Out) const;

	// Центр поля на уровне Z = 0, нужен для камеры
	FVector GetGridCenter() const;

	const FGridModel& GetModel() const { return Model; }

	UGridHighlightComponent* GetHighlight() const { return Highlight; }

	float GetTileSize() const { return TileSize; }

private:
	friend class UBattleSubsystem;

	FGridModel& GetMutableModel() { return Model; }

	// Центр клетки в локальных координатах актёра
	FVector GridToLocal(FIntPoint Coord) const;

	void ClearTileMeshes();

	UPROPERTY()
	FGridModel Model;

	UPROPERTY()
	TObjectPtr<UBattlefieldConfig> Config;

	// По одному ISM на тип тайла, без коллизии
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTileTypeDefinition>, TObjectPtr<UInstancedStaticMeshComponent>> TileMeshes;

	// Тонкий бокс на всё поле, верхняя грань на Z = 0. Блокирует только канал ECC_Grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> PickPlane;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGridHighlightComponent> Highlight;

	float TileSize = 100.f;
};
