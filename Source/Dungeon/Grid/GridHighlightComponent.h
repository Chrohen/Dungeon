#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GridHighlightComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

// Подсветка клеток: клетка под курсором и доступные клетки.
// Позиции клеток берутся из ADungeonGridActor::GridToWorld владельца
UCLASS(ClassGroup = (Dungeon), meta = (BlueprintSpawnableComponent))
class DUNGEON_API UGridHighlightComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGridHighlightComponent();

	// Подсвечивает клетку под курсором или снимает подсветку
	void SetHoveredTile(TOptional<FIntPoint> Tile);

	// Показывает доступные клетки, заменяя предыдущий набор
	void ShowReachable(const TArray<FIntPoint>& Tiles);

	void ClearReachable();

	// M_Highlight_Hover (осветление)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInterface> HoverMaterial;

	// M_Highlight_Reachable (зелёный полупрозрачный квадрат)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UMaterialInterface> ReachableMaterial;

	// Подъём над тайлом, чтобы подсветка не мерцала, сливаясь с его поверхностью
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	float HeightOffset = 1.f;

	// Плоский меш в один тайл. Масштабируется под размер тайла поля
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Highlight")
	TObjectPtr<UStaticMesh> PlaneMesh;

protected:
	virtual void OnRegister() override;

private:
	// ISM создаются в OnRegister через NewObject: в конструкторе другого компонента
	// подкомпоненты создаются ненадёжно. Метод идемпотентен, его же вызывают Show-методы
	void EnsureInstances();

	UInstancedStaticMeshComponent* CreateInstances(const TCHAR* Name);

	// Пишет ошибку в лог, если материал не задан или не подходит для ISM
	void WarnIfMaterialUnusable(const UMaterialInterface* Material, const TCHAR* PropertyName) const;

	void FillInstances(UInstancedStaticMeshComponent* Instances, UMaterialInterface* Material, const TArray<FIntPoint>& Tiles);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> HoverInstances;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ReachableInstances;
};
