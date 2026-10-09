#include "Grid/DungeonGridActor.h"
#include "Core/DungeonCollision.h"
#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Grid/GridHighlightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	// Толщина PickPlane под поверхностью поля
	constexpr float PickPlaneThickness = 10.f;
}

ADungeonGridActor::ADungeonGridActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PickPlane = CreateDefaultSubobject<UBoxComponent>(TEXT("PickPlane"));
	PickPlane->SetupAttachment(Root);
	PickPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickPlane->SetCollisionResponseToChannel(ECC_Grid, ECR_Block);
	PickPlane->SetGenerateOverlapEvents(false);
	PickPlane->SetHiddenInGame(true);

	Highlight = CreateDefaultSubobject<UGridHighlightComponent>(TEXT("Highlight"));
	Highlight->SetupAttachment(Root);
}

void ADungeonGridActor::BuildFromConfig(UBattlefieldConfig* InConfig)
{
	ClearTileMeshes();
	Config = InConfig;
	if (InConfig == nullptr)
	{
		Model = FGridModel();
		return;
	}

	TileSize = InConfig->TileSize;
	Model.Initialize(InConfig->Width, InConfig->Height, InConfig->DefaultTileType);
	for (const FTileOverride& TileOverride : InConfig->TileOverrides)
	{
		// Переопределение без типа считается незаданным, как в UBattlefieldConfig::IsDataValid
		if (TileOverride.Type != nullptr)
		{
			Model.SetTileType(TileOverride.Coord, TileOverride.Type);
		}
	}

	// Клетки каждого типа, в порядке обхода поля
	TMap<UTileTypeDefinition*, TArray<FIntPoint>> TilesByType;
	for (int32 Y = 0; Y < Model.Height; ++Y)
	{
		for (int32 X = 0; X < Model.Width; ++X)
		{
			const FIntPoint Coord(X, Y);
			UTileTypeDefinition* Type = Model.GetTile(Coord)->Type;
			if (Type != nullptr)
			{
				TilesByType.FindOrAdd(Type).Add(Coord);
			}
		}
	}

	// Высота поверхности пола в пространстве меша. Берётся у меша DefaultTileType и общая для всех
	// типов тайлов: все меши набора стоят на одной плите (низ меша в нуле), а Max.Z меша с
	// выступающими частями (сталагмит) равен высоте этих частей, а не пола. Если у типа по умолчанию
	// нет меша, берётся верх меша самого типа
	TOptional<float> FloorTop;
	if (InConfig->DefaultTileType != nullptr && InConfig->DefaultTileType->Mesh != nullptr)
	{
		FloorTop = static_cast<float>(InConfig->DefaultTileType->Mesh->GetBoundingBox().Max.Z);
	}

	for (const TPair<UTileTypeDefinition*, TArray<FIntPoint>>& Pair : TilesByType)
	{
		UTileTypeDefinition* Type = Pair.Key;
		UStaticMesh* Mesh = Type->Mesh;
		if (Mesh == nullptr)
		{
			// Тип без меша не рисуется (в тестах боя тайлы создаются без мешей)
			continue;
		}

		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Ism->SetStaticMesh(Mesh);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetupAttachment(GetRootComponent());
		Ism->RegisterComponent();

		// Поверхность пола должна лежать на Z = 0
		const float TopOffset = FloorTop.Get(static_cast<float>(Mesh->GetBoundingBox().Max.Z));
		for (const FIntPoint& Coord : Pair.Value)
		{
			FVector Location = GridToLocal(Coord);
			Location.Z -= TopOffset;
			Ism->AddInstance(FTransform(Location), /*bWorldSpace=*/false);
		}
		TileMeshes.Add(Type, Ism);
	}

	const float FieldWidth = Model.Width * TileSize;
	const float FieldHeight = Model.Height * TileSize;
	PickPlane->SetBoxExtent(FVector(FieldWidth * 0.5f, FieldHeight * 0.5f, PickPlaneThickness * 0.5f));
	PickPlane->SetRelativeLocation(FVector(FieldWidth * 0.5f, FieldHeight * 0.5f, -PickPlaneThickness * 0.5f));
}

FVector ADungeonGridActor::GridToWorld(FIntPoint Coord) const
{
	return GetActorTransform().TransformPosition(GridToLocal(Coord));
}

bool ADungeonGridActor::WorldToGrid(const FVector& World, FIntPoint& Out) const
{
	if (TileSize <= 0.f)
	{
		return false;
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(World);
	// FloorToInt, а не приведение к int32: точки с координатой от -1 до 0 не должны попасть в клетку 0
	const FIntPoint Coord(FMath::FloorToInt(Local.X / TileSize), FMath::FloorToInt(Local.Y / TileSize));
	if (!Model.IsInside(Coord))
	{
		return false;
	}
	Out = Coord;
	return true;
}

FVector ADungeonGridActor::GetGridCenter() const
{
	const FVector LocalCenter(Model.Width * TileSize * 0.5f, Model.Height * TileSize * 0.5f, 0.f);
	return GetActorTransform().TransformPosition(LocalCenter);
}

FVector ADungeonGridActor::GridToLocal(FIntPoint Coord) const
{
	return FVector((Coord.X + 0.5f) * TileSize, (Coord.Y + 0.5f) * TileSize, 0.f);
}

void ADungeonGridActor::ClearTileMeshes()
{
	for (const TPair<TObjectPtr<UTileTypeDefinition>, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : TileMeshes)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->DestroyComponent();
		}
	}
	TileMeshes.Empty();
}
