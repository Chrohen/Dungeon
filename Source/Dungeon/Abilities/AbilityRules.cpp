#include "Abilities/AbilityRules.h"
#include "Grid/GridModel.h"

int32 FAbilityRules::Distance(FIntPoint A, FIntPoint B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

EGridDirection FAbilityRules::GetDirection(FIntPoint From, FIntPoint To)
{
	if (From == To)
	{
		return EGridDirection::None;
	}
	if (From.X == To.X)
	{
		return To.Y > From.Y ? EGridDirection::Down : EGridDirection::Up;
	}
	if (From.Y == To.Y)
	{
		return To.X > From.X ? EGridDirection::Right : EGridDirection::Left;
	}
	return EGridDirection::None;
}

FIntPoint FAbilityRules::DirectionToOffset(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:
		return FIntPoint(0, -1);
	case EGridDirection::Down:
		return FIntPoint(0, 1);
	case EGridDirection::Left:
		return FIntPoint(-1, 0);
	case EGridDirection::Right:
		return FIntPoint(1, 0);
	default:
		return FIntPoint::ZeroValue;
	}
}

FIntPoint FAbilityRules::RotateOffset(FIntPoint OffsetForRight, EGridDirection Dir)
{
	const int32 A = OffsetForRight.X;
	const int32 B = OffsetForRight.Y;
	switch (Dir)
	{
	case EGridDirection::Down:
		return FIntPoint(-B, A);
	case EGridDirection::Left:
		return FIntPoint(-A, -B);
	case EGridDirection::Up:
		return FIntPoint(B, -A);
	default:
		return OffsetForRight;
	}
}

bool FAbilityRules::IsInTargetingShape(FIntPoint Source, FIntPoint Tile, const FAbilityTargeting& T)
{
	const int32 Dist = Distance(Source, Tile);
	if (Dist < T.MinRange || Dist > T.MaxRange)
	{
		return false;
	}
	return T.Shape != ETargetingShape::Cross || Source.X == Tile.X || Source.Y == Tile.Y;
}

FIntPoint FAbilityRules::TraceProjectile(const FGridModel& Grid, FIntPoint Source, EGridDirection Dir, int32 MaxRange)
{
	const FIntPoint Step = DirectionToOffset(Dir);
	FIntPoint Last = Source;
	for (int32 Index = 1; Index <= MaxRange; ++Index)
	{
		const FIntPoint Tile = Source + Step * Index;
		if (!Grid.IsInside(Tile))
		{
			break;
		}
		if (!Grid.IsPassable(Tile) || Grid.IsOccupied(Tile))
		{
			return Tile;
		}
		Last = Tile;
	}
	return Last;
}

bool FAbilityRules::CheckRequirement(const FBattleSimState& S, int32 SourceId, FIntPoint Tile, ETargetRequirement R)
{
	switch (R)
	{
	case ETargetRequirement::AnyTile:
		return S.Grid.IsInside(Tile);
	case ETargetRequirement::Unit:
		return S.FindUnitAt(Tile) != nullptr;
	case ETargetRequirement::Enemy:
	case ETargetRequirement::Ally:
	{
		const FSimUnit* Source = S.FindUnit(SourceId);
		const FSimUnit* Target = S.FindUnitAt(Tile);
		if (Source == nullptr || Target == nullptr)
		{
			return false;
		}
		const bool bSameTeam = Target->Team == Source->Team;
		return R == ETargetRequirement::Ally ? bSameTeam : !bSameTeam;
	}
	case ETargetRequirement::FreePassable:
		return S.Grid.IsPassable(Tile) && !S.Grid.IsOccupied(Tile);
	}
	return false;
}

bool FAbilityRules::MakeCast(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, FIntPoint SelectedTile, FAbilityCast& Out)
{
	// 1) источник жив, клетка внутри поля и в форме наведения
	const FSimUnit* Source = S.FindUnit(SourceId);
	if (Source == nullptr || !Source->bAlive
		|| !S.Grid.IsInside(SelectedTile)
		|| !IsInTargetingShape(Source->Position, SelectedTile, A.Targeting))
	{
		return false;
	}

	// 2) направление
	FAbilityCast Cast;
	Cast.SourceId = SourceId;
	Cast.SourcePosition = Source->Position;
	Cast.SelectedTile = SelectedTile;
	Cast.Direction = GetDirection(Source->Position, SelectedTile);

	// 3) точка цели. MinRange ограничивает только выбор клетки, снаряд летит с соседней клетки
	Cast.TargetPoint = A.Targeting.Trajectory == ETrajectory::Projectile
		? TraceProjectile(S.Grid, Source->Position, Cast.Direction, A.Targeting.MaxRange)
		: SelectedTile;

	// 4) требование проверяется для фактической точки цели
	if (!CheckRequirement(S, SourceId, Cast.TargetPoint, A.Targeting.Requirement))
	{
		return false;
	}

	// 5) область
	BuildArea(S.Grid, Source->Position, Cast.TargetPoint, Cast.Direction, A.Area, Cast.Area);
	Out = MoveTemp(Cast);
	return true;
}

bool FAbilityRules::MakeIntentCast(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, const FMonsterIntent& Intent, FAbilityCast& Out)
{
	const FSimUnit* Source = S.FindUnit(SourceId);
	if (Source == nullptr || !Source->bAlive)
	{
		return false;
	}

	FAbilityCast Cast;
	Cast.SourceId = SourceId;
	Cast.SourcePosition = Source->Position;
	Cast.SelectedTile = Source->Position + Intent.SelectedOffset;
	Cast.Direction = Intent.Direction;
	Cast.TargetPoint = A.Targeting.Trajectory == ETrajectory::Projectile
		? TraceProjectile(S.Grid, Source->Position, Cast.Direction, A.Targeting.MaxRange)
		: Cast.SelectedTile;
	BuildArea(S.Grid, Source->Position, Cast.TargetPoint, Cast.Direction, A.Area, Cast.Area);
	Out = MoveTemp(Cast);
	return true;
}

void FAbilityRules::GetSelectableTiles(const FBattleSimState& S, int32 SourceId, const FResolvedAbility& A, TArray<FIntPoint>& Out)
{
	Out.Reset();
	FAbilityCast Cast;
	for (int32 Y = 0; Y < S.Grid.Height; ++Y)
	{
		for (int32 X = 0; X < S.Grid.Width; ++X)
		{
			if (MakeCast(S, SourceId, A, FIntPoint(X, Y), Cast))
			{
				Out.Add(FIntPoint(X, Y));
			}
		}
	}
}

void FAbilityRules::BuildArea(const FGridModel& Grid, FIntPoint SourcePos, FIntPoint Target, EGridDirection Dir, const FAbilityAreaSpec& Spec, FAffectedArea& Out)
{
	Out = FAffectedArea();
	Out.Center = Target;

	bool bHasCenter = true;
	TArray<FIntPoint> Edge;
	const FIntPoint Step = DirectionToOffset(Dir);
	const bool bNeedsDirection = Spec.Type == EAreaType::Line || Spec.Type == EAreaType::CrossLine || Spec.Type == EAreaType::Path;

	if (bNeedsDirection && Dir == EGridDirection::None)
	{
		bHasCenter = false;
	}
	else
	{
		switch (Spec.Type)
		{
		case EAreaType::Tile:
			break;
		case EAreaType::Diamond:
			for (int32 DY = -Spec.Size; DY <= Spec.Size; ++DY)
			{
				const int32 Span = Spec.Size - FMath::Abs(DY);
				for (int32 DX = -Span; DX <= Span; ++DX)
				{
					if (DX != 0 || DY != 0)
					{
						Edge.Add(Target + FIntPoint(DX, DY));
					}
				}
			}
			break;
		case EAreaType::Line:
			// Size - длина вместе с точкой цели
			for (int32 Index = 1; Index < Spec.Size; ++Index)
			{
				Edge.Add(Target + Step * Index);
			}
			break;
		case EAreaType::CrossLine:
		{
			const FIntPoint Side(Step.Y != 0 ? 1 : 0, Step.X != 0 ? 1 : 0);
			for (int32 Index = 1; Index <= Spec.Size; ++Index)
			{
				Edge.Add(Target + Side * Index);
				Edge.Add(Target - Side * Index);
			}
			break;
		}
		case EAreaType::Path:
			// От соседней с источником клетки до точки цели, не включая её
			if (GetDirection(SourcePos, Target) == Dir)
			{
				for (int32 Index = 1; Index < Distance(SourcePos, Target); ++Index)
				{
					Edge.Add(SourcePos + Step * Index);
				}
			}
			break;
		case EAreaType::Pattern:
			bHasCenter = false;
			for (const FIntPoint& Offset : Spec.Offsets)
			{
				if (Offset == FIntPoint::ZeroValue)
				{
					bHasCenter = true;
				}
				else
				{
					Edge.AddUnique(Target + RotateOffset(Offset, Dir));
				}
			}
			break;
		}
	}

	// Клетки вне поля и тайл источника не входят в область. bAffectsSource тайл не добавляет
	auto IsExcluded = [&](const FIntPoint& Tile)
	{
		return !Grid.IsInside(Tile) || (!Spec.bAffectsSource && Tile == SourcePos);
	};
	Out.bHasCenter = bHasCenter && !IsExcluded(Target);
	for (const FIntPoint& Tile : Edge)
	{
		if (!IsExcluded(Tile))
		{
			Out.Edge.Add(Tile);
		}
	}
}
