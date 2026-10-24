#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AbilityTestHelpers.h"

using namespace AbilityTest;

BEGIN_DEFINE_SPEC(FAbilityParamsSpec, "Dungeon.Abilities.Params",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAbilityTestBed Bed;
	// Урон 2 и смещение 1, дальность 1-3, область Size 2, перезарядка 2.
	// Уровень 2: урон +1, перезарядка -1. Уровень 3: урон +1, MaxRange +1
	UAbilityDefinition* Def = nullptr;
END_DEFINE_SPEC(FAbilityParamsSpec)

void FAbilityParamsSpec::Define()
{
	BeforeEach([this]()
	{
		Bed.Init();
		Def = Bed.NewAbility();
		Def->Targeting.MinRange = 1;
		Def->Targeting.MaxRange = 3;
		Def->Area.Type = EAreaType::Diamond;
		Def->Area.Size = 2;
		Def->Cooldown = 2;
		Def->Effects = { MakeDamage(2), MakePush(EPushDirection::AwayFromSource, 1) };

		FAbilityLevel Level2;
		Level2.Changes = { MakeParam(EAbilityParam::EffectAmount, 1, 0), MakeParam(EAbilityParam::Cooldown, -1) };
		FAbilityLevel Level3;
		Level3.Changes = { MakeParam(EAbilityParam::EffectAmount, 1, 0), MakeParam(EAbilityParam::MaxRange, 1) };
		Def->Levels = { Level2, Level3 };
	});

	AfterEach([this]()
	{
		Def = nullptr;
		Bed.Reset();
	});

	Describe("Resolve", [this]()
	{
		It("copies the values of the definition at level 1", [this]()
		{
			const FResolvedAbility Result = FAbilityParams::Resolve(Def, {}, 1);
			TestTrue("Definition", Result.Definition == Def);
			TestEqual("MinRange", Result.Targeting.MinRange, 1);
			TestEqual("MaxRange", Result.Targeting.MaxRange, 3);
			TestEqual("Area.Size", Result.Area.Size, 2);
			TestEqual("Cooldown", Result.Cooldown, 2);
			TestEqual("Effects", Result.Effects.Num(), 2);
			TestEqual("Effects[0].Amount", Result.Effects[0].Amount, 2);
			TestEqual("Effects[1].PushDistance", Result.Effects[1].PushDistance, 1);
		});

		It("lets overrides replace every kind of parameter", [this]()
		{
			const FResolvedAbility Result = FAbilityParams::Resolve(Def, {
				MakeParam(EAbilityParam::Cooldown, 5),
				MakeParam(EAbilityParam::MinRange, 2),
				MakeParam(EAbilityParam::MaxRange, 4),
				MakeParam(EAbilityParam::AreaSize, 3),
				MakeParam(EAbilityParam::EffectAmount, 7, 0),
				MakeParam(EAbilityParam::PushDistance, 2, 1),
				MakeParam(EAbilityParam::StatusDuration, 4, 1) }, 1);
			TestEqual("Cooldown", Result.Cooldown, 5);
			TestEqual("MinRange", Result.Targeting.MinRange, 2);
			TestEqual("MaxRange", Result.Targeting.MaxRange, 4);
			TestEqual("Area.Size", Result.Area.Size, 3);
			TestEqual("Effects[0].Amount", Result.Effects[0].Amount, 7);
			TestEqual("Effects[1].PushDistance", Result.Effects[1].PushDistance, 2);
			TestEqual("Effects[1].Duration", Result.Effects[1].Duration, 4);
		});

		It("does not change the definition", [this]()
		{
			FAbilityParams::Resolve(Def, { MakeParam(EAbilityParam::EffectAmount, 9, 0) }, 3);
			TestEqual("Effects[0].Amount", Def->Effects[0].Amount, 2);
			TestEqual("Cooldown", Def->Cooldown, 2);
		});

		It("adds level changes in order: definition, then override, then levels", [this]()
		{
			const TArray<FAbilityParamValue> Overrides = { MakeParam(EAbilityParam::EffectAmount, 5, 0) };

			FResolvedAbility Result = FAbilityParams::Resolve(Def, Overrides, 1);
			TestEqual("level 1: override only", Result.Effects[0].Amount, 5);

			Result = FAbilityParams::Resolve(Def, Overrides, 2);
			TestEqual("level 2: override + 1", Result.Effects[0].Amount, 6);
			TestEqual("level 2: Cooldown 2 - 1", Result.Cooldown, 1);
			TestEqual("level 2: MaxRange unchanged", Result.Targeting.MaxRange, 3);

			Result = FAbilityParams::Resolve(Def, Overrides, 3);
			TestEqual("level 3: override + 1 + 1", Result.Effects[0].Amount, 7);
			TestEqual("level 3: Cooldown keeps the level 2 change", Result.Cooldown, 1);
			TestEqual("level 3: MaxRange + 1", Result.Targeting.MaxRange, 4);
		});

		It("clamps Level to 1..GetMaxLevel", [this]()
		{
			TestEqual("GetMaxLevel", Def->GetMaxLevel(), 3);
			TestEqual("Level 0 is level 1", FAbilityParams::Resolve(Def, {}, 0).Effects[0].Amount, 2);
			TestEqual("negative Level is level 1", FAbilityParams::Resolve(Def, {}, -4).Effects[0].Amount, 2);
			TestEqual("Level above the maximum is the maximum", FAbilityParams::Resolve(Def, {}, 99).Effects[0].Amount, 4);
		});

		It("clamps the resolved values", [this]()
		{
			const FResolvedAbility Result = FAbilityParams::Resolve(Def, {
				MakeParam(EAbilityParam::MinRange, -2),
				MakeParam(EAbilityParam::MaxRange, -5),
				MakeParam(EAbilityParam::Cooldown, -1),
				MakeParam(EAbilityParam::AreaSize, -3),
				MakeParam(EAbilityParam::EffectAmount, -4, 0),
				MakeParam(EAbilityParam::PushDistance, -1, 1) }, 1);
			TestEqual("MinRange >= 0", Result.Targeting.MinRange, 0);
			TestEqual("MaxRange >= MinRange", Result.Targeting.MaxRange, 0);
			TestEqual("Cooldown >= 0", Result.Cooldown, 0);
			TestEqual("Size >= 0", Result.Area.Size, 0);
			TestEqual("Amount >= 0", Result.Effects[0].Amount, 0);
			TestEqual("PushDistance >= 0", Result.Effects[1].PushDistance, 0);
		});

		It("raises MaxRange to MinRange when only MinRange grows", [this]()
		{
			const FResolvedAbility Result = FAbilityParams::Resolve(Def, { MakeParam(EAbilityParam::MinRange, 6) }, 1);
			TestEqual("MinRange", Result.Targeting.MinRange, 6);
			TestEqual("MaxRange", Result.Targeting.MaxRange, 6);
		});

		It("skips unknown keys with a warning and applies the rest", [this]()
		{
			AddExpectedMessage(TEXT("unknown parameter"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
			FAbilityLevel BadLevel;
			BadLevel.Changes = { MakeParam(EAbilityParam::PushDistance, 1, 7), MakeParam(EAbilityParam::Cooldown, 1) };
			Def->Levels[0] = BadLevel;

			const FResolvedAbility Result = FAbilityParams::Resolve(Def, {
				MakeParam(EAbilityParam::EffectAmount, 9, 2),
				MakeParam(EAbilityParam::StatusDuration, 1, INDEX_NONE),
				MakeParam(EAbilityParam::MaxRange, 8) }, 2);
			TestEqual("known override applied", Result.Targeting.MaxRange, 8);
			TestEqual("known level change applied", Result.Cooldown, 3);
			TestEqual("Effects[0].Amount untouched", Result.Effects[0].Amount, 2);
			TestEqual("Effects[1].PushDistance untouched", Result.Effects[1].PushDistance, 1);
		});

		It("returns an empty ability for a null definition", [this]()
		{
			AddExpectedMessage(TEXT("definition is null"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			const FResolvedAbility Result = FAbilityParams::Resolve(nullptr, {}, 1);
			TestNull("Definition", Result.Definition.Get());
			TestEqual("Effects", Result.Effects.Num(), 0);
		});
	});

	Describe("FindParam", [this]()
	{
		It("returns the field of every parameter kind", [this]()
		{
			FResolvedAbility Ability = FAbilityParams::Resolve(Def, {}, 1);
			TestTrue("Cooldown", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::Cooldown, 0).Key) == &Ability.Cooldown);
			TestTrue("MinRange", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::MinRange, 0).Key) == &Ability.Targeting.MinRange);
			TestTrue("MaxRange", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::MaxRange, 0).Key) == &Ability.Targeting.MaxRange);
			TestTrue("AreaSize", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::AreaSize, 0).Key) == &Ability.Area.Size);
			TestTrue("EffectAmount", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::EffectAmount, 0, 1).Key) == &Ability.Effects[1].Amount);
			TestTrue("PushDistance", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::PushDistance, 0, 1).Key) == &Ability.Effects[1].PushDistance);
			TestTrue("StatusDuration", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::StatusDuration, 0, 0).Key) == &Ability.Effects[0].Duration);
		});

		It("returns nullptr when EffectIndex is outside Effects", [this]()
		{
			FResolvedAbility Ability = FAbilityParams::Resolve(Def, {}, 1);
			TestNull("index == Num", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::EffectAmount, 0, 2).Key));
			TestNull("INDEX_NONE", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::PushDistance, 0, INDEX_NONE).Key));
			TestNotNull("EffectIndex is ignored for Cooldown", FAbilityParams::FindParam(Ability, MakeParam(EAbilityParam::Cooldown, 0, 9).Key));
		});
	});

	Describe("HasParam", [this]()
	{
		It("checks EffectIndex only for effect parameters", [this]()
		{
			TestTrue("EffectAmount[1]", Def->HasParam(MakeParam(EAbilityParam::EffectAmount, 0, 1).Key));
			TestFalse("EffectAmount[2]", Def->HasParam(MakeParam(EAbilityParam::EffectAmount, 0, 2).Key));
			TestFalse("PushDistance[-1]", Def->HasParam(MakeParam(EAbilityParam::PushDistance, 0, INDEX_NONE).Key));
			TestFalse("StatusDuration[5]", Def->HasParam(MakeParam(EAbilityParam::StatusDuration, 0, 5).Key));
			TestTrue("Cooldown", Def->HasParam(MakeParam(EAbilityParam::Cooldown, 0, 9).Key));
			TestTrue("MaxRange", Def->HasParam(MakeParam(EAbilityParam::MaxRange, 0).Key));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
