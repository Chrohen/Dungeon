#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Battle/MoveAction.h"
#include "Battle/UseAbilityAction.h"
#include "Components/CapsuleComponent.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Kismet/GameplayStatics.h"
#include "Save/DungeonSaveGame.h"
#include "Save/DungeonSaveStorage.h"
#include "Tests/AbilityTestHelpers.h"
#include "Tests/BattleTestHelpers.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

BEGIN_DEFINE_SPEC(FBattleSaveSpec, "Dungeon.Battle.Save",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	UBattleSubsystem* Battle = nullptr;
	FString Slot;

	bool Start(UAbilityDefinition* HeroAbility = nullptr, int32 HeroHealth = 100)
	{
		if (!TestNotNull(TEXT("battle subsystem"), Battle)) return false;
		TArray<UAbilityDefinition*> Abilities;
		if (HeroAbility != nullptr) Abilities.Add(HeroAbility);
		TestWorld.SetSingleHero(TestWorld.MakeHeroDefinition(HeroHealth, Abilities), FIntPoint(1, 2));
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		return TestEqual(TEXT("one hero"), Battle->GetHeroes().Num(), 1);
	}

	EBattleActionResult Move(AHeroUnit* Hero, FIntPoint Target)
	{
		UMoveAction* Action = NewObject<UMoveAction>();
		Action->Unit = Hero;
		Action->Target = Target;
		return Battle->TryExecute(Action);
	}

	EBattleActionResult Use(AHeroUnit* Hero, FIntPoint Target)
	{
		UUseAbilityAction* Action = NewObject<UUseAbilityAction>();
		Action->Unit = Hero;
		Action->AbilityIndex = 0;
		Action->SelectedTile = Target;
		return Battle->TryExecute(Action);
	}

	bool Capture(UDungeonSaveGame& Save)
	{
		FString Error;
		const bool bSuccess = Battle->CaptureSave(Save, Error);
		return TestTrue(FString::Printf(TEXT("capture succeeds: %s"), *Error), bSuccess);
	}

	bool Restore(const UDungeonSaveGame& Save)
	{
		FString Error;
		const bool bSuccess = Battle->RestoreSave(Save, TestWorld.GetGrid(), Error);
		return TestTrue(FString::Printf(TEXT("restore succeeds: %s"), *Error), bSuccess);
	}

	UDungeonSaveGame* LoadSlot()
	{
		FString Error;
		UDungeonSaveGame* Save = FDungeonSaveStorage::Load(Slot, Error);
		TestNotNull(FString::Printf(TEXT("load succeeds: %s"), *Error), Save);
		return Save;
	}

	void Compare(const UDungeonSaveGame& Expected, const UDungeonSaveGame& Actual)
	{
		TestEqual(TEXT("phase"), Actual.Phase, Expected.Phase);
		TestEqual(TEXT("result"), Actual.Result, Expected.Result);
		TestEqual(TEXT("turn"), Actual.TurnNumber, Expected.TurnNumber);
		TestEqual(TEXT("wave"), Actual.CurrentWave, Expected.CurrentWave);
		TestEqual(TEXT("total waves"), Actual.TotalWaves, Expected.TotalWaves);
		TestEqual(TEXT("next unit ID"), Actual.NextUnitId, Expected.NextUnitId);
		TestEqual(TEXT("initial RNG seed"), Actual.Random.GetInitialSeed(), Expected.Random.GetInitialSeed());
		TestEqual(TEXT("current RNG seed"), Actual.Random.GetCurrentSeed(), Expected.Random.GetCurrentSeed());
		TestTrue(TEXT("battlefield asset"), Actual.Battlefield == Expected.Battlefield);
		TestEqual(TEXT("unit count"), Actual.Units.Num(), Expected.Units.Num());
		for (int32 Index = 0; Index < FMath::Min(Expected.Units.Num(), Actual.Units.Num()); ++Index)
		{
			const FDungeonUnitSaveState& A = Actual.Units[Index];
			const FDungeonUnitSaveState& E = Expected.Units[Index];
			const FString Prefix = FString::Printf(TEXT("unit %d: "), Index);
			TestEqual(Prefix + TEXT("ID and order"), A.UnitId, E.UnitId);
			TestTrue(Prefix + TEXT("definition"), A.Definition == E.Definition);
			TestEqual(Prefix + TEXT("team"), A.Team, E.Team);
			TestEqual(Prefix + TEXT("position"), A.GridPosition, E.GridPosition);
			TestEqual(Prefix + TEXT("health"), A.Health, E.Health);
			TestEqual(Prefix + TEXT("alive"), A.bAlive, E.bAlive);
			TestEqual(Prefix + TEXT("level"), A.Level, E.Level);
			TestEqual(Prefix + TEXT("moved"), A.TurnState.bHasMoved, E.TurnState.bHasMoved);
			TestEqual(Prefix + TEXT("acted"), A.TurnState.bHasActed, E.TurnState.bHasActed);
			TestEqual(Prefix + TEXT("name"), A.HeroName.ToString(), E.HeroName.ToString());
			TestEqual(Prefix + TEXT("experience"), A.Experience, E.Experience);
			TestEqual(Prefix + TEXT("unspent points"), A.UnspentPoints, E.UnspentPoints);
			TestTrue(Prefix + TEXT("intent"), FMonsterIntent::StaticStruct()->CompareScriptStruct(&A.Intent, &E.Intent, 0));
			TestEqual(Prefix + TEXT("ability count"), A.Abilities.Num(), E.Abilities.Num());
			for (int32 AbilityIndex = 0; AbilityIndex < FMath::Min(A.Abilities.Num(), E.Abilities.Num()); ++AbilityIndex)
			{
				TestTrue(Prefix + FString::Printf(TEXT("ability %d parameters and cooldown"), AbilityIndex),
					FUnitAbilityState::StaticStruct()->CompareScriptStruct(&A.Abilities[AbilityIndex], &E.Abilities[AbilityIndex], 0));
			}
		}
	}
END_DEFINE_SPEC(FBattleSaveSpec)

void FBattleSaveSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		TestWorld.SetRules(1, 0.f);
		TestWorld.SetSeed(94731);
		Battle = TestWorld.GetBattle();
		Slot = TEXT("DungeonSaveSpec_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	});

	AfterEach([this]()
	{
		if (Battle != nullptr) Battle->ConfigureAutoSave(Slot, false);
		// Delete only the two slots that belong to this test.
		UGameplayStatics::DeleteGameInSlot(Slot + TEXT("_A"), 0);
		UGameplayStatics::DeleteGameInSlot(Slot + TEXT("_B"), 0);
		Battle = nullptr;
		TestWorld.Destroy();
	});

	It("restores action flags, abilities and a dead unit whose cell has a new occupant", [this]()
	{
		UAbilityDefinition* Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(3) }, 1, 14, 2);
		if (!Start(Strike)) return;
		AHeroUnit* Hero = Battle->GetHeroes()[0];
		AMonsterUnit* Dead = TestWorld.SpawnMonster(FIntPoint(2, 2), 1, nullptr, 7);
		if (!TestNotNull(TEXT("victim"), Dead)) return;
		const int32 DeadId = Dead->UnitId;
		TestEqual(TEXT("kill succeeds"), Use(Hero, Dead->GridPosition), EBattleActionResult::Success);
		AMonsterUnit* Living = TestWorld.SpawnMonster(FIntPoint(2, 2), 10, Strike);
		if (!TestNotNull(TEXT("new occupant"), Living)) return;
		const int32 LivingId = Living->UnitId;
		FMonsterIntent Intent;
		Intent.bActive = true;
		Intent.AbilityIndex = 0;
		Intent.SelectedOffset = FIntPoint(-1, 0);
		Intent.Order = 4;
		Living->SetIntent(Intent);
		Hero->SetHeroName(FText::FromString(TEXT("Mira")));
		Hero->SetHealth(61);
		Hero->Level = 2;
		Hero->UnspentPoints = 3;
		Hero->Abilities[0].Ability.Targeting.MaxRange = 12;
		TestEqual(TEXT("move succeeds"), Move(Hero, FIntPoint(1, 3)), EBattleActionResult::Success);

		TStrongObjectPtr<UDungeonSaveGame> Before(NewObject<UDungeonSaveGame>());
		if (!Capture(*Before)) return;
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("serialize save"), UGameplayStatics::SaveGameToMemory(Before.Get(), Bytes))) return;
		TStrongObjectPtr<UDungeonSaveGame> Loaded(Cast<UDungeonSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
		if (!TestNotNull(TEXT("deserialize save"), Loaded.Get())) return;

		Battle->EndPlayerTurn();
		TStrongObjectPtr<UBattleEventRecorder> Recorder(NewObject<UBattleEventRecorder>());
		Recorder->Subscribe(*Battle);
		if (!Restore(*Loaded)) return;
		TStrongObjectPtr<UDungeonSaveGame> After(NewObject<UDungeonSaveGame>());
		if (!Capture(*After)) return;
		Compare(*Before, *After);
		TestEqual(TEXT("restore does not replay moves"), Recorder->UnitMovedCount, 0);
		TestEqual(TEXT("restore does not replay abilities"), Recorder->AbilitySources.Num(), 0);
		TestEqual(TEXT("restore does not replay deaths"), Recorder->DiedUnits.Num(), 0);
		ADungeonUnit* RestoredDead = Battle->FindUnitById(DeadId);
		if (!TestNotNull(TEXT("dead unit remains in roster"), RestoredDead)) return;
		TestFalse(TEXT("dead unit stays dead"), RestoredDead->IsAlive());
		TestTrue(TEXT("dead actor is hidden"), RestoredDead->IsHidden());
		TestEqual(TEXT("dead collision is off"), RestoredDead->Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		const FGridOccupant* Occupant = TestWorld.GetGrid()->GetModel().GetOccupant(FIntPoint(2, 2));
		if (TestNotNull(TEXT("cell has a living occupant"), Occupant))
		{
			TestEqual(TEXT("cell belongs to the living unit"), Occupant->UnitId, LivingId);
			TestTrue(TEXT("cell points to the new actor"), Occupant->Unit.Get() == Battle->FindUnitById(LivingId));
		}
	});

	It("continues the same RNG, waves, intents and cooldowns after restore", [this]()
	{
		UAbilityDefinition* HeroStrike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 14, 2);
		UAbilityDefinition* MonsterStrike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 14, 1);
		TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(30, 2, { MonsterStrike }) }, 3);
		if (!Start(HeroStrike, 200)) return;
		if (!TestEqual(TEXT("first wave has one monster"), Battle->GetLivingMonsters().Num(), 1)) return;
		AHeroUnit* Hero = Battle->GetHeroes()[0];
		const int32 SavedHeroUnitId = Hero->UnitId;
		TestEqual(TEXT("hero uses ability"), Use(Hero, Battle->GetLivingMonsters()[0]->GridPosition), EBattleActionResult::Success);
		TStrongObjectPtr<UDungeonSaveGame> Checkpoint(NewObject<UDungeonSaveGame>());
		if (!Capture(*Checkpoint)) return;
		for (int32 Index = 0; Index < 3; ++Index) Battle->EndPlayerTurn();
		TStrongObjectPtr<UDungeonSaveGame> Expected(NewObject<UDungeonSaveGame>());
		if (!Capture(*Expected) || !Restore(*Checkpoint)) return;
		for (int32 Index = 0; Index < 3; ++Index) Battle->EndPlayerTurn();
		TStrongObjectPtr<UDungeonSaveGame> Actual(NewObject<UDungeonSaveGame>());
		if (!Capture(*Actual)) return;
		Compare(*Expected, *Actual);
		TestEqual(TEXT("all three waves appeared"), Actual->CurrentWave, 3);
		TestEqual(TEXT("cooldown 2 is ready on turn 4"), Actual->TurnNumber, 4);
		ADungeonUnit* RestoredHero = Battle->FindUnitById(SavedHeroUnitId);
		if (TestNotNull(TEXT("hero keeps the same ID"), RestoredHero))
		{
			TestEqual(TEXT("cooldown is zero"), RestoredHero->Abilities[0].RemainingCooldown, 0);
			TestEqual(TEXT("original use turn is kept"), RestoredHero->Abilities[0].UsedOnTurn, 1);
		}
		FIntPoint FreeCell(INDEX_NONE, INDEX_NONE);
		const FGridModel& Model = TestWorld.GetGrid()->GetModel();
		for (int32 Y = 0; Y < Model.Height && FreeCell.X == INDEX_NONE; ++Y)
		{
			for (int32 X = 0; X < Model.Width; ++X)
			{
				const FIntPoint Cell(X, Y);
				if (Model.IsPassable(Cell) && !Model.IsOccupied(Cell))
				{
					FreeCell = Cell;
					break;
				}
			}
		}
		ADungeonUnit* NewUnit = TestWorld.SpawnHero(FreeCell);
		if (TestNotNull(TEXT("unit can spawn after restore"), NewUnit))
		{
			TestEqual(TEXT("next ID does not repeat"), NewUnit->UnitId, Actual->NextUnitId);
		}
	});

	It("rejects invalid saves without changing the current battle", [this]()
	{
		if (!Start()) return;
		if (!TestNotNull(TEXT("second hero"), TestWorld.SpawnHero(FIntPoint(2, 2)))) return;
		TStrongObjectPtr<UDungeonSaveGame> Before(NewObject<UDungeonSaveGame>());
		if (!Capture(*Before)) return;
		ADungeonUnit* OriginalActor = Battle->GetUnits()[0];
		const int32 OriginalCount = TestWorld.CountUnitActors();
		for (int32 Case = 0; Case < 4; ++Case)
		{
			TStrongObjectPtr<UDungeonSaveGame> Invalid(DuplicateObject<UDungeonSaveGame>(Before.Get(), GetTransientPackage()));
			switch (Case)
			{
			case 0: Invalid->Version = UDungeonSaveGame::CurrentVersion + 1; break;
			case 1: Invalid->Units[1].UnitId = Invalid->Units[0].UnitId; break;
			case 2: Invalid->Units[1].GridPosition = Invalid->Units[0].GridPosition; break;
			case 3: Invalid->Units[0].Definition.Reset(); break;
			}
			FString Error;
			TestFalse(TEXT("invalid snapshot is rejected"), Battle->RestoreSave(*Invalid, TestWorld.GetGrid(), Error));
			TestFalse(TEXT("failure has a reason"), Error.IsEmpty());
			TestTrue(TEXT("old actor is retained"), Battle->GetUnits()[0] == OriginalActor);
			TestEqual(TEXT("no staged actors remain"), TestWorld.CountUnitActors(), OriginalCount);
			TStrongObjectPtr<UDungeonSaveGame> After(NewObject<UDungeonSaveGame>());
			if (!Capture(*After)) return;
			Compare(*Before, *After);
		}
	});

	It("restores a finished victory without another wave or repeated experience", [this]()
	{
		UAbilityDefinition* Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, 1, 14);
		UMonsterDefinition* Monster = TestWorld.MakeMonsterDefinition(1, 0);
		Monster->ExperienceReward = 9;
		TestWorld.SetWaves({ Monster }, 1);
		Battle->ConfigureAutoSave(Slot, true);
		if (!Start(Strike)) return;
		TestEqual(TEXT("winning ability"), Use(Battle->GetHeroes()[0], Battle->GetLivingMonsters()[0]->GridPosition), EBattleActionResult::Success);
		TStrongObjectPtr<UDungeonSaveGame> Finished(NewObject<UDungeonSaveGame>());
		if (!Capture(*Finished)) return;
		TestEqual(TEXT("victory is saved"), Finished->Result, EBattleResult::Victory);
		TStrongObjectPtr<UDungeonSaveGame> AutoSaved(LoadSlot());
		if (!AutoSaved.IsValid()) return;
		Compare(*Finished, *AutoSaved);
		Battle->ConfigureAutoSave(Slot, false);
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		if (!Restore(*Finished)) return;
		Battle->EndPlayerTurn();
		TestWorld.AdvanceTime(2.f);
		TStrongObjectPtr<UDungeonSaveGame> After(NewObject<UDungeonSaveGame>());
		if (!Capture(*After)) return;
		Compare(*Finished, *After);
		TestEqual(TEXT("finished phase stays"), Battle->GetCurrentPhase(), EBattlePhase::Finished);
		TestEqual(TEXT("reward is granted once"), Battle->GetHeroes()[0]->Experience, 9);
	});

	It("restores a defeat with a dead hero and no occupied hero cell", [this]()
	{
		UAbilityDefinition* Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(99) }, 1, 14);
		TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(10, 0, { Strike }) }, 1);
		Battle->ConfigureAutoSave(Slot, true);
		if (!Start(nullptr, 1)) return;
		Battle->EndPlayerTurn();
		TStrongObjectPtr<UDungeonSaveGame> Finished(NewObject<UDungeonSaveGame>());
		if (!Capture(*Finished)) return;
		TestEqual(TEXT("defeat is saved"), Finished->Result, EBattleResult::Defeat);
		TStrongObjectPtr<UDungeonSaveGame> AutoSaved(LoadSlot());
		if (!AutoSaved.IsValid()) return;
		Compare(*Finished, *AutoSaved);
		Battle->ConfigureAutoSave(Slot, false);
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		if (!Restore(*Finished)) return;
		TStrongObjectPtr<UDungeonSaveGame> After(NewObject<UDungeonSaveGame>());
		if (!Capture(*After)) return;
		Compare(*Finished, *After);
		TestFalse(TEXT("hero remains dead"), Battle->GetHeroes()[0]->IsAlive());
		TestFalse(TEXT("dead hero does not occupy the grid"), TestWorld.GetGrid()->GetModel().IsOccupied(FIntPoint(1, 2)));
	});

	It("autosaves successful player actions but does not save rejected actions", [this]()
	{
		UAbilityDefinition* Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 14, 2);
		Battle->ConfigureAutoSave(Slot, true);
		if (!Start(Strike)) return;
		TStrongObjectPtr<UDungeonSaveGame> Initial(LoadSlot());
		if (!Initial.IsValid()) return;
		AHeroUnit* Hero = Battle->GetHeroes()[0];
		TestEqual(TEXT("valid move"), Move(Hero, FIntPoint(1, 3)), EBattleActionResult::Success);
		TStrongObjectPtr<UDungeonSaveGame> Moved(LoadSlot());
		if (!Moved.IsValid()) return;
		TestTrue(TEXT("move writes a new generation"), Moved->Sequence > Initial->Sequence);
		TestTrue(TEXT("saved move flag"), Moved->Units[0].TurnState.bHasMoved);
		TestEqual(TEXT("repeat move is rejected"), Move(Hero, FIntPoint(1, 4)), EBattleActionResult::AlreadyMoved);
		TStrongObjectPtr<UDungeonSaveGame> Rejected(LoadSlot());
		if (!Rejected.IsValid()) return;
		TestEqual(TEXT("rejection does not write"), Rejected->Sequence, Moved->Sequence);
		TestEqual(TEXT("valid ability"), Use(Hero, FIntPoint(2, 3)), EBattleActionResult::Success);
		TStrongObjectPtr<UDungeonSaveGame> Acted(LoadSlot());
		if (!Acted.IsValid()) return;
		TestTrue(TEXT("ability writes a new generation"), Acted->Sequence > Moved->Sequence);
		TestTrue(TEXT("saved action flag"), Acted->Units[0].TurnState.bHasActed);
		TestEqual(TEXT("saved cooldown"), Acted->Units[0].Abilities[0].RemainingCooldown, 2);
		TestEqual(TEXT("repeat ability is rejected"), Use(Hero, FIntPoint(2, 3)), EBattleActionResult::AlreadyActed);
		TStrongObjectPtr<UDungeonSaveGame> RejectedAbility(LoadSlot());
		if (RejectedAbility.IsValid()) TestEqual(TEXT("rejected ability does not write"), RejectedAbility->Sequence, Acted->Sequence);
	});

	It("waits for the full delayed enemy turn before autosave", [this]()
	{
		UAbilityDefinition* Strike = TestWorld.MakeAbility({ AbilityTest::MakeDamage(1) }, 1, 14);
		TestWorld.SetRules(1, 0.5f);
		TestWorld.SetWaves({ TestWorld.MakeMonsterDefinition(10, 0, { Strike }) }, 2);
		Battle->ConfigureAutoSave(Slot, true);
		if (!Start()) return;
		TStrongObjectPtr<UDungeonSaveGame> Initial(LoadSlot());
		if (!Initial.IsValid()) return;
		Battle->EndPlayerTurn();
		TestEqual(TEXT("enemy action has a timer"), Battle->GetCurrentPhase(), EBattlePhase::ResolveIntents);
		TStrongObjectPtr<UDungeonSaveGame> During(LoadSlot());
		if (!During.IsValid()) return;
		TestEqual(TEXT("partial enemy turn is not saved"), During->Sequence, Initial->Sequence);
		TStrongObjectPtr<UDungeonSaveGame> Partial(NewObject<UDungeonSaveGame>());
		FString Error;
		TestFalse(TEXT("capture rejects a partial enemy turn"), Battle->CaptureSave(*Partial, Error));
		TestWorld.AdvanceTime(3.f);
		TestEqual(TEXT("control returns to the player"), Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		TStrongObjectPtr<UDungeonSaveGame> Complete(LoadSlot());
		if (!Complete.IsValid()) return;
		TestTrue(TEXT("enemy turn writes a new generation"), Complete->Sequence > Initial->Sequence);
		TestEqual(TEXT("next player turn is saved"), Complete->TurnNumber, 2);
		TestEqual(TEXT("next wave is saved"), Complete->CurrentWave, 2);
		TStrongObjectPtr<UDungeonSaveGame> Live(NewObject<UDungeonSaveGame>());
		if (Capture(*Live)) Compare(*Live, *Complete);
	});

	It("loads the previous generation when the latest storage slot is corrupt", [this]()
	{
		if (!Start()) return;
		TStrongObjectPtr<UDungeonSaveGame> First(NewObject<UDungeonSaveGame>());
		FString Error;
		if (!Capture(*First) || !TestTrue(TEXT("save first generation"), FDungeonSaveStorage::Save(Slot, *First, Error))) return;
		TestEqual(TEXT("move before second save"), Move(Battle->GetHeroes()[0], FIntPoint(1, 3)), EBattleActionResult::Success);
		TStrongObjectPtr<UDungeonSaveGame> Second(NewObject<UDungeonSaveGame>());
		if (!Capture(*Second) || !TestTrue(TEXT("save second generation"), FDungeonSaveStorage::Save(Slot, *Second, Error))) return;
		TestTrue(TEXT("second generation is newer"), Second->Sequence > First->Sequence);
		TStrongObjectPtr<UDungeonSaveGame> Latest(LoadSlot());
		if (!Latest.IsValid()) return;
		Compare(*Second, *Latest);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("read second slot"), UGameplayStatics::LoadDataFromSlot(Bytes, Slot + TEXT("_B"), 0))) return;
		if (!TestTrue(TEXT("slot has bytes"), !Bytes.IsEmpty())) return;
		Bytes.Last() ^= 0x5a;
		if (!TestTrue(TEXT("write a corrupt test slot"), UGameplayStatics::SaveDataToSlot(Bytes, Slot + TEXT("_B"), 0))) return;
		AddExpectedMessage(TEXT("Ignored"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		TStrongObjectPtr<UDungeonSaveGame> Fallback(LoadSlot());
		if (!Fallback.IsValid()) return;
		TestEqual(TEXT("previous generation is used"), Fallback->Sequence, First->Sequence);
		Compare(*First, *Fallback);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
