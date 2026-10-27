#include "Abilities/AbilityRules.h"
#include "Algo/Sort.h"
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

FIntPoint FAbilityRules::GetPushStep(EPushDirection PushDirection, FIntPoint SourcePos, FIntPoint TargetPoint, FIntPoint Position)
{
	// Шаг от опорной клетки к позиции по оси с большей разностью координат
	auto StepAway = [](FIntPoint Reference, FIntPoint Pos)
	{
		const FIntPoint Delta = Pos - Reference;
		const int32 AbsX = FMath::Abs(Delta.X);
		const int32 AbsY = FMath::Abs(Delta.Y);
		if (AbsX == AbsY)
		{
			return FIntPoint::ZeroValue;
		}
		return AbsX > AbsY ? FIntPoint(FMath::Sign(Delta.X), 0) : FIntPoint(0, FMath::Sign(Delta.Y));
	};

	switch (PushDirection)
	{
	case EPushDirection::AwayFromSource:
		return StepAway(SourcePos, Position);
	case EPushDirection::TowardSource:
		return FIntPoint::ZeroValue - StepAway(SourcePos, Position);
	case EPushDirection::AwayFromCenter:
		return StepAway(TargetPoint, Position);
	default:
		return FIntPoint::ZeroValue;
	}
}

int32 FAbilityRules::GetAreaDamage(const FResolvedAbility& A, const FAffectedArea& Area, FIntPoint Tile)
{
	int32 Damage = 0;
	for (const FAbilityEffect& Effect : A.Effects)
	{
		if (Effect.Type == EAbilityEffectType::Damage && Area.Contains(Tile, Effect.AreaPart))
		{
			Damage += Effect.Amount;
		}
	}
	return Damage;
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

namespace
{
	FAbilityEvent MakeEvent(EAbilityEventType Type, int32 UnitId, int32 OtherUnitId, int32 Amount, int32 EffectIndex)
	{
		FAbilityEvent Event;
		Event.Type = Type;
		Event.UnitId = UnitId;
		Event.OtherUnitId = OtherUnitId;
		Event.Amount = Amount;
		Event.EffectIndex = EffectIndex;
		return Event;
	}

	bool PassesFilter(ETargetFilter Filter, EUnitTeam SourceTeam, EUnitTeam Team)
	{
		switch (Filter)
		{
		case ETargetFilter::Enemies:
			return Team != SourceTeam;
		case ETargetFilter::Allies:
			return Team == SourceTeam;
		default:
			return true;
		}
	}

	// Живые юниты на клетках части области в момент вызова, по фильтру относительно команды источника.
	// Источник - цель, только если задан bAffectsSource
	void CollectTargets(const FBattleSimState& S, const FResolvedAbility& A, const FAbilityCast& Cast, const FAbilityEffect& Effect, TArray<int32>& OutIds)
	{
		OutIds.Reset();
		const FSimUnit* Source = S.FindUnit(Cast.SourceId);
		if (Source == nullptr)
		{
			return;
		}

		TArray<FIntPoint> Tiles;
		Cast.Area.GetTiles(Effect.AreaPart, Tiles);
		for (const FIntPoint& Tile : Tiles)
		{
			const FSimUnit* Unit = S.FindUnitAt(Tile);
			if (Unit == nullptr || !PassesFilter(Effect.Filter, Source->Team, Unit->Team))
			{
				continue;
			}
			if (Unit->UnitId == Cast.SourceId && !A.Area.bAffectsSource)
			{
				continue;
			}
			OutIds.Add(Unit->UnitId);
		}
	}

	void ApplyDamageEffect(FBattleSimState& S, const FAbilityCast& Cast, int32 EffectIndex, int32 UnitId, int32 Amount, bool bCollision, FAbilityOutcome& Out)
	{
		int32 Lost = 0;
		S.ApplyDamage(UnitId, Amount, Lost);
		if (Lost > 0)
		{
			FAbilityEvent Event = MakeEvent(EAbilityEventType::Damage, UnitId, Cast.SourceId, Lost, EffectIndex);
			Event.bCollision = bCollision;
			Out.Events.Add(Event);
		}
	}

	void ApplyPush(FBattleSimState& S, const FAbilityCast& Cast, const FAbilityEffect& Effect, int32 EffectIndex, const TArray<int32>& TargetIds, FAbilityOutcome& Out)
	{
		const FSimUnit* Source = S.FindUnit(Cast.SourceId);
		if (Source == nullptr || Effect.PushDistance <= 0)
		{
			return;
		}
		const FIntPoint SourcePos = Source->Position;

		struct FPushItem
		{
			int32 UnitId;
			FIntPoint Step;
			int32 Key;
		};

		// Направление и порядок считаются до начала смещения (опорные клетки в ходе эффекта не меняются)
		TArray<FPushItem> Items;
		for (int32 UnitId : TargetIds)
		{
			const FIntPoint Position = S.FindUnit(UnitId)->Position;
			FIntPoint Step = FIntPoint::ZeroValue;
			int32 Key = 0;
			switch (Effect.PushDirection)
			{
			case EPushDirection::AbilityDirection:
				Step = FAbilityRules::DirectionToOffset(Cast.Direction);
				Key = Position.X * Step.X + Position.Y * Step.Y;
				break;
			case EPushDirection::AwayFromSource:
				Step = FAbilityRules::GetPushStep(Effect.PushDirection, SourcePos, Cast.TargetPoint, Position);
				Key = FAbilityRules::Distance(Position, SourcePos);
				break;
			case EPushDirection::TowardSource:
				Step = FAbilityRules::GetPushStep(Effect.PushDirection, SourcePos, Cast.TargetPoint, Position);
				Key = -FAbilityRules::Distance(Position, SourcePos);
				break;
			case EPushDirection::AwayFromCenter:
				Step = FAbilityRules::GetPushStep(Effect.PushDirection, SourcePos, Cast.TargetPoint, Position);
				Key = FAbilityRules::Distance(Position, Cast.TargetPoint);
				break;
			}
			if (Step != FIntPoint::ZeroValue)
			{
				Items.Add({ UnitId, Step, Key });
			}
		}

		// Первым сдвигается самый дальний; при равенстве - по возрастанию UnitId
		Algo::Sort(Items, [](const FPushItem& A, const FPushItem& B)
		{
			return A.Key != B.Key ? A.Key > B.Key : A.UnitId < B.UnitId;
		});

		for (const FPushItem& Item : Items)
		{
			const FIntPoint Start = S.FindUnit(Item.UnitId)->Position;
			FIntPoint Position = Start;
			int32 Moved = 0;
			bool bCollided = false;
			int32 OtherId = INDEX_NONE;
			FIntPoint Blocked = Start;

			for (int32 Index = 0; Index < Effect.PushDistance; ++Index)
			{
				const FIntPoint Next = Position + Item.Step;
				if (!S.Grid.IsInside(Next) || !S.Grid.IsPassable(Next))
				{
					bCollided = true;
				}
				else if (const FGridOccupant* Occupant = S.Grid.GetOccupant(Next))
				{
					bCollided = true;
					OtherId = Occupant->UnitId;
				}
				else
				{
					S.MoveUnit(Item.UnitId, Next);
					Position = Next;
					++Moved;
				}
				if (bCollided)
				{
					Blocked = Next;
					break;
				}
			}

			if (Moved > 0)
			{
				FAbilityEvent Event = MakeEvent(EAbilityEventType::Push, Item.UnitId, INDEX_NONE, Moved, EffectIndex);
				Event.From = Start;
				Event.To = Position;
				Out.Events.Add(Event);
			}
			if (bCollided)
			{
				FAbilityEvent Event = MakeEvent(EAbilityEventType::Collision, Item.UnitId, OtherId, S.CollisionDamage, EffectIndex);
				Event.From = Position;
				Event.To = Blocked;
				Out.Events.Add(Event);

				ApplyDamageEffect(S, Cast, EffectIndex, Item.UnitId, S.CollisionDamage, true, Out);
				if (OtherId != INDEX_NONE)
				{
					ApplyDamageEffect(S, Cast, EffectIndex, OtherId, S.CollisionDamage, true, Out);
				}
			}
		}
	}

	void ApplyMoveSource(FBattleSimState& S, const FAbilityCast& Cast, const FAbilityEffect& Effect, int32 EffectIndex, FAbilityOutcome& Out)
	{
		const FSimUnit* Source = S.FindUnit(Cast.SourceId);
		if (Source == nullptr)
		{
			return;
		}
		const FIntPoint Start = Source->Position;
		const FIntPoint Target = Cast.TargetPoint;

		if (Effect.MoveMode == ESourceMoveMode::Dash)
		{
			// Идёт по направлению способности к точке цели, останавливается на ней или перед препятствием
			if (Cast.Direction == EGridDirection::None || FAbilityRules::GetDirection(Start, Target) != Cast.Direction)
			{
				return;
			}
			const FIntPoint Step = FAbilityRules::DirectionToOffset(Cast.Direction);
			const int32 Steps = FAbilityRules::Distance(Start, Target);
			for (int32 Index = 0; Index < Steps; ++Index)
			{
				if (!S.MoveUnit(Cast.SourceId, S.FindUnit(Cast.SourceId)->Position + Step))
				{
					break;
				}
			}
		}
		else
		{
			// Прыжок: в точку цели, если она внутри поля, проходима и свободна
			S.MoveUnit(Cast.SourceId, Target);
		}

		const FIntPoint End = S.FindUnit(Cast.SourceId)->Position;
		if (End != Start)
		{
			FAbilityEvent Event = MakeEvent(EAbilityEventType::SourceMove, Cast.SourceId, INDEX_NONE, 0, EffectIndex);
			Event.From = Start;
			Event.To = End;
			Out.Events.Add(Event);
		}
	}

	// Юниты со здоровьем 0 погибают, клетка освобождается
	void ResolveDeaths(FBattleSimState& S, const FAbilityCast& Cast, int32 EffectIndex, FAbilityOutcome& Out)
	{
		for (FSimUnit& Unit : S.Units)
		{
			if (Unit.bAlive && Unit.Health <= 0)
			{
				FAbilityEvent Event = MakeEvent(EAbilityEventType::Death, Unit.UnitId, Cast.SourceId, 0, EffectIndex);
				Event.From = Unit.Position;
				Event.To = Unit.Position;
				S.KillUnit(Unit.UnitId);
				Out.Events.Add(Event);
			}
		}
	}
}

void FAbilityRules::Simulate(FBattleSimState& S, const FResolvedAbility& A, const FAbilityCast& Cast, FAbilityOutcome& Out)
{
	Out.Cast = Cast;
	Out.Events.Reset();

	const FSimUnit* Source = S.FindUnit(Cast.SourceId);
	if (Source == nullptr || !Source->bAlive)
	{
		return;
	}

	TArray<int32> TargetIds;
	for (int32 EffectIndex = 0; EffectIndex < A.Effects.Num(); ++EffectIndex)
	{
		const FAbilityEffect& Effect = A.Effects[EffectIndex];
		switch (Effect.Type)
		{
		case EAbilityEffectType::Damage:
			CollectTargets(S, A, Cast, Effect, TargetIds);
			for (int32 UnitId : TargetIds)
			{
				ApplyDamageEffect(S, Cast, EffectIndex, UnitId, Effect.Amount, false, Out);
			}
			break;
		case EAbilityEffectType::Heal:
			CollectTargets(S, A, Cast, Effect, TargetIds);
			for (int32 UnitId : TargetIds)
			{
				int32 Gained = 0;
				S.ApplyHeal(UnitId, Effect.Amount, Gained);
				if (Gained > 0)
				{
					Out.Events.Add(MakeEvent(EAbilityEventType::Heal, UnitId, Cast.SourceId, Gained, EffectIndex));
				}
			}
			break;
		case EAbilityEffectType::Push:
			CollectTargets(S, A, Cast, Effect, TargetIds);
			ApplyPush(S, Cast, Effect, EffectIndex, TargetIds, Out);
			break;
		case EAbilityEffectType::MoveSource:
			ApplyMoveSource(S, Cast, Effect, EffectIndex, Out);
			break;
		case EAbilityEffectType::UnitStatus:
		case EAbilityEffectType::TileStatus:
			//CURRENT-STAGE: состояния не используются
			break;
		}
		ResolveDeaths(S, Cast, EffectIndex, Out);
	}
}

bool FAbilityRules::Preview(const FBattleSimState& S, const FResolvedAbility& A, const FAbilityCast& Cast, FAbilityOutcome& Out)
{
	Out = FAbilityOutcome();
	Out.Cast = Cast;
	const FSimUnit* Source = S.FindUnit(Cast.SourceId);
	if (Source == nullptr || !Source->bAlive)
	{
		return false;
	}

	FBattleSimState Copy = S;
	Simulate(Copy, A, Cast, Out);
	return true;
}
