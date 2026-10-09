#include "Grid/GridHighlightComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

void UGridHighlightComponent::SetHoveredTile(TOptional<FIntPoint> Tile)
{
	TArray<FIntPoint> Tiles;
	if (Tile.IsSet())
	{
		Tiles.Add(Tile.GetValue());
	}
	EnsureInstances();
	FillInstances(HoverInstances, HoverMaterial, Tiles);
}

void UGridHighlightComponent::ShowReachable(const TArray<FIntPoint>& Tiles)
{
	EnsureInstances();
	FillInstances(ReachableInstances, ReachableMaterial, Tiles);
}

void UGridHighlightComponent::ClearReachable()
{
	if (ReachableInstances != nullptr)
	{
		ReachableInstances->ClearInstances();
	}
}

void UGridHighlightComponent::EnsureInstances()
{
	if (!IsRegistered() || GetOwner() == nullptr)
	{
		return;
	}
	if (HoverInstances == nullptr)
	{
		HoverInstances = CreateInstances(TEXT("HoverInstances"));
	}
	if (ReachableInstances == nullptr)
	{
		ReachableInstances = CreateInstances(TEXT("ReachableInstances"));
	}
}

UInstancedStaticMeshComponent* UGridHighlightComponent::CreateInstances(const TCHAR* Name)
{
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner(), Name, RF_Transient);
	Instances->SetStaticMesh(PlaneMesh);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	Instances->SetupAttachment(this);
	Instances->RegisterComponent();
	return Instances;
}

void UGridHighlightComponent::FillInstances(UInstancedStaticMeshComponent* Instances, UMaterialInterface* Material, const TArray<FIntPoint>& Tiles)
{
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

	// Меш плоскости приводится к размеру одного тайла
	const float PlaneSize = static_cast<float>(PlaneMesh->GetBoundingBox().GetSize().X);
	const float TileScale = PlaneSize > 0.f ? Grid->GetTileSize() / PlaneSize : 1.f;
	const FQuat Rotation = Grid->GetActorQuat();
	const FVector Scale = Grid->GetActorScale3D() * TileScale;

	if (Instances->GetMaterial(0) != Material)
	{
		Instances->SetMaterial(0, Material);
	}
	for (const FIntPoint& Tile : Tiles)
	{
		const FVector Location = Grid->GridToWorld(Tile) + FVector(0.f, 0.f, HeightOffset);
		Instances->AddInstance(FTransform(Rotation, Location, Scale), /*bWorldSpace=*/true);
	}
}
