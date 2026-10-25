#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Grid/BattleOverlayComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridHighlightComponent.h"
#include "Materials/Material.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Намерения монстров и предпросмотр способности на поле. Один герой на (1,2) с 5 здоровья, два монстра
// на (6,6) и (6,5) без способностей: намерения собираются вручную (FIntentView) и показываются через
// Overlay. Тайлы без мешей, деления без виджетов - предпросмотр делений читается через GetShownPreview
BEGIN_DEFINE_SPEC(FBattleOverlaySpec, "Dungeon.Grid.Overlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;
	ADungeonGridActor* Grid = nullptr;
	UBattleOverlayComponent* Overlay = nullptr;
	UGridHighlightComponent* Highlight = nullptr;
	AHeroUnit* Hero = nullptr;
	AMonsterUnit* First = nullptr;
	AMonsterUnit* Second = nullptr;

	// Бой запущен, герой и два монстра стоят на местах
	bool Start()
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(5), FIntPoint(1, 2));
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Hero = Battle->GetHeroes().Num() > 0 ? Battle->GetHeroes()[0] : nullptr;
		First = TestWorld.SpawnMonster(FIntPoint(6, 6), 3);
		Second = TestWorld.SpawnMonster(FIntPoint(6, 5), 3);
		Recorder->Reset();
		return Hero != nullptr && First != nullptr && Second != nullptr;
	}

	// Намерение с областью: центр - точка цели, край - заданные клетки
	FIntentView MakeView(AMonsterUnit* Monster, int32 Order, FIntPoint Target, const TArray<FIntPoint>& Edge, const TArray<FIntentTileDamage>& Damage = {}) const
	{
		FIntentView View;
		View.Monster = Monster;
		View.Order = Order;
		View.Cast.SourceId = Monster->UnitId;
		View.Cast.SourcePosition = Monster->GridPosition;
		View.Cast.SelectedTile = Target;
		View.Cast.TargetPoint = Target;
		View.Cast.Area.Center = Target;
		View.Cast.Area.bHasCenter = true;
		View.Cast.Area.Edge = Edge;
		View.TileDamage = Damage;
		return View;
	}

	static FIntentTileDamage Dmg(FIntPoint Tile, int32 Damage)
	{
		FIntentTileDamage TileDamage;
		TileDamage.Tile = Tile;
		TileDamage.Damage = Damage;
		return TileDamage;
	}

	static void AddDamage(FAbilityOutcome& Outcome, const ADungeonUnit* Unit, int32 Amount, bool bKill = false)
	{
		FAbilityEvent Event;
		Event.Type = EAbilityEventType::Damage;
		Event.UnitId = Unit->UnitId;
		Event.Amount = Amount;
		Outcome.Events.Add(Event);
		if (bKill)
		{
			Event.Type = EAbilityEventType::Death;
			Outcome.Events.Add(Event);
		}
	}

	// Исход способности с областью: центр в Target и край из Edge
	FAbilityOutcome MakeOutcome(const ADungeonUnit* Source, FIntPoint Target, const TArray<FIntPoint>& Edge) const
	{
		FAbilityOutcome Outcome;
		Outcome.Cast.SourceId = Source->UnitId;
		Outcome.Cast.SourcePosition = Source->GridPosition;
		Outcome.Cast.SelectedTile = Target;
		Outcome.Cast.TargetPoint = Target;
		Outcome.Cast.Area.Center = Target;
		Outcome.Cast.Area.bHasCenter = true;
		Outcome.Cast.Area.Edge = Edge;
		return Outcome;
	}

	// Яркость рамки Threat на клетке или -1, если клетки в слое нет
	float ThreatBrightness(FIntPoint Tile) const
	{
		const UInstancedStaticMeshComponent* Instances = Highlight->GetLayerInstances(EGridHighlightLayer::Threat);
		for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
		{
			FTransform Transform;
			Instances->GetInstanceTransform(Index, Transform, /*bWorldSpace=*/true);
			FIntPoint Cell;
			if (Grid->WorldToGrid(Transform.GetLocation(), Cell) && Cell == Tile)
			{
				return Instances->PerInstanceSMCustomData[Index];
			}
		}
		return -1.f;
	}

	const FOverlayTileLabel* FindLabel(FIntPoint Tile) const
	{
		const FVector Expected = Grid->GridToWorld(Tile) + FVector(0.0, 0.0, Overlay->LabelHeight);
		return Overlay->GetLabels().FindByPredicate([&Expected](const FOverlayTileLabel& Label)
		{
			return Label.Location.Equals(Expected, 0.01);
		});
	}

	static bool IsOutlined(const AMonsterUnit* Monster)
	{
		return Monster->Mesh->bRenderCustomDepth && Monster->Mesh->CustomDepthStencilValue == 2;
	}

	bool IsPreviewed(ADungeonUnit* Unit, int32 ExpectedLost, bool bExpectedDie) const
	{
		int32 Lost = 0;
		bool bDie = false;
		return Overlay->GetShownPreview(Unit, Lost, bDie) && Lost == ExpectedLost && bDie == bExpectedDie;
	}
END_DEFINE_SPEC(FBattleOverlaySpec)

void FBattleOverlaySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Grid = TestWorld.GetGrid();
		Overlay = Grid->GetOverlay();
		Highlight = Grid->GetHighlight();
		Hero = nullptr;
		First = nullptr;
		Second = nullptr;

		// Актёры тестового мира сами BeginPlay не получают, а подписку на подсистему компонент делает в нём
		if (!Grid->HasActorBegunPlay())
		{
			Grid->DispatchBeginPlay();
		}
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
		if (Overlay && Highlight)
		{
			// Меш стрелки: плоскость подсветки
			Overlay->ArrowMesh = Highlight->PlaneMesh;
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Hero = nullptr;
		First = nullptr;
		Second = nullptr;
		Overlay = nullptr;
		Highlight = nullptr;
		Grid = nullptr;
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("sub-components", [this]()
	{
		It("creates arrows, silhouette and labels attached to the overlay, transient and without collision", [this]()
		{
			if (!Start()) return;
			Overlay->TileLabelClass = UTestTileLabelWidget::StaticClass();
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}, { Dmg(FIntPoint(2, 2), 1) }) });

			TestNotNull("IntentArrows", Overlay->GetIntentArrows());
			TestNotNull("PreviewArrows", Overlay->GetPreviewArrows());
			TestNotNull("Silhouette", Overlay->GetSilhouette());
			TestFalse("silhouette is hidden", Overlay->GetSilhouette() && Overlay->GetSilhouette()->IsVisible());
			TestEqual("label pool", Overlay->GetLabelPoolSize(), 1);

			int32 Found = 0;
			for (const UActorComponent* Component : Grid->GetComponents())
			{
				const USceneComponent* Scene = Cast<USceneComponent>(Component);
				if (Scene == nullptr || Scene->GetAttachParent() != Overlay)
				{
					continue;
				}
				++Found;
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Scene);
				TestNotNull("primitive", Primitive);
				if (Primitive)
				{
					TestEqual("collision", Primitive->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				}
				TestTrue("transient", Scene->HasAnyFlags(RF_Transient));
			}
			TestEqual("sub-components under the overlay", Found, 4);
		});

		It("does not create a second set when the intents are shown again", [this]()
		{
			if (!Start()) return;
			const UInstancedStaticMeshComponent* Before = Overlay->GetIntentArrows();
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}) });
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(3, 3), {}) });
			TestTrue("same arrows", Overlay->GetIntentArrows() == Before);
		});
	});

	Describe("intents", [this]()
	{
		It("fills the Threat layer with the tiles of all areas and clears it in HideIntents", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), { FIntPoint(3, 2), FIntPoint(2, 3) }),
				MakeView(Second, 2, FIntPoint(3, 2), { FIntPoint(4, 2) }) });
			TestEqual("distinct tiles", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 4);

			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}) });
			TestEqual("replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 1);

			Overlay->HideIntents();
			TestEqual("cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 0);
		});

		It("shows one label per tile with the sum of TileDamage of all intents, and no label for zero damage", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), { FIntPoint(3, 2), FIntPoint(2, 3) }, { Dmg(FIntPoint(2, 2), 2), Dmg(FIntPoint(3, 2), 1), Dmg(FIntPoint(2, 3), 0) }),
				MakeView(Second, 2, FIntPoint(3, 2), { FIntPoint(4, 2) }, { Dmg(FIntPoint(3, 2), 3), Dmg(FIntPoint(4, 2), 1) }) });

			TestEqual("labels", Overlay->GetLabels().Num(), 3);
			const FOverlayTileLabel* Single = FindLabel(FIntPoint(2, 2));
			const FOverlayTileLabel* Summed = FindLabel(FIntPoint(3, 2));
			const FOverlayTileLabel* Other = FindLabel(FIntPoint(4, 2));
			TestTrue("(2,2) has a label", Single != nullptr);
			TestTrue("(3,2) has a label", Summed != nullptr);
			TestTrue("(4,2) has a label", Other != nullptr);
			TestTrue("(2,3) has no label", FindLabel(FIntPoint(2, 3)) == nullptr);
			if (Single && Summed && Other)
			{
				TestEqual("(2,2) damage", Single->Value, 2);
				TestEqual("(3,2) sum of two intents", Summed->Value, 4);
				TestEqual("(4,2) damage", Other->Value, 1);
				TestFalse("a damage label is not a collision", Summed->bCollision);
			}

			Overlay->HideIntents();
			TestEqual("labels cleared", Overlay->GetLabels().Num(), 0);
		});

		It("skips the label widgets without TileLabelClass and creates no pool", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}, { Dmg(FIntPoint(2, 2), 2) }) });
			TestEqual("labels are still computed", Overlay->GetLabels().Num(), 1);
			TestEqual("no pool", Overlay->GetLabelPoolSize(), 0);
		});

		It("reuses pool components for labels", [this]()
		{
			if (!Start()) return;
			Overlay->TileLabelClass = UTestTileLabelWidget::StaticClass();
			auto WithDamage = [this](int32 Count)
			{
				TArray<FIntentTileDamage> Damage;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					Damage.Add(Dmg(FIntPoint(2 + Index, 2), 1));
				}
				Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}, Damage) });
			};

			WithDamage(3);
			TestEqual("pool after 3 labels", Overlay->GetLabelPoolSize(), 3);
			TestEqual("visible after 3 labels", Overlay->GetVisibleLabelCount(), 3);

			WithDamage(1);
			TestEqual("pool is kept", Overlay->GetLabelPoolSize(), 3);
			TestEqual("surplus is hidden", Overlay->GetVisibleLabelCount(), 1);

			WithDamage(2);
			TestEqual("pool is reused", Overlay->GetLabelPoolSize(), 3);
			TestEqual("visible after 2 labels", Overlay->GetVisibleLabelCount(), 2);

			Overlay->HideIntents();
			TestEqual("all hidden", Overlay->GetVisibleLabelCount(), 0);
		});

		It("draws an arrow from the monster to the target point and short arrows for push marks", [this]()
		{
			if (!Start()) return;
			FIntentView View = MakeView(First, 1, FIntPoint(2, 6), { FIntPoint(1, 6) });
			FIntentPushMark Mark;
			Mark.Tile = FIntPoint(2, 6);
			Mark.Direction = EGridDirection::Up;
			View.PushMarks.Add(Mark);
			Overlay->ShowIntents({ View });

			const UInstancedStaticMeshComponent* Arrows = Overlay->GetIntentArrows();
			TestEqual("arrows", Arrows->GetInstanceCount(), 2);
			if (Arrows->GetInstanceCount() == 2)
			{
				FTransform Flight;
				Arrows->GetInstanceTransform(0, Flight, /*bWorldSpace=*/true);
				const FVector Tail = Grid->GridToWorld(FIntPoint(6, 6)) + FVector(0.0, 0.0, Overlay->ArrowHeight);
				TestTrue("tail at the monster plus ArrowHeight", Flight.GetLocation().Equals(Tail, 0.01));
				TestTrue("points to the target", Flight.GetRotation().GetForwardVector().Equals(FVector(-1.0, 0.0, 0.0), 0.001));
				TestTrue("scale X is the length in tiles", FMath::IsNearlyEqual(Flight.GetScale3D().X, 4.0, 0.001));

				FTransform Push;
				Arrows->GetInstanceTransform(1, Push, /*bWorldSpace=*/true);
				const FVector PushTail = Grid->GridToWorld(FIntPoint(2, 6)) + FVector(0.0, 0.0, Overlay->ArrowHeight);
				TestTrue("push arrow on the tile", Push.GetLocation().Equals(PushTail, 0.01));
				TestTrue("push arrow direction (Up is -Y)", Push.GetRotation().GetForwardVector().Equals(FVector(0.0, -1.0, 0.0), 0.001));
				TestTrue("push arrow is short", FMath::IsNearlyEqual(Push.GetScale3D().X, static_cast<double>(Overlay->PushMarkLength), 0.001));
			}

			Overlay->HideIntents();
			TestEqual("arrows cleared", Arrows->GetInstanceCount(), 0);
		});

		It("turns the arrows with the actor and draws none for an intent on the monster's own tile", [this]()
		{
			if (!Start()) return;
			Grid->SetActorRotation(FRotator(0.0, 90.0, 0.0));
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 6), {}), MakeView(Second, 2, FIntPoint(6, 5), {}) });

			const UInstancedStaticMeshComponent* Arrows = Overlay->GetIntentArrows();
			TestEqual("self-target has no arrow", Arrows->GetInstanceCount(), 1);
			if (Arrows->GetInstanceCount() == 1)
			{
				FTransform Flight;
				Arrows->GetInstanceTransform(0, Flight, /*bWorldSpace=*/true);
				// Локальный -X при повороте на 90 градусов смотрит вдоль мирового -Y
				TestTrue("rotated direction", Flight.GetRotation().GetForwardVector().Equals(FVector(0.0, -1.0, 0.0), 0.001));
			}
		});

		It("draws no arrows without ArrowMesh and warns once", [this]()
		{
			AddExpectedMessagePlain(TEXT("ArrowMesh is not set"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			if (!Start()) return;
			Overlay->ArrowMesh = nullptr;
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 6), {}) });
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(3, 6), {}) });
			TestEqual("arrows", Overlay->GetIntentArrows()->GetInstanceCount(), 0);
		});

		It("puts the order on monsters with an intent and removes it from the others", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}), MakeView(Second, 2, FIntPoint(3, 2), {}) });
			TestEqual("first", Overlay->GetShownOrder(First), 1);
			TestEqual("second", Overlay->GetShownOrder(Second), 2);

			Overlay->ShowIntents({ MakeView(Second, 1, FIntPoint(3, 2), {}) });
			TestEqual("first lost its intent", Overlay->GetShownOrder(First), 0);
			TestEqual("second renumbered", Overlay->GetShownOrder(Second), 1);

			Overlay->HideIntents();
			TestEqual("hidden", Overlay->GetShownOrder(Second), 0);
		});
	});

	Describe("focus", [this]()
	{
		It("gives the tiles of the focused monster brightness 1 and the others 0.35", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), { FIntPoint(3, 2) }),
				MakeView(Second, 2, FIntPoint(3, 2), { FIntPoint(4, 2) }) });
			TestEqual("no focus (2,2)", ThreatBrightness(FIntPoint(2, 2)), 1.f);
			TestEqual("no focus (4,2)", ThreatBrightness(FIntPoint(4, 2)), 1.f);

			Overlay->SetFocus(First, {});
			TestEqual("own tile", ThreatBrightness(FIntPoint(2, 2)), 1.f);
			TestEqual("shared tile", ThreatBrightness(FIntPoint(3, 2)), 1.f);
			TestEqual("other tile is dimmed", ThreatBrightness(FIntPoint(4, 2)), 0.35f);

			Overlay->SetFocus(Second, {});
			TestEqual("first monster's tile is dimmed", ThreatBrightness(FIntPoint(2, 2)), 0.35f);
			TestEqual("second monster's tile", ThreatBrightness(FIntPoint(4, 2)), 1.f);

			Overlay->SetFocus(nullptr, {});
			TestEqual("focus removed (2,2)", ThreatBrightness(FIntPoint(2, 2)), 1.f);
			TestEqual("focus removed (4,2)", ThreatBrightness(FIntPoint(4, 2)), 1.f);
		});

		It("shows the damage preview of the focused intent on the units and removes it with the focus", [this]()
		{
			if (!Start()) return;
			FIntentView Wound = MakeView(First, 1, FIntPoint(1, 2), {});
			AddDamage(Wound.Preview, Hero, 2);
			FIntentView Kill = MakeView(Second, 2, FIntPoint(1, 2), {});
			AddDamage(Kill.Preview, Hero, 5, /*bKill=*/true);
			Overlay->ShowIntents({ Wound, Kill });

			int32 Lost = 0;
			bool bDie = false;
			TestFalse("nothing without focus", Overlay->GetShownPreview(Hero, Lost, bDie));

			Overlay->SetFocus(First, {});
			TestTrue("wound", IsPreviewed(Hero, 2, false));

			Overlay->SetFocus(Second, {});
			TestTrue("kill", IsPreviewed(Hero, 5, true));

			Overlay->SetFocus(nullptr, {});
			TestFalse("removed", Overlay->GetShownPreview(Hero, Lost, bDie));
		});

		It("outlines the monsters whose areas contain the focused tile", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), { FIntPoint(3, 2) }),
				MakeView(Second, 2, FIntPoint(3, 2), { FIntPoint(4, 2) }) });

			Overlay->SetFocus(nullptr, FIntPoint(2, 2));
			TestTrue("first outlined", IsOutlined(First));
			TestFalse("second is not", Second->Mesh->bRenderCustomDepth);
			TestEqual("brightness is not changed by a tile", ThreatBrightness(FIntPoint(4, 2)), 1.f);

			Overlay->SetFocus(nullptr, FIntPoint(3, 2));
			TestTrue("both outlined (first)", IsOutlined(First));
			TestTrue("both outlined (second)", IsOutlined(Second));

			Overlay->SetFocus(nullptr, FIntPoint(5, 5));
			TestFalse("tile without intents (first)", First->Mesh->bRenderCustomDepth);
			TestFalse("tile without intents (second)", Second->Mesh->bRenderCustomDepth);

			Overlay->SetFocus(nullptr, FIntPoint(4, 2));
			TestTrue("second outlined", IsOutlined(Second));
			Overlay->SetFocus(nullptr, {});
			TestFalse("empty call removes the outline", Second->Mesh->bRenderCustomDepth);
		});

		It("keeps the focus over a new set of intents and drops it when the monster has no intent", [this]()
		{
			if (!Start()) return;
			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), {}),
				MakeView(Second, 2, FIntPoint(3, 2), {}) });
			Overlay->SetFocus(First, {});

			Overlay->ShowIntents({
				MakeView(First, 1, FIntPoint(2, 2), {}),
				MakeView(Second, 2, FIntPoint(3, 3), {}) });
			TestEqual("still focused", ThreatBrightness(FIntPoint(3, 3)), 0.35f);

			Overlay->ShowIntents({ MakeView(Second, 1, FIntPoint(3, 3), {}) });
			TestEqual("the focused monster has no intent now", ThreatBrightness(FIntPoint(3, 3)), 1.f);
		});

		It("removes the focus, the outline and the preview in HideIntents", [this]()
		{
			if (!Start()) return;
			FIntentView View = MakeView(First, 1, FIntPoint(1, 2), {});
			AddDamage(View.Preview, Hero, 2);
			Overlay->ShowIntents({ View });
			Overlay->SetFocus(First, {});
			Overlay->HideIntents();

			int32 Lost = 0;
			bool bDie = false;
			TestFalse("preview", Overlay->GetShownPreview(Hero, Lost, bDie));

			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}), MakeView(Second, 2, FIntPoint(4, 4), {}) });
			TestEqual("no focus after the hide", ThreatBrightness(FIntPoint(4, 4)), 1.f);

			Overlay->SetFocus(nullptr, FIntPoint(2, 2));
			Overlay->HideIntents();
			TestFalse("outline", First->Mesh->bRenderCustomDepth);
		});
	});

	Describe("ability preview", [this]()
	{
		It("fills the AreaCenter and AreaEdge layers and clears them", [this]()
		{
			if (!Start()) return;
			Overlay->ShowAbilityPreview(MakeOutcome(Hero, FIntPoint(3, 3), { FIntPoint(2, 3), FIntPoint(4, 3), FIntPoint(3, 2), FIntPoint(3, 4) }));
			TestEqual("center", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 1);
			TestEqual("edge", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 4);

			Overlay->ShowAbilityPreview(MakeOutcome(Hero, FIntPoint(4, 4), {}));
			TestEqual("center is replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 1);
			TestEqual("edge is replaced", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 0);

			Overlay->ClearAbilityPreview();
			TestEqual("center cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 0);
			TestEqual("edge cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 0);
		});

		It("has no center layer tile when the area has no center", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), { FIntPoint(2, 3) });
			Outcome.Cast.Area.bHasCenter = false;
			Overlay->ShowAbilityPreview(Outcome);
			TestEqual("center", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 0);
			TestEqual("edge", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 1);
		});

		It("draws the flight line from the source to the actual target point for a projectile only", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(5, 2), {});
			Outcome.Cast.SelectedTile = FIntPoint(7, 2);

			Overlay->ShowAbilityPreview(Outcome, /*bProjectile=*/false);
			TestEqual("instant: no line", Overlay->GetPreviewArrows()->GetInstanceCount(), 0);

			Overlay->ShowAbilityPreview(Outcome, /*bProjectile=*/true);
			const UInstancedStaticMeshComponent* Arrows = Overlay->GetPreviewArrows();
			TestEqual("projectile: one line", Arrows->GetInstanceCount(), 1);
			if (Arrows->GetInstanceCount() == 1)
			{
				FTransform Line;
				Arrows->GetInstanceTransform(0, Line, /*bWorldSpace=*/true);
				TestTrue("tail at the source", Line.GetLocation().Equals(Grid->GridToWorld(FIntPoint(1, 2)) + FVector(0.0, 0.0, Overlay->ArrowHeight), 0.01));
				TestTrue("points along +X", Line.GetRotation().GetForwardVector().Equals(FVector(1.0, 0.0, 0.0), 0.001));
				TestTrue("length to the target point, not to the selected tile", FMath::IsNearlyEqual(Line.GetScale3D().X, 4.0, 0.001));
			}
			TestEqual("intent arrows are not touched", Overlay->GetIntentArrows()->GetInstanceCount(), 0);
		});

		It("shows blinking and skull previews for all units in the area, including allies", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			AddDamage(Outcome, Hero, 1);
			AddDamage(Outcome, First, 3, /*bKill=*/true);
			Overlay->ShowAbilityPreview(Outcome);

			TestTrue("ally takes damage", IsPreviewed(Hero, 1, false));
			TestTrue("enemy dies", IsPreviewed(First, 3, true));
			int32 Lost = 0;
			bool bDie = false;
			TestFalse("untouched unit", Overlay->GetShownPreview(Second, Lost, bDie));

			Overlay->ClearAbilityPreview();
			TestFalse("ally cleared", Overlay->GetShownPreview(Hero, Lost, bDie));
			TestFalse("enemy cleared", Overlay->GetShownPreview(First, Lost, bDie));
		});

		It("sums collision damage into the preview of the unit", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			AddDamage(Outcome, First, 2);
			FAbilityEvent Collision;
			Collision.Type = EAbilityEventType::Damage;
			Collision.UnitId = First->UnitId;
			Collision.Amount = 1;
			Collision.bCollision = true;
			Outcome.Events.Add(Collision);
			Overlay->ShowAbilityPreview(Outcome);
			TestTrue("2 + 1", IsPreviewed(First, 3, false));
		});

		It("draws the displacement arrow from From to To", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			FAbilityEvent Push;
			Push.Type = EAbilityEventType::Push;
			Push.UnitId = First->UnitId;
			Push.From = FIntPoint(3, 3);
			Push.To = FIntPoint(3, 5);
			Outcome.Events.Add(Push);
			FAbilityEvent Stay = Push;
			Stay.To = Stay.From;
			Outcome.Events.Add(Stay);
			Overlay->ShowAbilityPreview(Outcome);

			const UInstancedStaticMeshComponent* Arrows = Overlay->GetPreviewArrows();
			TestEqual("one arrow, none for a push that did not move", Arrows->GetInstanceCount(), 1);
			if (Arrows->GetInstanceCount() == 1)
			{
				FTransform Arrow;
				Arrows->GetInstanceTransform(0, Arrow, /*bWorldSpace=*/true);
				TestTrue("tail at From", Arrow.GetLocation().Equals(Grid->GridToWorld(FIntPoint(3, 3)) + FVector(0.0, 0.0, Overlay->ArrowHeight), 0.01));
				TestTrue("points to To (+Y)", Arrow.GetRotation().GetForwardVector().Equals(FVector(0.0, 1.0, 0.0), 0.001));
				TestTrue("two tiles", FMath::IsNearlyEqual(Arrow.GetScale3D().X, 2.0, 0.001));
			}
		});

		It("puts the collision label between From and To, also when To is outside the field", [this]()
		{
			if (!Start()) return;
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			FAbilityEvent Collision;
			Collision.Type = EAbilityEventType::Collision;
			Collision.UnitId = First->UnitId;
			Collision.Amount = 2;
			Collision.From = FIntPoint(7, 4);
			Collision.To = FIntPoint(8, 4);
			Outcome.Events.Add(Collision);
			Overlay->ShowAbilityPreview(Outcome);

			TestEqual("labels", Overlay->GetLabels().Num(), 1);
			if (Overlay->GetLabels().Num() == 1)
			{
				const FOverlayTileLabel& Label = Overlay->GetLabels()[0];
				const FVector Expected = (Grid->GridToWorld(FIntPoint(7, 4)) + Grid->GridToWorld(FIntPoint(8, 4))) * 0.5 + FVector(0.0, 0.0, Overlay->LabelHeight);
				TestTrue("between the tiles", Label.Location.Equals(Expected, 0.01));
				TestEqual("value", Label.Value, 2);
				TestTrue("impact icon", Label.bCollision);
			}

			Overlay->ClearAbilityPreview();
			TestEqual("cleared", Overlay->GetLabels().Num(), 0);
		});

		It("keeps the intent labels next to the preview labels and removes only its own", [this]()
		{
			if (!Start()) return;
			Overlay->TileLabelClass = UTestTileLabelWidget::StaticClass();
			Overlay->ShowIntents({ MakeView(First, 1, FIntPoint(2, 2), {}, { Dmg(FIntPoint(2, 2), 2) }) });

			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			FAbilityEvent Collision;
			Collision.Type = EAbilityEventType::Collision;
			Collision.UnitId = First->UnitId;
			Collision.From = FIntPoint(3, 3);
			Collision.To = FIntPoint(3, 4);
			Outcome.Events.Add(Collision);
			Overlay->ShowAbilityPreview(Outcome);
			TestEqual("both labels", Overlay->GetLabels().Num(), 2);
			TestEqual("both visible", Overlay->GetVisibleLabelCount(), 2);

			Overlay->ClearAbilityPreview();
			TestEqual("intent label stays", Overlay->GetLabels().Num(), 1);
			TestEqual("one visible", Overlay->GetVisibleLabelCount(), 1);
		});

		It("does not show the silhouette when the source has no mesh or the ghost material is missing", [this]()
		{
			if (!Start()) return;
			TStrongObjectPtr<UMaterial> Ghost(NewObject<UMaterial>());
			Overlay->GhostMaterial = Ghost.Get();
			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			FAbilityEvent Move;
			Move.Type = EAbilityEventType::SourceMove;
			Move.UnitId = Hero->UnitId;
			Move.From = Hero->GridPosition;
			Move.To = FIntPoint(2, 2);
			Outcome.Events.Add(Move);

			Overlay->ShowAbilityPreview(Outcome);
			TestFalse("no source mesh", Overlay->GetSilhouette()->IsVisible());

			Overlay->GhostMaterial = nullptr;
			Overlay->ShowAbilityPreview(Outcome);
			TestFalse("no ghost material", Overlay->GetSilhouette()->IsVisible());
		});

		It("shows the silhouette of the source mesh on the To tile", [this]()
		{
			if (!Start()) return;
			TStrongObjectPtr<USkeletalMesh> SourceMesh(NewObject<USkeletalMesh>());
			TStrongObjectPtr<UMaterial> Ghost(NewObject<UMaterial>());
			Hero->Mesh->SetSkeletalMeshAsset(SourceMesh.Get());
			Overlay->GhostMaterial = Ghost.Get();

			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			FAbilityEvent Move;
			Move.Type = EAbilityEventType::SourceMove;
			Move.UnitId = Hero->UnitId;
			Move.From = Hero->GridPosition;
			Move.To = FIntPoint(4, 2);
			Outcome.Events.Add(Move);
			Overlay->ShowAbilityPreview(Outcome);

			const USkeletalMeshComponent* Silhouette = Overlay->GetSilhouette();
			TestTrue("visible", Silhouette->IsVisible());
			TestTrue("mesh of the source", Silhouette->GetSkeletalMeshAsset() == SourceMesh.Get());
			const FVector Expected = Hero->Mesh->GetComponentLocation() + Grid->GridToWorld(FIntPoint(4, 2)) - Grid->GridToWorld(Hero->GridPosition);
			TestTrue("moved by the distance between the tiles", Silhouette->GetComponentLocation().Equals(Expected, 0.01));

			Overlay->ClearAbilityPreview();
			TestFalse("hidden after the clear", Silhouette->IsVisible());
			Hero->Mesh->SetSkeletalMeshAsset(nullptr);
		});

		It("gives the ability preview priority over the focus preview and restores the focus preview after it", [this]()
		{
			if (!Start()) return;
			FIntentView View = MakeView(First, 1, FIntPoint(1, 2), {});
			AddDamage(View.Preview, Hero, 2);
			Overlay->ShowIntents({ View });
			Overlay->SetFocus(First, {});
			TestTrue("focus preview", IsPreviewed(Hero, 2, false));

			FAbilityOutcome Outcome = MakeOutcome(Hero, FIntPoint(3, 3), {});
			AddDamage(Outcome, Hero, 1);
			Overlay->ShowAbilityPreview(Outcome);
			TestTrue("ability preview wins", IsPreviewed(Hero, 1, false));

			Overlay->ClearAbilityPreview();
			TestTrue("focus preview is back", IsPreviewed(Hero, 2, false));
		});

		It("tolerates ClearAbilityPreview without a preview", [this]()
		{
			if (!Start()) return;
			Overlay->ClearAbilityPreview();
			TestEqual("center", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 0);
		});
	});

	Describe("battle subscriptions", [this]()
	{
		It("shows the intents only in PlayerTurn and hides them in the other phases", [this]()
		{
			if (!Start()) return;
			AMonsterUnit* Attacker = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }));
			TestNotNull("attacker", Attacker);

			int32 ThreatDuringResolve = INDEX_NONE;
			Recorder->PhaseHook = [this, &ThreatDuringResolve](EBattlePhase Phase)
			{
				if (Phase == EBattlePhase::ResolveIntents)
				{
					ThreatDuringResolve = Highlight->GetLayerTileCount(EGridHighlightLayer::Threat);
				}
			};

			TestEqual("no intents yet", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 0);
			Battle->EndPlayerTurn();

			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestEqual("hidden while the intents are resolved", ThreatDuringResolve, 0);
			TestEqual("shown in PlayerTurn", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 1);
			TestEqual("label", Overlay->GetLabels().Num(), 1);
			TestEqual("arrow", Overlay->GetIntentArrows()->GetInstanceCount(), 1);
			TestEqual("order", Overlay->GetShownOrder(Attacker), 1);
		});

		It("clears the ability preview when the phase leaves PlayerTurn", [this]()
		{
			if (!Start()) return;
			Overlay->ShowAbilityPreview(MakeOutcome(Hero, FIntPoint(3, 3), { FIntPoint(4, 3) }));
			TestEqual("preview shown", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 1);

			Battle->EndPlayerTurn();
			TestEqual("center cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 0);
			TestEqual("edge cleared", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaEdge), 0);
		});

		It("reads the current state when it subscribes", [this]()
		{
			if (!Start()) return;
			TestWorld.SpawnMonster(FIntPoint(2, 2), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }));
			Battle->EndPlayerTurn();
			TestEqual("first overlay", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 1);

			// Второе поле появляется посреди хода игрока: BeginPlay подписывает его и сразу показывает намерения
			ADungeonGridActor* LateGrid = TestWorld.GetWorld()->SpawnActor<ADungeonGridActor>();
			TestNotNull("late grid", LateGrid);
			if (LateGrid)
			{
				if (!LateGrid->HasActorBegunPlay())
				{
					LateGrid->DispatchBeginPlay();
				}
				TestEqual("late overlay", LateGrid->GetHighlight()->GetLayerTileCount(EGridHighlightLayer::Threat), 1);
				TestEqual("late labels", LateGrid->GetOverlay()->GetLabels().Num(), 1);
				LateGrid->Destroy();
			}
		});

		It("clears everything when the next battle starts", [this]()
		{
			if (!Start()) return;
			TestWorld.SpawnMonster(FIntPoint(2, 2), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }));
			Battle->EndPlayerTurn();
			TestEqual("intents are shown", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 1);
			Overlay->ShowAbilityPreview(MakeOutcome(Hero, FIntPoint(3, 3), {}));

			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			TestEqual("no monsters, no intents", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 0);
			TestEqual("no arrows", Overlay->GetIntentArrows()->GetInstanceCount(), 0);
			TestEqual("no labels", Overlay->GetLabels().Num(), 0);
			TestEqual("preview center", Highlight->GetLayerTileCount(EGridHighlightLayer::AreaCenter), 0);
		});

		It("updates the display when the intents change in PlayerTurn", [this]()
		{
			if (!Start()) return;
			AMonsterUnit* Attacker = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }));
			Battle->EndPlayerTurn();
			Overlay->SetFocus(Attacker, {});
			const int32 ChangesBefore = Recorder->IntentsChangedCount;

			// Герой уходит с клетки, на которую смотрело намерение: рамка остаётся (намерение привязано к тайлам)
			UMoveAction* Move = NewObject<UMoveAction>();
			Move->Unit = Hero;
			Move->Target = FIntPoint(1, 4);
			Battle->TryExecute(Move);
			TestTrue("OnIntentsChanged was sent", Recorder->IntentsChangedCount > ChangesBefore);
			TestEqual("frame stays on the tile", Highlight->GetLayerTileCount(EGridHighlightLayer::Threat), 1);
			TestEqual("order stays", Overlay->GetShownOrder(Attacker), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
