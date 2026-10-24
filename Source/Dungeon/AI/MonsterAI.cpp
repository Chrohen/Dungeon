#include "AI/MonsterAI.h"
#include "Abilities/AbilityRules.h"
#include "Grid/GridPathfinder.h"

namespace
{
	// Оценки равны, если отличаются меньше допуска (Plan-2, решение 9)
	constexpr float ScoreTolerance = 1e-4f;

	// Число значений EGridDirection
	constexpr int32 DirectionCount = 5;

	// Тайл перемещения и стоимость пути до него
	struct FMoveTile
	{
		FIntPoint Tile = FIntPoint::ZeroValue;
		int32 PathCost = 0;
	};

	bool IsLiveHero(const FSimUnit& Unit)
	{
		return Unit.bAlive && Unit.Team == EUnitTeam::Hero;
	}

	// Ожидаемое здоровье героя. Героя нет в контексте - берётся текущее здоровье
	int32 GetExpectedHealth(const FMonsterAIContext& Context, const FSimUnit& Hero)
	{
		const int32* Expected = Context.ExpectedHealth.Find(Hero.UnitId);
		return FMath::Max(0, Expected != nullptr ? *Expected : Hero.Health);
	}

	// Расстояние от клетки до ближайшего живого героя или INDEX_NONE, если героев нет
	int32 GetNearestHeroDistance(const FBattleSimState& S, FIntPoint Tile)
	{
		int32 Nearest = INDEX_NONE;
		for (const FSimUnit& Unit : S.Units)
		{
			if (IsLiveHero(Unit))
			{
				const int32 Distance = FAbilityRules::Distance(Tile, Unit.Position);
				Nearest = Nearest == INDEX_NONE ? Distance : FMath::Min(Nearest, Distance);
			}
		}
		return Nearest;
	}

	// Тайлы перемещения: текущая клетка (стоимость 0), затем конечные клетки поиска пути по строкам.
	// Порядок фиксирован, чтобы случайный выбор при заданном зерне был воспроизводим
	void GetMoveTiles(const FBattleSimState& S, const FSimUnit& Monster, int32 MovePoints, bool bAllowMove, TArray<FMoveTile>& Out)
	{
		Out.Reset();
		Out.Add({ Monster.Position, 0 });
		if (!bAllowMove)
		{
			return;
		}

		FPathQuery Query;
		Query.Origin = Monster.Position;
		Query.MovePoints = MovePoints;
		Query.Team = Monster.Team;
		const FReachableArea Area = FGridPathfinder::ComputeReachable(S.Grid, Query);

		TArray<FMoveTile> Destinations;
		for (const TPair<FIntPoint, FPathNode>& Pair : Area.Nodes)
		{
			if (Area.IsDestination(Pair.Key))
			{
				Destinations.Add({ Pair.Key, Pair.Value.Cost });
			}
		}
		Destinations.Sort([](const FMoveTile& A, const FMoveTile& B)
		{
			return A.Tile.Y != B.Tile.Y ? A.Tile.Y < B.Tile.Y : A.Tile.X < B.Tile.X;
		});
		Out.Append(Destinations);
	}

	// Клетки цели, которые рассматривает AI. Для снаряда все клетки одного направления дают одну
	// точку цели, поэтому берётся ближайшая к монстру допустимая; клетка без направления остаётся
	void GetCandidateTargets(FIntPoint SourcePos, const FAbilityTargeting& Targeting, const TArray<FIntPoint>& Selectable, TArray<FIntPoint>& Out)
	{
		if (Targeting.Trajectory != ETrajectory::Projectile)
		{
			Out = Selectable;
			return;
		}

		int32 NearestDistance[DirectionCount];
		for (int32& Distance : NearestDistance)
		{
			Distance = MAX_int32;
		}
		for (const FIntPoint& Tile : Selectable)
		{
			const int32 Dir = static_cast<int32>(FAbilityRules::GetDirection(SourcePos, Tile));
			NearestDistance[Dir] = FMath::Min(NearestDistance[Dir], FAbilityRules::Distance(SourcePos, Tile));
		}

		Out.Reset();
		for (const FIntPoint& Tile : Selectable)
		{
			const EGridDirection Direction = FAbilityRules::GetDirection(SourcePos, Tile);
			if (Direction == EGridDirection::None
				|| FAbilityRules::Distance(SourcePos, Tile) == NearestDistance[static_cast<int32>(Direction)])
			{
				Out.Add(Tile);
			}
		}
	}

	// Индекс лучшего варианта: наибольший Key (равны в пределах допуска), затем меньшая стоимость
	// пути, затем случайный среди оставшихся в порядке перебора. Keys не пуст
	int32 SelectBest(const TArray<float>& Keys, const TArray<int32>& PathCosts, FRandomStream& Random)
	{
		float MaxKey = Keys[0];
		for (const float Key : Keys)
		{
			MaxKey = FMath::Max(MaxKey, Key);
		}

		int32 MinCost = MAX_int32;
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			if (FMath::IsNearlyEqual(Keys[Index], MaxKey, ScoreTolerance))
			{
				MinCost = FMath::Min(MinCost, PathCosts[Index]);
			}
		}

		TArray<int32> Best;
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			if (FMath::IsNearlyEqual(Keys[Index], MaxKey, ScoreTolerance) && PathCosts[Index] == MinCost)
			{
				Best.Add(Index);
			}
		}
		return Best.Num() == 1 ? Best[0] : Best[Random.RandRange(0, Best.Num() - 1)];
	}

	// Считает bAttacking, bSafe и Score варианта. Local - снимок с монстром на тайле перемещения
	// до применения способности, HeroDistance - расстояние до ближайшего живого героя в нём
	void Evaluate(const FBattleSimState& Local, int32 HeroDistance, const FMonsterBehaviorProfile& Profile,
		const FMonsterAIContext& Context, FAIVariant& Variant)
	{
		int32 Damage = 0;
		int32 Kills = 0;
		Variant.bAttacking = false;
		for (const FSimUnit& Unit : Local.Units)
		{
			if (!IsLiveHero(Unit))
			{
				continue;
			}
			const int32 Dealt = Variant.Outcome.GetDamageTo(Unit.UnitId);
			const int32 Expected = GetExpectedHealth(Context, Unit);
			Variant.bAttacking |= Dealt > 0;
			Damage += FMath::Min(Dealt, Expected);
			Kills += (Expected > 0 && Dealt >= Expected) ? 1 : 0;
		}

		// Безопасный: способность не наносит урон монстрам (включая себя и столкновения),
		// а тайл не в области намерений предыдущих монстров
		Variant.bSafe = !Context.DangerTiles.Contains(Variant.MoveTile);
		for (const FAbilityEvent& Event : Variant.Outcome.Events)
		{
			if (Event.Type == EAbilityEventType::Damage)
			{
				const FSimUnit* Victim = Local.FindUnit(Event.UnitId);
				if (Victim != nullptr && Victim->Team == EUnitTeam::Monster)
				{
					Variant.bSafe = false;
					break;
				}
			}
		}

		Variant.Score = Damage * Profile.DamageWeight + Kills * Profile.KillWeight
			- FMath::Abs(HeroDistance - Profile.PreferredDistance) * Profile.DistanceWeight;
	}
}

FAIVariant FMonsterAI::ChooseVariant(const FBattleSimState& S, int32 MonsterId, int32 MovePoints,
	const TArray<FUnitAbilityState>& Abilities, const FMonsterBehaviorProfile& Profile, bool bAllowMove,
	const FMonsterAIContext& Context, FRandomStream& Random)
{
	const FSimUnit* Monster = S.FindUnit(MonsterId);
	if (Monster == nullptr || !Monster->bAlive)
	{
		return FAIVariant();
	}
	const FIntPoint Origin = Monster->Position;

	// Вариант "текущая клетка, без способности": ожидание, а без героев и без атаки в фазе намерений - нет намерения
	FAIVariant Idle;
	Idle.MoveTile = Origin;
	Idle.bSafe = !Context.DangerTiles.Contains(Origin);
	if (GetNearestHeroDistance(S, Origin) == INDEX_NONE)
	{
		return Idle;
	}

	TArray<FMoveTile> MoveTiles;
	GetMoveTiles(S, *Monster, MovePoints, bAllowMove, MoveTiles);

	TArray<FAIVariant> Attacking;
	// Кандидаты на движение к героям: ключ - минус отклонение от предпочтительной дистанции
	TArray<FMoveTile> MoveCandidates;
	TArray<float> MoveKeys;
	TArray<int32> MoveCosts;

	for (const FMoveTile& Move : MoveTiles)
	{
		FBattleSimState Local = S;
		if (Move.Tile != Origin && !Local.MoveUnit(MonsterId, Move.Tile))
		{
			continue;
		}

		const int32 HeroDistance = GetNearestHeroDistance(Local, Move.Tile);
		MoveCandidates.Add(Move);
		MoveKeys.Add(-static_cast<float>(FMath::Abs(HeroDistance - Profile.PreferredDistance)));
		MoveCosts.Add(Move.PathCost);

		for (int32 AbilityIndex = 0; AbilityIndex < Abilities.Num(); ++AbilityIndex)
		{
			const FUnitAbilityState& State = Abilities[AbilityIndex];
			if (!State.IsReady())
			{
				continue;
			}

			TArray<FIntPoint> Selectable;
			FAbilityRules::GetSelectableTiles(Local, MonsterId, State.Ability, Selectable);
			TArray<FIntPoint> Targets;
			GetCandidateTargets(Move.Tile, State.Ability.Targeting, Selectable, Targets);

			for (const FIntPoint& Target : Targets)
			{
				FAIVariant Variant;
				Variant.MoveTile = Move.Tile;
				Variant.PathCost = Move.PathCost;
				Variant.AbilityIndex = AbilityIndex;
				Variant.SelectedTile = Target;

				FAbilityCast Cast;
				if (!FAbilityRules::MakeCast(Local, MonsterId, State.Ability, Target, Cast)
					|| !FAbilityRules::Preview(Local, State.Ability, Cast, Variant.Outcome))
				{
					continue;
				}

				Evaluate(Local, HeroDistance, Profile, Context, Variant);
				if (Variant.bAttacking)
				{
					Attacking.Add(MoveTemp(Variant));
				}
			}
		}
	}

	// Атакующие варианты: сначала безопасные, если их нет - все
	if (Attacking.Num() > 0)
	{
		TArray<int32> Candidates;
		for (int32 Index = 0; Index < Attacking.Num(); ++Index)
		{
			if (Attacking[Index].bSafe)
			{
				Candidates.Add(Index);
			}
		}
		if (Candidates.IsEmpty())
		{
			for (int32 Index = 0; Index < Attacking.Num(); ++Index)
			{
				Candidates.Add(Index);
			}
		}

		TArray<float> Keys;
		TArray<int32> Costs;
		for (const int32 Index : Candidates)
		{
			Keys.Add(Attacking[Index].Score);
			Costs.Add(Attacking[Index].PathCost);
		}
		return MoveTemp(Attacking[Candidates[SelectBest(Keys, Costs, Random)]]);
	}

	// Движение к героям: ближе всего к предпочтительной дистанции, текущая клетка выигрывает ничью по стоимости пути
	if (bAllowMove && MoveCandidates.Num() > 0)
	{
		const int32 Best = SelectBest(MoveKeys, MoveCosts, Random);
		FAIVariant Variant;
		Variant.MoveTile = MoveCandidates[Best].Tile;
		Variant.PathCost = MoveCandidates[Best].PathCost;
		Variant.Score = MoveKeys[Best] * Profile.DistanceWeight;
		Variant.bSafe = !Context.DangerTiles.Contains(Variant.MoveTile);
		return Variant;
	}

	return Idle;
}

void FMonsterAI::RegisterChoice(const FBattleSimState& S, const FAIVariant& Variant, FMonsterAIContext& Context)
{
	if (!Variant.bAttacking)
	{
		return;
	}

	for (const FIntPoint& Tile : Variant.Outcome.Cast.Area.GetAllTiles())
	{
		Context.DangerTiles.Add(Tile);
	}

	for (const FSimUnit& Unit : S.Units)
	{
		if (!IsLiveHero(Unit))
		{
			continue;
		}
		const int32 Damage = Variant.Outcome.GetDamageTo(Unit.UnitId);
		if (Damage > 0)
		{
			Context.ExpectedHealth.Add(Unit.UnitId, FMath::Max(0, GetExpectedHealth(Context, Unit) - Damage));
		}
	}
}

FMonsterAIContext FMonsterAI::MakeContext(const FBattleSimState& S)
{
	FMonsterAIContext Context;
	for (const FSimUnit& Unit : S.Units)
	{
		if (IsLiveHero(Unit))
		{
			Context.ExpectedHealth.Add(Unit.UnitId, Unit.Health);
		}
	}
	return Context;
}
