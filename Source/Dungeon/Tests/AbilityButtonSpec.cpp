#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/AbilityDefinition.h"
#include "Data/UnitDefinition.h"
#include "Tests/AbilityTestHelpers.h"
#include "UI/AbilityButtonWidget.h"
#include "Units/HeroUnit.h"

// Состояние кнопки способности (TD 7.5) и текст подсказки. Герой создаётся вне мира, как в Dungeon.Units.Unit
BEGIN_DEFINE_SPEC(FAbilityButtonSpec, "Dungeon.UI.AbilityButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UUnitDefinition> Definition;
	TStrongObjectPtr<AHeroUnit> Hero;
	TStrongObjectPtr<UAbilityDefinition> Strike;
	TStrongObjectPtr<UAbilityDefinition> Slow;

	static constexpr bool bPlayerTurn = true;
	static constexpr bool bOtherPhase = false;

	EAbilityButtonState State(int32 Index, int32 SelectedIndex, bool bIsPlayerTurn) const
	{
		return UAbilityButtonWidget::GetButtonState(*Hero, Index, SelectedIndex, bIsPlayerTurn);
	}
END_DEFINE_SPEC(FAbilityButtonSpec)

void FAbilityButtonSpec::Define()
{
	BeforeEach([this]()
	{
		// Способность 0 без перезарядки, способность 1 с перезарядкой 2
		Strike.Reset(NewObject<UAbilityDefinition>());
		Strike->DisplayName = FText::FromString(TEXT("Strike"));
		Strike->Description = FText::FromString(TEXT("Hits a tile."));
		Strike->Targeting.MinRange = 1;
		Strike->Targeting.MaxRange = 3;
		Strike->Effects = { AbilityTest::MakeDamage(3, EAreaPart::Center), AbilityTest::MakeDamage(1, EAreaPart::Edge), AbilityTest::MakeHeal(2) };

		Slow.Reset(NewObject<UAbilityDefinition>());
		Slow->Cooldown = 2;

		Definition.Reset(NewObject<UUnitDefinition>());
		for (UAbilityDefinition* Ability : { Strike.Get(), Slow.Get() })
		{
			FUnitAbilityEntry Entry;
			Entry.Ability = Ability;
			Definition->Abilities.Add(Entry);
		}

		Hero.Reset(NewObject<AHeroUnit>());
		Hero->InitializeUnit(Definition.Get(), FIntPoint(1, 1), 0);
	});

	AfterEach([this]()
	{
		Hero.Reset();
		Definition.Reset();
		Strike.Reset();
		Slow.Reset();
	});

	Describe("GetButtonState", [this]()
	{
		It("is Selected for the ability the hero is aiming with", [this]()
		{
			TestEqual("Selected", State(0, 0, bPlayerTurn), EAbilityButtonState::Selected);
			TestEqual("the other ability is Available", State(1, 0, bPlayerTurn), EAbilityButtonState::Available);
		});

		It("is Selected before every other check", [this]()
		{
			Hero->StartCooldown(1, 1);
			Hero->MarkActed();
			TestEqual("Selected over Cooldown and Unavailable", State(1, 1, bOtherPhase), EAbilityButtonState::Selected);
		});

		It("is Cooldown while the remaining cooldown is above 0", [this]()
		{
			Hero->StartCooldown(1, 1);
			TestEqual("Cooldown", State(1, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Cooldown);
			TestEqual("an ability without cooldown is still Available", State(0, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Available);

			Hero->TickCooldowns(2);
			Hero->TickCooldowns(3);
			TestEqual("Available when the cooldown is over", State(1, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Available);
		});

		It("is Unavailable when the hero has already acted", [this]()
		{
			Hero->MarkActed();
			TestEqual("Unavailable", State(0, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Unavailable);
		});

		It("is not Unavailable when only the hero has moved", [this]()
		{
			Hero->MarkMoved();
			TestEqual("Available", State(0, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Available);
		});

		It("is Unavailable outside the player turn", [this]()
		{
			TestEqual("Unavailable", State(0, INDEX_NONE, bOtherPhase), EAbilityButtonState::Unavailable);
		});

		It("shows Cooldown before Unavailable (TD 13, item 21)", [this]()
		{
			Hero->StartCooldown(1, 1);
			Hero->MarkActed();
			TestEqual("the hero has acted", State(1, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Cooldown);
			TestEqual("not the player turn", State(1, INDEX_NONE, bOtherPhase), EAbilityButtonState::Cooldown);
			TestEqual("without cooldown it is Unavailable", State(0, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Unavailable);
		});

		It("is Available otherwise", [this]()
		{
			TestEqual("Available", State(0, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Available);
			TestEqual("the second ability", State(1, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Available);
		});

		It("is Unavailable for an index outside the ability list", [this]()
		{
			TestEqual("negative", State(-1, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Unavailable);
			TestEqual("past the end", State(2, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Unavailable);
			TestEqual("INDEX_NONE is not a selected ability", State(INDEX_NONE, INDEX_NONE, bPlayerTurn), EAbilityButtonState::Unavailable);
		});
	});

	Describe("MakeTooltipText", [this]()
	{
		It("has the name, the description, the damage of every Damage effect with its area part, the range and the cooldown", [this]()
		{
			const FString Text = UAbilityButtonWidget::MakeTooltipText(Hero->GetAbilities()[0].Ability).ToString();
			TestTrue("name", Text.Contains(TEXT("Strike")));
			TestTrue("description", Text.Contains(TEXT("Hits a tile.")));
			TestTrue("center damage", Text.Contains(TEXT("Урон: 3 (центр)")));
			TestTrue("edge damage", Text.Contains(TEXT("Урон: 1 (край)")));
			TestFalse("a heal is not damage", Text.Contains(TEXT("Урон: 2")));
			TestTrue("range Min-Max", Text.Contains(TEXT("Дальность: 1–3")));
			TestTrue("cooldown", Text.Contains(TEXT("Перезарядка: 0")));
		});

		It("uses the final values of the unit's ability", [this]()
		{
			const FString Text = UAbilityButtonWidget::MakeTooltipText(Hero->GetAbilities()[1].Ability).ToString();
			TestTrue("cooldown", Text.Contains(TEXT("Перезарядка: 2")));
			TestFalse("no damage lines", Text.Contains(TEXT("Урон")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
