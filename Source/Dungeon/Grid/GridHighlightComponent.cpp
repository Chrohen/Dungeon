#include "Grid/GridHighlightComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Индекс слоя в порядке перечисления: определяет высоту и приоритет сортировки
	int32 GetLayerIndex(EGridHighlightLayer Layer)
	{
		return static_cast<int32>(Layer);
	}
}

UGridHighlightComponent::UGridHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneFinder.Succeeded())
	{
		PlaneMesh = PlaneFinder.Object;
	}
}

void UGridHighlightComponent::OnRegister()
{
	Super::OnRegister();
	EnsureInstances();
}

void UGridHighlightComponent::SetLayerTiles(EGridHighlightLayer Layer, const TArray<FIntPoint>& Tiles, const TArray<float>* Brightness)
{
	EnsureInstances();

	UInstancedStaticMeshComponent* Instances = LayerInstances.FindRef(Layer);
	if (Instances == nullptr)
	{
		return;
	}

	Instances->ClearInstances();
	if (Tiles.IsEmpty())
	{
		return;
	}

	const ADungeonGridActor* Grid = Cast<ADungeonGridActor>(GetOwner());
	if (Grid == nullptr || PlaneMesh == nullptr)
	{
		return;
	}

	const bool bUseBrightness = Brightness != nullptr && Brightness->Num() == Tiles.Num();
	if (Brightness != nullptr && !bUseBrightness)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGridHighlightComponent: Brightness has %d values for %d tiles of layer %s, using 1."),
			Brightness->Num(), Tiles.Num(), *StaticEnum<EGridHighlightLayer>()->GetNameStringByValue(GetLayerIndex(Layer)));
	}

	// Меш плоскости приводится к размеру одного тайла
	const float PlaneSize = static_cast<float>(PlaneMesh->GetBoundingBox().GetSize().X);
	const float TileScale = PlaneSize > 0.f ? Grid->GetTileSize() / PlaneSize : 1.f;
	const FQuat Rotation = Grid->GetActorQuat();
	const FVector Scale = Grid->GetActorScale3D() * TileScale;
	const float LayerHeight = HeightOffset + GetLayerIndex(Layer) * LayerHeightStep;

	UMaterialInterface* Material = GetLayerMaterial(Layer);
	if (Instances->GetMaterial(0) != Material)
	{
		Instances->SetMaterial(0, Material);
	}
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		const FVector Location = Grid->GridToWorld(Tiles[Index]) + FVector(0.f, 0.f, LayerHeight);
		Instances->AddInstance(FTransform(Rotation, Location, Scale), /*bWorldSpace=*/true);

		// Данные экземпляра создаются нулями, а яркость по умолчанию 1
		const float Value = bUseBrightness ? (*Brightness)[Index] : 1.f;
		Instances->SetCustomDataValue(Index, 0, Value, /*bMarkRenderStateDirty=*/Index == Tiles.Num() - 1);
	}
}

void UGridHighlightComponent::ClearLayer(EGridHighlightLayer Layer)
{
	if (UInstancedStaticMeshComponent* Instances = LayerInstances.FindRef(Layer))
	{
		Instances->ClearInstances();
	}
}

void UGridHighlightComponent::SetHoveredTile(TOptional<FIntPoint> Tile)
{
	TArray<FIntPoint> Tiles;
	if (Tile.IsSet())
	{
		Tiles.Add(Tile.GetValue());
	}
	SetLayerTiles(EGridHighlightLayer::Hover, Tiles);
}

void UGridHighlightComponent::ShowReachable(const TArray<FIntPoint>& Tiles)
{
	SetLayerTiles(EGridHighlightLayer::Reachable, Tiles);
}

void UGridHighlightComponent::ClearReachable()
{
	ClearLayer(EGridHighlightLayer::Reachable);
}

int32 UGridHighlightComponent::GetLayerTileCount(EGridHighlightLayer Layer) const
{
	const UInstancedStaticMeshComponent* Instances = GetLayerInstances(Layer);
	return Instances != nullptr ? Instances->GetInstanceCount() : 0;
}

const UInstancedStaticMeshComponent* UGridHighlightComponent::GetLayerInstances(EGridHighlightLayer Layer) const
{
	return LayerInstances.FindRef(Layer);
}

void UGridHighlightComponent::EnsureInstances()
{
	if (!IsRegistered() || GetOwner() == nullptr)
	{
		return;
	}
	for (const EGridHighlightLayer Layer : TEnumRange<EGridHighlightLayer>())
	{
		if (LayerInstances.FindRef(Layer) == nullptr)
		{
			LayerInstances.Add(Layer, CreateInstances(Layer));
		}
	}
}

UMaterialInterface* UGridHighlightComponent::GetLayerMaterial(EGridHighlightLayer Layer) const
{
	switch (Layer)
	{
	case EGridHighlightLayer::Hover:
		return HoverMaterial;
	case EGridHighlightLayer::Reachable:
		return ReachableMaterial;
	default:
		return LayerMaterials.FindRef(Layer);
	}
}

void UGridHighlightComponent::WarnIfMaterialUnusable(const UMaterialInterface* Material, const FString& PropertyName) const
{
	// Без материала или без флага Used with Instanced Static Meshes движок рисует плоскость
	// материалом по умолчанию (серая пластина)
	if (Material == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGridHighlightComponent: %s is not set, the highlight uses the default grey material."), *PropertyName);
		return;
	}
	const UMaterial* BaseMaterial = Material->GetMaterial();
	if (BaseMaterial != nullptr && !BaseMaterial->bUsedWithInstancedStaticMeshes)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGridHighlightComponent: %s (%s) lacks 'Used with Instanced Static Meshes'. Enable it in the material (Details > Usage), otherwise the default grey material is drawn."),
			*PropertyName, *GetNameSafe(BaseMaterial));
	}
}

UInstancedStaticMeshComponent* UGridHighlightComponent::CreateInstances(EGridHighlightLayer Layer)
{
	const FString LayerName = StaticEnum<EGridHighlightLayer>()->GetNameStringByValue(GetLayerIndex(Layer));
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner(), *(LayerName + TEXT("Instances")), RF_Transient);
	Instances->SetStaticMesh(PlaneMesh);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	Instances->SetNumCustomDataFloats(1);
	Instances->SetTranslucentSortPriority(GetLayerIndex(Layer));
	Instances->SetupAttachment(this);
	Instances->RegisterComponent();

	const FString PropertyName = Layer == EGridHighlightLayer::Hover ? FString(TEXT("HoverMaterial"))
		: Layer == EGridHighlightLayer::Reachable ? FString(TEXT("ReachableMaterial"))
		: FString::Printf(TEXT("LayerMaterials[%s]"), *LayerName);
	WarnIfMaterialUnusable(GetLayerMaterial(Layer), PropertyName);
	return Instances;
}
