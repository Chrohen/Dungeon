#include "Grid/GridPathfinder.h"
#include "Grid/GridModel.h"
#include "Algo/Reverse.h"

FReachableArea FGridPathfinder::ComputeReachable(const FGridModel& Model, const FPathQuery& Query)
{
	FReachableArea Area;
	Area.Origin = Query.Origin;
	if (!Model.IsInside(Query.Origin))
	{
		return Area;
	}

	FPathNode OriginNode;
	OriginNode.Cost = 0;
	OriginNode.Parent = Query.Origin;
	OriginNode.bCanStop = false;
	Area.Nodes.Add(Query.Origin, OriginNode);

	// Поле маленькое, поэтому минимум в открытом списке ищется линейно
	TArray<FIntPoint> Open;
	Open.Add(Query.Origin);
	TSet<FIntPoint> Closed;
	TArray<FIntPoint> Neighbors;

	while (Open.Num() > 0)
	{
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Open.Num(); ++Index)
		{
			if (Area.Nodes[Open[Index]].Cost < Area.Nodes[Open[BestIndex]].Cost)
			{
				BestIndex = Index;
			}
		}
		const FIntPoint Current = Open[BestIndex];
		Open.RemoveAtSwap(BestIndex);
		Closed.Add(Current);

		const int32 CurrentCost = Area.Nodes[Current].Cost;
		Model.GetNeighbors4(Current, Neighbors);
		for (const FIntPoint& Neighbor : Neighbors)
		{
			if (Closed.Contains(Neighbor) || !Model.IsPassable(Neighbor))
			{
				continue;
			}

			const FGridOccupant* Occupant = Model.GetOccupant(Neighbor);
			if (Occupant != nullptr && Occupant->Team != Query.Team)
			{
				continue;
			}

			const int32 NewCost = CurrentCost + Model.GetMoveCost(Neighbor);
			if (NewCost > Query.MovePoints)
			{
				continue;
			}

			FPathNode* Existing = Area.Nodes.Find(Neighbor);
			if (Existing == nullptr || NewCost < Existing->Cost)
			{
				FPathNode Node;
				Node.Cost = NewCost;
				Node.Parent = Current;
				Node.bCanStop = (Occupant == nullptr);
				Area.Nodes.Add(Neighbor, Node);
				Open.AddUnique(Neighbor);
			}
		}
	}

	return Area;
}

bool FGridPathfinder::BuildPath(const FReachableArea& Area, FIntPoint Target, TArray<FIntPoint>& OutPath)
{
	OutPath.Reset();
	if (!Area.Nodes.Contains(Target))
	{
		return false;
	}

	// Ограничение на число шагов защищает от цикла в испорченных данных
	FIntPoint Current = Target;
	for (int32 Step = 0; Current != Area.Origin; ++Step)
	{
		const FPathNode* Node = Area.Nodes.Find(Current);
		if (Node == nullptr || Step >= Area.Nodes.Num())
		{
			OutPath.Reset();
			return false;
		}
		OutPath.Add(Current);
		Current = Node->Parent;
	}

	Algo::Reverse(OutPath);
	return true;
}
