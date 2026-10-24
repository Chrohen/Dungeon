#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Tests/AbilityTestHelpers.h"

using namespace AbilityTest;

namespace
{
	FAbilityAreaSpec MakeSpec(EAreaType Type, int32 Size = 0, bool bAffectsSource = false)
	{
		FAbilityAreaSpec Spec;
		Spec.Type = Type;
		Spec.Size = Size;
		Spec.bAffectsSource = bAffectsSource;
		return Spec;
	}

	// Источник далеко от проверяемых областей, чтобы не мешал
	const FIntPoint FarSource(7, 7);
}

BEGIN_DEFINE_SPEC(FAbilityAreaRulesSpec, "Dungeon.Abilities.Area",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
	FAffectedArea Area;
	FAbilityAreaSpec PatternSpec;

	void Build(FIntPoint Source, FIntPoint Target, EGridDirection Direction, const FAbilityAreaSpec& Spec)
	{
		FAbilityRules::BuildArea(Bed.State.Grid, Source, Target, Direction, Spec, Area);
	}
END_DEFINE_SPEC(FAbilityAreaRulesSpec)

void FAbilityAreaRulesSpec::Define()
{
	BeforeEach([this]()
	{
		Bed.Init();
		Area = FAffectedArea();
	});

	AfterEach([this]()
	{
		Bed.Reset();
	});

	Describe("directions", [this]()
	{
		It("GetDirection needs the same row or column and different tiles", [this]()
		{
			TestEqual("up", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(3, 0)), EGridDirection::Up);
			TestEqual("down", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(3, 5)), EGridDirection::Down);
			TestEqual("left", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(1, 3)), EGridDirection::Left);
			TestEqual("right", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(4, 3)), EGridDirection::Right);
			TestEqual("same tile", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(3, 3)), EGridDirection::None);
			TestEqual("diagonal", FAbilityRules::GetDirection(FIntPoint(3, 3), FIntPoint(4, 4)), EGridDirection::None);
		});

		It("DirectionToOffset: Up is towards row 0", [this]()
		{
			TestEqual("Up", FAbilityRules::DirectionToOffset(EGridDirection::Up), FIntPoint(0, -1));
			TestEqual("Down", FAbilityRules::DirectionToOffset(EGridDirection::Down), FIntPoint(0, 1));
			TestEqual("Left", FAbilityRules::DirectionToOffset(EGridDirection::Left), FIntPoint(-1, 0));
			TestEqual("Right", FAbilityRules::DirectionToOffset(EGridDirection::Right), FIntPoint(1, 0));
			TestEqual("None", FAbilityRules::DirectionToOffset(EGridDirection::None), FIntPoint(0, 0));
		});

		It("RotateOffset turns the offsets clockwise from Right", [this]()
		{
			const FIntPoint Offset(2, 1);
			TestEqual("Right", FAbilityRules::RotateOffset(Offset, EGridDirection::Right), FIntPoint(2, 1));
			TestEqual("Down", FAbilityRules::RotateOffset(Offset, EGridDirection::Down), FIntPoint(-1, 2));
			TestEqual("Left", FAbilityRules::RotateOffset(Offset, EGridDirection::Left), FIntPoint(-2, -1));
			TestEqual("Up", FAbilityRules::RotateOffset(Offset, EGridDirection::Up), FIntPoint(1, -2));
			TestEqual("None", FAbilityRules::RotateOffset(Offset, EGridDirection::None), FIntPoint(2, 1));
		});

		It("RotateOffset maps the right-hand neighbour onto the neighbour in the direction", [this]()
		{
			for (EGridDirection Direction : { EGridDirection::Up, EGridDirection::Down, EGridDirection::Left, EGridDirection::Right })
			{
				TestEqual("(1,0) rotated", FAbilityRules::RotateOffset(FIntPoint(1, 0), Direction), FAbilityRules::DirectionToOffset(Direction));
			}
		});
	});

	Describe("Tile", [this]()
	{
		It("is the target point only", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Tile));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Center", Area.Center, FIntPoint(3, 3));
			TestEqual("Edge", Area.Edge.Num(), 0);
		});
	});

	Describe("Diamond", [this]()
	{
		It("radius 1 is the center and four neighbours", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Diamond, 1));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(4, 3), FIntPoint(3, 2), FIntPoint(3, 4) }));
		});

		It("radius 2 has 12 edge tiles at distance 1 and 2", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Diamond, 2));
			TestEqual("Edge", Area.Edge.Num(), 12);
			TestTrue("distance 2", Area.Edge.Contains(FIntPoint(5, 3)));
			TestTrue("distance 2 diagonal", Area.Edge.Contains(FIntPoint(4, 4)));
			TestFalse("distance 3", Area.Edge.Contains(FIntPoint(6, 3)));
			TestFalse("the center is not in the edge", Area.Edge.Contains(FIntPoint(3, 3)));
		});

		It("radius 0 is the center only", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Diamond, 0));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});

		It("is clipped by the field edge", [this]()
		{
			Build(FarSource, FIntPoint(0, 0), EGridDirection::None, MakeSpec(EAreaType::Diamond, 2));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(1, 0), FIntPoint(2, 0), FIntPoint(0, 1), FIntPoint(0, 2), FIntPoint(1, 1) }));
		});

		It("keeps the edge inside the field when the center is outside", [this]()
		{
			Build(FarSource, FIntPoint(-1, 3), EGridDirection::None, MakeSpec(EAreaType::Diamond, 1));
			TestFalse("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(0, 3) }));
		});
	});

	Describe("Line", [this]()
	{
		It("Size is the length including the target point", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, MakeSpec(EAreaType::Line, 3));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(4, 3), FIntPoint(5, 3) }));
		});

		It("Size 1 is the center only", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, MakeSpec(EAreaType::Line, 1));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});

		It("goes along every direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Left, MakeSpec(EAreaType::Line, 3));
			TestTrue("Left", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(1, 3) }));
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Up, MakeSpec(EAreaType::Line, 3));
			TestTrue("Up", SameTiles(Area.Edge, { FIntPoint(3, 2), FIntPoint(3, 1) }));
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Down, MakeSpec(EAreaType::Line, 3));
			TestTrue("Down", SameTiles(Area.Edge, { FIntPoint(3, 4), FIntPoint(3, 5) }));
		});

		It("is clipped by the field edge", [this]()
		{
			Build(FarSource, FIntPoint(6, 3), EGridDirection::Right, MakeSpec(EAreaType::Line, 4));
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(7, 3) }));
		});

		It("is empty without a direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Line, 3));
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});
	});

	Describe("CrossLine", [this]()
	{
		It("Size is the half-width across a horizontal direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, MakeSpec(EAreaType::CrossLine, 1));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(3, 2), FIntPoint(3, 4) }));
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Left, MakeSpec(EAreaType::CrossLine, 2));
			TestTrue("Left, half-width 2", SameTiles(Area.Edge, { FIntPoint(3, 2), FIntPoint(3, 1), FIntPoint(3, 4), FIntPoint(3, 5) }));
		});

		It("goes across a vertical direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Up, MakeSpec(EAreaType::CrossLine, 2));
			TestTrue("Up", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(1, 3), FIntPoint(4, 3), FIntPoint(5, 3) }));
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Down, MakeSpec(EAreaType::CrossLine, 1));
			TestTrue("Down", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(4, 3) }));
		});

		It("is clipped by the field edge", [this]()
		{
			Build(FarSource, FIntPoint(3, 0), EGridDirection::Right, MakeSpec(EAreaType::CrossLine, 2));
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(3, 1), FIntPoint(3, 2) }));
		});

		It("is empty without a direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::CrossLine, 2));
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});
	});

	Describe("Path", [this]()
	{
		It("covers the tiles from the neighbour of the source to the target", [this]()
		{
			Build(FIntPoint(1, 3), FIntPoint(4, 3), EGridDirection::Right, MakeSpec(EAreaType::Path));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Center", Area.Center, FIntPoint(4, 3));
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(3, 3) }));
		});

		It("works upwards", [this]()
		{
			Build(FIntPoint(3, 5), FIntPoint(3, 2), EGridDirection::Up, MakeSpec(EAreaType::Path));
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(3, 4), FIntPoint(3, 3) }));
		});

		It("has no edge when the target is next to the source", [this]()
		{
			Build(FIntPoint(1, 3), FIntPoint(2, 3), EGridDirection::Right, MakeSpec(EAreaType::Path));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});

		It("is empty without a direction", [this]()
		{
			Build(FIntPoint(1, 3), FIntPoint(4, 3), EGridDirection::None, MakeSpec(EAreaType::Path));
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});
	});

	Describe("Pattern", [this]()
	{
		BeforeEach([this]()
		{
			PatternSpec = MakeSpec(EAreaType::Pattern);
			PatternSpec.Offsets = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1) };
		});

		It("rotates the offsets for each direction", [this]()
		{
			const FIntPoint Target(3, 3);
			Build(FarSource, Target, EGridDirection::Right, PatternSpec);
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Right", SameTiles(Area.Edge, { FIntPoint(4, 3), FIntPoint(3, 4) }));
			Build(FarSource, Target, EGridDirection::Down, PatternSpec);
			TestTrue("Down", SameTiles(Area.Edge, { FIntPoint(3, 4), FIntPoint(2, 3) }));
			Build(FarSource, Target, EGridDirection::Left, PatternSpec);
			TestTrue("Left", SameTiles(Area.Edge, { FIntPoint(2, 3), FIntPoint(3, 2) }));
			Build(FarSource, Target, EGridDirection::Up, PatternSpec);
			TestTrue("Up", SameTiles(Area.Edge, { FIntPoint(3, 2), FIntPoint(4, 3) }));
		});

		It("does not rotate the offsets without a direction", [this]()
		{
			Build(FarSource, FIntPoint(3, 3), EGridDirection::None, PatternSpec);
			TestTrue("bHasCenter", Area.bHasCenter);
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(4, 3), FIntPoint(3, 4) }));
		});

		It("includes the center only when an offset is (0,0)", [this]()
		{
			PatternSpec.Offsets = { FIntPoint(1, 0), FIntPoint(2, 0) };
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, PatternSpec);
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Center is still the target point", Area.Center, FIntPoint(3, 3));
			TestTrue("Edge", SameTiles(Area.Edge, { FIntPoint(4, 3), FIntPoint(5, 3) }));
		});

		It("has no tiles for an empty pattern", [this]()
		{
			PatternSpec.Offsets.Reset();
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, PatternSpec);
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 0);
		});

		It("is clipped by the field edge", [this]()
		{
			Build(FarSource, FIntPoint(0, 0), EGridDirection::Left, PatternSpec);
			TestTrue("bHasCenter", Area.bHasCenter);
			TestEqual("Edge: (-1,0) and (0,-1) are outside", Area.Edge.Num(), 0);
		});

		It("ignores duplicate offsets", [this]()
		{
			PatternSpec.Offsets = { FIntPoint(1, 0), FIntPoint(1, 0) };
			Build(FarSource, FIntPoint(3, 3), EGridDirection::Right, PatternSpec);
			TestEqual("Edge", Area.Edge.Num(), 1);
		});
	});

	Describe("the source tile", [this]()
	{
		It("is removed from the area", [this]()
		{
			Build(FIntPoint(3, 4), FIntPoint(3, 3), EGridDirection::Up, MakeSpec(EAreaType::Diamond, 1));
			TestTrue("bHasCenter", Area.bHasCenter);
			TestFalse("source tile", Area.Edge.Contains(FIntPoint(3, 4)));
			TestEqual("Edge", Area.Edge.Num(), 3);
		});

		It("is removed from the center: bHasCenter becomes false", [this]()
		{
			Build(FIntPoint(3, 3), FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Diamond, 1));
			TestFalse("bHasCenter", Area.bHasCenter);
			TestEqual("Edge", Area.Edge.Num(), 4);
		});

		It("stays in the area with bAffectsSource", [this]()
		{
			Build(FIntPoint(3, 4), FIntPoint(3, 3), EGridDirection::Up, MakeSpec(EAreaType::Diamond, 1, true));
			TestTrue("source tile", Area.Edge.Contains(FIntPoint(3, 4)));
			TestEqual("Edge", Area.Edge.Num(), 4);

			Build(FIntPoint(3, 3), FIntPoint(3, 3), EGridDirection::None, MakeSpec(EAreaType::Tile, 0, true));
			TestTrue("self-targeted center", Area.bHasCenter);
		});

		It("is not added by bAffectsSource when the shape does not cover it", [this]()
		{
			Build(FIntPoint(1, 1), FIntPoint(5, 5), EGridDirection::None, MakeSpec(EAreaType::Diamond, 1, true));
			TestFalse("source tile", Area.Edge.Contains(FIntPoint(1, 1)));
			TestEqual("Edge", Area.Edge.Num(), 4);
		});

		It("is removed from a path that starts next to it", [this]()
		{
			Build(FIntPoint(1, 3), FIntPoint(4, 3), EGridDirection::Right, MakeSpec(EAreaType::Path));
			TestFalse("source tile", Area.Edge.Contains(FIntPoint(1, 3)));
		});
	});

	Describe("FAffectedArea parts", [this]()
	{
		BeforeEach([this]()
		{
			Area.Center = FIntPoint(3, 3);
			Area.bHasCenter = true;
			Area.Edge = { FIntPoint(4, 3), FIntPoint(2, 3) };
		});

		It("Contains respects the part", [this]()
		{
			TestTrue("center in Center", Area.Contains(FIntPoint(3, 3), EAreaPart::Center));
			TestFalse("edge tile in Center", Area.Contains(FIntPoint(4, 3), EAreaPart::Center));
			TestTrue("edge tile in Edge", Area.Contains(FIntPoint(4, 3), EAreaPart::Edge));
			TestFalse("center in Edge", Area.Contains(FIntPoint(3, 3), EAreaPart::Edge));
			TestTrue("center in All", Area.Contains(FIntPoint(3, 3), EAreaPart::All));
			TestTrue("edge tile in All", Area.Contains(FIntPoint(2, 3), EAreaPart::All));
			TestFalse("other tile in All", Area.Contains(FIntPoint(0, 0), EAreaPart::All));
		});

		It("a missing center is not contained", [this]()
		{
			Area.bHasCenter = false;
			TestFalse("Center", Area.Contains(FIntPoint(3, 3), EAreaPart::Center));
			TestFalse("All", Area.Contains(FIntPoint(3, 3), EAreaPart::All));
			TestEqual("GetAllTiles", Area.GetAllTiles().Num(), 2);
		});

		It("GetTiles and GetAllTiles follow the same rules", [this]()
		{
			TArray<FIntPoint> Tiles = { FIntPoint(9, 9) };
			Area.GetTiles(EAreaPart::Center, Tiles);
			TestTrue("Center", SameTiles(Tiles, { FIntPoint(3, 3) }));
			Area.GetTiles(EAreaPart::Edge, Tiles);
			TestTrue("Edge", SameTiles(Tiles, { FIntPoint(4, 3), FIntPoint(2, 3) }));
			Area.GetTiles(EAreaPart::All, Tiles);
			TestTrue("All", SameTiles(Tiles, { FIntPoint(3, 3), FIntPoint(4, 3), FIntPoint(2, 3) }));
			TestTrue("GetAllTiles", SameTiles(Area.GetAllTiles(), Tiles));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
