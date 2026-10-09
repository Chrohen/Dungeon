#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/TileTypeDefinition.h"
#include "Grid/GridModel.h"

namespace
{
	FGridOccupant MakeOccupant(EUnitTeam Team)
	{
		FGridOccupant Occupant;
		Occupant.Unit = nullptr;
		Occupant.Team = Team;
		return Occupant;
	}
}

BEGIN_DEFINE_SPEC(FGridModelSpec, "Dungeon.Grid.Model",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	// Тип тайла держится через TStrongObjectPtr, чтобы сборщик мусора не удалил его между шагами спеки
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UTileTypeDefinition> Wall;
	TStrongObjectPtr<UTileTypeDefinition> Swamp;
	FGridModel Model;
END_DEFINE_SPEC(FGridModelSpec)

void FGridModelSpec::Define()
{
	BeforeEach([this]()
	{
		Floor.Reset(NewObject<UTileTypeDefinition>());
		Floor->bPassable = true;
		Floor->MoveCost = 1;

		Wall.Reset(NewObject<UTileTypeDefinition>());
		Wall->bPassable = false;
		Wall->MoveCost = 1;

		Swamp.Reset(NewObject<UTileTypeDefinition>());
		Swamp->bPassable = true;
		Swamp->MoveCost = 3;

		// Поле не квадратное, чтобы перепутанные X и Y были заметны
		Model = FGridModel();
		Model.Initialize(4, 3, Floor.Get());
	});

	AfterEach([this]()
	{
		Model = FGridModel();
		Floor.Reset();
		Wall.Reset();
		Swamp.Reset();
	});

	Describe("IsInside", [this]()
	{
		It("accepts the corners of the field", [this]()
		{
			TestTrue("(0,0)", Model.IsInside(FIntPoint(0, 0)));
			TestTrue("(3,0)", Model.IsInside(FIntPoint(3, 0)));
			TestTrue("(0,2)", Model.IsInside(FIntPoint(0, 2)));
			TestTrue("(3,2)", Model.IsInside(FIntPoint(3, 2)));
		});

		It("rejects cells just outside the field", [this]()
		{
			TestFalse("(-1,0)", Model.IsInside(FIntPoint(-1, 0)));
			TestFalse("(0,-1)", Model.IsInside(FIntPoint(0, -1)));
			TestFalse("(4,0) equals Width", Model.IsInside(FIntPoint(4, 0)));
			TestFalse("(0,3) equals Height", Model.IsInside(FIntPoint(0, 3)));
			TestFalse("(4,3)", Model.IsInside(FIntPoint(4, 3)));
		});

		It("treats a field with non-positive size as empty", [this]()
		{
			Model.Initialize(0, -2, Floor.Get());
			TestFalse("(0,0)", Model.IsInside(FIntPoint(0, 0)));
			TestEqual("Tiles", Model.Tiles.Num(), 0);
		});
	});

	Describe("Indexing", [this]()
	{
		It("stores tile (X,Y) at index Y * Width + X", [this]()
		{
			Model.SetTileType(FIntPoint(2, 1), Wall.Get());

			TestEqual("Tiles count", Model.Tiles.Num(), 12);
			TestTrue("Tiles[1 * 4 + 2] is the changed tile", Model.Tiles[1 * 4 + 2].Type == Wall.Get());
			TestTrue("GetTile(2,1) points to Tiles[6]", Model.GetTile(FIntPoint(2, 1)) == &Model.Tiles[6]);
			TestTrue("GetTile(1,2) is not the changed tile", Model.GetTile(FIntPoint(1, 2))->Type == Floor.Get());
			TestFalse("(2,1) is impassable", Model.IsPassable(FIntPoint(2, 1)));
			TestTrue("(1,2) is passable", Model.IsPassable(FIntPoint(1, 2)));
		});

		It("returns nullptr from GetTile outside the field", [this]()
		{
			TestNull("(-1,0)", Model.GetTile(FIntPoint(-1, 0)));
			TestNull("(4,0)", Model.GetTile(FIntPoint(4, 0)));
			TestNull("(0,3)", Model.GetTile(FIntPoint(0, 3)));
		});

		It("ignores SetTileType outside the field", [this]()
		{
			Model.SetTileType(FIntPoint(4, 0), Wall.Get());
			Model.SetTileType(FIntPoint(0, 3), Wall.Get());
			for (const FTileData& Tile : Model.Tiles)
			{
				TestTrue("Tile keeps the default type", Tile.Type == Floor.Get());
			}
		});
	});

	Describe("Move cost", [this]()
	{
		It("returns the cost of a passable tile", [this]()
		{
			Model.SetTileType(FIntPoint(1, 1), Swamp.Get());
			TestEqual("floor", Model.GetMoveCost(FIntPoint(0, 0)), 1);
			TestEqual("swamp", Model.GetMoveCost(FIntPoint(1, 1)), 3);
		});

		It("returns 0 for impassable tiles and cells outside the field", [this]()
		{
			Model.SetTileType(FIntPoint(1, 1), Wall.Get());
			TestEqual("wall", Model.GetMoveCost(FIntPoint(1, 1)), 0);
			TestEqual("outside", Model.GetMoveCost(FIntPoint(9, 9)), 0);
		});
	});

	Describe("PlaceUnit", [this]()
	{
		It("places a unit on a free passable cell", [this]()
		{
			TestTrue("PlaceUnit", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(1, 2)));
			TestTrue("IsOccupied", Model.IsOccupied(FIntPoint(1, 2)));
			TestFalse("neighbour is free", Model.IsOccupied(FIntPoint(2, 1)));
			const FGridOccupant* Occupant = Model.GetOccupant(FIntPoint(1, 2));
			TestNotNull("GetOccupant", Occupant);
			if (Occupant)
			{
				TestEqual("Team", Occupant->Team, EUnitTeam::Hero);
			}
			TestNull("GetOccupant on a free cell", Model.GetOccupant(FIntPoint(2, 1)));
		});

		It("returns false for an occupied cell and keeps the first unit", [this]()
		{
			TestTrue("first", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(1, 1)));
			TestFalse("second", Model.PlaceUnit(MakeOccupant(EUnitTeam::Monster), FIntPoint(1, 1)));
			TestEqual("Team", Model.GetOccupant(FIntPoint(1, 1))->Team, EUnitTeam::Hero);
			TestEqual("Occupants", Model.Occupants.Num(), 1);
		});

		It("returns false for an impassable cell", [this]()
		{
			Model.SetTileType(FIntPoint(2, 2), Wall.Get());
			TestFalse("PlaceUnit", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(2, 2)));
			TestFalse("IsOccupied", Model.IsOccupied(FIntPoint(2, 2)));
		});

		It("returns false outside the field", [this]()
		{
			TestFalse("(-1,0)", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(-1, 0)));
			TestFalse("(4,0)", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(4, 0)));
			TestFalse("(0,3)", Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(0, 3)));
			TestEqual("Occupants", Model.Occupants.Num(), 0);
		});
	});

	Describe("MoveUnit", [this]()
	{
		It("moves a unit to a free cell", [this]()
		{
			Model.PlaceUnit(MakeOccupant(EUnitTeam::Monster), FIntPoint(0, 0));
			TestTrue("MoveUnit", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(3, 2)));
			TestFalse("From is free", Model.IsOccupied(FIntPoint(0, 0)));
			TestTrue("To is occupied", Model.IsOccupied(FIntPoint(3, 2)));
			TestEqual("Team is kept", Model.GetOccupant(FIntPoint(3, 2))->Team, EUnitTeam::Monster);
			TestEqual("Occupants", Model.Occupants.Num(), 1);
		});

		It("returns false when there is no unit on From", [this]()
		{
			TestFalse("MoveUnit", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(1, 0)));
			TestFalse("To stays free", Model.IsOccupied(FIntPoint(1, 0)));
		});

		It("returns false and changes nothing for an invalid To", [this]()
		{
			Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(0, 0));
			Model.PlaceUnit(MakeOccupant(EUnitTeam::Monster), FIntPoint(1, 0));
			Model.SetTileType(FIntPoint(0, 1), Wall.Get());

			TestFalse("occupied", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(1, 0)));
			TestFalse("impassable", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(0, 1)));
			TestFalse("outside", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(-1, 0)));
			TestFalse("onto itself", Model.MoveUnit(FIntPoint(0, 0), FIntPoint(0, 0)));

			TestEqual("Hero stays on From", Model.GetOccupant(FIntPoint(0, 0))->Team, EUnitTeam::Hero);
			TestEqual("Monster stays", Model.GetOccupant(FIntPoint(1, 0))->Team, EUnitTeam::Monster);
			TestEqual("Occupants", Model.Occupants.Num(), 2);
		});
	});

	Describe("RemoveUnit", [this]()
	{
		It("frees the cell and ignores empty cells", [this]()
		{
			Model.PlaceUnit(MakeOccupant(EUnitTeam::Hero), FIntPoint(2, 0));
			Model.RemoveUnit(FIntPoint(2, 0));
			Model.RemoveUnit(FIntPoint(2, 0));
			Model.RemoveUnit(FIntPoint(9, 9));
			TestFalse("IsOccupied", Model.IsOccupied(FIntPoint(2, 0)));
			TestEqual("Occupants", Model.Occupants.Num(), 0);
		});
	});

	Describe("GetNeighbors4", [this]()
	{
		It("returns four neighbours for an inner cell", [this]()
		{
			TArray<FIntPoint> Neighbors;
			Model.GetNeighbors4(FIntPoint(1, 1), Neighbors);
			TestEqual("Num", Neighbors.Num(), 4);
			TestTrue("(2,1)", Neighbors.Contains(FIntPoint(2, 1)));
			TestTrue("(0,1)", Neighbors.Contains(FIntPoint(0, 1)));
			TestTrue("(1,2)", Neighbors.Contains(FIntPoint(1, 2)));
			TestTrue("(1,0)", Neighbors.Contains(FIntPoint(1, 0)));
		});

		It("returns only cells inside the field at a corner", [this]()
		{
			TArray<FIntPoint> Neighbors;
			Model.GetNeighbors4(FIntPoint(0, 0), Neighbors);
			TestEqual("Num", Neighbors.Num(), 2);
			TestTrue("(1,0)", Neighbors.Contains(FIntPoint(1, 0)));
			TestTrue("(0,1)", Neighbors.Contains(FIntPoint(0, 1)));

			Model.GetNeighbors4(FIntPoint(3, 2), Neighbors);
			TestEqual("opposite corner replaces the old content", Neighbors.Num(), 2);
			TestTrue("(2,2)", Neighbors.Contains(FIntPoint(2, 2)));
			TestTrue("(3,1)", Neighbors.Contains(FIntPoint(3, 1)));
		});

		It("does not return diagonal neighbours", [this]()
		{
			TArray<FIntPoint> Neighbors;
			Model.GetNeighbors4(FIntPoint(1, 1), Neighbors);
			TestFalse("(2,2)", Neighbors.Contains(FIntPoint(2, 2)));
			TestFalse("(0,0)", Neighbors.Contains(FIntPoint(0, 0)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
