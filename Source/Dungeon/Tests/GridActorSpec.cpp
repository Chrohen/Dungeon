#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/BoxComponent.h"
#include "Core/DungeonCollision.h"
#include "Data/BattlefieldConfig.h"
#include "Data/TileTypeDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/BattleTestHelpers.h"

// Проверяет преобразование клеток и мира без уровня: актёр создаётся вне мира, тайлы без мешей
BEGIN_DEFINE_SPEC(FGridActorSpec, "Dungeon.Grid.Actor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UBattlefieldConfig> Config;
	TStrongObjectPtr<ADungeonGridActor> Grid;

	// Мир нужен только тестам мешей: компоненты ISM регистрируются в мире
	FBattleTestWorld MeshWorld;
END_DEFINE_SPEC(FGridActorSpec)

void FGridActorSpec::Define()
{
	BeforeEach([this]()
	{
		Floor.Reset(NewObject<UTileTypeDefinition>());
		Floor->bPassable = true;

		Config.Reset(NewObject<UBattlefieldConfig>());
		Config->Width = 4;
		Config->Height = 3;
		Config->TileSize = 100.f;
		Config->DefaultTileType = Floor.Get();

		Grid.Reset(NewObject<ADungeonGridActor>());
		Grid->BuildFromConfig(Config.Get());
	});

	AfterEach([this]()
	{
		Grid.Reset();
		Config.Reset();
		Floor.Reset();
	});

	It("builds the model from the config", [this]()
	{
		TestEqual("Width", Grid->GetModel().Width, 4);
		TestEqual("Height", Grid->GetModel().Height, 3);
		TestTrue("Tile type", Grid->GetModel().GetTile(FIntPoint(3, 2))->Type == Floor.Get());
	});

	It("puts the cell centre at ((X + 0.5) * TileSize, (Y + 0.5) * TileSize, 0)", [this]()
	{
		TestEqual("(0,0)", Grid->GridToWorld(FIntPoint(0, 0)), FVector(50.0, 50.0, 0.0));
		TestEqual("(3,2)", Grid->GridToWorld(FIntPoint(3, 2)), FVector(350.0, 250.0, 0.0));
		TestEqual("grid centre", Grid->GetGridCenter(), FVector(200.0, 150.0, 0.0));
	});

	It("maps world points to cells and back", [this]()
	{
		FIntPoint Cell;
		TestTrue("centre of (2,1)", Grid->WorldToGrid(Grid->GridToWorld(FIntPoint(2, 1)), Cell));
		TestEqual("cell", Cell, FIntPoint(2, 1));
		TestTrue("lower edge of (1,1)", Grid->WorldToGrid(FVector(100.0, 100.0, 0.0), Cell));
		TestEqual("edge cell", Cell, FIntPoint(1, 1));
	});

	It("rejects points outside the field, including those between -1 and 0", [this]()
	{
		FIntPoint Cell;
		TestFalse("X = -0.5", Grid->WorldToGrid(FVector(-0.5, 50.0, 0.0), Cell));
		TestFalse("Y = -0.5", Grid->WorldToGrid(FVector(50.0, -0.5, 0.0), Cell));
		TestFalse("X = Width * TileSize", Grid->WorldToGrid(FVector(400.0, 50.0, 0.0), Cell));
		TestFalse("Y = Height * TileSize", Grid->WorldToGrid(FVector(50.0, 300.0, 0.0), Cell));
	});

	It("takes the actor transform into account", [this]()
	{
		Grid->SetActorLocationAndRotation(FVector(1000.0, 0.0, 0.0), FRotator(0.0, 90.0, 0.0));

		// Локальная ось X смотрит вдоль мирового Y: центр клетки (150, 50) превращается в (-50, 150)
		const FVector World = Grid->GridToWorld(FIntPoint(1, 0));
		TestTrue("rotated position", World.Equals(FVector(950.0, 150.0, 0.0), 0.01));

		FIntPoint Cell;
		TestTrue("WorldToGrid", Grid->WorldToGrid(World, Cell));
		TestEqual("cell", Cell, FIntPoint(1, 0));
	});

	It("configures PickPlane to block only the Grid channel", [this]()
	{
		const UBoxComponent* PickPlane = nullptr;
		for (const UActorComponent* Component : Grid->GetComponents())
		{
			if (Component->GetFName() == TEXT("PickPlane"))
			{
				PickPlane = Cast<UBoxComponent>(Component);
			}
		}
		TestNotNull("PickPlane", PickPlane);
		if (PickPlane)
		{
			TestEqual("Grid response", PickPlane->GetCollisionResponseToChannel(ECC_Grid), ECR_Block);
			TestEqual("Unit response", PickPlane->GetCollisionResponseToChannel(ECC_Unit), ECR_Ignore);
			TestEqual("Visibility response", PickPlane->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
			TestEqual("Collision", PickPlane->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestEqual("Top face Z", PickPlane->GetRelativeLocation().Z + PickPlane->GetUnscaledBoxExtent().Z, 0.0);
			TestEqual("Extent X", PickPlane->GetUnscaledBoxExtent().X, 200.0);
			TestEqual("Extent Y", PickPlane->GetUnscaledBoxExtent().Y, 150.0);
		}
	});

	Describe("tile meshes", [this]()
	{
		BeforeEach([this]()
		{
			MeshWorld.Create();
		});

		AfterEach([this]()
		{
			MeshWorld.Destroy();
		});

		It("puts the floor surface of every tile type on Z = 0, even for a mesh with a tall part", [this]()
		{
			// Пол высотой 26 и тайл с выступом высотой 200 на такой же плите
			auto MakeMesh = [](double Height)
			{
				UStaticMesh* Mesh = NewObject<UStaticMesh>();
				Mesh->SetExtendedBounds(FBoxSphereBounds(FBox(FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, Height))));
				return Mesh;
			};
			TStrongObjectPtr<UStaticMesh> FloorMesh(MakeMesh(26.0));
			TStrongObjectPtr<UStaticMesh> SpikeMesh(MakeMesh(200.0));

			TStrongObjectPtr<UTileTypeDefinition> FloorType(NewObject<UTileTypeDefinition>());
			FloorType->Mesh = FloorMesh.Get();
			TStrongObjectPtr<UTileTypeDefinition> SpikeType(NewObject<UTileTypeDefinition>());
			SpikeType->bPassable = false;
			SpikeType->Mesh = SpikeMesh.Get();

			FTileOverride Override;
			Override.Coord = FIntPoint(1, 1);
			Override.Type = SpikeType.Get();

			UBattlefieldConfig* MeshConfig = MeshWorld.GetConfig();
			MeshConfig->TileOverrides = { Override };
			MeshConfig->DefaultTileType = FloorType.Get();
			MeshWorld.GetGrid()->BuildFromConfig(MeshConfig);

			int32 TypesChecked = 0;
			for (const UActorComponent* Component : MeshWorld.GetGrid()->GetComponents())
			{
				const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Component);
				if (Ism == nullptr || Ism->GetInstanceCount() == 0)
				{
					continue;
				}
				FTransform Instance;
				Ism->GetInstanceTransform(0, Instance, /*bWorldSpace=*/false);
				TestEqual(FString::Printf(TEXT("Z of %s"), *GetNameSafe(Ism->GetStaticMesh())), Instance.GetLocation().Z, -26.0);
				++TypesChecked;
			}
			TestEqual("tile types with meshes", TypesChecked, 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
