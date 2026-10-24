#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Data/AbilityDefinition.h"
#include "Data/BattleRulesConfig.h"
#include "Data/MonsterDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Уровень подсистемы через открытые методы: UnitId, снимок боя, применение способности и смерть, конец боя.
// Закрытые методы (волны, намерения, перезарядки) проверяют спеки фаз: Waves, Intents, Cooldown, End
BEGIN_DEFINE_SPEC(FBattleSubsystemSpec, "Dungeon.Battle.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;

	// Подсистема создана, бой запущен (герои на (1,2), (2,1), (3,1), UnitId 0, 1, 2)
	bool StartBattle()
	{
		if (!Battle)
		{
			AddError("No subsystem");
			return false;
		}
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Recorder->Reset();
		return true;
	}

	ADungeonUnit* SpawnHero(FIntPoint Cell, UAbilityDefinition* Ability = nullptr, int32 Health = 3)
	{
		return TestWorld.SpawnHero(Cell, Ability, Health);
	}

	AMonsterUnit* SpawnMonster(FIntPoint Cell, int32 Health, UAbilityDefinition* Ability = nullptr, int32 Reward = 0)
	{
		return TestWorld.SpawnMonster(Cell, Health, Ability, Reward);
	}

	// Применяет способность через ExecuteAbilityInternal; MakeCast считается на текущем снимке
	bool UseAbility(ADungeonUnit* Source, int32 AbilityIndex, FIntPoint Tile)
	{
		FAbilityCast Cast;
		if (!FAbilityRules::MakeCast(Battle->MakeSimState(), Source->UnitId, Source->GetAbilities()[AbilityIndex].Ability, Tile, Cast))
		{
			AddError("MakeCast failed");
			return false;
		}
		Battle->ExecuteAbilityInternal(Source, AbilityIndex, Cast);
		return true;
	}

	// Убивает всех героев боя ударом монстра (KillUnit закрыт). Монстр остаётся на (2,2)
	void KillAllHeroes()
	{
		UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, 1, 3);
		AMonsterUnit* Killer = SpawnMonster(FIntPoint(2, 2), 1, Deadly);
		for (AHeroUnit* Hero : Battle->GetHeroes())
		{
			UseAbility(Killer, 0, Hero->GridPosition);
		}
	}

	// Герой-снайпер на (0,7) убивает всех живых монстров ударом через ExecuteAbilityInternal (KillUnit закрыт)
	void KillAllMonsters()
	{
		UAbilityDefinition* Deadly = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, 1, 14);
		ADungeonUnit* Sniper = TestWorld.SpawnHero(FIntPoint(0, 7), Deadly);
		for (AMonsterUnit* Monster : Battle->GetLivingMonsters())
		{
			UseAbility(Sniper, 0, Monster->GridPosition);
		}
	}

	static bool HasUnit(const TArray<TWeakObjectPtr<ADungeonUnit>>& List, const ADungeonUnit* Unit)
	{
		return List.ContainsByPredicate([Unit](const TWeakObjectPtr<ADungeonUnit>& Item) { return Item.Get() == Unit; });
	}
END_DEFINE_SPEC(FBattleSubsystemSpec)

void FBattleSubsystemSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Battle = nullptr;
		TestWorld.Destroy();
	});

	Describe("UnitId", [this]()
	{
		It("is given in order of appearance and stored in the grid model", [this]()
		{
			if (!StartBattle()) return;

			TestEqual("Units", Battle->GetUnits().Num(), 3);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				ADungeonUnit* Unit = Battle->GetUnits()[Index];
				TestEqual(FString::Printf(TEXT("UnitId of unit %d"), Index), Unit->UnitId, Index);
				const FGridOccupant* Occupant = TestWorld.GetGrid()->GetModel().GetOccupant(Unit->GridPosition);
				TestNotNull("occupant", Occupant);
				if (Occupant)
				{
					TestEqual("FGridOccupant::UnitId", Occupant->UnitId, Index);
				}
			}

			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 5), 1);
			TestNotNull("monster", Monster);
			if (Monster)
			{
				TestEqual("UnitId of the next unit", Monster->UnitId, 3);
				TestTrue("FindUnitById", Battle->FindUnitById(3) == Monster);
			}
			TestNull("FindUnitById of an unknown id", Battle->FindUnitById(99));
		});

		It("is not spent by a failed spawn", [this]()
		{
			if (!StartBattle()) return;

			TestNull("occupied cell", SpawnHero(FIntPoint(1, 2)));
			ADungeonUnit* Unit = SpawnHero(FIntPoint(6, 6));
			TestNotNull("unit", Unit);
			if (Unit)
			{
				TestEqual("UnitId", Unit->UnitId, 3);
			}
		});

		It("starts from 0 again after a new StartBattle", [this]()
		{
			if (!StartBattle()) return;
			SpawnMonster(FIntPoint(5, 5), 1);

			if (!StartBattle()) return;
			TestEqual("Units", Battle->GetUnits().Num(), 3);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual("UnitId", Battle->GetUnits()[Index]->UnitId, Index);
			}
		});
	});

	Describe("MakeSimState", [this]()
	{
		It("contains the living units with team, position and health", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 5), 2);

			const FBattleSimState Sim = Battle->MakeSimState();
			TestEqual("Units", Sim.Units.Num(), 4);
			for (const ADungeonUnit* Unit : Battle->GetUnits())
			{
				const FSimUnit* SimUnit = Sim.FindUnit(Unit->UnitId);
				TestNotNull("sim unit", SimUnit);
				if (SimUnit)
				{
					TestEqual("Team", SimUnit->Team, Unit->Team);
					TestEqual("Position", SimUnit->Position, Unit->GridPosition);
					TestEqual("Health", SimUnit->Health, Unit->GetHealth());
					TestEqual("MaxHealth", SimUnit->MaxHealth, Unit->GetMaxHealth());
					TestTrue("bAlive", SimUnit->bAlive);
				}
			}
			TestEqual("the monster has 2 health", Sim.FindUnit(Monster->UnitId)->Health, 2);
			TestEqual("the model holds the occupant", Sim.Grid.GetOccupant(FIntPoint(5, 5))->UnitId, Monster->UnitId);
		});

		It("does not contain a dead unit", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Kill = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			ADungeonUnit* Hero = SpawnHero(FIntPoint(5, 5), Kill);
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 1);
			TestEqual("before", Battle->MakeSimState().Units.Num(), 5);

			UseAbility(Hero, 0, FIntPoint(5, 6));

			const FBattleSimState Sim = Battle->MakeSimState();
			TestEqual("Units", Sim.Units.Num(), 4);
			TestNull("the dead monster", Sim.FindUnit(Monster->UnitId));
			TestFalse("its cell is free in the model", Sim.Grid.IsOccupied(FIntPoint(5, 6)));
		});

		It("takes CollisionDamage from the default rules without Config->Rules", [this]()
		{
			if (!StartBattle()) return;
			const int32 DefaultDamage = GetDefault<UBattleRulesConfig>()->CollisionDamage;

			TestTrue("GetRules is the default object", &Battle->GetRules() == GetDefault<UBattleRulesConfig>());
			TestEqual("CollisionDamage", Battle->MakeSimState().CollisionDamage, DefaultDamage);
		});

		It("takes CollisionDamage from Config->Rules", [this]()
		{
			UBattleRulesConfig* Rules = TestWorld.SetRules(3, 0.f);
			if (!StartBattle()) return;

			TestTrue("GetRules is Config->Rules", &Battle->GetRules() == Rules);
			TestEqual("CollisionDamage", Battle->MakeSimState().CollisionDamage, 3);
		});

		It("is empty before StartBattle", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			const FBattleSimState Sim = Battle->MakeSimState();
			TestEqual("Units", Sim.Units.Num(), 0);
			TestEqual("CollisionDamage", Sim.CollisionDamage, GetDefault<UBattleRulesConfig>()->CollisionDamage);
		});
	});

	Describe("StartBattle again", [this]()
	{
		It("destroys the units of the previous battle and frees their cells", [this]()
		{
			if (!StartBattle()) return;
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 5), 1);
			TArray<TWeakObjectPtr<ADungeonUnit>> OldUnits;
			for (ADungeonUnit* Unit : Battle->GetUnits())
			{
				OldUnits.Add(Unit);
			}
			TestEqual("units of the first battle", OldUnits.Num(), 4);
			TestNotNull("monster", Monster);

			if (!StartBattle()) return;

			for (const TWeakObjectPtr<ADungeonUnit>& OldUnit : OldUnits)
			{
				TestFalse("old unit is destroyed", OldUnit.IsValid());
			}
			TestEqual("Units", Battle->GetUnits().Num(), 3);
			TestEqual("unit actors in the world", TestWorld.CountUnitActors(), 3);
			TestEqual("occupied cells", TestWorld.GetGrid()->GetModel().Occupants.Num(), 3);
			TestFalse("the monster's cell is free", TestWorld.GetGrid()->GetModel().IsOccupied(FIntPoint(5, 5)));
		});

		It("resets the state of a finished battle", [this]()
		{
			TestWorld.SetWaves({}, 0);
			if (!StartBattle()) return;
			TestTrue("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);

			TestWorld.SetWaves({}, 100);
			Recorder->Reset();
			Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestFalse("IsBattleOver", Battle->IsBattleOver());
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
			TestEqual("turn", Battle->GetTurnNumber(), 1);
			TestEqual("TotalWaves", Battle->GetTotalWaves(), 100);
			TestEqual("OnBattleStarted", Recorder->BattleStartedCount, 1);
		});
	});

	Describe("Reading the state", [this]()
	{
		It("GetHeroes includes dead heroes, GetLivingMonsters and HasLivingHeroes skip dead units", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Kill = TestWorld.MakeAbility({ AbilityTest::MakeDamage(9) });
			AMonsterUnit* Killer = SpawnMonster(FIntPoint(5, 5), 1, Kill);
			AMonsterUnit* First = SpawnMonster(FIntPoint(5, 6), 1);
			AMonsterUnit* Second = SpawnMonster(FIntPoint(6, 5), 1);
			TestEqual("GetLivingMonsters", Battle->GetLivingMonsters().Num(), 3);
			if (Battle->GetLivingMonsters().Num() == 3)
			{
				TestTrue("in order of appearance", Battle->GetLivingMonsters()[0] == Killer
					&& Battle->GetLivingMonsters()[1] == First && Battle->GetLivingMonsters()[2] == Second);
			}

			UseAbility(Killer, 0, FIntPoint(5, 6));
			TestEqual("GetLivingMonsters", Battle->GetLivingMonsters().Num(), 2);
			TestEqual("GetHeroes", Battle->GetHeroes().Num(), 3);

			TestTrue("HasLivingHeroes", Battle->HasLivingHeroes());
			KillAllHeroes();
			TestFalse("HasLivingHeroes without heroes", Battle->HasLivingHeroes());
			TestEqual("GetHeroes still returns all", Battle->GetHeroes().Num(), 3);
			for (const AHeroUnit* DeadHero : Battle->GetHeroes())
			{
				TestFalse("dead", DeadHero->IsAlive());
			}
		});
	});

	Describe("GetSelectableTiles and PreviewAbility", [this]()
	{
		It("list the cells of the ability and do not change the battle", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) });
			ADungeonUnit* Hero = SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 3);

			TArray<FIntPoint> Tiles;
			TestTrue("GetSelectableTiles", Battle->GetSelectableTiles(Hero, 0, Tiles));
			TestTrue("cells at distance 1", AbilityTest::SameTiles(Tiles,
				{ FIntPoint(5, 4), FIntPoint(5, 6), FIntPoint(4, 5), FIntPoint(6, 5) }));
			TestFalse("an index outside the list", Battle->GetSelectableTiles(Hero, 1, Tiles));
			TestEqual("Out is empty", Tiles.Num(), 0);
			TestFalse("no unit", Battle->GetSelectableTiles(nullptr, 0, Tiles));

			FAbilityOutcome Outcome;
			TestTrue("PreviewAbility", Battle->PreviewAbility(Hero, 0, FIntPoint(5, 6), Outcome));
			TestEqual("damage in the preview", Outcome.GetDamageTo(Monster->UnitId), 2);
			TestEqual("the monster is untouched", Monster->GetHealth(), 3);
			TestFalse("the hero has not acted", Hero->TurnState.bHasActed);
			TestEqual("no cooldown", Hero->GetAbilities()[0].RemainingCooldown, 0);
			TestEqual("no events sent", Recorder->ChangedUnits.Num(), 0);

			FAbilityOutcome Far;
			TestFalse("a cell out of range", Battle->PreviewAbility(Hero, 0, FIntPoint(5, 7), Far));
			TestEqual("Out is empty", Far.Events.Num(), 0);
		});
	});

	Describe("ExecuteAbilityInternal", [this]()
	{
		It("applies the preview result, starts the cooldown and marks the hero", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(2) }, 1, 1, 2);
			ADungeonUnit* Hero = SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 3);
			Recorder->Reset();

			FAbilityOutcome Preview;
			TestTrue("PreviewAbility", Battle->PreviewAbility(Hero, 0, FIntPoint(5, 6), Preview));
			TestTrue("UseAbility", UseAbility(Hero, 0, FIntPoint(5, 6)));

			TestEqual("health as in the preview", Monster->GetHealth(), 3 - Preview.GetDamageTo(Monster->UnitId));
			TestEqual("health", Monster->GetHealth(), 1);
			TestTrue("bHasActed", Hero->TurnState.bHasActed);
			TestFalse("bHasMoved", Hero->TurnState.bHasMoved);
			TestEqual("RemainingCooldown", Hero->GetAbilities()[0].RemainingCooldown, 2);
			TestEqual("UsedOnTurn", Hero->GetAbilities()[0].UsedOnTurn, Battle->GetTurnNumber());

			TestEqual("OnAbilityUsed count", Recorder->AbilitySources.Num(), 1);
			if (Recorder->AbilitySources.Num() == 1)
			{
				TestTrue("Source", Recorder->AbilitySources[0].Get() == Hero);
				TestEqual("AbilityIndex", Recorder->AbilityIndices[0], 0);
				TestEqual("TargetPoint", Recorder->AbilityTargetPoints[0], FIntPoint(5, 6));
			}
			TestTrue("OnUnitChanged for the monster", HasUnit(Recorder->ChangedUnits, Monster));
			TestTrue("OnUnitChanged for the hero", HasUnit(Recorder->ChangedUnits, Hero));
			TestEqual("OnIntentsChanged", Recorder->IntentsChangedCount, 1);
			TestEqual("no death", Recorder->DiedUnits.Num(), 0);
		});

		It("moves units through the grid model and reports OnUnitMoved", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakePush(EPushDirection::AbilityDirection, 1) });
			ADungeonUnit* Hero = SpawnHero(FIntPoint(5, 5), Ability);
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 3);
			Recorder->Reset();

			UseAbility(Hero, 0, FIntPoint(5, 6));

			TestEqual("GridPosition", Monster->GridPosition, FIntPoint(5, 7));
			TestFalse("the old cell is free", TestWorld.GetGrid()->GetModel().IsOccupied(FIntPoint(5, 6)));
			TestEqual("the new cell", TestWorld.GetGrid()->GetModel().GetOccupant(FIntPoint(5, 7))->UnitId, Monster->UnitId);
			TestEqual("OnUnitMoved count", Recorder->UnitMovedCount, 1);
			if (Recorder->UnitMovedCount == 1)
			{
				TestTrue("Unit", Recorder->MovedUnits[0].Get() == Monster);
				TestEqual("From", Recorder->MovedFrom[0], FIntPoint(5, 6));
				TestEqual("To", Recorder->MovedTo[0], FIntPoint(5, 7));
			}
			TestFalse("a push does not mark the target as moved", Monster->TurnState.bHasMoved);
		});

		It("kills a unit: removes it from the field and gives the killing hero the experience", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			AHeroUnit* Hero = Cast<AHeroUnit>(SpawnHero(FIntPoint(5, 5), Ability));
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 1, nullptr, 4);
			Recorder->Reset();

			UseAbility(Hero, 0, FIntPoint(5, 6));

			TestEqual("Experience", Hero->Experience, 4);
			TestFalse("the monster is dead", Monster->IsAlive());
			TestTrue("hidden", Monster->IsHidden());
			TestTrue("the actor stays in Units", Battle->GetUnits().Contains(Monster));
			TestFalse("its cell is free", TestWorld.GetGrid()->GetModel().IsOccupied(FIntPoint(5, 6)));
			TestEqual("OnUnitDied count", Recorder->DiedUnits.Num(), 1);
			if (Recorder->DiedUnits.Num() == 1)
			{
				TestTrue("Unit", Recorder->DiedUnits[0].Get() == Monster);
				TestTrue("Killer", Recorder->Killers[0].Get() == Hero);
			}
			TestTrue("OnUnitChanged for the killer", HasUnit(Recorder->ChangedUnits, Hero));
			TestTrue("OnUnitChanged for the dead monster", HasUnit(Recorder->ChangedUnits, Monster));
		});

		It("gives the experience for a lethal collision damage", [this]()
		{
			TestWorld.BuildWalls({ FIntPoint(5, 7) });
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakePush(EPushDirection::AbilityDirection, 1) });
			AHeroUnit* Hero = Cast<AHeroUnit>(SpawnHero(FIntPoint(5, 5), Ability));
			AMonsterUnit* Monster = SpawnMonster(FIntPoint(5, 6), 1, nullptr, 2);

			UseAbility(Hero, 0, FIntPoint(5, 6));

			TestFalse("the monster died in the collision", Monster->IsAlive());
			TestEqual("Experience", Hero->Experience, 2);
		});

		It("gives no experience for a monster killed by another monster", [this]()
		{
			if (!StartBattle()) return;
			UAbilityDefinition* Ability = TestWorld.MakeAbility({ AbilityTest::MakeDamage(5) });
			AMonsterUnit* Killer = SpawnMonster(FIntPoint(5, 5), 1, Ability);
			AMonsterUnit* Victim = SpawnMonster(FIntPoint(5, 6), 1, nullptr, 3);
			Recorder->Reset();

			UseAbility(Killer, 0, FIntPoint(5, 6));

			TestFalse("the victim is dead", Victim->IsAlive());
			TestFalse("a monster does not get the hero flag", Killer->TurnState.bHasActed);
			for (const AHeroUnit* Hero : Battle->GetHeroes())
			{
				TestEqual("Experience", Hero->Experience, 0);
			}
			TestEqual("OnUnitDied count", Recorder->DiedUnits.Num(), 1);
			if (Recorder->DiedUnits.Num() == 1)
			{
				TestTrue("Killer", Recorder->Killers[0].Get() == Killer);
			}
			TestEqual("the cooldown starts", Killer->GetAbilities()[0].UsedOnTurn, Battle->GetTurnNumber());
		});

		It("ignores an unknown ability index", [this]()
		{
			if (!StartBattle()) return;
			ADungeonUnit* Hero = Battle->GetUnits()[0];
			AddExpectedError(TEXT("ExecuteAbilityInternal"), EAutomationExpectedErrorFlags::Contains, 1);

			Battle->ExecuteAbilityInternal(Hero, 0, FAbilityCast());
			TestFalse("bHasActed", Hero->TurnState.bHasActed);
			TestEqual("OnAbilityUsed count", Recorder->AbilitySources.Num(), 0);
		});
	});

	Describe("CheckBattleEnd", [this]()
	{
		// Ноль волн: победа наступает, как только на поле нет монстров. Конец боя проверяет только CheckBattleEnd,
		// ExecuteAbilityInternal и KillUnit его не вызывают

		It("does nothing before StartBattle", [this]()
		{
			if (!Battle)
			{
				AddError("No subsystem");
				return;
			}
			TestFalse("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 0);
		});

		It("does not end the battle while the waves remain", [this]()
		{
			if (!StartBattle()) return;
			TestFalse("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		});

		It("does not end the battle while a monster is alive", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle()) return;
			TestEqual("the first wave", Battle->GetLivingMonsters().Num(), 1);
			TestFalse("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
		});

		It("ends with Victory when the monsters of the last wave are dead, and reports it once", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle()) return;
			KillAllMonsters();
			TestEqual("the ability does not check the end", Battle->GetResult(), EBattleResult::None);
			Recorder->Reset();

			TestTrue("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
			TestEqual("OnBattleEnded count", Recorder->EndedResults.Num(), 1);

			TestTrue("CheckBattleEnd again", Battle->CheckBattleEnd());
			TestEqual("OnBattleEnded count stays", Recorder->EndedResults.Num(), 1);
		});

		It("ends with Defeat when no hero is alive", [this]()
		{
			if (!StartBattle()) return;
			KillAllHeroes();
			TestEqual("the ability does not check the end", Battle->GetResult(), EBattleResult::None);

			TestTrue("CheckBattleEnd", Battle->CheckBattleEnd());
			TestEqual("Result", Battle->GetResult(), EBattleResult::Defeat);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
		});

		It("is called by TryExecute after a successful action", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle()) return;
			KillAllMonsters();
			TestEqual("not checked yet", Battle->GetResult(), EBattleResult::None);

			UMoveAction* Action = NewObject<UMoveAction>();
			Action->Unit = Battle->GetHeroes()[0];
			Action->Target = FIntPoint(1, 3);
			TestEqual("TryExecute", Battle->TryExecute(Action), EBattleActionResult::Success);
			TestEqual("Result", Battle->GetResult(), EBattleResult::Victory);
			TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::Finished);
		});

		It("is not called when the action fails", [this]()
		{
			TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(1, 0) }, 1);
			if (!StartBattle()) return;
			KillAllMonsters();

			UMoveAction* Action = NewObject<UMoveAction>();
			Action->Unit = Battle->GetHeroes()[0];
			Action->Target = FIntPoint(7, 7);
			TestEqual("TryExecute", Battle->TryExecute(Action), EBattleActionResult::Unreachable);
			TestEqual("Result", Battle->GetResult(), EBattleResult::None);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
