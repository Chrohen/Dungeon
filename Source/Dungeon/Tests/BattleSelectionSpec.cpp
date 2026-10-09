#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/UnitDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Input/BattleSelectionComponent.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Строки таблицы TDD 7.2. Курсор задаётся напрямую через FCursorTarget
BEGIN_DEFINE_SPEC(FBattleSelectionSpec, "Dungeon.Input.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	UBattleSubsystem* Battle = nullptr;
	UBattleSelectionComponent* Selection = nullptr;

	// Герои из HeroPlacements: (1,2), (2,1), (3,1)
	ADungeonUnit* HeroAt(FIntPoint Coord) const { return Battle->GetGrid()->GetModel().GetOccupant(Coord)->Unit.Get(); }
	AHeroUnit* HeroA() const { return Cast<AHeroUnit>(HeroAt(FIntPoint(1, 2))); }
	AHeroUnit* HeroB() const { return Cast<AHeroUnit>(HeroAt(FIntPoint(2, 1))); }

	static FCursorTarget OnUnit(ADungeonUnit* Unit)
	{
		FCursorTarget Target;
		Target.bOnGrid = true;
		Target.Tile = Unit->GridPosition;
		Target.Unit = Unit;
		return Target;
	}

	static FCursorTarget OnTile(FIntPoint Tile)
	{
		FCursorTarget Target;
		Target.bOnGrid = true;
		Target.Tile = Tile;
		return Target;
	}

	static FCursorTarget OffGrid() { return FCursorTarget(); }

	void MakeSelection()
	{
		AActor* Owner = TestWorld.GetWorld()->SpawnActor<AActor>();
		if (!Owner->HasActorBegunPlay())
		{
			Owner->DispatchBeginPlay();
		}
		Selection = NewObject<UBattleSelectionComponent>(Owner);
		Selection->RegisterComponent();
		if (!Selection->HasBegunPlay())
		{
			AddError("The component has not begun play");
		}
	}

	bool StartBattle()
	{
		if (!Battle || !Selection)
		{
			AddError("No subsystem or component");
			return false;
		}
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		return true;
	}

	// Выбран герой A
	bool StartAndSelectHeroA()
	{
		if (!StartBattle()) return false;
		Selection->HandleSelect(OnUnit(HeroA()));
		TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
		return true;
	}
END_DEFINE_SPEC(FBattleSelectionSpec)

void FBattleSelectionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		MakeSelection();
	});

	AfterEach([this]()
	{
		Selection = nullptr;
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("Locked", [this]()
	{
		It("is the initial state and ignores input", [this]()
		{
			TestEqual("State", Selection->GetState(), ESelectionState::Locked);
			Selection->HandleSelect(OnTile(FIntPoint(1, 2)));
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			Selection->HandleCancel();
			TestEqual("State", Selection->GetState(), ESelectionState::Locked);
		});

		It("becomes Idle when the phase becomes PlayerTurn", [this]()
		{
			if (!StartBattle()) return;
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
		});

		It("becomes Idle at once when created during PlayerTurn", [this]()
		{
			if (!StartBattle()) return;
			Selection = nullptr;
			MakeSelection();
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
		});

		It("is entered again when the player turn ends", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Battle->EndPlayerTurn();
			// Фазы после хода игрока проходят мгновенно, поэтому к концу вызова снова Idle,
			// но выбор снят
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestFalse("outline", Hero->Mesh->bRenderCustomDepth);
		});
	});

	Describe("Idle", [this]()
	{
		It("selects a hero on LMB", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroA());
			TestTrue("outline", HeroA()->Mesh->bRenderCustomDepth);
		});

		It("ignores LMB on a tile, off the field, RMB and Esc", [this]()
		{
			if (!StartBattle()) return;
			Selection->HandleSelect(OnTile(FIntPoint(5, 5)));
			Selection->HandleSelect(OffGrid());
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			Selection->HandleCancel();
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestEqual("hero not moved", HeroA()->GridPosition, FIntPoint(1, 2));
		});

		It("ignores LMB on a monster", [this]()
		{
			if (!StartBattle()) return;
			UUnitDefinition* MonsterDefinition = TestWorld.MakeHeroDefinition();
			MonsterDefinition->UnitClass = AMonsterUnit::StaticClass();
			ADungeonUnit* Monster = Battle->SpawnUnit(MonsterDefinition, FIntPoint(6, 6));
			TestNotNull("monster", Monster);
			Selection->HandleSelect(OnUnit(Monster));
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
		});
	});

	Describe("HeroSelected", [this]()
	{
		It("switches to another hero on LMB", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* First = HeroA();
			Selection->HandleSelect(OnUnit(HeroB()));
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroB());
			TestFalse("old outline", First->Mesh->bRenderCustomDepth);
			TestTrue("new outline", HeroB()->Mesh->bRenderCustomDepth);
		});

		It("keeps the selection on LMB on the same hero", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			Selection->HandleSelect(OnUnit(HeroA()));
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroA());
			TestTrue("outline", HeroA()->Mesh->bRenderCustomDepth);
		});

		It("keeps the selection on LMB on an empty tile or off the field", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			Selection->HandleSelect(OnTile(FIntPoint(5, 5)));
			Selection->HandleSelect(OffGrid());
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroA());
		});

		It("clears the selection on LMB on a monster", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			UUnitDefinition* MonsterDefinition = TestWorld.MakeHeroDefinition();
			MonsterDefinition->UnitClass = AMonsterUnit::StaticClass();
			ADungeonUnit* Monster = Battle->SpawnUnit(MonsterDefinition, FIntPoint(6, 6));
			TestNotNull("monster", Monster);
			Selection->HandleSelect(OnUnit(Monster));
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestFalse("outline", HeroA()->Mesh->bRenderCustomDepth);
		});

		It("clears the selection on Esc", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleCancel();
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestFalse("outline", Hero->Mesh->bRenderCustomDepth);
		});

		It("moves the hero on RMB on a reachable tile and stays selected", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(1, 4)));
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 4));
			TestFalse("CanMove", Hero->CanMove());
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == Hero);
		});

		It("uses only Tile on RMB, ignoring the Unit of the target", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			// Цель ПКМ берётся по Tile: Unit в цели не важен
			FCursorTarget Target = OnTile(FIntPoint(1, 3));
			Target.Unit = HeroB();
			Selection->HandleConfirm(Target);
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 3));
		});

		It("does nothing on RMB on an unreachable, occupied or off-field tile", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(7, 7)));
			Selection->HandleConfirm(OnTile(FIntPoint(2, 1)));
			Selection->HandleConfirm(OffGrid());
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 2));
			TestTrue("CanMove", Hero->CanMove());
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == Hero);
		});

		It("does not move a hero that has already moved", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			Selection->HandleConfirm(OnTile(FIntPoint(1, 4)));
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 3));
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
		});

		It("behaves as after ClearSelection when the hero disappears", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			HeroA()->Destroy();
			Selection->HandleSelect(OffGrid());
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
		});
	});

	Describe("Turn", [this]()
	{
		It("lets a hero that has moved be selected and moved again after the turn ends", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			Battle->EndPlayerTurn();
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			Selection->HandleSelect(OnUnit(Hero));
			Selection->HandleConfirm(OnTile(FIntPoint(1, 4)));
			TestEqual("GridPosition", Hero->GridPosition, FIntPoint(1, 4));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
