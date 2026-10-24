#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/MonsterAI.h"
#include "Tests/AbilityTestHelpers.h"

using namespace AbilityTest;

namespace
{
	constexpr int32 MonsterId = 1;
	constexpr int32 OtherMonsterId = 2;
	constexpr int32 HeroA = 10;
	constexpr int32 HeroB = 11;

	FUnitAbilityState MakeState(const FResolvedAbility& Ability, int32 RemainingCooldown = 0)
	{
		FUnitAbilityState State;
		State.Ability = Ability;
		State.RemainingCooldown = RemainingCooldown;
		return State;
	}

	// Мгновенная способность по одной клетке: ромб 1..MaxRange, область "тайл"
	FResolvedAbility MakeSingleTarget(const TArray<FAbilityEffect>& Effects, int32 MaxRange = 1)
	{
		FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, MaxRange);
		Ability.Area.Type = EAreaType::Tile;
		Ability.Effects = Effects;
		return Ability;
	}

	// Урон по врагам источника
	FResolvedAbility MakePoke(int32 Amount, int32 MaxRange = 1)
	{
		return MakeSingleTarget({ MakeDamage(Amount, EAreaPart::All, ETargetFilter::Enemies) }, MaxRange);
	}

	// Урон всем юнитам в ромбе радиуса 1 вокруг цели (задевает и союзников источника)
	FResolvedAbility MakeBlast(int32 Amount)
	{
		FResolvedAbility Ability = MakeTargeting(ETargetingShape::Diamond, 1, 1);
		Ability.Area.Type = EAreaType::Diamond;
		Ability.Area.Size = 1;
		Ability.Effects = { MakeDamage(Amount) };
		return Ability;
	}
}

BEGIN_DEFINE_SPEC(FMonsterAISpec, "Dungeon.AI.Monster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
	TArray<FUnitAbilityState> Abilities;
	FMonsterBehaviorProfile Profile;
	FMonsterAIContext Context;
	FRandomStream Random;

	FAIVariant Choose(int32 Id, int32 MovePoints, bool bAllowMove)
	{
		return FMonsterAI::ChooseVariant(Bed.State, Id, MovePoints, Abilities, Profile, bAllowMove, Context, Random);
	}

	// Контекст по текущему снимку
	void ResetContext()
	{
		Context = FMonsterAI::MakeContext(Bed.State);
	}
END_DEFINE_SPEC(FMonsterAISpec)

void FMonsterAISpec::Define()
{
	BeforeEach([this]()
	{
		Bed.Init();
		Abilities.Reset();
		Profile = FMonsterBehaviorProfile();
		Context = FMonsterAIContext();
		Random.Initialize(1);
	});

	AfterEach([this]()
	{
		Bed.Reset();
	});

	Describe("Attacking and non-attacking variants", [this]()
	{
		It("a variant without damage to heroes is non-attacking and is not chosen", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 3);
			ResetContext();
			// лечение героя и урон только по союзникам монстра не наносят урон героям
			Abilities = {
				MakeState(MakeSingleTarget({ MakeHeal(2, EAreaPart::All, ETargetFilter::Enemies) })),
				MakeState(MakeSingleTarget({ MakeDamage(2, EAreaPart::All, ETargetFilter::Allies) })) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("AbilityIndex", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestFalse("bAttacking", Variant.bAttacking);
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(3, 3));
			TestEqual("PathCost", Variant.PathCost, 0);
		});

		It("an ability on cooldown is skipped", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(3), 1), MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("the ready ability is used", Variant.AbilityIndex, 1);
			TestTrue("bAttacking", Variant.bAttacking);
			TestEqual("SelectedTile", Variant.SelectedTile, FIntPoint(4, 3));
		});

		It("an unknown or dead monster gives an empty variant", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			TestEqual("unknown", Choose(99, 3, true).AbilityIndex, static_cast<int32>(INDEX_NONE));
			Bed.State.KillUnit(MonsterId);
			const FAIVariant Variant = Choose(MonsterId, 3, true);
			TestEqual("dead", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestFalse("dead is not attacking", Variant.bAttacking);
		});

		It("does not change the snapshot", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(1, 1), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 1), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(2)) };

			const FAIVariant Variant = Choose(MonsterId, 3, true);
			TestTrue("an attack was found", Variant.bAttacking);
			TestEqual("monster position", Bed.State.FindUnit(MonsterId)->Position, FIntPoint(1, 1));
			TestTrue("monster is on its tile", Bed.State.FindUnitAt(FIntPoint(1, 1)) != nullptr);
			TestEqual("hero health", Bed.State.FindUnit(HeroA)->Health, 5);
		});
	});

	Describe("Safety", [this]()
	{
		It("a safe attacking variant beats an unsafe one with a higher score", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			Bed.AddUnit(OtherMonsterId, EUnitTeam::Monster, FIntPoint(5, 3), 3);
			ResetContext();
			// ромб с уроном 3 задевает второго монстра за героем, точечный удар на 1 - нет
			Abilities = { MakeState(MakeBlast(3)), MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("AbilityIndex", Variant.AbilityIndex, 1);
			TestTrue("bSafe", Variant.bSafe);
			TestEqual("Score", Variant.Score, 10.f);
		});

		It("collision damage to a monster makes a variant unsafe", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			Bed.AddUnit(OtherMonsterId, EUnitTeam::Monster, FIntPoint(5, 3), 3);
			ResetContext();
			// удар с отталкиванием героя в монстра: урон 3 герою, столкновение ранит обоих
			Abilities = {
				MakeState(MakeSingleTarget({ MakeDamage(2, EAreaPart::All, ETargetFilter::Enemies),
					MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::All, ETargetFilter::Enemies) })),
				MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("AbilityIndex", Variant.AbilityIndex, 1);
			TestTrue("bSafe", Variant.bSafe);
		});

		It("damage to the monster itself makes a variant unsafe", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			ResetContext();
			FResolvedAbility Blast = MakeBlast(1);
			Blast.Area.bAffectsSource = true;
			Abilities = { MakeState(Blast) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestTrue("bAttacking", Variant.bAttacking);
			TestFalse("bSafe", Variant.bSafe);
		});

		It("without safe attacking variants the best attacking one is chosen", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			Bed.AddUnit(OtherMonsterId, EUnitTeam::Monster, FIntPoint(5, 3), 3);
			ResetContext();
			Abilities = { MakeState(MakeBlast(2)), MakeState(MakeBlast(3)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("AbilityIndex", Variant.AbilityIndex, 1);
			TestFalse("bSafe", Variant.bSafe);
			TestEqual("Score", Variant.Score, 30.f);
		});

		It("a tile in DangerTiles is unsafe", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };
			Context.DangerTiles.Add(FIntPoint(3, 3));

			const FAIVariant Stay = Choose(MonsterId, 0, false);
			TestTrue("the only variant is still chosen", Stay.bAttacking);
			TestFalse("bSafe", Stay.bSafe);

			// со свободой перемещения монстр уходит на безопасную клетку, хотя путь до неё дороже
			const FAIVariant Move = Choose(MonsterId, 2, true);
			TestTrue("bAttacking", Move.bAttacking);
			TestTrue("bSafe", Move.bSafe);
			TestTrue("moved out of the danger tile", Move.MoveTile != FIntPoint(3, 3));
			TestEqual("PathCost", Move.PathCost, 2);
			TestEqual("SelectedTile", Move.SelectedTile, FIntPoint(4, 3));
		});
	});

	Describe("Expected health and score", [this]()
	{
		// Два героя в зоне досягаемости двух монстров: A (здоровье 2) на (3,3), B (здоровье 5) на (3,5)
		auto SetupTwoHeroes = [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(OtherMonsterId, EUnitTeam::Monster, FIntPoint(2, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(3, 3), 2);
			Bed.AddUnit(HeroB, EUnitTeam::Hero, FIntPoint(3, 5), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(2, 2)) };
		};

		It("damage is limited by the expected health, the second monster does not finish a killed hero", [this, SetupTwoHeroes]()
		{
			SetupTwoHeroes();

			// без контекста второй монстр выбрал бы убийство героя A: 20 + 20 - 1
			const FAIVariant Fresh = Choose(OtherMonsterId, 0, false);
			TestEqual("fresh: target", Fresh.SelectedTile, FIntPoint(3, 3));
			TestEqual("fresh: Score", Fresh.Score, 39.f);

			const FAIVariant First = Choose(MonsterId, 0, false);
			TestEqual("first: target", First.SelectedTile, FIntPoint(3, 3));
			TestEqual("first: Score", First.Score, 39.f);
			FMonsterAI::RegisterChoice(Bed.State, First, Context);

			const FAIVariant Second = Choose(OtherMonsterId, 0, false);
			TestTrue("second: bAttacking, the hit is real", Second.bAttacking);
			TestEqual("second: target", Second.SelectedTile, FIntPoint(3, 5));
			TestEqual("second: Score", Second.Score, 19.f);
		});

		It("a hit on a hero with zero expected health still counts as attacking", [this, SetupTwoHeroes]()
		{
			SetupTwoHeroes();
			Bed.State.KillUnit(HeroB);
			Context.ExpectedHealth.Remove(HeroB);
			Context.ExpectedHealth.Add(HeroA, 0);

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestTrue("bAttacking by the real damage", Variant.bAttacking);
			TestEqual("Score has no damage and no kill", Variant.Score, -1.f);
		});

		It("kill weight raises the score of a killing variant", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(3, 3), 1);
			Bed.AddUnit(HeroB, EUnitTeam::Hero, FIntPoint(3, 5), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(2, 2)) };

			// A: 1 урон (по ожидаемому здоровью) + убийство, B: 2 урона
			const FAIVariant WithKill = Choose(MonsterId, 0, false);
			TestEqual("A is killed", WithKill.SelectedTile, FIntPoint(3, 3));
			TestEqual("Score", WithKill.Score, 29.f);

			Profile.KillWeight = 0.f;
			const FAIVariant NoKill = Choose(MonsterId, 0, false);
			TestEqual("B takes more damage", NoKill.SelectedTile, FIntPoint(3, 5));
			TestEqual("Score", NoKill.Score, 19.f);
		});

		It("the score follows the profile formula and measures the distance before the ability", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 6), 5);
			ResetContext();
			// удар отбрасывает героя на (4,7), после применения расстояние было бы 3
			Abilities = { MakeState(MakeSingleTarget({ MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies),
				MakePush(EPushDirection::AwayFromSource, 1, EAreaPart::All, ETargetFilter::Enemies) }, 2)) };
			Profile.DamageWeight = 10.f;
			Profile.PreferredDistance = 1;
			Profile.DistanceWeight = 3.f;

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestTrue("bAttacking", Variant.bAttacking);
			TestEqual("Score = 1 * 10 - |2 - 1| * 3", Variant.Score, 7.f);
		});

		It("the distance penalty moves a ranged monster to the preferred distance", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 6), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1, 2)) };

			Profile.PreferredDistance = 2;
			const FAIVariant Ranged = Choose(MonsterId, 3, true);
			TestEqual("stays at distance 2", Ranged.MoveTile, FIntPoint(4, 4));
			TestEqual("Score", Ranged.Score, 10.f);

			Profile.PreferredDistance = 1;
			const FAIVariant Melee = Choose(MonsterId, 3, true);
			TestEqual("steps next to the hero", Melee.MoveTile, FIntPoint(4, 5));
			TestEqual("PathCost", Melee.PathCost, 1);
			TestEqual("Score", Melee.Score, 10.f);
		});
	});

	Describe("Ties", [this]()
	{
		It("equal scores are resolved by the smaller path cost", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 5), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 4), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 4, true);
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(4, 5));
			TestEqual("PathCost", Variant.PathCost, 0);
		});

		It("remaining ties are resolved by the random stream and do not spend it otherwise", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(2, 2), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 4), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			// рядом с героем на расстоянии пути 3 только (3,4) и (4,3)
			TSet<FIntPoint> Seen;
			for (int32 Seed = 0; Seed < 32; ++Seed)
			{
				Random.Initialize(Seed);
				const FAIVariant First = Choose(MonsterId, 4, true);
				TestEqual("PathCost", First.PathCost, 3);
				TestTrue("one of the two tiles", First.MoveTile == FIntPoint(3, 4) || First.MoveTile == FIntPoint(4, 3));
				Seen.Add(First.MoveTile);

				Random.Initialize(Seed);
				TestEqual("the same seed gives the same tile", Choose(MonsterId, 4, true).MoveTile, First.MoveTile);
			}
			TestEqual("both tiles are chosen for different seeds", Seen.Num(), 2);

			// единственный лучший вариант поток не расходует
			Bed.State.MoveUnit(MonsterId, FIntPoint(4, 5));
			Random.Initialize(5);
			const int32 SeedBefore = Random.GetCurrentSeed();
			Choose(MonsterId, 4, true);
			TestEqual("the stream is untouched", Random.GetCurrentSeed(), SeedBefore);
		});
	});

	Describe("Movement towards heroes", [this]()
	{
		It("moves to the reachable tile nearest to the preferred distance", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(0, 7), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(7, 7), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 3, true);
			TestEqual("AbilityIndex", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestFalse("bAttacking", Variant.bAttacking);
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(3, 7));
			TestEqual("PathCost", Variant.PathCost, 3);
		});

		It("stays if the current tile is not worse", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			ResetContext();

			// без способностей: клетки (4,2), (4,4) и (5,3) не хуже, но дороже
			const FAIVariant Variant = Choose(MonsterId, 3, true);
			TestEqual("AbilityIndex", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(3, 3));
			TestEqual("PathCost", Variant.PathCost, 0);
		});
	});

	Describe("Without moving", [this]()
	{
		It("no move and no attack gives no intent", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(0, 7), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(7, 7), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 3, false);
			TestEqual("AbilityIndex", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestFalse("bAttacking", Variant.bAttacking);
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(0, 7));
			TestEqual("PathCost", Variant.PathCost, 0);
		});

		It("without living heroes stays on the tile without an ability", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			Bed.State.KillUnit(HeroA);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 3, true);
			TestEqual("AbilityIndex", Variant.AbilityIndex, static_cast<int32>(INDEX_NONE));
			TestEqual("MoveTile", Variant.MoveTile, FIntPoint(3, 3));
		});

		It("the intent phase sees a monster that blocked the projectile", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 1), 5);
			FResolvedAbility Shot = MakeTargeting(ETargetingShape::Cross, 1, 5, ETrajectory::Projectile, ETargetRequirement::Enemy);
			Shot.Area.Type = EAreaType::Tile;
			Shot.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) };
			Abilities = { MakeState(Shot) };
			ResetContext();

			const FAIVariant Before = Choose(MonsterId, 0, false);
			TestTrue("the shot reaches the hero", Before.bAttacking);
			TestEqual("the nearest valid tile of the direction", Before.SelectedTile, FIntPoint(4, 3));
			TestEqual("TargetPoint", Before.Outcome.Cast.TargetPoint, FIntPoint(4, 1));

			// другой монстр встал на линию огня: фактическая цель - союзник, требование не выполнено
			Bed.AddUnit(OtherMonsterId, EUnitTeam::Monster, FIntPoint(4, 2), 3);
			ResetContext();
			const FAIVariant After = Choose(MonsterId, 0, false);
			TestFalse("bAttacking", After.bAttacking);
			TestEqual("AbilityIndex", After.AbilityIndex, static_cast<int32>(INDEX_NONE));
		});

		It("a projectile considers one tile per direction, the nearest allowed one", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(4, 4), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 1), 5);
			FResolvedAbility Shot = MakeTargeting(ETargetingShape::Cross, 2, 5, ETrajectory::Projectile, ETargetRequirement::Enemy);
			Shot.Area.Type = EAreaType::Tile;
			Shot.Effects = { MakeDamage(1, EAreaPart::All, ETargetFilter::Enemies) };
			Abilities = { MakeState(Shot) };
			ResetContext();

			// MinRange 2 ограничивает выбор клетки, снаряд летит с соседней
			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("SelectedTile", Variant.SelectedTile, FIntPoint(4, 2));
			TestEqual("TargetPoint", Variant.Outcome.Cast.TargetPoint, FIntPoint(4, 1));
			TestEqual("Direction", Variant.Outcome.Cast.Direction, EGridDirection::Up);
		});
	});

	Describe("RegisterChoice", [this]()
	{
		It("an attacking variant adds the area to DangerTiles and lowers the expected health", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 5);
			Bed.AddUnit(HeroB, EUnitTeam::Hero, FIntPoint(0, 0), 4);
			ResetContext();
			Abilities = { MakeState(MakePoke(2)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			FMonsterAI::RegisterChoice(Bed.State, Variant, Context);
			TestEqual("DangerTiles", Context.DangerTiles.Num(), 1);
			TestTrue("the area tile", Context.DangerTiles.Contains(FIntPoint(4, 3)));
			TestEqual("hit hero", Context.ExpectedHealth[HeroA], 3);
			TestEqual("other hero", Context.ExpectedHealth[HeroB], 4);
		});

		It("the expected health does not go below zero", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(4, 3), 3);
			ResetContext();
			Context.ExpectedHealth[HeroA] = 1;
			Abilities = { MakeState(MakePoke(2)) };

			const FAIVariant Variant = Choose(MonsterId, 0, false);
			TestEqual("Score has the damage limited to 1 and a kill", Variant.Score, 30.f);
			FMonsterAI::RegisterChoice(Bed.State, Variant, Context);
			TestEqual("ExpectedHealth", Context.ExpectedHealth[HeroA], 0);
		});

		It("a non-attacking variant does not change the context", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(0, 7), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(7, 7), 5);
			ResetContext();
			Abilities = { MakeState(MakePoke(1)) };

			const FAIVariant Variant = Choose(MonsterId, 3, true);
			FMonsterAI::RegisterChoice(Bed.State, Variant, Context);
			TestEqual("DangerTiles", Context.DangerTiles.Num(), 0);
			TestEqual("ExpectedHealth", Context.ExpectedHealth[HeroA], 5);
		});
	});

	Describe("MakeContext", [this]()
	{
		It("holds the current health of living heroes only", [this]()
		{
			Bed.AddUnit(MonsterId, EUnitTeam::Monster, FIntPoint(3, 3), 3);
			Bed.AddUnit(HeroA, EUnitTeam::Hero, FIntPoint(0, 0), 2, 5);
			Bed.AddUnit(HeroB, EUnitTeam::Hero, FIntPoint(1, 0), 4);
			Bed.AddUnit(12, EUnitTeam::Hero, FIntPoint(2, 0), 1);
			Bed.State.KillUnit(12);

			const FMonsterAIContext Made = FMonsterAI::MakeContext(Bed.State);
			TestEqual("DangerTiles", Made.DangerTiles.Num(), 0);
			TestEqual("ExpectedHealth.Num", Made.ExpectedHealth.Num(), 2);
			TestEqual("wounded hero", Made.ExpectedHealth[HeroA], 2);
			TestEqual("healthy hero", Made.ExpectedHealth[HeroB], 4);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
