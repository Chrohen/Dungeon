#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/TileTypeDefinition.h"
#include "Grid/GridModel.h"
#include "Grid/GridPathfinder.h"

namespace
{
	FGridOccupant MakePathOccupant(EUnitTeam Team)
	{
		FGridOccupant Occupant;
		Occupant.Unit = nullptr;
		Occupant.Team = Team;
		return Occupant;
	}

	FPathQuery MakeQuery(FIntPoint Origin, int32 MovePoints, EUnitTeam Team = EUnitTeam::Hero)
	{
		FPathQuery Query;
		Query.Origin = Origin;
		Query.MovePoints = MovePoints;
		Query.Team = Team;
		return Query;
	}
}

BEGIN_DEFINE_SPEC(FGridPathfinderSpec, "Dungeon.Grid.Pathfinder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	// Тип тайла держится через TStrongObjectPtr, чтобы сборщик мусора не удалил его между шагами спеки
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UTileTypeDefinition> Wall;
	TStrongObjectPtr<UTileTypeDefinition> Swamp;
	FGridModel Model;
END_DEFINE_SPEC(FGridPathfinderSpec)

void FGridPathfinderSpec::Define()
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

		Model = FGridModel();
		Model.Initialize(8, 8, Floor.Get());
	});

	AfterEach([this]()
	{
		Model = FGridModel();
		Floor.Reset();
		Wall.Reset();
		Swamp.Reset();
	});

	Describe("ComputeReachable on an empty field", [this]()
	{
		It("reaches every cell within Manhattan distance 5 of (3,3)", [this]()
		{
			const FIntPoint Origin(3, 3);
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(Origin, 5));

			int32 Expected = 0;
			for (int32 Y = 0; Y < 8; ++Y)
			{
				for (int32 X = 0; X < 8; ++X)
				{
					const int32 Distance = FMath::Abs(X - Origin.X) + FMath::Abs(Y - Origin.Y);
					const FPathNode* Node = Area.Nodes.Find(FIntPoint(X, Y));
					if (Distance <= 5)
					{
						++Expected;
						TestNotNull(FString::Printf(TEXT("(%d,%d) is reachable"), X, Y), Node);
						if (Node)
						{
							TestEqual(FString::Printf(TEXT("(%d,%d) cost"), X, Y), Node->Cost, Distance);
						}
					}
					else
					{
						TestNull(FString::Printf(TEXT("(%d,%d) is out of range"), X, Y), Node);
					}
				}
			}
			TestEqual("Nodes count", Area.Nodes.Num(), Expected);
			TestEqual("Destinations exclude only Origin", Area.GetDestinations().Num(), Expected - 1);
		});

		It("does not count Origin as a destination", [this]()
		{
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(3, 3), 5));
			const FPathNode* OriginNode = Area.Nodes.Find(FIntPoint(3, 3));
			TestNotNull("Origin is in Nodes", OriginNode);
			if (OriginNode)
			{
				TestEqual("Origin cost", OriginNode->Cost, 0);
			}
			TestFalse("IsDestination(Origin)", Area.IsDestination(FIntPoint(3, 3)));
			TestFalse("GetDestinations has no Origin", Area.GetDestinations().Contains(FIntPoint(3, 3)));
			TestTrue("IsDestination(neighbour)", Area.IsDestination(FIntPoint(3, 4)));
			TestFalse("IsDestination outside Nodes", Area.IsDestination(FIntPoint(0, 0)));
		});

		It("returns only Origin without move points", [this]()
		{
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(3, 3), 0));
			TestEqual("Nodes count", Area.Nodes.Num(), 1);
			TestEqual("Destinations", Area.GetDestinations().Num(), 0);
		});

		It("returns nothing when Origin is outside the field", [this]()
		{
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(8, 0), 5));
			TestEqual("Nodes count", Area.Nodes.Num(), 0);
		});
	});

	Describe("Units", [this]()
	{
		It("is blocked by monsters", [this]()
		{
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Hero), FIntPoint(0, 0));
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Monster), FIntPoint(1, 0));
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Monster), FIntPoint(0, 1));

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 5));
			TestEqual("Only Origin remains", Area.Nodes.Num(), 1);
			TestFalse("monster cell (1,0) is not in Nodes", Area.Nodes.Contains(FIntPoint(1, 0)));
		});

		It("goes around a monster", [this]()
		{
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Hero), FIntPoint(0, 0));
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Monster), FIntPoint(1, 0));

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 5));
			TestFalse("monster cell is not a destination", Area.IsDestination(FIntPoint(1, 0)));
			const FPathNode* Node = Area.Nodes.Find(FIntPoint(2, 0));
			TestNotNull("(2,0) is reachable by a detour", Node);
			if (Node)
			{
				TestEqual("detour cost", Node->Cost, 4);
			}
		});

		It("passes through an ally but cannot stop on its cell", [this]()
		{
			Model.Initialize(5, 1, Floor.Get());
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Hero), FIntPoint(0, 0));
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Hero), FIntPoint(1, 0));

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 3));
			const FPathNode* AllyNode = Area.Nodes.Find(FIntPoint(1, 0));
			TestNotNull("ally cell is in Nodes", AllyNode);
			if (AllyNode)
			{
				TestFalse("ally cell bCanStop", AllyNode->bCanStop);
			}
			TestFalse("IsDestination(ally cell)", Area.IsDestination(FIntPoint(1, 0)));
			TestTrue("IsDestination behind the ally", Area.IsDestination(FIntPoint(2, 0)));
			TestTrue("(3,0) with the last point", Area.IsDestination(FIntPoint(3, 0)));
			TestFalse("(4,0) is too far", Area.Nodes.Contains(FIntPoint(4, 0)));
			TestFalse("ally cell is not in GetDestinations", Area.GetDestinations().Contains(FIntPoint(1, 0)));
		});

		It("is blocked by heroes when searching for a monster", [this]()
		{
			Model.Initialize(5, 1, Floor.Get());
			Model.PlaceUnit(MakePathOccupant(EUnitTeam::Hero), FIntPoint(1, 0));

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 4, EUnitTeam::Monster));
			TestFalse("hero blocks a monster", Area.Nodes.Contains(FIntPoint(1, 0)));
			TestFalse("nothing behind the hero", Area.Nodes.Contains(FIntPoint(2, 0)));
		});
	});

	Describe("Tiles", [this]()
	{
		It("is blocked by an impassable tile", [this]()
		{
			Model.Initialize(5, 1, Floor.Get());
			Model.SetTileType(FIntPoint(1, 0), Wall.Get());

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 5));
			TestFalse("wall is not in Nodes", Area.Nodes.Contains(FIntPoint(1, 0)));
			TestFalse("cell behind the wall is not in Nodes", Area.Nodes.Contains(FIntPoint(2, 0)));
		});

		It("spends the move cost of a tile when entering it", [this]()
		{
			Model.Initialize(6, 1, Floor.Get());
			Model.SetTileType(FIntPoint(1, 0), Swamp.Get());

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 5));
			TestEqual("swamp cost", Area.Nodes.FindRef(FIntPoint(1, 0)).Cost, 3);
			TestEqual("cell behind the swamp", Area.Nodes.FindRef(FIntPoint(2, 0)).Cost, 4);
			TestEqual("last reachable cell", Area.Nodes.FindRef(FIntPoint(3, 0)).Cost, 5);
			TestFalse("(4,0) costs 6", Area.Nodes.Contains(FIntPoint(4, 0)));
		});

		It("does not enter a tile that costs more than the remaining points", [this]()
		{
			Model.Initialize(3, 1, Floor.Get());
			Model.SetTileType(FIntPoint(1, 0), Swamp.Get());

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 2));
			TestEqual("Nodes count", Area.Nodes.Num(), 1);
		});

		It("prefers a cheaper detour over an expensive direct tile", [this]()
		{
			Swamp->MoveCost = 5;
			Model.Initialize(3, 2, Floor.Get());
			Model.SetTileType(FIntPoint(1, 0), Swamp.Get());

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 9));
			const FPathNode Node = Area.Nodes.FindRef(FIntPoint(2, 0));
			TestEqual("cost through the lower row", Node.Cost, 4);
			TestEqual("parent", Node.Parent, FIntPoint(2, 1));
		});
	});

	Describe("BuildPath", [this]()
	{
		It("goes around an obstacle and excludes Origin", [this]()
		{
			Model.Initialize(3, 3, Floor.Get());
			Model.SetTileType(FIntPoint(1, 0), Wall.Get());

			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(0, 0), 5));
			TArray<FIntPoint> Path;
			TestTrue("BuildPath", FGridPathfinder::BuildPath(Area, FIntPoint(2, 0), Path));

			const TArray<FIntPoint> Expected = { FIntPoint(0, 1), FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(2, 0) };
			TestEqual("Path length", Path.Num(), Expected.Num());
			if (Path.Num() == Expected.Num())
			{
				for (int32 Index = 0; Index < Expected.Num(); ++Index)
				{
					TestEqual(FString::Printf(TEXT("Path[%d]"), Index), Path[Index], Expected[Index]);
				}
			}
		});

		It("returns false for a cell outside Nodes", [this]()
		{
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(3, 3), 2));
			TArray<FIntPoint> Path = { FIntPoint(9, 9) };
			TestFalse("BuildPath", FGridPathfinder::BuildPath(Area, FIntPoint(7, 7), Path));
			TestEqual("Path is cleared", Path.Num(), 0);
		});

		It("returns an empty path for Origin", [this]()
		{
			const FReachableArea Area = FGridPathfinder::ComputeReachable(Model, MakeQuery(FIntPoint(3, 3), 2));
			TArray<FIntPoint> Path;
			TestTrue("BuildPath", FGridPathfinder::BuildPath(Area, FIntPoint(3, 3), Path));
			TestEqual("Path length", Path.Num(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
