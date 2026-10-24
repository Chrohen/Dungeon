#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AbilityTestHelpers.h"

BEGIN_DEFINE_SPEC(FBattleSimStateSpec, "Dungeon.Abilities.SimState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
END_DEFINE_SPEC(FBattleSimStateSpec)

void FBattleSimStateSpec::Define()
{
	BeforeEach([this]()
	{
		Bed.Init({ FIntPoint(5, 5) });
		Bed.AddUnit(1, EUnitTeam::Hero, FIntPoint(1, 1), 3, 5);
		Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 2);
	});

	AfterEach([this]()
	{
		Bed.Reset();
	});

	Describe("test bed", [this]()
	{
		It("rejects units on occupied, impassable and outside cells", [this]()
		{
			TestFalse("occupied", Bed.AddUnit(3, EUnitTeam::Hero, FIntPoint(1, 1), 1));
			TestFalse("wall", Bed.AddUnit(3, EUnitTeam::Hero, FIntPoint(5, 5), 1));
			TestFalse("outside", Bed.AddUnit(3, EUnitTeam::Hero, FIntPoint(8, 0), 1));
			TestEqual("Units", Bed.State.Units.Num(), 2);
		});
	});

	Describe("FindUnit", [this]()
	{
		It("finds units by id", [this]()
		{
			const FSimUnit* Unit = Bed.State.FindUnit(2);
			TestNotNull("unit 2", Unit);
			if (Unit)
			{
				TestEqual("Team", Unit->Team, EUnitTeam::Monster);
				TestEqual("MaxHealth defaults to Health", Unit->MaxHealth, 2);
			}
			TestNull("unknown id", Bed.State.FindUnit(99));
		});
	});

	Describe("FindUnitAt", [this]()
	{
		It("returns the living unit on the tile", [this]()
		{
			const FSimUnit* Unit = Bed.State.FindUnitAt(FIntPoint(1, 1));
			TestNotNull("unit", Unit);
			if (Unit)
			{
				TestEqual("UnitId", Unit->UnitId, 1);
			}
			TestNull("empty tile", Bed.State.FindUnitAt(FIntPoint(0, 0)));
			TestNull("outside", Bed.State.FindUnitAt(FIntPoint(-1, 0)));
		});

		It("does not return a dead unit even if the grid still holds it", [this]()
		{
			Bed.State.FindUnit(1)->bAlive = false;
			TestNull("dead", Bed.State.FindUnitAt(FIntPoint(1, 1)));
		});
	});

	Describe("MoveUnit", [this]()
	{
		It("moves the unit in the grid and in Position", [this]()
		{
			TestTrue("MoveUnit", Bed.State.MoveUnit(1, FIntPoint(1, 2)));
			TestEqual("Position", Bed.State.FindUnit(1)->Position, FIntPoint(1, 2));
			TestEqual("found at the new tile", Bed.State.FindUnitAt(FIntPoint(1, 2))->UnitId, 1);
			TestNull("old tile is free", Bed.State.FindUnitAt(FIntPoint(1, 1)));
		});

		It("fails for an occupied tile, a wall, an unknown or a dead unit", [this]()
		{
			TestFalse("occupied", Bed.State.MoveUnit(1, FIntPoint(2, 1)));
			TestFalse("wall", Bed.State.MoveUnit(1, FIntPoint(5, 5)));
			TestFalse("unknown unit", Bed.State.MoveUnit(99, FIntPoint(0, 0)));
			TestEqual("Position is kept", Bed.State.FindUnit(1)->Position, FIntPoint(1, 1));

			Bed.State.KillUnit(2);
			TestFalse("dead unit", Bed.State.MoveUnit(2, FIntPoint(0, 0)));
		});
	});

	Describe("KillUnit", [this]()
	{
		It("marks the unit dead and frees its tile", [this]()
		{
			Bed.State.KillUnit(2);
			TestFalse("bAlive", Bed.State.FindUnit(2)->bAlive);
			TestFalse("tile is free", Bed.State.Grid.IsOccupied(FIntPoint(2, 1)));
			TestNull("FindUnitAt", Bed.State.FindUnitAt(FIntPoint(2, 1)));
			TestTrue("another unit can enter", Bed.State.MoveUnit(1, FIntPoint(2, 1)));
		});

		It("does not free a tile that a living unit took after the death", [this]()
		{
			Bed.State.KillUnit(2);
			Bed.State.MoveUnit(1, FIntPoint(2, 1));
			Bed.State.KillUnit(2);
			TestEqual("still occupied by the hero", Bed.State.FindUnitAt(FIntPoint(2, 1))->UnitId, 1);
		});

		It("ignores an unknown id", [this]()
		{
			Bed.State.KillUnit(99);
			TestEqual("Occupants", Bed.State.Grid.Occupants.Num(), 2);
		});
	});

	Describe("ApplyDamage", [this]()
	{
		It("reduces health and reports the loss", [this]()
		{
			int32 Lost = -1;
			Bed.State.ApplyDamage(1, 2, Lost);
			TestEqual("Lost", Lost, 2);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 1);
		});

		It("never goes below 0 and reports the real loss", [this]()
		{
			int32 Lost = -1;
			Bed.State.ApplyDamage(1, 10, Lost);
			TestEqual("Lost", Lost, 3);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 0);

			Bed.State.ApplyDamage(1, 1, Lost);
			TestEqual("Lost at 0 health", Lost, 0);
		});

		It("ignores negative amounts, unknown and dead units", [this]()
		{
			int32 Lost = -1;
			Bed.State.ApplyDamage(1, -4, Lost);
			TestEqual("negative", Lost, 0);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 3);

			Lost = -1;
			Bed.State.ApplyDamage(99, 1, Lost);
			TestEqual("unknown", Lost, 0);

			Bed.State.KillUnit(2);
			Lost = -1;
			Bed.State.ApplyDamage(2, 1, Lost);
			TestEqual("dead", Lost, 0);
		});
	});

	Describe("ApplyHeal", [this]()
	{
		It("raises health and reports the gain", [this]()
		{
			int32 Gained = -1;
			Bed.State.ApplyHeal(1, 1, Gained);
			TestEqual("Gained", Gained, 1);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 4);
		});

		It("never goes above MaxHealth and reports the real gain", [this]()
		{
			int32 Gained = -1;
			Bed.State.ApplyHeal(1, 10, Gained);
			TestEqual("Gained", Gained, 2);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 5);

			Bed.State.ApplyHeal(1, 1, Gained);
			TestEqual("Gained at full health", Gained, 0);
		});

		It("ignores negative amounts, unknown and dead units", [this]()
		{
			int32 Gained = -1;
			Bed.State.ApplyHeal(1, -4, Gained);
			TestEqual("negative", Gained, 0);
			TestEqual("Health", Bed.State.FindUnit(1)->Health, 3);

			Gained = -1;
			Bed.State.ApplyHeal(99, 1, Gained);
			TestEqual("unknown", Gained, 0);

			Bed.State.FindUnit(2)->Health = 0;
			Bed.State.KillUnit(2);
			Gained = -1;
			Bed.State.ApplyHeal(2, 1, Gained);
			TestEqual("dead", Gained, 0);
		});
	});

	Describe("copy", [this]()
	{
		It("is independent of the original", [this]()
		{
			FBattleSimState Copy = Bed.State;
			Copy.MoveUnit(1, FIntPoint(0, 0));
			int32 Lost = 0;
			Copy.ApplyDamage(2, 1, Lost);
			Copy.KillUnit(2);

			TestEqual("original Position", Bed.State.FindUnit(1)->Position, FIntPoint(1, 1));
			TestEqual("original Health", Bed.State.FindUnit(2)->Health, 2);
			TestTrue("original bAlive", Bed.State.FindUnit(2)->bAlive);
			TestEqual("original unit on the grid", Bed.State.FindUnitAt(FIntPoint(2, 1))->UnitId, 2);
			TestEqual("copy keeps UnitId in the grid", Copy.Grid.GetOccupant(FIntPoint(0, 0))->UnitId, 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
