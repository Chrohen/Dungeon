#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Abilities/AbilityParams.h"
#include "Abilities/BattleSimState.h"
#include "Data/AbilityDefinition.h"
#include "Data/TileTypeDefinition.h"

// Помощник тестов способностей и AI (Dungeon.Abilities.*, Dungeon.AI.*): снимок боя без актёров
// и способности через NewObject. Объекты держатся через TStrongObjectPtr, чтобы сборщик мусора
// не удалил их между шагами спеки. Создаётся в BeforeEach, уничтожается в AfterEach (Reset)
class FAbilityTestBed
{
public:
	~FAbilityTestBed() { Reset(); }

	// Поле Width x Height из проходимых тайлов, клетки Walls непроходимы. Заменяет прежний снимок
	void Init(const TArray<FIntPoint>& Walls = {}, int32 Width = 8, int32 Height = 8)
	{
		if (!Floor.IsValid())
		{
			Floor.Reset(NewObject<UTileTypeDefinition>());
			Floor->bPassable = true;
			Floor->MoveCost = 1;

			Wall.Reset(NewObject<UTileTypeDefinition>());
			Wall->bPassable = false;
		}

		State = FBattleSimState();
		State.Grid.Initialize(Width, Height, Floor.Get());
		for (const FIntPoint& Cell : Walls)
		{
			State.Grid.SetTileType(Cell, Wall.Get());
		}
	}

	// Добавляет живой юнит в снимок и на клетку. MaxHealth <= 0 означает "равен Health".
	// false, если клетка занята, непроходима или вне поля (юнит при этом не добавляется)
	bool AddUnit(int32 UnitId, EUnitTeam Team, FIntPoint Position, int32 Health, int32 MaxHealth = 0)
	{
		FGridOccupant Occupant;
		Occupant.Team = Team;
		Occupant.UnitId = UnitId;
		if (!State.Grid.PlaceUnit(Occupant, Position))
		{
			return false;
		}

		FSimUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Team = Team;
		Unit.Position = Position;
		Unit.Health = Health;
		Unit.MaxHealth = MaxHealth > 0 ? MaxHealth : Health;
		Unit.bAlive = true;
		State.Units.Add(Unit);
		return true;
	}

	// Новая способность с полями по умолчанию: ромб 0..1, область "тайл", без эффектов
	UAbilityDefinition* NewAbility()
	{
		Abilities.Emplace(NewObject<UAbilityDefinition>());
		return Abilities.Last().Get();
	}

	// Итоговые значения способности без переопределений
	static FResolvedAbility Resolve(const UAbilityDefinition* Def, int32 Level = 1)
	{
		return FAbilityParams::Resolve(Def, TArray<FAbilityParamValue>(), Level);
	}

	void Reset()
	{
		State = FBattleSimState();
		Abilities.Reset();
		Floor.Reset();
		Wall.Reset();
	}

	FBattleSimState State;

private:
	TStrongObjectPtr<UTileTypeDefinition> Floor;
	TStrongObjectPtr<UTileTypeDefinition> Wall;
	TArray<TStrongObjectPtr<UAbilityDefinition>> Abilities;
};

// Короткие конструкторы данных для тестов способностей
namespace AbilityTest
{
	inline FAbilityEffect MakeDamage(int32 Amount, EAreaPart Part = EAreaPart::All, ETargetFilter Filter = ETargetFilter::AllUnits)
	{
		FAbilityEffect Effect;
		Effect.Type = EAbilityEffectType::Damage;
		Effect.AreaPart = Part;
		Effect.Filter = Filter;
		Effect.Amount = Amount;
		return Effect;
	}

	inline FAbilityEffect MakeHeal(int32 Amount, EAreaPart Part = EAreaPart::All, ETargetFilter Filter = ETargetFilter::AllUnits)
	{
		FAbilityEffect Effect = MakeDamage(Amount, Part, Filter);
		Effect.Type = EAbilityEffectType::Heal;
		return Effect;
	}

	inline FAbilityEffect MakePush(EPushDirection Direction, int32 Distance, EAreaPart Part = EAreaPart::All, ETargetFilter Filter = ETargetFilter::AllUnits)
	{
		FAbilityEffect Effect;
		Effect.Type = EAbilityEffectType::Push;
		Effect.AreaPart = Part;
		Effect.Filter = Filter;
		Effect.PushDirection = Direction;
		Effect.PushDistance = Distance;
		return Effect;
	}

	inline FAbilityEffect MakeMoveSource(ESourceMoveMode Mode)
	{
		FAbilityEffect Effect;
		Effect.Type = EAbilityEffectType::MoveSource;
		Effect.MoveMode = Mode;
		return Effect;
	}

	// Совпадают ли два набора клеток без учёта порядка (повторы считаются)
	inline bool SameTiles(const TArray<FIntPoint>& A, const TArray<FIntPoint>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (const FIntPoint& Tile : A)
		{
			if (!B.Contains(Tile))
			{
				return false;
			}
		}
		return true;
	}

	// Способность с итоговыми значениями наведения без определения (для правил, которым нужна только FResolvedAbility)
	inline FResolvedAbility MakeTargeting(ETargetingShape Shape, int32 MinRange, int32 MaxRange,
		ETrajectory Trajectory = ETrajectory::Instant, ETargetRequirement Requirement = ETargetRequirement::AnyTile)
	{
		FResolvedAbility Ability;
		Ability.Targeting.Shape = Shape;
		Ability.Targeting.MinRange = MinRange;
		Ability.Targeting.MaxRange = MaxRange;
		Ability.Targeting.Trajectory = Trajectory;
		Ability.Targeting.Requirement = Requirement;
		return Ability;
	}

	inline FAbilityParamValue MakeParam(EAbilityParam Param, int32 Value, int32 EffectIndex = INDEX_NONE)
	{
		FAbilityParamValue Result;
		Result.Key.Param = Param;
		Result.Key.EffectIndex = EffectIndex;
		Result.Value = Value;
		return Result;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
