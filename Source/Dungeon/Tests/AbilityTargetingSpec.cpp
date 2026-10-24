#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Tests/AbilityTestHelpers.h"

using namespace AbilityTest;

namespace
{
	constexpr int32 HeroId = 1;
	constexpr int32 EnemyId = 2;
	constexpr int32 AllyId = 3;
}

BEGIN_DEFINE_SPEC(FAbilityTargetingSpec, "Dungeon.Abilities.Targeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
	FResolvedAbility IntentAbility;
	FMonsterIntent IntentData;
END_DEFINE_SPEC(FAbilityTargetingSpec)

void FAbilityTargetingSpec::Define()
{
	BeforeEach([this]()
	{
		Bed.Init();
	});

	AfterEach([this]()
	{
		Bed.Reset();
	});

	Describe("Distance", [this]()
	{
		It("is the Manhattan distance", [this]()
		{
			TestEqual("(1,1)-(3,2)", FAbilityRules::Distance(FIntPoint(1, 1), FIntPoint(3, 2)), 3);
			TestEqual("symmetric", FAbilityRules::Distance(FIntPoint(3, 2), FIntPoint(1, 1)), 3);
			TestEqual("same tile", FAbilityRules::Distance(FIntPoint(4, 4), FIntPoint(4, 4)), 0);
			TestEqual("negative coordinates", FAbilityRules::Distance(FIntPoint(-1, -2), FIntPoint(1, 1)), 5);
		});
	});

	Describe("IsInTargetingShape", [this]()
	{
		It("accepts a diamond between MinRange and MaxRange", [this]()
		{
			const FAbilityTargeting T = MakeTargeting(ETargetingShape::Diamond, 2, 3).Targeting;
			const FIntPoint Source(4, 4);
			TestFalse("distance 1", FAbilityRules::IsInTargetingShape(Source, FIntPoint(5, 4), T));
			TestTrue("distance 2", FAbilityRules::IsInTargetingShape(Source, FIntPoint(5, 5), T));
			TestTrue("distance 3", FAbilityRules::IsInTargetingShape(Source, FIntPoint(2, 5), T));
			TestFalse("distance 4", FAbilityRules::IsInTargetingShape(Source, FIntPoint(6, 6), T));
		});

		It("accepts a cross only in the row or column of the source", [this]()
		{
			const FAbilityTargeting T = MakeTargeting(ETargetingShape::Cross, 1, 3).Targeting;
			const FIntPoint Source(4, 4);
			TestTrue("same row", FAbilityRules::IsInTargetingShape(Source, FIntPoint(7, 4), T));
			TestTrue("same column", FAbilityRules::IsInTargetingShape(Source, FIntPoint(4, 2), T));
			TestFalse("diagonal", FAbilityRules::IsInTargetingShape(Source, FIntPoint(5, 5), T));
			TestFalse("beyond MaxRange", FAbilityRules::IsInTargetingShape(Source, FIntPoint(4, 8), T));
			TestFalse("the source with MinRange 1", FAbilityRules::IsInTargetingShape(Source, Source, T));
		});
	});

	Describe("GetSelectableTiles", [this]()
	{
		It("returns 20 tiles for a diamond 2-3 from (4,4) on an 8x8 field", [this]()
		{
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(4, 4), 3);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 2, 3), Tiles);
			TestEqual("Num", Tiles.Num(), 20);
			TestFalse("distance 1 is excluded", Tiles.Contains(FIntPoint(4, 5)));
			TestTrue("distance 2", Tiles.Contains(FIntPoint(6, 4)));
			TestTrue("distance 3", Tiles.Contains(FIntPoint(1, 4)));
		});

		It("returns 14 tiles for a cross 1-8 from (0,0)", [this]()
		{
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(0, 0), 3);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Cross, 1, 8), Tiles);
			TestEqual("Num", Tiles.Num(), 14);
			TestTrue("end of the row", Tiles.Contains(FIntPoint(7, 0)));
			TestTrue("end of the column", Tiles.Contains(FIntPoint(0, 7)));
			TestFalse("diagonal", Tiles.Contains(FIntPoint(1, 1)));
		});

		It("includes the source tile when MinRange is 0", [this]()
		{
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(3, 3), 3);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 0, 1), Tiles);
			TestEqual("Num", Tiles.Num(), 5);
			TestTrue("source tile", Tiles.Contains(FIntPoint(3, 3)));

			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 0, 0), Tiles);
			TestTrue("self-targeted ability", SameTiles(Tiles, { FIntPoint(3, 3) }));

			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 1, 1), Tiles);
			TestFalse("MinRange 1 excludes the source", Tiles.Contains(FIntPoint(3, 3)));
		});

		It("clips the shape to the field", [this]()
		{
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(7, 7), 3);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 1, 1), Tiles);
			TestTrue("corner", SameTiles(Tiles, { FIntPoint(6, 7), FIntPoint(7, 6) }));
		});

		It("returns nothing for an unknown or dead source", [this]()
		{
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(3, 3), 3);
			Bed.State.KillUnit(HeroId);
			TArray<FIntPoint> Tiles = { FIntPoint(0, 0) };
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, MakeTargeting(ETargetingShape::Diamond, 0, 3), Tiles);
			TestEqual("dead", Tiles.Num(), 0);
			FAbilityRules::GetSelectableTiles(Bed.State, 99, MakeTargeting(ETargetingShape::Diamond, 0, 3), Tiles);
			TestEqual("unknown", Tiles.Num(), 0);
		});
	});

	Describe("CheckRequirement", [this]()
	{
		BeforeEach([this]()
		{
			Bed.Init({ FIntPoint(6, 6) });
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(2, 2), 3);
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(3, 2), 2);
			Bed.AddUnit(AllyId, EUnitTeam::Hero, FIntPoint(2, 3), 2);
		});

		It("AnyTile accepts every tile inside the field", [this]()
		{
			TestTrue("free", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(0, 0), ETargetRequirement::AnyTile));
			TestTrue("unit", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::AnyTile));
			TestTrue("wall", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(6, 6), ETargetRequirement::AnyTile));
			TestFalse("outside", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(8, 0), ETargetRequirement::AnyTile));
		});

		It("Unit needs a living unit on the tile", [this]()
		{
			TestTrue("enemy", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::Unit));
			TestTrue("ally", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 3), ETargetRequirement::Unit));
			TestTrue("the source itself", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 2), ETargetRequirement::Unit));
			TestFalse("free tile", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(0, 0), ETargetRequirement::Unit));
			Bed.State.KillUnit(EnemyId);
			TestFalse("dead unit", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::Unit));
		});

		It("Enemy needs a unit of another team than the source", [this]()
		{
			TestTrue("enemy", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::Enemy));
			TestFalse("ally", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 3), ETargetRequirement::Enemy));
			TestFalse("the source itself", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 2), ETargetRequirement::Enemy));
			TestFalse("free tile", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(0, 0), ETargetRequirement::Enemy));
		});

		It("Ally needs a unit of the team of the source, the source included", [this]()
		{
			TestTrue("ally", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 3), ETargetRequirement::Ally));
			TestTrue("the source itself", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(2, 2), ETargetRequirement::Ally));
			TestFalse("enemy", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::Ally));
			TestFalse("free tile", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(0, 0), ETargetRequirement::Ally));
		});

		It("compares teams relative to the source, not by name", [this]()
		{
			TestTrue("monster sees the hero as an enemy", FAbilityRules::CheckRequirement(Bed.State, EnemyId, FIntPoint(2, 2), ETargetRequirement::Enemy));
			TestFalse("monster does not see itself as an enemy", FAbilityRules::CheckRequirement(Bed.State, EnemyId, FIntPoint(3, 2), ETargetRequirement::Enemy));
			TestTrue("monster sees itself as an ally", FAbilityRules::CheckRequirement(Bed.State, EnemyId, FIntPoint(3, 2), ETargetRequirement::Ally));
		});

		It("Enemy and Ally fail for an unknown source", [this]()
		{
			TestFalse("Enemy", FAbilityRules::CheckRequirement(Bed.State, 99, FIntPoint(3, 2), ETargetRequirement::Enemy));
			TestFalse("Ally", FAbilityRules::CheckRequirement(Bed.State, 99, FIntPoint(3, 2), ETargetRequirement::Ally));
		});

		It("FreePassable needs a passable tile without a unit", [this]()
		{
			TestTrue("free", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(0, 0), ETargetRequirement::FreePassable));
			TestFalse("unit", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::FreePassable));
			TestFalse("wall", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(6, 6), ETargetRequirement::FreePassable));
			TestFalse("outside", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(-1, 0), ETargetRequirement::FreePassable));
			Bed.State.KillUnit(EnemyId);
			TestTrue("tile of a dead unit", FAbilityRules::CheckRequirement(Bed.State, HeroId, FIntPoint(3, 2), ETargetRequirement::FreePassable));
		});
	});

	Describe("TraceProjectile", [this]()
	{
		BeforeEach([this]()
		{
			Bed.Init({ FIntPoint(2, 6) });
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(2, 2), 3);
		});

		It("stops on the first unit", [this]()
		{
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(5, 2), 2);
			Bed.AddUnit(AllyId, EUnitTeam::Monster, FIntPoint(6, 2), 2);
			TestEqual("Right", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 8), FIntPoint(5, 2));
		});

		It("stops on the first impassable tile", [this]()
		{
			TestEqual("Down", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Down, 8), FIntPoint(2, 6));
		});

		It("ends on the last tile within MaxRange", [this]()
		{
			TestEqual("Left, MaxRange 1", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Left, 1), FIntPoint(1, 2));
			TestEqual("Right, MaxRange 3", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 3), FIntPoint(5, 2));
		});

		It("ends on the last tile of the field when the range is longer", [this]()
		{
			TestEqual("Left", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Left, 8), FIntPoint(0, 2));
			TestEqual("Up", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Up, 8), FIntPoint(2, 0));
			TestEqual("Right", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 8), FIntPoint(7, 2));
		});

		It("ignores the units beyond MaxRange", [this]()
		{
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(6, 2), 2);
			TestEqual("Right", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 3), FIntPoint(5, 2));
		});

		It("returns the neighbour when it is blocked", [this]()
		{
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(3, 2), 2);
			TestEqual("Right", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 8), FIntPoint(3, 2));
		});

		It("returns the source when there is no tile to fly to", [this]()
		{
			TestEqual("MaxRange 0", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 0), FIntPoint(2, 2));
			TestEqual("field edge", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(0, 0), EGridDirection::Left, 8), FIntPoint(0, 0));
			TestEqual("no direction", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::None, 8), FIntPoint(2, 2));
		});

		It("does not stop on a tile freed by a dead unit", [this]()
		{
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(4, 2), 2);
			Bed.State.KillUnit(EnemyId);
			TestEqual("Right", FAbilityRules::TraceProjectile(Bed.State.Grid, FIntPoint(2, 2), EGridDirection::Right, 8), FIntPoint(7, 2));
		});
	});

	Describe("MakeCast", [this]()
	{
		BeforeEach([this]()
		{
			Bed.Init({ FIntPoint(2, 6) });
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(2, 2), 3);
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(5, 2), 2);
			Bed.AddUnit(AllyId, EUnitTeam::Hero, FIntPoint(2, 4), 2);
		});

		It("builds a cast for an instant ability", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 3, ETrajectory::Instant, ETargetRequirement::Enemy);
			Ability.Area.Type = EAreaType::Diamond;
			Ability.Area.Size = 1;
			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(5, 2), Cast));
			TestEqual("SourceId", Cast.SourceId, HeroId);
			TestEqual("SourcePosition", Cast.SourcePosition, FIntPoint(2, 2));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(5, 2));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(5, 2));
			TestEqual("Direction", Cast.Direction, EGridDirection::Right);
			TestTrue("center", Cast.Area.bHasCenter && Cast.Area.Center == FIntPoint(5, 2));
			TestEqual("edge", Cast.Area.Edge.Num(), 4);
		});

		It("gives no direction for a diagonal selection", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 3);
			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 3), Cast));
			TestEqual("Direction", Cast.Direction, EGridDirection::None);
		});

		It("fails outside the targeting shape and outside the field", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 3);
			FAbilityCast Cast;
			TestFalse("too far", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(6, 2), Cast));
			TestFalse("too close", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(2, 2), Cast));
			TestFalse("diagonal", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 3), Cast));
			TestFalse("outside the field", FAbilityRules::MakeCast(Bed.State, HeroId, MakeTargeting(ETargetingShape::Cross, 1, 8), FIntPoint(-1, 2), Cast));
		});

		It("fails for a dead or unknown source", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 3);
			FAbilityCast Cast;
			TestFalse("unknown", FAbilityRules::MakeCast(Bed.State, 99, Ability, FIntPoint(3, 2), Cast));
			Bed.State.KillUnit(HeroId);
			TestFalse("dead", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 2), Cast));
		});

		It("fails when the requirement is not met and leaves Out unchanged", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 3, ETrajectory::Instant, ETargetRequirement::Enemy);
			FAbilityCast Cast;
			Cast.SourceId = 42;
			TestFalse("free tile", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 2), Cast));
			TestFalse("ally", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(2, 4), Cast));
			TestEqual("Out is unchanged", Cast.SourceId, 42);
		});

		It("projectile: the target is the first unit, the selected tile only gives the direction", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::Unit);
			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 2), Cast));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(3, 2));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(5, 2));
			TestEqual("Direction", Cast.Direction, EGridDirection::Right);
		});

		It("projectile: the target is the first impassable tile", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::AnyTile);
			Ability.Area.Type = EAreaType::Tile;
			FAbilityCast Cast;
			Bed.State.KillUnit(AllyId);
			TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(2, 3), Cast));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(2, 6));
		});

		It("projectile: the target is the last tile within range and field", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 3, ETrajectory::Projectile, ETargetRequirement::AnyTile);
			FAbilityCast Cast;
			TestTrue("range", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(2, 1), Cast));
			TestEqual("TargetPoint in range", Cast.TargetPoint, FIntPoint(2, 0));
			TestTrue("left", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(1, 2), Cast));
			TestEqual("TargetPoint at the field edge", Cast.TargetPoint, FIntPoint(0, 2));
		});

		It("projectile: the requirement is checked for the actual target point", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::Enemy);
			FAbilityCast Cast;
			TestTrue("an empty selected tile is fine when the projectile hits an enemy", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 2), Cast));
			TestFalse("the projectile hits an ally", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(2, 3), Cast));

			FResolvedAbility OnlyUnits = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::Unit);
			TestTrue("an unit hit by the projectile", FAbilityRules::MakeCast(Bed.State, HeroId, OnlyUnits, FIntPoint(4, 2), Cast));
			TestFalse("nothing in the way", FAbilityRules::MakeCast(Bed.State, HeroId, OnlyUnits, FIntPoint(1, 2), Cast));

			FResolvedAbility Free = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::FreePassable);
			TestFalse("the target is a unit or a wall", FAbilityRules::MakeCast(Bed.State, HeroId, Free, FIntPoint(3, 2), Cast));
			TestTrue("the target is the last free tile", FAbilityRules::MakeCast(Bed.State, HeroId, Free, FIntPoint(1, 2), Cast));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(0, 2));
		});

		It("projectile: MinRange limits only the selection, the flight starts at the neighbour", [this]()
		{
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(3, 2), 2);
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 2, 8, ETrajectory::Projectile, ETargetRequirement::Enemy);
			FAbilityCast Cast;
			TestFalse("the neighbour is not selectable", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(3, 2), Cast));
			TestTrue("a farther tile is", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, FIntPoint(4, 2), Cast));
			TestEqual("the unit next to the source is hit", Cast.TargetPoint, FIntPoint(3, 2));
		});

		It("projectile: all selectable tiles of a direction give one target point", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Cross, 1, 8, ETrajectory::Projectile, ETargetRequirement::Enemy);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, Ability, Tiles);
			TestTrue("the whole row to the right", SameTiles(Tiles, { FIntPoint(3, 2), FIntPoint(4, 2), FIntPoint(5, 2), FIntPoint(6, 2), FIntPoint(7, 2) }));

			for (const FIntPoint& Tile : Tiles)
			{
				FAbilityCast Cast;
				TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, HeroId, Ability, Tile, Cast));
				TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(5, 2));
			}
		});

		It("GetSelectableTiles returns exactly the tiles MakeCast accepts", [this]()
		{
			FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 4, ETrajectory::Instant, ETargetRequirement::Unit);
			TArray<FIntPoint> Tiles;
			FAbilityRules::GetSelectableTiles(Bed.State, HeroId, Ability, Tiles);
			TestTrue("the enemy at distance 3 and the ally at distance 2", SameTiles(Tiles, { FIntPoint(5, 2), FIntPoint(2, 4) }));
		});
	});

	Describe("MakeIntentCast", [this]()
	{
		BeforeEach([this]()
		{
			Bed.Init({ FIntPoint(2, 6) });
			Bed.AddUnit(EnemyId, EUnitTeam::Monster, FIntPoint(5, 2), 2);
			IntentData = FMonsterIntent();
			IntentData.bActive = true;
			IntentData.Direction = EGridDirection::Left;
			IntentData.SelectedOffset = FIntPoint(-2, 0);
			IntentAbility = MakeTargeting(ETargetingShape::Cross, 1, 3, ETrajectory::Instant, ETargetRequirement::Enemy);
			IntentAbility.Area.Type = EAreaType::Diamond;
			IntentAbility.Area.Size = 1;
		});

		It("selects the monster position plus the offset and keeps the direction of the intent", [this]()
		{
			FAbilityCast Cast;
			TestTrue("MakeIntentCast", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("SourceId", Cast.SourceId, EnemyId);
			TestEqual("SourcePosition", Cast.SourcePosition, FIntPoint(5, 2));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(3, 2));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(3, 2));
			TestEqual("Direction", Cast.Direction, EGridDirection::Left);
			TestTrue("center", Cast.Area.bHasCenter && Cast.Area.Center == FIntPoint(3, 2));
			TestEqual("edge", Cast.Area.Edge.Num(), 4);
		});

		It("follows the monster when it was moved", [this]()
		{
			Bed.State.MoveUnit(EnemyId, FIntPoint(5, 4));
			FAbilityCast Cast;
			TestTrue("MakeIntentCast", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("SourcePosition", Cast.SourcePosition, FIntPoint(5, 4));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(3, 4));
			TestEqual("TargetPoint", Cast.TargetPoint, FIntPoint(3, 4));
			TestEqual("Direction is not recalculated", Cast.Direction, EGridDirection::Left);
		});

		It("does not check targeting or the requirement", [this]()
		{
			IntentData.SelectedOffset = FIntPoint(-3, 1);
			IntentData.Direction = EGridDirection::Up;
			FAbilityCast Cast;
			TestTrue("outside the shape, on a free tile", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(2, 3));
			TestEqual("Direction", Cast.Direction, EGridDirection::Up);
		});

		It("clips the area to the field when the target is outside", [this]()
		{
			Bed.State.MoveUnit(EnemyId, FIntPoint(1, 2));
			FAbilityCast Cast;
			TestTrue("MakeIntentCast", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("SelectedTile", Cast.SelectedTile, FIntPoint(-1, 2));
			TestFalse("no center", Cast.Area.bHasCenter);
			TestTrue("only the edge tile inside the field", SameTiles(Cast.Area.Edge, { FIntPoint(0, 2) }));
		});

		It("projectile: recalculates the target point from the current position", [this]()
		{
			IntentAbility.Targeting.Trajectory = ETrajectory::Projectile;
			IntentAbility.Targeting.MaxRange = 8;
			IntentAbility.Area.Type = EAreaType::Tile;
			Bed.AddUnit(HeroId, EUnitTeam::Hero, FIntPoint(1, 2), 3);

			FAbilityCast Cast;
			TestTrue("before", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("the hero is hit", Cast.TargetPoint, FIntPoint(1, 2));
			TestEqual("SelectedTile stays the offset tile", Cast.SelectedTile, FIntPoint(3, 2));

			Bed.State.MoveUnit(HeroId, FIntPoint(0, 3));
			TestTrue("after the hero left", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("the projectile flies to the field edge", Cast.TargetPoint, FIntPoint(0, 2));

			Bed.State.MoveUnit(EnemyId, FIntPoint(5, 4));
			TestTrue("after the monster was moved", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
			TestEqual("the flight starts from the new position", Cast.TargetPoint, FIntPoint(0, 4));
		});

		It("fails for a dead or unknown source", [this]()
		{
			FAbilityCast Cast;
			TestFalse("unknown", FAbilityRules::MakeIntentCast(Bed.State, 99, IntentAbility, IntentData, Cast));
			Bed.State.KillUnit(EnemyId);
			TestFalse("dead", FAbilityRules::MakeIntentCast(Bed.State, EnemyId, IntentAbility, IntentData, Cast));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
