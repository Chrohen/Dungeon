#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/BattlePlayerController.h"
#include "Data/AbilityDefinition.h"
#include "Data/BattlefieldConfig.h"
#include "Data/MonsterDefinition.h"
#include "Data/UnitDefinition.h"
#include "Grid/BattleOverlayComponent.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridHighlightComponent.h"
#include "Input/BattleSelectionComponent.h"
#include "Tests/AbilityTestHelpers.h"
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

	// Aiming. Герой A (1,2): Strike (урон 1, дальность 1-3) и Slow (урон 1, дальность 2, перезарядка 2); герой B (2,1): Strike
	UAbilityDefinition* Strike = nullptr;
	UAbilityDefinition* Slow = nullptr;

	void SetAimingHeroes()
	{
		Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 3);
		Slow = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 2, 2, 2);
		FHeroPlacement A;
		A.Hero = TestWorld.MakeHeroDefinition(3, { Strike, Slow });
		A.Position = FIntPoint(1, 2);
		FHeroPlacement B;
		B.Hero = TestWorld.MakeHeroDefinition(3, { Strike });
		B.Position = FIntPoint(2, 1);
		TestWorld.GetConfig()->HeroPlacements = { A, B };
	}

	// Бой запущен, герой A выбран и целится способностью Index
	bool StartAiming(int32 Index)
	{
		if (!StartAndSelectHeroA()) return false;
		Selection->SelectAbility(Index);
		TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
		return Selection->GetState() == ESelectionState::Aiming;
	}

	UGridHighlightComponent* Highlight() const { return Battle->GetGrid()->GetHighlight(); }
	UBattleOverlayComponent* Overlay() const { return Battle->GetGrid()->GetOverlay(); }
	int32 LayerCount(EGridHighlightLayer Layer) const { return Highlight()->GetLayerTileCount(Layer); }

	// Поле получает BeginPlay, чтобы Overlay подписался на подсистему (актёры тестового мира сами его не получают)
	void BeginPlayOnGrid()
	{
		if (!TestWorld.GetGrid()->HasActorBegunPlay())
		{
			TestWorld.GetGrid()->DispatchBeginPlay();
		}
	}

	// Монстр на (1,3) со способностью урона 2 по герою A: после первого хода игрока у него намерение на клетку (1,2)
	AMonsterUnit* Attacker = nullptr;

	bool StartWithIntent()
	{
		if (!StartBattle()) return false;
		Attacker = TestWorld.SpawnMonster(FIntPoint(1, 3), 3, TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }));
		Battle->EndPlayerTurn();
		TestTrue("the monster has an intent", Attacker != nullptr && Attacker->HasIntent());
		return Attacker != nullptr && Attacker->HasIntent();
	}

	bool IsPreviewed(ADungeonUnit* Unit, int32 ExpectedLost) const
	{
		int32 Lost = 0;
		bool bDie = false;
		return Overlay()->GetShownPreview(Unit, Lost, bDie) && Lost == ExpectedLost;
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

	Describe("Aiming", [this]()
	{
		BeforeEach([this]()
		{
			SetAimingHeroes();
		});

		It("is entered from HeroSelected by an available ability: reachable tiles hide, targetable tiles show", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			TestTrue("reachable tiles are shown", LayerCount(EGridHighlightLayer::Reachable) > 0);

			Selection->SelectAbility(0);
			TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), 0);
			TestTrue("the hero stays selected", Selection->GetSelectedHero() == HeroA());
			TestEqual("Reachable hidden", LayerCount(EGridHighlightLayer::Reachable), 0);

			TArray<FIntPoint> Expected;
			Battle->GetSelectableTiles(HeroA(), 0, Expected);
			TestTrue("targetable tiles", Expected.Num() > 0 && AbilityTest::SameTiles(Selection->GetCachedTargetable(), Expected));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), Expected.Num());
		});

		It("returns to HeroSelected on Esc and shows the reachable tiles again", [this]()
		{
			if (!StartAiming(0)) return;
			Selection->HandleCancel();
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
			TestTrue("Reachable layer", LayerCount(EGridHighlightLayer::Reachable) > 0);
			TestTrue("the hero stays selected", Selection->GetSelectedHero() == HeroA());

			Selection->HandleCancel();
			TestEqual("the second Esc clears the selection", Selection->GetState(), ESelectionState::Idle);
		});

		It("returns to HeroSelected on the button of the same ability", [this]()
		{
			if (!StartAiming(0)) return;
			Selection->SelectAbility(0);
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
		});

		It("switches the ability on the button of another available ability", [this]()
		{
			if (!StartAiming(0)) return;
			const int32 StrikeTiles = LayerCount(EGridHighlightLayer::Targetable);
			Selection->SelectAbility(1);
			TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), 1);
			TestTrue("other tiles", LayerCount(EGridHighlightLayer::Targetable) != StrikeTiles);
			TestEqual("Targetable layer is the cache", LayerCount(EGridHighlightLayer::Targetable), Selection->GetCachedTargetable().Num());
			TestEqual("Reachable stays hidden", LayerCount(EGridHighlightLayer::Reachable), 0);
		});

		It("ignores buttons of unavailable abilities and wrong indices", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			Selection->SelectAbility(-1);
			Selection->SelectAbility(2);
			TestEqual("indices out of the list", Selection->GetState(), ESelectionState::HeroSelected);

			// Slow на перезарядке после использования
			Selection->SelectAbility(1);
			AHeroUnit* Hero = HeroA();
			TestEqual("aiming Slow", Selection->GetState(), ESelectionState::Aiming);
			Selection->HandleConfirm(OnTile(FIntPoint(3, 2)));
			TestEqual("Slow was used", Selection->GetState(), ESelectionState::HeroSelected);
			TestFalse("hero has acted", Hero->CanAct());
			Selection->SelectAbility(0);
			TestEqual("acted hero cannot aim", Selection->GetState(), ESelectionState::HeroSelected);

			Battle->EndPlayerTurn();
			Selection->HandleSelect(OnUnit(Hero));
			TestTrue("Slow is on cooldown", !Hero->GetAbilities()[1].IsReady());
			Selection->SelectAbility(1);
			TestEqual("cooldown", Selection->GetState(), ESelectionState::HeroSelected);
			Selection->SelectAbility(0);
			TestEqual("Strike is available", Selection->GetState(), ESelectionState::Aiming);
		});

		It("ignores ability buttons in Idle and Locked", [this]()
		{
			Selection->SelectAbility(0);
			TestEqual("Locked", Selection->GetState(), ESelectionState::Locked);
			if (!StartBattle()) return;
			Selection->SelectAbility(0);
			TestEqual("Idle", Selection->GetState(), ESelectionState::Idle);
		});

		It("keeps the aiming on LMB on a tile, a monster, off the field and the same hero", [this]()
		{
			if (!StartAiming(0)) return;
			ADungeonUnit* Monster = TestWorld.SpawnMonster(FIntPoint(5, 5), 3);
			Selection->HandleSelect(OnTile(FIntPoint(3, 2)));
			Selection->HandleSelect(OnUnit(Monster));
			Selection->HandleSelect(OffGrid());
			Selection->HandleSelect(OnUnit(HeroA()));
			TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), 0);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroA());
			TestTrue("Targetable layer", LayerCount(EGridHighlightLayer::Targetable) > 0);
		});

		It("keeps the aiming on RMB on a tile that is not targetable or off the field", [this]()
		{
			if (!StartAiming(0)) return;
			ADungeonUnit* Monster = TestWorld.SpawnMonster(FIntPoint(6, 6), 3);
			Selection->HandleConfirm(OnTile(FIntPoint(6, 6)));
			Selection->HandleConfirm(OffGrid());
			TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
			TestEqual("the monster is not hurt", Monster->GetHealth(), 3);
			TestTrue("the hero can still act", HeroA()->CanAct());
		});

		It("selects another hero on LMB on him or on his card and leaves the aiming", [this]()
		{
			if (!StartAiming(0)) return;
			AHeroUnit* First = HeroA();
			Selection->HandleSelect(OnUnit(HeroB()));
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero", Selection->GetSelectedHero() == HeroB());
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
			TestFalse("old outline", First->Mesh->bRenderCustomDepth);
			TestTrue("new outline", HeroB()->Mesh->bRenderCustomDepth);
			TestTrue("the new hero's tiles", LayerCount(EGridHighlightLayer::Reachable) > 0);

			Selection->SelectAbility(0);
			TestEqual("aiming for the new hero", Selection->GetState(), ESelectionState::Aiming);
			Selection->SelectHeroFromCard(First);
			TestEqual("by the card", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("SelectedHero by the card", Selection->GetSelectedHero() == First);
		});

		It("uses the ability on RMB on a targetable tile and returns to HeroSelected with the reachable tiles", [this]()
		{
			if (!StartAiming(0)) return;
			AHeroUnit* Hero = HeroA();
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(3, 2), 3);
			Selection->HandleConfirm(OnTile(FIntPoint(3, 2)));

			TestEqual("the monster is hurt", Monster->GetHealth(), 2);
			TestFalse("the hero has acted", Hero->CanAct());
			TestEqual("State", Selection->GetState(), ESelectionState::HeroSelected);
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestTrue("SelectedHero", Selection->GetSelectedHero() == Hero);
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
			TestEqual("AreaCenter layer", LayerCount(EGridHighlightLayer::AreaCenter), 0);
			TestTrue("the hero has not moved: reachable tiles are shown", LayerCount(EGridHighlightLayer::Reachable) > 0);
		});

		It("shows no reachable tiles after the ability when the hero has already moved", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			TestEqual("no tiles after the move", LayerCount(EGridHighlightLayer::Reachable), 0);
			TestEqual("hero position", Hero->GridPosition, FIntPoint(1, 3));
			Selection->SelectAbility(0);
			TestEqual("State", Selection->GetState(), ESelectionState::Aiming);
			Selection->HandleConfirm(OnTile(FIntPoint(1, 5)));
			TestEqual("State after the ability", Selection->GetState(), ESelectionState::HeroSelected);
			TestEqual("Reachable stays empty", LayerCount(EGridHighlightLayer::Reachable), 0);
		});

		It("does not move the hero on RMB in Aiming", [this]()
		{
			if (!StartAiming(0)) return;
			Selection->HandleConfirm(OnTile(FIntPoint(1, 3)));
			TestEqual("the hero stays", HeroA()->GridPosition, FIntPoint(1, 2));
			TestTrue("CanMove", HeroA()->CanMove());
			TestFalse("the ability was used on the tile instead", HeroA()->CanAct());
		});

		It("goes to Idle when the selected hero dies of his own ability", [this]()
		{
			UAbilityDefinition* SelfHit = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) }, 0, 0);
			SelfHit->Area.bAffectsSource = true;
			FHeroPlacement A;
			A.Hero = TestWorld.MakeHeroDefinition(3, { SelfHit });
			A.Position = FIntPoint(1, 2);
			FHeroPlacement B;
			B.Hero = TestWorld.MakeHeroDefinition(3);
			B.Position = FIntPoint(2, 1);
			TestWorld.GetConfig()->HeroPlacements = { A, B };

			if (!StartAiming(0)) return;
			AHeroUnit* Hero = HeroA();
			Selection->HandleConfirm(OnTile(FIntPoint(1, 2)));

			TestFalse("the hero died", Hero->IsAlive());
			TestFalse("the battle goes on", Battle->IsBattleOver());
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
			TestEqual("Reachable layer", LayerCount(EGridHighlightLayer::Reachable), 0);

			Selection->SelectHeroFromCard(Hero);
			TestEqual("a dead hero's card is ignored", Selection->GetState(), ESelectionState::Idle);
		});

		It("goes to Locked when the ability ends the battle", [this]()
		{
			UAbilityDefinition* Kill = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) }, 1, 20);
			FHeroPlacement A;
			A.Hero = TestWorld.MakeHeroDefinition(3, { Kill });
			A.Position = FIntPoint(1, 2);
			TestWorld.GetConfig()->HeroPlacements = { A };
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);

			if (!StartAiming(0)) return;
			TArray<AMonsterUnit*> Monsters = Battle->GetLivingMonsters();
			TestEqual("one monster of the only wave", Monsters.Num(), 1);
			if (Monsters.Num() != 1) return;
			Selection->HandleConfirm(OnTile(Monsters[0]->GridPosition));

			TestEqual("Victory", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("State", Selection->GetState(), ESelectionState::Locked);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
		});

		It("goes through Locked back to Idle when the turn ends in Aiming", [this]()
		{
			if (!StartAiming(0)) return;
			AHeroUnit* Hero = HeroA();
			Battle->EndPlayerTurn();
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestEqual("SelectedAbility", Selection->GetSelectedAbility(), static_cast<int32>(INDEX_NONE));
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
			TestFalse("outline", Hero->Mesh->bRenderCustomDepth);
		});

		It("is cleared by the start of a new battle", [this]()
		{
			if (!StartAiming(0)) return;
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			TestEqual("Idle in the new battle's PlayerTurn", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
		});

		It("behaves as after ClearSelection when the hero disappears", [this]()
		{
			if (!StartAiming(0)) return;
			HeroA()->Destroy();
			Selection->HandleSelect(OffGrid());
			TestEqual("State", Selection->GetState(), ESelectionState::Idle);
			TestNull("SelectedHero", Selection->GetSelectedHero());
			TestEqual("Targetable layer", LayerCount(EGridHighlightLayer::Targetable), 0);
		});
	});

	Describe("Hover", [this]()
	{
		BeforeEach([this]()
		{
			SetAimingHeroes();
			BeginPlayOnGrid();
		});

		It("shows the zone and the result preview on a targetable tile and clears them on another", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(3, 2), 3);
			Selection->HandleSelect(OnUnit(HeroA()));
			Selection->SelectAbility(0);

			Selection->HandleHover(OnUnit(Monster));
			TestEqual("zone center", LayerCount(EGridHighlightLayer::AreaCenter), 1);
			TestTrue("result preview", IsPreviewed(Monster, 1));
			TestEqual("the Hover layer", LayerCount(EGridHighlightLayer::Hover), 1);

			Selection->HandleHover(OnTile(FIntPoint(7, 7)));
			TestEqual("no zone on a tile that is not targetable", LayerCount(EGridHighlightLayer::AreaCenter), 0);
			TestFalse("no preview", IsPreviewed(Monster, 1));

			Selection->HandleHover(OnUnit(Monster));
			Selection->HandleHover(OffGrid());
			TestEqual("no zone off the field", LayerCount(EGridHighlightLayer::AreaCenter), 0);
			TestEqual("no Hover off the field", LayerCount(EGridHighlightLayer::Hover), 0);
		});

		It("shows the preview at once when the aiming starts over the cursor, and removes it on exit", [this]()
		{
			if (!StartAndSelectHeroA()) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(3, 2), 3);
			Selection->HandleHover(OnUnit(Monster));
			TestEqual("no zone before the aiming", LayerCount(EGridHighlightLayer::AreaCenter), 0);

			Selection->SelectAbility(0);
			TestEqual("zone at once", LayerCount(EGridHighlightLayer::AreaCenter), 1);

			Selection->HandleCancel();
			TestEqual("zone removed", LayerCount(EGridHighlightLayer::AreaCenter), 0);
			TestFalse("preview removed", IsPreviewed(Monster, 1));
		});

		It("sends OnHoveredUnitChanged only when the unit changes", [this]()
		{
			if (!StartBattle()) return;
			TStrongObjectPtr<USelectionEventRecorder> Events(NewObject<USelectionEventRecorder>());
			Events->Subscribe(*Selection);

			Selection->HandleHover(OnUnit(HeroA()));
			TestEqual("the first unit", Events->HoveredUnits.Num(), 1);
			Selection->HandleHover(OnUnit(HeroA()));
			TestEqual("the same unit", Events->HoveredUnits.Num(), 1);
			Selection->HandleHover(OnUnit(HeroB()));
			TestEqual("another unit", Events->HoveredUnits.Num(), 2);
			Selection->HandleHover(OnTile(FIntPoint(5, 5)));
			TestEqual("no unit", Events->HoveredUnits.Num(), 3);
			Selection->HandleHover(OffGrid());
			TestEqual("still no unit", Events->HoveredUnits.Num(), 3);
			if (Events->HoveredUnits.Num() == 3)
			{
				TestTrue("unit 1", Events->HoveredUnits[0].Get() == HeroA());
				TestTrue("unit 2", Events->HoveredUnits[1].Get() == HeroB());
				TestNull("nullptr", Events->HoveredUnits[2].Get());
			}
		});

		Describe("intent focus", [this]()
		{
			BeforeEach([this]()
			{
				Attacker = nullptr;
			});

			It("focuses the intent of a hovered monster with the damage preview, and removes it", [this]()
			{
				if (!StartWithIntent()) return;
				Selection->HandleHover(OnUnit(Attacker));
				TestTrue("preview on the hero", IsPreviewed(HeroA(), 2));
				TestFalse("the monster itself is not outlined", Attacker->Mesh->bRenderCustomDepth);

				Selection->HandleHover(OnTile(FIntPoint(6, 6)));
				TestFalse("preview removed", IsPreviewed(HeroA(), 2));
			});

			It("outlines the monsters aiming at the hovered tile", [this]()
			{
				if (!StartWithIntent()) return;
				Selection->HandleHover(OnTile(FIntPoint(1, 2)));
				TestTrue("outlined", Attacker->Mesh->bRenderCustomDepth && Attacker->Mesh->CustomDepthStencilValue == 2);

				Selection->HandleHover(OnTile(FIntPoint(6, 6)));
				TestFalse("outline removed", Attacker->Mesh->bRenderCustomDepth);
			});

			It("does the same in HeroSelected", [this]()
			{
				if (!StartWithIntent()) return;
				Selection->HandleSelect(OnUnit(HeroB()));
				Selection->HandleHover(OnUnit(Attacker));
				TestTrue("preview on the hero", IsPreviewed(HeroA(), 2));
			});

			It("does not change the intent focus while aiming", [this]()
			{
				if (!StartWithIntent()) return;
				Selection->HandleHover(OnUnit(Attacker));
				Selection->HandleSelect(OnUnit(HeroB()));
				Selection->SelectAbility(0);
				TestEqual("State", Selection->GetState(), ESelectionState::Aiming);

				Selection->HandleHover(OnTile(FIntPoint(7, 7)));
				TestTrue("the focus preview stays", IsPreviewed(HeroA(), 2));
				Selection->HandleHover(OffGrid());
				TestTrue("the focus preview stays off the field", IsPreviewed(HeroA(), 2));
			});

			It("does not focus in Locked", [this]()
			{
				if (!StartWithIntent()) return;
				Selection->HandleHover(OnUnit(Attacker));
				Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
				Selection->HandleHover(OnUnit(HeroA()));
				TestFalse("no preview of the old intent", IsPreviewed(HeroA(), 2));
			});
		});
	});

	Describe("Events", [this]()
	{
		BeforeEach([this]()
		{
			SetAimingHeroes();
		});

		It("sends OnSelectionChanged once per change of state, hero or ability", [this]()
		{
			TStrongObjectPtr<USelectionEventRecorder> Events(NewObject<USelectionEventRecorder>());
			Events->Subscribe(*Selection);
			if (!StartBattle()) return;
			TestEqual("Locked to Idle", Events->States.Num(), 1);

			AHeroUnit* A = HeroA();
			AHeroUnit* B = HeroB();
			Selection->HandleSelect(OnUnit(A));
			Selection->HandleSelect(OnUnit(A));
			TestEqual("selecting a hero", Events->States.Num(), 2);

			Selection->HandleSelect(OnUnit(B));
			TestEqual("switching the hero is one event", Events->States.Num(), 3);

			Selection->HandleSelect(OnUnit(A));
			Selection->SelectAbility(0);
			TestEqual("aiming", Events->States.Num(), 5);

			Selection->SelectAbility(1);
			TestEqual("switching the ability is one event", Events->States.Num(), 6);

			Selection->HandleSelect(OffGrid());
			Selection->HandleConfirm(OnTile(FIntPoint(7, 7)));
			TestEqual("misses are not events", Events->States.Num(), 6);

			Selection->HandleCancel();
			Selection->HandleCancel();
			TestEqual("exit and clear", Events->States.Num(), 8);

			if (Events->States.Num() == 8)
			{
				TestEqual("1: Idle", Events->States[0], ESelectionState::Idle);
				TestEqual("2: HeroSelected", Events->States[1], ESelectionState::HeroSelected);
				TestTrue("2: hero A", Events->Heroes[1].Get() == A);
				TestTrue("3: hero B", Events->Heroes[2].Get() == B);
				TestEqual("5: Aiming", Events->States[4], ESelectionState::Aiming);
				TestEqual("5: ability", Events->Abilities[4], 0);
				TestEqual("6: ability", Events->Abilities[5], 1);
				TestEqual("7: HeroSelected", Events->States[6], ESelectionState::HeroSelected);
				TestEqual("7: no ability", Events->Abilities[6], static_cast<int32>(INDEX_NONE));
				TestEqual("8: Idle", Events->States[7], ESelectionState::Idle);
				TestNull("8: no hero", Events->Heroes[7].Get());
			}
		});

		It("sends OnSelectionChanged on the card selection and when the turn ends", [this]()
		{
			TStrongObjectPtr<USelectionEventRecorder> Events(NewObject<USelectionEventRecorder>());
			Events->Subscribe(*Selection);
			if (!StartBattle()) return;
			Events->Reset();

			Selection->SelectHeroFromCard(HeroA());
			TestEqual("card", Events->States.Num(), 1);
			Selection->SelectHeroFromCard(HeroA());
			TestEqual("the same card", Events->States.Num(), 1);

			Battle->EndPlayerTurn();
			TestEqual("Locked when the turn ends and Idle when the next one starts", Events->States.Num(), 3);
		});
	});

	Describe("Cards", [this]()
	{
		It("select a hero in Idle and HeroSelected and are ignored in Locked and for the selected hero", [this]()
		{
			Selection->SelectHeroFromCard(nullptr);
			if (!StartBattle()) return;
			Selection->SelectHeroFromCard(nullptr);
			TestEqual("null", Selection->GetState(), ESelectionState::Idle);

			AHeroUnit* A = HeroA();
			AHeroUnit* B = HeroB();
			Selection->SelectHeroFromCard(A);
			TestEqual("Idle to HeroSelected", Selection->GetState(), ESelectionState::HeroSelected);
			TestTrue("hero A", Selection->GetSelectedHero() == A);
			Selection->SelectHeroFromCard(A);
			TestTrue("the same hero", Selection->GetSelectedHero() == A);
			Selection->SelectHeroFromCard(B);
			TestTrue("another hero", Selection->GetSelectedHero() == B);
			TestFalse("old outline", A->Mesh->bRenderCustomDepth);
		});

		It("are ignored in Locked, for example after the battle has ended", [this]()
		{
			// Без монстров и волн бой заканчивается победой при входе в ход игрока
			TestWorld.SetWaves({}, 0);
			if (!StartBattle()) return;
			TestTrue("the battle is over", Battle->IsBattleOver());
			TestEqual("State", Selection->GetState(), ESelectionState::Locked);
			Selection->SelectHeroFromCard(HeroA());
			TestEqual("the card is ignored", Selection->GetState(), ESelectionState::Locked);
			TestNull("SelectedHero", Selection->GetSelectedHero());
		});
	});

	Describe("Controller", [this]()
	{
		It("gives the selection component and the last hover target", [this]()
		{
			ABattlePlayerController* Controller = TestWorld.GetWorld()->SpawnActor<ABattlePlayerController>();
			TestNotNull("controller", Controller);
			if (Controller == nullptr) return;
			TestNotNull("GetSelection", Controller->GetSelection());
			TestFalse("no last target yet", Controller->GetLastHoverTarget().bOnGrid);
			TestNull("no last unit yet", Controller->GetLastHoverTarget().Unit.Get());
			Controller->Destroy();
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
