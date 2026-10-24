#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Grid/GridTypes.h"
#include "GridHighlightComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

// Подсветка клеток по слоям EGridHighlightLayer: один ISM на слой.
// Позиции клеток берутся из ADungeonGridActor::GridToWorld владельца
UCLASS(ClassGroup = (Dungeon), meta = (BlueprintSpawnableComponent))
class DUNGEON_API UGridHighlightComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGridHighlightComponent();

	// Заменяет набор клеток слоя. Brightness - по значению на клетку, пишется в PerInstanceCustomData[0]
	// (слой Threat использует её для приглушения). Без массива яркость 1; если размер массива не равен
	// числу клеток, пишется предупреждение и яркость 1
	void SetLayerTiles(EGridHighlightLayer Layer, const TArray<FIntPoint>& Tiles, const TArray<float>* Brightness = nullptr);

	// Убирает все клетки слоя
	void ClearLayer(EGridHighlightLayer Layer);

	// Обёртка над слоем Hover: клетка под курсором или снятие подсветки
	void SetHoveredTile(TOptional<FIntPoint> Tile);

	// Обёртка над слоем Reachable: показывает доступные клетки, заменяя предыдущий набор
	void ShowReachable(const TArray<FIntPoint>& Tiles);

	// Обёртка над ClearLayer(Reachable)
	void ClearReachable();

	// Число клеток, показанных в слое
	int32 GetLayerTileCount(EGridHighlightLayer Layer) const;

	// ISM слоя или nullptr, если он ещё не создан
	const UInstancedStaticMeshComponent* GetLayerInstances(EGridHighlightLayer Layer) const;

	// M_Highlight_Hover (осветление). Материал слоя Hover
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInterface> HoverMaterial;

	// M_Highlight_Reachable (зелёный полупрозрачный квадрат). Материал слоя Reachable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInterface> ReachableMaterial;

	// Материалы остальных слоёв. Для Hover и Reachable берутся поля выше
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TMap<EGridHighlightLayer, TObjectPtr<UMaterialInterface>> LayerMaterials;

	// Подъём над тайлом, чтобы подсветка не мерцала, сливаясь с его поверхностью
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	float HeightOffset = 1.f;

	// Дополнительный подъём каждого следующего слоя в порядке перечисления, чтобы слои не мерцали
	// между собой. Порядок отрисовки полупрозрачных слоёв задаёт TranslucencySortPriority
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	float LayerHeightStep = 0.1f;

	// Плоский меш в один тайл. Масштабируется под размер тайла поля
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UStaticMesh> PlaneMesh;

protected:
	virtual void OnRegister() override;

private:
	// ISM создаются в OnRegister через NewObject: в конструкторе другого компонента
	// подкомпоненты создаются ненадёжно. Метод идемпотентен, его же вызывает SetLayerTiles
	void EnsureInstances();

	UInstancedStaticMeshComponent* CreateInstances(EGridHighlightLayer Layer);

	UMaterialInterface* GetLayerMaterial(EGridHighlightLayer Layer) const;

	// Пишет предупреждение в лог, если материал не задан или не подходит для ISM
	void WarnIfMaterialUnusable(const UMaterialInterface* Material, const FString& PropertyName) const;

	UPROPERTY(Transient)
	TMap<EGridHighlightLayer, TObjectPtr<UInstancedStaticMeshComponent>> LayerInstances;
};
