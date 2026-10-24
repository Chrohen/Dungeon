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
#include "Grid/GridHighlightComponent.h"
#include "Materials/Material.h"
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

	Describe("highlight layers", [this]()
	{
		BeforeEach([this]()
		{
			MeshWorld.Create();
		});

		AfterEach([this]()
		{
			MeshWorld.Destroy();
		});

		It("replaces the tile set of a layer in SetLayerTiles and keeps other layers", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			Highlight->SetLayerTiles(EGridHighlightLayer::Reachable, { FIntPoint(0, 0) });
			Highlight->SetLayerTiles(EGridHighlightLayer::Targetable, { FIntPoint(1, 0), FIntPoint(2, 0), FIntPoint(3, 0) });
			TestEqual("Targetable", Highlight->GetLayerTileCount(EGridHighlightLayer::Targetable), 3);

			Highlight->SetLayerTiles(EGridHighlightLayer::Targetable, { FIntPoint(4, 4), FIntPoint(5, 5) });
			TestEqual("Targetable replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::Targetable), 2);
			TestEqual("Reachable untouched", Highlight->GetLayerTileCount(EGridHighlightLayer::Reachable), 1);

			FTransform Instance;
			Highlight->GetLayerInstances(EGridHighlightLayer::Targetable)->GetInstanceTransform(0, Instance, /*bWorldSpace=*/true);
			const FVector Expected = MeshWorld.GetGrid()->GridToWorld(FIntPoint(4, 4));
			TestEqual("X of the first new tile", Instance.GetLocation().X, Expected.X);
			TestEqual("Y of the first new tile", Instance.GetLocation().Y, Expected.Y);

			Highlight->SetLayerTiles(EGridHighlightLayer::Targetable, {});
			TestEqual("empty set clears the layer", Highlight->GetLayerTileCount(EGridHighlightLayer::Targetable), 0);
		});

		It("clears one layer in ClearLayer and tolerates an empty layer", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			Highlight->ClearLayer(EGridHighlightLayer::AreaEdge);
			TestEqual("empty layer", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 0);

			Highlight->SetLayerTiles(EGridHighlightLayer::AreaEdge, { FIntPoint(1, 1), FIntPoint(2, 1) });
			Highlight->SetLayerTiles(EGridHighlightLayer::AreaCenter, { FIntPoint(1, 2) });
			Highlight->ClearLayer(EGridHighlightLayer::AreaEdge);
			TestEqual("cleared layer", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 0);
			TestEqual("other layer", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 1);
		});

		It("keeps the sprint 1 wrappers working on the Hover and Reachable layers", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			Highlight->SetHoveredTile(FIntPoint(2, 3));
			TestEqual("Hover", Highlight->GetLayerTileCount(EGridHighlightLayer::Hover), 1);
			Highlight->SetHoveredTile(FIntPoint(3, 3));
			TestEqual("Hover replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::Hover), 1);
			Highlight->SetHoveredTile(TOptional<FIntPoint>());
			TestEqual("Hover cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::Hover), 0);

			Highlight->ShowReachable({ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0) });
			TestEqual("Reachable", Highlight->GetLayerTileCount(EGridHighlightLayer::Reachable), 3);
			Highlight->ShowReachable({ FIntPoint(0, 1) });
			TestEqual("Reachable replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::Reachable), 1);
			TestEqual("Hover untouched", Highlight->GetLayerTileCount(EGridHighlightLayer::Hover), 0);
			Highlight->ClearReachable();
			TestEqual("Reachable cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::Reachable), 0);
		});

		It("creates one ISM per layer once, without collision, with one custom data float", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			const UInstancedStaticMeshComponent* Before = Highlight->GetLayerInstances(EGridHighlightLayer::Threat);
			Highlight->SetLayerTiles(EGridHighlightLayer::Threat, { FIntPoint(0, 0) });
			Highlight->SetLayerTiles(EGridHighlightLayer::Threat, { FIntPoint(1, 1) });
			TestNotNull("created on registration", Before);
			TestTrue("same ISM", Highlight->GetLayerInstances(EGridHighlightLayer::Threat) == Before);

			int32 IsmCount = 0;
			for (const UActorComponent* Component : MeshWorld.GetGrid()->GetComponents())
			{
				const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Component);
				if (Ism != nullptr)
				{
					++IsmCount;
					TestEqual("collision", Ism->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
					TestEqual("custom data floats", Ism->NumCustomDataFloats, 1);
					TestTrue("transient", Ism->HasAnyFlags(RF_Transient));
				}
			}
			TestEqual("ISM count", IsmCount, 6);
		});

		It("lifts every layer by LayerHeightStep and sorts translucent layers in layer order", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			for (const EGridHighlightLayer Layer : TEnumRange<EGridHighlightLayer>())
			{
				Highlight->SetLayerTiles(Layer, { FIntPoint(2, 2) });
				const UInstancedStaticMeshComponent* Instances = Highlight->GetLayerInstances(Layer);
				const int32 Index = static_cast<int32>(Layer);

				FTransform Instance;
				Instances->GetInstanceTransform(0, Instance, /*bWorldSpace=*/true);
				TestTrue(FString::Printf(TEXT("height of layer %d"), Index),
					FMath::IsNearlyEqual(Instance.GetLocation().Z, Highlight->HeightOffset + Index * Highlight->LayerHeightStep, 0.001));
				TestEqual(FString::Printf(TEXT("sort priority of layer %d"), Index), Instances->TranslucencySortPriority, Index);
			}
			TestEqual("default LayerHeightStep", Highlight->LayerHeightStep, 0.1f);
		});

		It("writes Brightness to PerInstanceCustomData[0], and 1 without it", [this]()
		{
			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			const TArray<FIntPoint> Tiles = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0) };

			const TArray<float> Brightness = { 1.f, 0.35f, 0.5f };
			Highlight->SetLayerTiles(EGridHighlightLayer::Threat, Tiles, &Brightness);
			TestTrue("with Brightness", Highlight->GetLayerInstances(EGridHighlightLayer::Threat)->PerInstanceSMCustomData == Brightness);

			Highlight->SetLayerTiles(EGridHighlightLayer::Threat, Tiles);
			TestTrue("without Brightness", Highlight->GetLayerInstances(EGridHighlightLayer::Threat)->PerInstanceSMCustomData == TArray<float>({ 1.f, 1.f, 1.f }));
		});

		It("warns and uses 1 when Brightness does not match the tile count", [this]()
		{
			AddExpectedMessagePlain(TEXT("Brightness has 2 values for 3 tiles"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

			UGridHighlightComponent* Highlight = MeshWorld.GetGrid()->GetHighlight();
			const TArray<float> Brightness = { 0.35f, 0.35f };
			Highlight->SetLayerTiles(EGridHighlightLayer::Threat, { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0) }, &Brightness);
			TestEqual("tiles", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 3);
			TestTrue("values", Highlight->GetLayerInstances(EGridHighlightLayer::Threat)->PerInstanceSMCustomData == TArray<float>({ 1.f, 1.f, 1.f }));
		});

		It("warns about an unset layer material and a material without the ISM usage flag", [this]()
		{
			AddExpectedMessagePlain(TEXT("LayerMaterials[Threat] is not set"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			AddExpectedMessage(TEXT("LayerMaterials\\[Targetable\\] \\(.*\\) lacks 'Used with Instanced Static Meshes'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

			TStrongObjectPtr<UMaterial> NoIsmFlag(NewObject<UMaterial>());
			NoIsmFlag->bUsedWithInstancedStaticMeshes = false;

			// Материалы должны быть заданы до регистрации компонента, как у BP_DungeonGridActor: берём их из шаблона
			TStrongObjectPtr<ADungeonGridActor> Template(NewObject<ADungeonGridActor>());
			Template->GetHighlight()->LayerMaterials.Add(EGridHighlightLayer::Targetable, NoIsmFlag.Get());
			FActorSpawnParameters Params;
			Params.Template = Template.Get();
			MeshWorld.GetWorld()->SpawnActor<ADungeonGridActor>(ADungeonGridActor::StaticClass(), FTransform::Identity, Params);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
