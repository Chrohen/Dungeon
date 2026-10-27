#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/AbilityRules.h"
#include "Tests/AbilityTestHelpers.h"

using namespace AbilityTest;

namespace
{
	constexpr int32 EffectsHeroId = 1;
}

BEGIN_DEFINE_SPEC(FAbilityEffectsSpec, "Dungeon.Abilities.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
	FResolvedAbility Ability;
	FAbilityOutcome Outcome;

	// Новое поле со стенами; источник (герой, id 1, здоровье 5) стоит на Source
	void NewField(const TArray<FIntPoint>& Walls = {}, FIntPoint Source = FIntPoint(1, 1))
	{
		Bed.Init(Walls);
		Bed.AddUnit(EffectsHeroId, EUnitTeam::Hero, Source, 5);
		Ability = FResolvedAbility();
		Outcome = FAbilityOutcome();
	}

	void Setup(ETargetingShape Shape, int32 MinRange, int32 MaxRange, EAreaType AreaType = EAreaType::Tile, int32 AreaSize = 0)
	{
		Ability = MakeTargeting(Shape, MinRange, MaxRange);
		Ability.Area.Type = AreaType;
		Ability.Area.Size = AreaSize;
	}

	// MakeCast и Simulate на снимке Bed.State
	bool Run(FIntPoint Selected)
	{
		FAbilityCast Cast;
		if (!FAbilityRules::MakeCast(Bed.State, EffectsHeroId, Ability, Selected, Cast))
		{
			return false;
		}
		FAbilityRules::Simulate(Bed.State, Ability, Cast, Outcome);
		return true;
	}

	TArray<FAbilityEvent> EventsOf(EAbilityEventType Type) const
	{
		return Outcome.Events.FilterByPredicate([Type](const FAbilityEvent& Event) { return Event.Type == Type; });
	}

	TArray<int32> UnitIdsOf(EAbilityEventType Type) const
	{
		TArray<int32> Result;
		for (const FAbilityEvent& Event : EventsOf(Type))
		{
			Result.Add(Event.UnitId);
		}
		return Result;
	}

	FIntPoint PositionOf(int32 UnitId) const
	{
		return Bed.State.FindUnit(UnitId)->Position;
	}

	int32 HealthOf(int32 UnitId) const
	{
		return Bed.State.FindUnit(UnitId)->Health;
	}
END_DEFINE_SPEC(FAbilityEffectsSpec)

void FAbilityEffectsSpec::Define()
{
	BeforeEach([this]()
	{
		NewField();
	});

	AfterEach([this]()
	{
		Bed.Reset();
	});

	Describe("Damage and Heal", [this]()
	{
		It("damage is limited by the current health and the event holds the real loss", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 2);
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeDamage(5) };
			TestTrue("Run", Run(FIntPoint(2, 1)));

			TestEqual("events", Outcome.Events.Num(), 2);
			TestEqual("Damage", Outcome.Events[0].Type, EAbilityEventType::Damage);
			TestEqual("Damage.UnitId", Outcome.Events[0].UnitId, 2);
			TestEqual("Damage.OtherUnitId is the source", Outcome.Events[0].OtherUnitId, EffectsHeroId);
			TestEqual("Damage.Amount", Outcome.Events[0].Amount, 2);
			TestFalse("Damage.bCollision", Outcome.Events[0].bCollision);
			TestEqual("Death", Outcome.Events[1].Type, EAbilityEventType::Death);
			TestEqual("health", HealthOf(2), 0);
			TestEqual("GetDamageTo", Outcome.GetDamageTo(2), 2);
			TestTrue("IsKilled", Outcome.IsKilled(2));
		});

		It("damage below the health leaves the unit alive", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 5);
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeDamage(2) };
			Run(FIntPoint(2, 1));
			TestEqual("health", HealthOf(2), 3);
			TestEqual("events", Outcome.Events.Num(), 1);
			TestFalse("IsKilled", Outcome.IsKilled(2));
		});

		It("heal is limited by the maximum health", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Hero, FIntPoint(2, 1), 1, 3);
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeHeal(10) };
			Run(FIntPoint(2, 1));
			TestEqual("Heal events", EventsOf(EAbilityEventType::Heal).Num(), 1);
			TestEqual("Heal.Amount", EventsOf(EAbilityEventType::Heal)[0].Amount, 2);
			TestEqual("Heal.OtherUnitId is the source", EventsOf(EAbilityEventType::Heal)[0].OtherUnitId, EffectsHeroId);
			TestEqual("health", HealthOf(2), 3);

			Run(FIntPoint(2, 1));
			TestEqual("no event at full health", Outcome.Events.Num(), 0);
		});

		It("the filter is relative to the team of the source", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 1), 5);
			Bed.AddUnit(3, EUnitTeam::Hero, FIntPoint(4, 1), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(3, 2), 5);
			Setup(ETargetingShape::Diamond, 2, 2, EAreaType::Diamond, 1);

			Ability.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::AllUnits) };
			Run(FIntPoint(3, 1));
			TestEqual("AllUnits: monster", Outcome.GetDamageTo(2), 1);
			TestEqual("AllUnits: ally", Outcome.GetDamageTo(3), 1);
			TestEqual("AllUnits: second monster", Outcome.GetDamageTo(4), 1);

			Ability.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) };
			Run(FIntPoint(3, 1));
			TestEqual("Enemies: monster", Outcome.GetDamageTo(2), 1);
			TestEqual("Enemies: ally", Outcome.GetDamageTo(3), 0);
			TestEqual("Enemies: second monster", Outcome.GetDamageTo(4), 1);

			Ability.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::Allies) };
			Run(FIntPoint(3, 1));
			TestEqual("Allies: monster", Outcome.GetDamageTo(2), 0);
			TestEqual("Allies: ally", Outcome.GetDamageTo(3), 1);
		});

		It("the filter is relative to the source also for a monster", [this]()
		{
			NewField({}, FIntPoint(1, 1));
			Bed.State.FindUnit(EffectsHeroId)->Team = EUnitTeam::Monster;
			Bed.State.Grid.Occupants.Find(FIntPoint(1, 1))->Team = EUnitTeam::Monster;
			Bed.AddUnit(2, EUnitTeam::Hero, FIntPoint(2, 1), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(3, 1), 5);
			Setup(ETargetingShape::Diamond, 1, 1, EAreaType::Line, 2);
			Ability.Targeting.Shape = ETargetingShape::Cross;
			Ability.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) };
			Run(FIntPoint(2, 1));
			TestEqual("hero is an enemy of a monster", Outcome.GetDamageTo(2), 1);
			TestEqual("monster is not", Outcome.GetDamageTo(3), 0);
		});

		It("the effect can act on the center or the edge only", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 4), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(4, 5), 5);
			Bed.AddUnit(4, EUnitTeam::Hero, FIntPoint(3, 4), 5);
			Setup(ETargetingShape::Diamond, 0, 8, EAreaType::Diamond, 1);
			Ability.Effects = { MakeDamage(2, EAreaPart::Center), MakeDamage(1, EAreaPart::Edge) };
			TestTrue("Run", Run(FIntPoint(4, 4)));
			TestEqual("center", HealthOf(2), 3);
			TestEqual("edge monster", HealthOf(3), 4);
			TestEqual("edge hero", HealthOf(4), 4);
			TestEqual("the source is not in the area", HealthOf(EffectsHeroId), 5);
			TestEqual("EffectIndex of the center hit", EventsOf(EAbilityEventType::Damage)[0].EffectIndex, 0);
			TestEqual("EffectIndex of an edge hit", EventsOf(EAbilityEventType::Damage)[1].EffectIndex, 1);
		});

		It("the source is a target only with bAffectsSource", [this]()
		{
			Setup(ETargetingShape::Diamond, 1, 1, EAreaType::Diamond, 1);
			Ability.Effects = { MakeDamage(1) };
			Run(FIntPoint(2, 1));
			TestEqual("without the flag", HealthOf(EffectsHeroId), 5);

			Ability.Area.bAffectsSource = true;
			Run(FIntPoint(2, 1));
			TestEqual("with the flag", HealthOf(EffectsHeroId), 4);
		});

		It("the source that dashed into the area is not a target without bAffectsSource", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 1), 3);
			Setup(ETargetingShape::Cross, 1, 4, EAreaType::Path);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash), MakeDamage(1) };
			TestTrue("Run", Run(FIntPoint(4, 1)));
			TestEqual("the source stands on a path tile", PositionOf(EffectsHeroId), FIntPoint(3, 1));
			TestEqual("the source is not damaged", Outcome.GetDamageTo(EffectsHeroId), 0);
			TestEqual("the monster is damaged", HealthOf(2), 2);
		});

		It("the source that dashed into the area is a target with bAffectsSource", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 1), 3);
			Setup(ETargetingShape::Cross, 1, 4, EAreaType::Path);
			Ability.Area.bAffectsSource = true;
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash), MakeDamage(1) };
			Run(FIntPoint(4, 1));
			TestEqual("the source is damaged", Outcome.GetDamageTo(EffectsHeroId), 1);
		});

		It("status effects are skipped", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 3);
			Setup(ETargetingShape::Diamond, 1, 1);
			FAbilityEffect UnitStatus;
			UnitStatus.Type = EAbilityEffectType::UnitStatus;
			FAbilityEffect TileStatus;
			TileStatus.Type = EAbilityEffectType::TileStatus;
			Ability.Effects = { UnitStatus, TileStatus, MakeDamage(1) };
			Run(FIntPoint(2, 1));
			TestEqual("events", Outcome.Events.Num(), 1);
			TestEqual("EffectIndex of the damage", Outcome.Events[0].EffectIndex, 2);
		});
	});

	Describe("Death", [this]()
	{
		It("a unit dies right after the effect and the next effects skip it", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 1, 5);
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeDamage(1), MakeDamage(1), MakeHeal(5) };
			Run(FIntPoint(2, 1));

			TestEqual("events", Outcome.Events.Num(), 2);
			TestEqual("Damage", Outcome.Events[0].Type, EAbilityEventType::Damage);
			TestEqual("Death", Outcome.Events[1].Type, EAbilityEventType::Death);
			TestEqual("Death.EffectIndex", Outcome.Events[1].EffectIndex, 0);
			TestFalse("bAlive", Bed.State.FindUnit(2)->bAlive);
			TestNull("the tile is free", Bed.State.FindUnitAt(FIntPoint(2, 1)));
			TestFalse("the grid is free", Bed.State.Grid.IsOccupied(FIntPoint(2, 1)));
		});

		It("the Death event holds the killer and the tile", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 1);
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeDamage(1) };
			Run(FIntPoint(2, 1));
			const TArray<FAbilityEvent> Deaths = EventsOf(EAbilityEventType::Death);
			TestEqual("Death events", Deaths.Num(), 1);
			TestEqual("UnitId", Deaths[0].UnitId, 2);
			TestEqual("OtherUnitId", Deaths[0].OtherUnitId, EffectsHeroId);
			TestEqual("From", Deaths[0].From, FIntPoint(2, 1));
			TestEqual("To", Deaths[0].To, FIntPoint(2, 1));
		});

		It("a killed unit is not a target of the next effect but its neighbours are", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 1), 1);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(4, 1), 4);
			Setup(ETargetingShape::Cross, 2, 2, EAreaType::Line, 2);
			Ability.Effects = { MakeDamage(1, EAreaPart::Center), MakeDamage(1, EAreaPart::All) };
			Run(FIntPoint(3, 1));
			TestEqual("the dead one was damaged once", Outcome.GetDamageTo(2), 1);
			TestEqual("the other one was damaged by the second effect", Outcome.GetDamageTo(3), 1);
		});

		It("the source dies from its own area", [this]()
		{
			Bed.State.FindUnit(EffectsHeroId)->Health = 1;
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Tile);
			Ability.Area.bAffectsSource = true;
			Ability.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::AllUnits) };
			TestTrue("Run", Run(FIntPoint(1, 1)));
			TestTrue("IsKilled", Outcome.IsKilled(EffectsHeroId));
			TestEqual("OtherUnitId", EventsOf(EAbilityEventType::Death)[0].OtherUnitId, EffectsHeroId);
		});
	});

	Describe("effects after a push", [this]()
	{
		It("an effect does not act on a unit pushed out of its part and acts on a unit pushed into it", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 1), 5);
			Setup(ETargetingShape::Cross, 2, 2, EAreaType::Line, 2);
			Ability.Effects = {
				MakePush(EPushDirection::AbilityDirection, 1, EAreaPart::Center),
				MakeDamage(1, EAreaPart::Edge),
				MakeDamage(1, EAreaPart::Center) };
			TestTrue("Run", Run(FIntPoint(3, 1)));
			TestEqual("pushed to the edge tile", PositionOf(2), FIntPoint(4, 1));
			TestEqual("hit once, as an edge unit", HealthOf(2), 4);
			TestEqual("Damage events", EventsOf(EAbilityEventType::Damage).Num(), 1);
			TestEqual("by the second effect", EventsOf(EAbilityEventType::Damage)[0].EffectIndex, 1);
		});

		It("the area is not rebuilt when the source moves", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 3), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(1, 2), 5);
			Setup(ETargetingShape::Diamond, 2, 4, EAreaType::Diamond, 1);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Jump), MakeDamage(1) };
			TestTrue("Run", Run(FIntPoint(4, 2)));
			TestEqual("the source jumped", PositionOf(EffectsHeroId), FIntPoint(4, 2));
			TestEqual("a neighbour of the target is damaged", HealthOf(2), 4);
			TestEqual("a neighbour of the old source tile is not", HealthOf(3), 5);
			TestEqual("the source on the center tile is not damaged", HealthOf(EffectsHeroId), 5);
			TestEqual("Outcome.Cast keeps the area", Outcome.Cast.Area.Center, FIntPoint(4, 2));
			TestEqual("Outcome.Cast keeps SourcePosition", Outcome.Cast.SourcePosition, FIntPoint(1, 1));
		});
	});

	Describe("Push directions", [this]()
	{
		It("AbilityDirection moves the unit along the direction of the ability", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 5);
			Setup(ETargetingShape::Cross, 1, 1);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 2) };
			Run(FIntPoint(2, 1));

			TestEqual("position", PositionOf(2), FIntPoint(4, 1));
			TestEqual("events", Outcome.Events.Num(), 1);
			const FAbilityEvent& Push = Outcome.Events[0];
			TestEqual("Type", Push.Type, EAbilityEventType::Push);
			TestEqual("UnitId", Push.UnitId, 2);
			TestEqual("Amount", Push.Amount, 2);
			TestEqual("From", Push.From, FIntPoint(2, 1));
			TestEqual("To", Push.To, FIntPoint(4, 1));
			FIntPoint Final;
			TestTrue("GetFinalPosition", Outcome.GetFinalPosition(2, Final));
			TestEqual("final position", Final, FIntPoint(4, 1));
			TestFalse("the source did not move", Outcome.GetFinalPosition(EffectsHeroId, Final));
		});

		It("AbilityDirection does nothing without a direction", [this]()
		{
			NewField({}, FIntPoint(4, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 5), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 1);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1) };
			TestTrue("Run", Run(FIntPoint(4, 4)));
			TestEqual("events", Outcome.Events.Num(), 0);
			TestEqual("position", PositionOf(2), FIntPoint(4, 5));
		});

		It("AwayFromSource moves every unit away from the source", [this]()
		{
			NewField({}, FIntPoint(4, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 4), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(5, 4), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(4, 3), 5);
			Bed.AddUnit(5, EUnitTeam::Monster, FIntPoint(4, 5), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 1);
			Ability.Effects = { MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::Edge) };
			TestTrue("Run", Run(FIntPoint(4, 4)));
			TestEqual("left", PositionOf(2), FIntPoint(2, 4));
			TestEqual("right", PositionOf(3), FIntPoint(6, 4));
			TestEqual("up", PositionOf(4), FIntPoint(4, 2));
			TestEqual("down", PositionOf(5), FIntPoint(4, 6));
			TestEqual("equal keys: events in ascending UnitId order", UnitIdsOf(EAbilityEventType::Push), TArray<int32>({ 2, 3, 4, 5 }));
		});

		It("AwayFromSource uses the axis with the larger difference and does nothing on equal differences", [this]()
		{
			NewField({}, FIntPoint(4, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 5), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(5, 5), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(3, 2), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 3);
			Ability.Effects = { MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::Edge) };
			Run(FIntPoint(4, 4));
			TestEqual("(2,1) goes along X", PositionOf(2), FIntPoint(7, 5));
			TestEqual("(1,1) does not move", PositionOf(3), FIntPoint(5, 5));
			TestEqual("(-1,-2) goes along Y", PositionOf(4), FIntPoint(3, 1));
			TestEqual("Push events", EventsOf(EAbilityEventType::Push).Num(), 2);
		});

		It("TowardSource moves units to the source and a unit next to it collides with it", [this]()
		{
			NewField({}, FIntPoint(4, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 6), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(6, 4), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(3, 4), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 2);
			Ability.Effects = { MakePush(EPushDirection::TowardSource, 1, EAreaPart::Edge) };
			Run(FIntPoint(4, 4));
			TestEqual("from below", PositionOf(2), FIntPoint(4, 5));
			TestEqual("from the right", PositionOf(3), FIntPoint(5, 4));
			TestEqual("the neighbour does not move", PositionOf(4), FIntPoint(3, 4));
			const TArray<FAbilityEvent> Collisions = EventsOf(EAbilityEventType::Collision);
			TestEqual("Collision events", Collisions.Num(), 1);
			TestEqual("Collision.OtherUnitId is the source", Collisions[0].OtherUnitId, EffectsHeroId);
			TestEqual("the monster is damaged", HealthOf(4), 4);
			TestEqual("the source is damaged too", HealthOf(EffectsHeroId), 4);
		});

		It("AwayFromCenter moves units from the target point and leaves the unit in the center", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 4), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(3, 4), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(5, 4), 5);
			Bed.AddUnit(5, EUnitTeam::Monster, FIntPoint(4, 3), 5);
			Bed.AddUnit(6, EUnitTeam::Monster, FIntPoint(4, 5), 5);
			Setup(ETargetingShape::Diamond, 6, 6, EAreaType::Diamond, 1);
			Ability.Effects = { MakePush(EPushDirection::AwayFromCenter, 1) };
			TestTrue("Run", Run(FIntPoint(4, 4)));
			TestEqual("center", PositionOf(2), FIntPoint(4, 4));
			TestEqual("left", PositionOf(3), FIntPoint(2, 4));
			TestEqual("right", PositionOf(4), FIntPoint(6, 4));
			TestEqual("up", PositionOf(5), FIntPoint(4, 2));
			TestEqual("down", PositionOf(6), FIntPoint(4, 6));
			TestFalse("no Push event for the center", UnitIdsOf(EAbilityEventType::Push).Contains(2));
			TestEqual("no collisions", Outcome.Events.FilterByPredicate([](const FAbilityEvent& E) { return E.Type == EAbilityEventType::Collision; }).Num(), 0);
		});

		It("the filter selects who is pushed", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 2), 5);
			Bed.AddUnit(3, EUnitTeam::Hero, FIntPoint(3, 1), 5);
			Setup(ETargetingShape::Diamond, 1, 1, EAreaType::Diamond, 1);
			Ability.Effects = { MakePush(EPushDirection::AwayFromCenter, 1, EAreaPart::Edge, ETargetFilter::Enemies) };
			Run(FIntPoint(2, 1));
			TestEqual("the enemy is pushed", PositionOf(2), FIntPoint(2, 3));
			TestEqual("the ally stays", PositionOf(3), FIntPoint(3, 1));
		});
	});

	Describe("Push order", [this]()
	{
		It("AbilityDirection moves the farthest unit first", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(3, 1), 5);
			Bed.AddUnit(4, EUnitTeam::Monster, FIntPoint(4, 1), 5);
			Setup(ETargetingShape::Cross, 1, 6, EAreaType::Line, 3);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1) };
			Run(FIntPoint(2, 1));
			TestEqual("first", PositionOf(2), FIntPoint(3, 1));
			TestEqual("second", PositionOf(3), FIntPoint(4, 1));
			TestEqual("third", PositionOf(4), FIntPoint(5, 1));
			TestEqual("order of the events", UnitIdsOf(EAbilityEventType::Push), TArray<int32>({ 4, 3, 2 }));
			TestEqual("no collisions", EventsOf(EAbilityEventType::Collision).Num(), 0);
		});

		It("AwayFromSource moves the farthest unit first", [this]()
		{
			NewField({}, FIntPoint(1, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 4), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(3, 4), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 3);
			Ability.Effects = { MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::Edge) };
			Run(FIntPoint(1, 4));
			TestEqual("order of the events", UnitIdsOf(EAbilityEventType::Push), TArray<int32>({ 3, 2 }));
			TestEqual("near", PositionOf(2), FIntPoint(3, 4));
			TestEqual("far", PositionOf(3), FIntPoint(4, 4));
			TestEqual("no collisions", EventsOf(EAbilityEventType::Collision).Num(), 0);
		});

		It("TowardSource moves the closest unit first", [this]()
		{
			NewField({}, FIntPoint(1, 4));
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 4), 5);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(4, 4), 5);
			Setup(ETargetingShape::Diamond, 0, 0, EAreaType::Diamond, 3);
			Ability.Effects = { MakePush(EPushDirection::TowardSource, 1, EAreaPart::Edge) };
			Run(FIntPoint(1, 4));
			TestEqual("order of the events", UnitIdsOf(EAbilityEventType::Push), TArray<int32>({ 2, 3 }));
			TestEqual("near", PositionOf(2), FIntPoint(2, 4));
			TestEqual("far", PositionOf(3), FIntPoint(3, 4));
			TestEqual("no collisions", EventsOf(EAbilityEventType::Collision).Num(), 0);
		});

		It("AwayFromCenter moves the farthest unit from the center first", [this]()
		{
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(5, 4), 5);
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 4), 5);
			Setup(ETargetingShape::Diamond, 6, 6, EAreaType::Diamond, 3);
			Ability.Effects = { MakePush(EPushDirection::AwayFromCenter, 1, EAreaPart::Edge) };
			Run(FIntPoint(4, 4));
			TestEqual("order of the events", UnitIdsOf(EAbilityEventType::Push), TArray<int32>({ 2, 3 }));
			TestEqual("near", PositionOf(3), FIntPoint(6, 4));
			TestEqual("far", PositionOf(2), FIntPoint(7, 4));
		});
	});

	Describe("collisions", [this]()
	{
		It("with the field edge: the unit stops, takes damage, no unit is hit", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 1), 5);
			Setup(ETargetingShape::Cross, 1, 5);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 3) };
			TestTrue("Run", Run(FIntPoint(6, 1)));

			TestEqual("events", Outcome.Events.Num(), 3);
			TestEqual("Push", Outcome.Events[0].Type, EAbilityEventType::Push);
			TestEqual("Push.Amount", Outcome.Events[0].Amount, 1);
			TestEqual("Push.To", Outcome.Events[0].To, FIntPoint(7, 1));
			TestEqual("Collision", Outcome.Events[1].Type, EAbilityEventType::Collision);
			TestEqual("Collision.UnitId", Outcome.Events[1].UnitId, 2);
			TestEqual("Collision.OtherUnitId", Outcome.Events[1].OtherUnitId, static_cast<int32>(INDEX_NONE));
			TestEqual("Collision.Amount", Outcome.Events[1].Amount, 1);
			TestEqual("Collision.From", Outcome.Events[1].From, FIntPoint(7, 1));
			TestEqual("Collision.To is outside the field", Outcome.Events[1].To, FIntPoint(8, 1));
			TestEqual("Damage", Outcome.Events[2].Type, EAbilityEventType::Damage);
			TestTrue("Damage.bCollision", Outcome.Events[2].bCollision);
			TestEqual("Damage.Amount", Outcome.Events[2].Amount, 1);
			TestEqual("Damage.OtherUnitId is the source", Outcome.Events[2].OtherUnitId, EffectsHeroId);
			TestEqual("health", HealthOf(2), 4);
			TestEqual("position", PositionOf(2), FIntPoint(7, 1));
		});

		It("with the field edge without any step: no Push event", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(7, 1), 5);
			Setup(ETargetingShape::Cross, 1, 6);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 2) };
			Run(FIntPoint(7, 1));
			TestEqual("Push events", EventsOf(EAbilityEventType::Push).Num(), 0);
			TestEqual("Collision events", EventsOf(EAbilityEventType::Collision).Num(), 1);
			TestEqual("Collision.From", EventsOf(EAbilityEventType::Collision)[0].From, FIntPoint(7, 1));
			TestEqual("health", HealthOf(2), 4);
		});

		It("with an impassable tile: only the pushed unit takes damage", [this]()
		{
			NewField({ FIntPoint(4, 1) });
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 5);
			Setup(ETargetingShape::Cross, 1, 1);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 3) };
			Run(FIntPoint(2, 1));
			TestEqual("position", PositionOf(2), FIntPoint(3, 1));
			TestEqual("Collision.OtherUnitId", EventsOf(EAbilityEventType::Collision)[0].OtherUnitId, static_cast<int32>(INDEX_NONE));
			TestEqual("Collision.To is the wall", EventsOf(EAbilityEventType::Collision)[0].To, FIntPoint(4, 1));
			TestEqual("health", HealthOf(2), 4);
			TestEqual("Damage events", EventsOf(EAbilityEventType::Damage).Num(), 1);
		});

		for (const EUnitTeam OtherTeam : { EUnitTeam::Monster, EUnitTeam::Hero })
		{
			It(FString::Printf(TEXT("with a unit of any team (%s): both take collision damage"), OtherTeam == EUnitTeam::Hero ? TEXT("same team as the source") : TEXT("other team")), [this, OtherTeam]()
			{
				Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 3);
				Bed.AddUnit(3, OtherTeam, FIntPoint(3, 1), 3);
				Setup(ETargetingShape::Cross, 1, 1);
				Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 2) };
				Run(FIntPoint(2, 1));

				TestEqual("the pushed unit does not move", PositionOf(2), FIntPoint(2, 1));
				TestEqual("Push events", EventsOf(EAbilityEventType::Push).Num(), 0);
				TestEqual("Collision.OtherUnitId", EventsOf(EAbilityEventType::Collision)[0].OtherUnitId, 3);
				TestEqual("Collision.From", EventsOf(EAbilityEventType::Collision)[0].From, FIntPoint(2, 1));
				TestEqual("Collision.To", EventsOf(EAbilityEventType::Collision)[0].To, FIntPoint(3, 1));
				TestEqual("pushed", HealthOf(2), 2);
				TestEqual("hit", HealthOf(3), 2);
				TestEqual("Damage events", EventsOf(EAbilityEventType::Damage).Num(), 2);
				TestTrue("both are collision damage", !EventsOf(EAbilityEventType::Damage).ContainsByPredicate([](const FAbilityEvent& E) { return !E.bCollision; }));
			});
		}

		It("uses CollisionDamage of the snapshot", [this]()
		{
			Bed.State.CollisionDamage = 2;
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(7, 1), 5);
			Setup(ETargetingShape::Cross, 1, 6);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1) };
			Run(FIntPoint(7, 1));
			TestEqual("Collision.Amount", EventsOf(EAbilityEventType::Collision)[0].Amount, 2);
			TestEqual("health", HealthOf(2), 3);
		});

		It("a unit killed by the collision dies after the effect with the source as the killer", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 1), 1);
			Setup(ETargetingShape::Cross, 1, 5);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 3) };
			Run(FIntPoint(6, 1));
			TestEqual("last event", Outcome.Events.Last().Type, EAbilityEventType::Death);
			TestEqual("Death.OtherUnitId", Outcome.Events.Last().OtherUnitId, EffectsHeroId);
			TestEqual("Death.From", Outcome.Events.Last().From, FIntPoint(7, 1));
			TestTrue("IsKilled", Outcome.IsKilled(2));
			TestEqual("GetDamageTo includes the collision", Outcome.GetDamageTo(2), 1);
			FIntPoint Final;
			TestTrue("GetFinalPosition", Outcome.GetFinalPosition(2, Final));
			TestEqual("final position", Final, FIntPoint(7, 1));
		});

		It("a unit with 0 health still stands until the end of the effect", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 1), 3);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(7, 1), 1);
			Setup(ETargetingShape::Cross, 1, 5, EAreaType::Line, 2);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1) };
			Run(FIntPoint(6, 1));

			TestEqual("the first unit hits the edge, then the second one collides with the first", UnitIdsOf(EAbilityEventType::Collision), TArray<int32>({ 3, 2 }));
			TestEqual("the second unit did not enter the tile", PositionOf(2), FIntPoint(6, 1));
			TestEqual("Collision.OtherUnitId", EventsOf(EAbilityEventType::Collision)[1].OtherUnitId, 3);
			TestEqual("the second unit is damaged", HealthOf(2), 2);
			TestEqual("a unit with 0 health takes no more damage events", EventsOf(EAbilityEventType::Damage).Num(), 2);
			TestEqual("Death is the last event", Outcome.Events.Last().Type, EAbilityEventType::Death);
			TestEqual("Death.UnitId", Outcome.Events.Last().UnitId, 3);
		});

		It("a pushed unit that died frees its tile for the next effect", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(7, 1), 1);
			Setup(ETargetingShape::Cross, 1, 6);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1), MakeMoveSource(ESourceMoveMode::Jump) };
			Run(FIntPoint(7, 1));
			TestEqual("the source jumped onto the freed tile", PositionOf(EffectsHeroId), FIntPoint(7, 1));
		});
	});

	Describe("MoveSource", [this]()
	{
		It("Dash goes to the target point", [this]()
		{
			Setup(ETargetingShape::Cross, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			TestTrue("Run", Run(FIntPoint(4, 1)));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(4, 1));
			TestEqual("events", Outcome.Events.Num(), 1);
			TestEqual("Type", Outcome.Events[0].Type, EAbilityEventType::SourceMove);
			TestEqual("UnitId", Outcome.Events[0].UnitId, EffectsHeroId);
			TestEqual("From", Outcome.Events[0].From, FIntPoint(1, 1));
			TestEqual("To", Outcome.Events[0].To, FIntPoint(4, 1));
			FIntPoint Final;
			TestTrue("GetFinalPosition", Outcome.GetFinalPosition(EffectsHeroId, Final));
			TestEqual("final position", Final, FIntPoint(4, 1));
		});

		It("Dash stops before a unit", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 1), 3);
			Setup(ETargetingShape::Cross, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			Run(FIntPoint(4, 1));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(3, 1));
			TestEqual("the unit does not move", PositionOf(2), FIntPoint(4, 1));
		});

		It("Dash stops before an unit of any team in the way", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Hero, FIntPoint(3, 1), 3);
			Setup(ETargetingShape::Cross, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			Run(FIntPoint(4, 1));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(2, 1));
		});

		It("Dash stops before an impassable tile", [this]()
		{
			NewField({ FIntPoint(3, 1) });
			Setup(ETargetingShape::Cross, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			Run(FIntPoint(4, 1));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(2, 1));
		});

		It("Dash does not move and writes no event when the first tile is blocked", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 3);
			Setup(ETargetingShape::Cross, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			Run(FIntPoint(4, 1));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(1, 1));
			TestEqual("events", Outcome.Events.Num(), 0);
		});

		It("Dash works in every direction", [this]()
		{
			NewField({}, FIntPoint(4, 4));
			Setup(ETargetingShape::Cross, 1, 3);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash) };
			Run(FIntPoint(4, 2));
			TestEqual("Up", PositionOf(EffectsHeroId), FIntPoint(4, 2));
			Run(FIntPoint(4, 4));
			TestEqual("Down", PositionOf(EffectsHeroId), FIntPoint(4, 4));
			Run(FIntPoint(2, 4));
			TestEqual("Left", PositionOf(EffectsHeroId), FIntPoint(2, 4));
		});

		It("Jump moves to the target point ignoring the tiles in between", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 3);
			Setup(ETargetingShape::Diamond, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Jump) };
			TestTrue("Run", Run(FIntPoint(3, 2)));
			TestEqual("position", PositionOf(EffectsHeroId), FIntPoint(3, 2));
			TestEqual("SourceMove.From", Outcome.Events[0].From, FIntPoint(1, 1));
			TestEqual("SourceMove.To", Outcome.Events[0].To, FIntPoint(3, 2));
		});

		It("Jump onto an occupied or impassable tile does nothing", [this]()
		{
			NewField({ FIntPoint(3, 2) });
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 2), 3);
			Setup(ETargetingShape::Diamond, 1, 4);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Jump) };
			Run(FIntPoint(2, 2));
			TestEqual("occupied: position", PositionOf(EffectsHeroId), FIntPoint(1, 1));
			TestEqual("occupied: events", Outcome.Events.Num(), 0);
			Run(FIntPoint(3, 2));
			TestEqual("impassable: position", PositionOf(EffectsHeroId), FIntPoint(1, 1));
			TestEqual("impassable: events", Outcome.Events.Num(), 0);
		});

		It("does not use targets or the filter", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 3);
			Setup(ETargetingShape::Diamond, 3, 3);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Jump) };
			Run(FIntPoint(4, 1));
			TestEqual("health of the monster", HealthOf(2), 3);
			TestEqual("events", Outcome.Events.Num(), 1);
		});
	});

	Describe("Lunge", [this]()
	{
		It("dashes to the unit and damages the path and the target", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(4, 1), 3);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(4, 2), 3);
			Setup(ETargetingShape::Cross, 1, 3, EAreaType::Path);
			Ability.Targeting.Requirement = ETargetRequirement::Enemy;
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash), MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) };
			TestTrue("Run", Run(FIntPoint(4, 1)));
			TestEqual("source", PositionOf(EffectsHeroId), FIntPoint(3, 1));
			TestEqual("target", HealthOf(2), 2);
			TestEqual("a unit next to the path", HealthOf(3), 3);
			TestEqual("SourceMove before Damage", Outcome.Events[0].Type, EAbilityEventType::SourceMove);
			TestEqual("Damage", Outcome.Events[1].Type, EAbilityEventType::Damage);
		});
	});

	Describe("Preview", [this]()
	{
		It("does not change the snapshot and returns the same events as Simulate", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 1), 1);
			Bed.AddUnit(3, EUnitTeam::Monster, FIntPoint(3, 1), 3);
			Setup(ETargetingShape::Cross, 1, 6, EAreaType::Line, 2);
			Ability.Effects = { MakeMoveSource(ESourceMoveMode::Dash), MakeDamage(2), MakePush(EPushDirection::AbilityDirection, 3) };

			FAbilityCast Cast;
			TestTrue("MakeCast", FAbilityRules::MakeCast(Bed.State, EffectsHeroId, Ability, FIntPoint(6, 1), Cast));
			const FBattleSimState Before = Bed.State;

			FAbilityOutcome Previewed;
			TestTrue("Preview", FAbilityRules::Preview(Bed.State, Ability, Cast, Previewed));
			TestTrue("the outcome has events", Previewed.Events.Num() > 0);

			TestEqual("Units", Bed.State.Units.Num(), Before.Units.Num());
			for (int32 Index = 0; Index < Before.Units.Num(); ++Index)
			{
				TestEqual("Position", Bed.State.Units[Index].Position, Before.Units[Index].Position);
				TestEqual("Health", Bed.State.Units[Index].Health, Before.Units[Index].Health);
				TestEqual("bAlive", Bed.State.Units[Index].bAlive, Before.Units[Index].bAlive);
			}
			TestEqual("Occupants", Bed.State.Grid.Occupants.Num(), Before.Grid.Occupants.Num());
			for (const TPair<FIntPoint, FGridOccupant>& Pair : Before.Grid.Occupants)
			{
				const FGridOccupant* Occupant = Bed.State.Grid.GetOccupant(Pair.Key);
				TestNotNull("occupant", Occupant);
				if (Occupant)
				{
					TestEqual("occupant UnitId", Occupant->UnitId, Pair.Value.UnitId);
				}
			}

			FAbilityRules::Simulate(Bed.State, Ability, Cast, Outcome);
			TestEqual("Event count", Outcome.Events.Num(), Previewed.Events.Num());
			for (int32 Index = 0; Index < Outcome.Events.Num() && Index < Previewed.Events.Num(); ++Index)
			{
				TestEqual("Type", Previewed.Events[Index].Type, Outcome.Events[Index].Type);
				TestEqual("UnitId", Previewed.Events[Index].UnitId, Outcome.Events[Index].UnitId);
				TestEqual("Amount", Previewed.Events[Index].Amount, Outcome.Events[Index].Amount);
				TestEqual("To", Previewed.Events[Index].To, Outcome.Events[Index].To);
			}
			TestEqual("Cast.TargetPoint", Previewed.Cast.TargetPoint, Cast.TargetPoint);
		});

		It("fails for a dead or unknown source", [this]()
		{
			Setup(ETargetingShape::Diamond, 1, 1);
			Ability.Effects = { MakeDamage(1) };
			FAbilityCast Cast;
			Cast.SourceId = 99;
			FAbilityOutcome Previewed;
			TestFalse("unknown", FAbilityRules::Preview(Bed.State, Ability, Cast, Previewed));
			TestEqual("no events", Previewed.Events.Num(), 0);

			Bed.State.KillUnit(EffectsHeroId);
			Cast.SourceId = EffectsHeroId;
			TestFalse("dead", FAbilityRules::Preview(Bed.State, Ability, Cast, Previewed));
		});
	});

	Describe("FAbilityOutcome", [this]()
	{
		It("GetDamageTo sums the damage of the effects and of the collisions", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(6, 1), 5);
			Setup(ETargetingShape::Cross, 1, 5);
			Ability.Effects = { MakeDamage(2), MakePush(EPushDirection::AbilityDirection, 2) };
			Run(FIntPoint(6, 1));
			TestEqual("GetDamageTo", Outcome.GetDamageTo(2), 3);
			TestEqual("a unit without events", Outcome.GetDamageTo(EffectsHeroId), 0);
			TestFalse("IsKilled", Outcome.IsKilled(2));
		});

		It("GetFinalPosition returns the end of the last move", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(2, 1), 5);
			Setup(ETargetingShape::Cross, 1, 1, EAreaType::Line, 3);
			Ability.Effects = { MakePush(EPushDirection::AbilityDirection, 1), MakePush(EPushDirection::AbilityDirection, 1) };
			Run(FIntPoint(2, 1));
			FIntPoint Final;
			TestTrue("GetFinalPosition", Outcome.GetFinalPosition(2, Final));
			TestEqual("after two pushes", Final, FIntPoint(4, 1));
			TestEqual("Push events", EventsOf(EAbilityEventType::Push).Num(), 2);
			TestFalse("unknown unit", Outcome.GetFinalPosition(99, Final));
		});
	});

	Describe("FAbilityRules helpers shared with the intent display", [this]()
	{
		It("GetPushStep goes along the axis with the larger difference", [this]()
		{
			const FIntPoint Source(2, 2);
			TestEqual("away, right", FAbilityRules::GetPushStep(EPushDirection::AwayFromSource, Source, FIntPoint(2, 2), FIntPoint(5, 3)), FIntPoint(1, 0));
			TestEqual("away, up", FAbilityRules::GetPushStep(EPushDirection::AwayFromSource, Source, FIntPoint(2, 2), FIntPoint(1, 0)), FIntPoint(0, -1));
			TestEqual("toward, left", FAbilityRules::GetPushStep(EPushDirection::TowardSource, Source, FIntPoint(2, 2), FIntPoint(5, 3)), FIntPoint(-1, 0));
			TestEqual("equal differences: no step", FAbilityRules::GetPushStep(EPushDirection::AwayFromSource, Source, FIntPoint(2, 2), FIntPoint(4, 4)), FIntPoint::ZeroValue);
			TestEqual("same tile: no step", FAbilityRules::GetPushStep(EPushDirection::AwayFromSource, Source, FIntPoint(2, 2), Source), FIntPoint::ZeroValue);
			TestEqual("from the center, not from the source", FAbilityRules::GetPushStep(EPushDirection::AwayFromCenter, Source, FIntPoint(6, 6), FIntPoint(6, 4)), FIntPoint(0, -1));
			TestEqual("AbilityDirection has no step", FAbilityRules::GetPushStep(EPushDirection::AbilityDirection, Source, FIntPoint(2, 2), FIntPoint(5, 3)), FIntPoint::ZeroValue);
		});

		It("GetPushStep matches the displacement made by Simulate", [this]()
		{
			Bed.AddUnit(2, EUnitTeam::Monster, FIntPoint(3, 1), 5);
			Setup(ETargetingShape::Cross, 1, 3);
			Ability.Effects = { MakePush(EPushDirection::AwayFromSource, 1) };
			Run(FIntPoint(3, 1));
			FIntPoint Final;
			TestTrue("pushed", Outcome.GetFinalPosition(2, Final));
			TestEqual("same as the step", Final, FIntPoint(3, 1) + FAbilityRules::GetPushStep(EPushDirection::AwayFromSource, FIntPoint(1, 1), FIntPoint(3, 1), FIntPoint(3, 1)));
		});

		It("GetAreaDamage sums the Damage effects whose part contains the tile", [this]()
		{
			FResolvedAbility Fireball;
			Fireball.Effects = { MakeDamage(2), MakeDamage(1) };
			Fireball.Effects[0].AreaPart = EAreaPart::Center;
			Fireball.Effects[1].AreaPart = EAreaPart::Edge;
			FAffectedArea Area;
			Area.Center = FIntPoint(4, 4);
			Area.bHasCenter = true;
			Area.Edge = { FIntPoint(3, 4), FIntPoint(5, 4) };
			TestEqual("center", FAbilityRules::GetAreaDamage(Fireball, Area, FIntPoint(4, 4)), 2);
			TestEqual("edge", FAbilityRules::GetAreaDamage(Fireball, Area, FIntPoint(3, 4)), 1);
			TestEqual("outside", FAbilityRules::GetAreaDamage(Fireball, Area, FIntPoint(0, 0)), 0);
			Fireball.Effects[1].AreaPart = EAreaPart::All;
			TestEqual("All part adds to the center", FAbilityRules::GetAreaDamage(Fireball, Area, FIntPoint(4, 4)), 3);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
