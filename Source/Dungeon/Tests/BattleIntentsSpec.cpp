#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Battle/UseAbilityAction.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Намерения монстров: формирование в MonstersIntent и реализация в ResolveIntents.
// Один герой на (1,2); монстры с 0 очков перемещения ставятся вручную после StartBattle и не уходят с места.
// Намерения появляются после первого EndPlayerTurn(): ходу игрока 1 предшествовали только Preparation и перемещение
BEGIN_DEFINE_SPEC(FBattleIntentsSpec, "Dungeon.Battle.Intents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;
	AHeroUnit* Hero = nullptr;

	// Единственный герой на (1,2) с заданным здоровьем и способностью (или без неё); бой запущен
	bool StartBattle(int32 HeroHealth, UAbilityDefinition* HeroAbility = nullptr)
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		TArray<UAbilityDefinition*> Abilities;
		if (HeroAbility != nullptr)
		{
			Abilities.Add(HeroAbility);
		}
		TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(HeroHealth, Abilities), FIntPoint(1, 2));
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Hero = Battle->GetHeroes().Num() > 0 ? Battle->GetHeroes()[0] : nullptr;
		Recorder->Reset();
		return Hero != nullptr;
	}

	UAbilityDefinition* Strike(int32 Damage, int32 MinRange = 1, int32 MaxRange = 1, int32 Cooldown = 0)
	{
		return TestWorld.MakeAbility({ AbilityTest::MakeDamage(Damage) }, MinRange, MaxRange, Cooldown);
	}

	EBattleActionResult MoveHero(FIntPoint Target)
	{
		UMoveAction* Action = NewObject<UMoveAction>();
		Action->Unit = Hero;
		Action->Target = Target;
		return Battle->TryExecute(Action);
	}

	EBattleActionResult UseHeroAbility(FIntPoint Target)
	{
		UUseAbilityAction* Action = NewObject<UUseAbilityAction>();
		Action->Unit = Hero;
		Action->AbilityIndex = 0;
		Action->SelectedTile = Target;
		return Battle->TryExecute(Action);
	}

	// Сколько раз использовала способность каждая из сторон
	int32 CountUses(const ADungeonUnit* Source) const
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<ADungeonUnit>& Used : Recorder->AbilitySources)
		{
			Count += Used.Get() == Source ? 1 : 0;
		}
		return Count;
	}
END_DEFINE_SPEC(FBattleIntentsSpec)

void FBattleIntentsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Hero = nullptr;
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Hero = nullptr;
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("Forming", [this]()
	{
		It("gives an attacking monster the intent with direction, offset and order", [this]()
		{
			if (!StartBattle(5)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(2));
			TestFalse("no intent before the first turn is passed", Monster->HasIntent());

			Battle->EndPlayerTurn();

			TestTrue("HasIntent", Monster->HasIntent());
			TestEqual("AbilityIndex", Monster->Intent.AbilityIndex, 0);
			TestEqual("Direction", Monster->Intent.Direction, EGridDirection::Left);
			TestEqual("SelectedOffset", Monster->Intent.SelectedOffset, FIntPoint(-1, 0));
			TestEqual("Order", Monster->Intent.Order, 1);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);

			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 1);
			if (Battle->GetIntentViews().Num() == 1)
			{
				const FIntentView& View = Battle->GetIntentViews()[0];
				TestTrue("Monster", View.Monster.Get() == Monster);
				TestEqual("Order", View.Order, 1);
				TestEqual("SelectedTile", View.Cast.SelectedTile, FIntPoint(1, 2));
				TestEqual("TileDamage", View.TileDamage.Num(), 1);
				if (View.TileDamage.Num() == 1)
				{
					TestEqual("tile", View.TileDamage[0].Tile, FIntPoint(1, 2));
					TestEqual("damage", View.TileDamage[0].Damage, 2);
				}
				TestEqual("Preview damage to the hero", View.Preview.GetDamageTo(Hero->UnitId), 2);
			}
			TestEqual("the hero is not hurt yet", Hero->GetHealth(), 5);
			TestTrue("OnIntentsChanged was sent", Recorder->IntentsChangedCount > 0);
		});

		It("gives no intent to a monster that cannot hurt a hero", [this]()
		{
			if (!StartBattle(5)) return;
			AMonsterUnit* Far = TestWorld.SpawnMonster(FIntPoint(6, 6), 3, Strike(2));
			AMonsterUnit* Unarmed = TestWorld.SpawnMonster(FIntPoint(2, 2), 3);

			Battle->EndPlayerTurn();

			TestFalse("too far for the ability", Far->HasIntent());
			TestFalse("no ability", Unarmed->HasIntent());
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 0);
		});

		It("numbers the intents 1, 2, ... in order of appearance and skips monsters without an intent", [this]()
		{
			if (!StartBattle(20)) return;
			UAbilityDefinition* Ability = Strike(1);
			AMonsterUnit* Below = TestWorld.SpawnMonster(FIntPoint(1, 3), 3, Ability);
			AMonsterUnit* Right = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Ability);
			AMonsterUnit* Far = TestWorld.SpawnMonster(FIntPoint(6, 6), 3, Ability);
			AMonsterUnit* Left = TestWorld.SpawnMonster(FIntPoint(0, 2), 3, Ability);

			Battle->EndPlayerTurn();

			TestEqual("first", Below->Intent.Order, 1);
			TestEqual("second", Right->Intent.Order, 2);
			TestFalse("no intent", Far->HasIntent());
			TestEqual("third", Left->Intent.Order, 3);
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 3);
			if (Battle->GetIntentViews().Num() == 3)
			{
				TestTrue("view 1", Battle->GetIntentViews()[0].Monster.Get() == Below);
				TestTrue("view 2", Battle->GetIntentViews()[1].Monster.Get() == Right);
				TestTrue("view 3", Battle->GetIntentViews()[2].Monster.Get() == Left);
			}

			// Реализация идёт в том же порядке
			Recorder->Reset();
			Battle->EndPlayerTurn();
			TestEqual("abilities used", Recorder->AbilitySources.Num(), 3);
			if (Recorder->AbilitySources.Num() >= 3)
			{
				TestTrue("first", Recorder->AbilitySources[0].Get() == Below);
				TestTrue("second", Recorder->AbilitySources[1].Get() == Right);
				TestTrue("third", Recorder->AbilitySources[2].Get() == Left);
			}
			TestEqual("the hero took 3 damage", Hero->GetHealth(), 17);
		});

		It("does not give a monster on cooldown an intent", [this]()
		{
			if (!StartBattle(50)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(1, 1, 1, 1));

			Battle->EndPlayerTurn();
			TestTrue("turn 2: ready", Monster->HasIntent());
			Battle->EndPlayerTurn();
			TestFalse("turn 3: the cooldown lasts", Monster->HasIntent());
			TestEqual("the monster has attacked once", CountUses(Monster), 1);
		});

		It("sums the damage effects over the parts of the area and keeps cells without damage", [this]()
		{
			if (!StartBattle(9)) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({
				AbilityTest::MakeDamage(2, EAreaPart::Center),
				AbilityTest::MakeDamage(1, EAreaPart::Center),
				AbilityTest::MakePush(EPushDirection::AbilityDirection, 1, EAreaPart::Center),
				AbilityTest::MakePush(EPushDirection::AwayFromCenter, 1, EAreaPart::Edge) });
			Ability->Area.Type = EAreaType::Diamond;
			Ability->Area.Size = 1;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Ability);

			Battle->EndPlayerTurn();

			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 1);
			if (Battle->GetIntentViews().Num() == 1)
			{
				const FIntentView& View = Battle->GetIntentViews()[0];
				// Центр (1,2): 2 + 1. Край (0,2), (1,1), (1,3): 0, на них действует только смещение. Клетка монстра (2,2) не входит в область
				TMap<FIntPoint, int32> Damage;
				for (const FIntentTileDamage& Entry : View.TileDamage)
				{
					Damage.Add(Entry.Tile, Entry.Damage);
				}
				TestEqual("tiles", Damage.Num(), 4);
				TestEqual("center", Damage.FindRef(FIntPoint(1, 2)), 3);
				TestTrue("edge (0,2) is listed without damage", Damage.Contains(FIntPoint(0, 2)) && Damage[FIntPoint(0, 2)] == 0);
				TestTrue("edge (1,1) is listed without damage", Damage.Contains(FIntPoint(1, 1)) && Damage[FIntPoint(1, 1)] == 0);
				TestTrue("edge (1,3) is listed without damage", Damage.Contains(FIntPoint(1, 3)) && Damage[FIntPoint(1, 3)] == 0);

				// Стрелка по направлению способности на точке цели; на пустых клетках края стрелок нет
				TestEqual("PushMarks", View.PushMarks.Num(), 1);
				if (View.PushMarks.Num() == 1)
				{
					TestEqual("mark tile", View.PushMarks[0].Tile, FIntPoint(1, 2));
					TestEqual("mark direction", View.PushMarks[0].Direction, EGridDirection::Left);
				}
			}
			TestTrue("the monster has the intent", Monster->HasIntent());
		});
	});

	Describe("Binding to tiles", [this]()
	{
		It("does nothing to a hero who has left the tile", [this]()
		{
			if (!StartBattle(5)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(2));
			Battle->EndPlayerTurn();
			TestTrue("HasIntent", Monster->HasIntent());

			TestEqual("MoveHero", MoveHero(FIntPoint(1, 5)), EBattleActionResult::Success);
			TestEqual("the view still shows the tile", Battle->GetIntentViews().Num(), 1);
			if (Battle->GetIntentViews().Num() == 1)
			{
				TestEqual("tile", Battle->GetIntentViews()[0].Cast.SelectedTile, FIntPoint(1, 2));
				TestEqual("the preview does not hurt the hero", Battle->GetIntentViews()[0].Preview.GetDamageTo(Hero->UnitId), 0);
			}

			Recorder->Reset();
			Battle->EndPlayerTurn();
			TestEqual("the monster used the ability", CountUses(Monster), 1);
			TestEqual("the hero is not hurt", Hero->GetHealth(), 5);
		});

		It("hurts another unit that stepped on the tile, even a monster", [this]()
		{
			if (!StartBattle(5)) return;
			AMonsterUnit* Attacker = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(2));
			Battle->EndPlayerTurn();
			TestTrue("HasIntent", Attacker->HasIntent());

			TestEqual("MoveHero", MoveHero(FIntPoint(1, 5)), EBattleActionResult::Success);
			AMonsterUnit* Stranger = TestWorld.SpawnMonster(FIntPoint(1, 2), 5);

			Battle->EndPlayerTurn();

			TestEqual("the stranger took the hit", Stranger->GetHealth(), 3);
			TestEqual("the hero is not hurt", Hero->GetHealth(), 5);
		});

		It("hits from the shifted area in the same direction when the monster was shifted", [this]()
		{
			UAbilityDefinition* Push = TestWorld.MakeAbility({ AbilityTest::MakePush(EPushDirection::AbilityDirection, 1) });
			if (!StartBattle(5, Push)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(2));
			Battle->EndPlayerTurn();
			TestTrue("HasIntent", Monster->HasIntent());

			// Герой толкает монстра вправо: цель намерения сдвигается вместе с ним на (2,2), направление остаётся
			TestEqual("push", UseHeroAbility(FIntPoint(2, 2)), EBattleActionResult::Success);
			TestEqual("the monster is shifted", Monster->GridPosition, FIntPoint(3, 2));
			TestEqual("the direction stays", Monster->Intent.Direction, EGridDirection::Left);
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 1);
			if (Battle->GetIntentViews().Num() == 1)
			{
				TestEqual("the shifted tile", Battle->GetIntentViews()[0].Cast.SelectedTile, FIntPoint(2, 2));
			}

			TestEqual("MoveHero", MoveHero(FIntPoint(2, 2)), EBattleActionResult::Success);
			Battle->EndPlayerTurn();

			TestEqual("the hero on the shifted tile is hurt", Hero->GetHealth(), 3);
		});

		It("recalculates the target of a projectile after the hero moves, and stops at a new obstacle", [this]()
		{
			if (!StartBattle(5)) return;
			UAbilityDefinition* Arrow = TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }, 1, 8);
			Arrow->Targeting.Shape = ETargetingShape::Cross;
			Arrow->Targeting.Trajectory = ETrajectory::Projectile;
			AMonsterUnit* Archer = TestWorld.SpawnMonster(FIntPoint(6, 2), 3, Arrow);
			Battle->EndPlayerTurn();

			TestTrue("HasIntent", Archer->HasIntent());
			TestEqual("the intent keeps the nearest cell", Archer->Intent.SelectedOffset, FIntPoint(-1, 0));
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 1);
			if (Battle->GetIntentViews().Num() == 1)
			{
				TestEqual("the arrow flies to the hero", Battle->GetIntentViews()[0].Cast.TargetPoint, FIntPoint(1, 2));
			}

			// Герой ушёл с линии: снаряд долетает до края поля
			TestEqual("MoveHero", MoveHero(FIntPoint(1, 3)), EBattleActionResult::Success);
			if (Battle->GetIntentViews().Num() == 1)
			{
				TestEqual("the arrow flies to the last cell", Battle->GetIntentViews()[0].Cast.TargetPoint, FIntPoint(0, 2));
			}

			// На линии появился другой монстр: снаряд остановится на нём
			AMonsterUnit* Blocker = TestWorld.SpawnMonster(FIntPoint(4, 2), 5);
			Battle->EndPlayerTurn();
			TestEqual("the blocker took the hit", Blocker->GetHealth(), 3);
			TestEqual("the hero is not hurt", Hero->GetHealth(), 5);
		});
	});

	Describe("Deaths", [this]()
	{
		It("a monster killed before the resolution does not realize its intent", [this]()
		{
			UAbilityDefinition* Deadly = Strike(99);
			if (!StartBattle(5, Deadly)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, Strike(2));
			Battle->EndPlayerTurn();
			TestTrue("HasIntent", Monster->HasIntent());
			TestEqual("IntentViews", Battle->GetIntentViews().Num(), 1);

			TestEqual("kill", UseHeroAbility(FIntPoint(2, 2)), EBattleActionResult::Success);
			TestFalse("the intent is cancelled", Monster->HasIntent());
			TestEqual("the view is gone", Battle->GetIntentViews().Num(), 0);
			TestEqual("the hero got no damage", Hero->GetHealth(), 5);

			Recorder->Reset();
			Battle->EndPlayerTurn();
			TestEqual("the dead monster used nothing", CountUses(Monster), 0);
			TestEqual("the hero is not hurt", Hero->GetHealth(), 5);
		});

		It("a monster killed by another monster's ability does not realize its intent and gives no experience", [this]()
		{
			if (!StartBattle(10)) return;
			// Первый монстр бьёт по ромбу вокруг героя и задевает второго, стоящего на краю области
			UAbilityDefinition* Blast = Strike(2);
			Blast->Area.Type = EAreaType::Diamond;
			Blast->Area.Size = 1;
			AMonsterUnit* First = TestWorld.SpawnMonster(FIntPoint(2, 2), 5, Blast);
			AMonsterUnit* Second = TestWorld.SpawnMonster(FIntPoint(1, 3), 1, Strike(3), 4);
			Battle->EndPlayerTurn();
			TestEqual("first order", First->Intent.Order, 1);
			TestEqual("second order", Second->Intent.Order, 2);

			Recorder->Reset();
			Battle->EndPlayerTurn();

			TestFalse("the second monster is dead", Second->IsAlive());
			TestEqual("the first monster used its ability", CountUses(First), 1);
			TestEqual("the second monster did not", CountUses(Second), 0);
			TestEqual("the hero took only the first hit", Hero->GetHealth(), 8);
			TestEqual("no experience", Hero->Experience, 0);
			TestEqual("OnUnitDied count", Recorder->DiedUnits.Num(), 1);
			if (Recorder->DiedUnits.Num() == 1)
			{
				TestTrue("Unit", Recorder->DiedUnits[0].Get() == Second);
				TestTrue("Killer", Recorder->Killers[0].Get() == First);
			}
		});

		It("a hero kill still gives experience in the same battle", [this]()
		{
			UAbilityDefinition* Deadly = Strike(99);
			if (!StartBattle(5, Deadly)) return;
			AMonsterUnit* Monster = TestWorld.SpawnMonster(FIntPoint(2, 2), 3, nullptr, 4);

			TestEqual("kill", UseHeroAbility(FIntPoint(2, 2)), EBattleActionResult::Success);
			TestFalse("dead", Monster->IsAlive());
			TestEqual("experience", Hero->Experience, 4);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
