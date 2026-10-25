#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Battle/BattleSubsystem.h"
#include "Core/DungeonBattleGameMode.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Engine/World.h"
#include "Grid/DungeonGridActor.h"
#include "Tests/BattleTestHelpers.h"
#include "UI/UnitTooltipWidget.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Текст намерения в подсказке юнита (решение 12) без виджетов и мира
BEGIN_DEFINE_SPEC(FUnitTooltipSpec, "Dungeon.UI.UnitTooltip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UMonsterDefinition> Definition;
	TStrongObjectPtr<UAbilityDefinition> Ability;
	TStrongObjectPtr<AMonsterUnit> Monster;

	FIntentView MakeView(int32 Order, const TArray<int32>& Damage) const
	{
		FIntentView View;
		View.Monster = Monster.Get();
		View.Order = Order;
		for (int32 Index = 0; Index < Damage.Num(); ++Index)
		{
			FIntentTileDamage TileDamage;
			TileDamage.Tile = FIntPoint(Index, 0);
			TileDamage.Damage = Damage[Index];
			View.TileDamage.Add(TileDamage);
		}
		return View;
	}
END_DEFINE_SPEC(FUnitTooltipSpec)

void FUnitTooltipSpec::Define()
{
	BeforeEach([this]()
	{
		Ability.Reset(NewObject<UAbilityDefinition>());
		Ability->DisplayName = FText::FromString(TEXT("Strike"));

		Definition.Reset(NewObject<UMonsterDefinition>());
		FUnitAbilityEntry Entry;
		Entry.Ability = Ability.Get();
		Definition->Abilities.Add(Entry);

		Monster.Reset(NewObject<AMonsterUnit>());
		Monster->InitializeUnit(Definition.Get(), FIntPoint(4, 4), 5);
	});

	AfterEach([this]()
	{
		Monster.Reset();
		Definition.Reset();
		Ability.Reset();
	});

	It("shows the greatest TileDamage as the damage of the intent", [this]()
	{
		TestEqual("area damage", UUnitTooltipWidget::GetIntentDamage(MakeView(1, { 1, 3, 2 })), 3);
		TestEqual("single tile", UUnitTooltipWidget::GetIntentDamage(MakeView(1, { 2 })), 2);
		TestEqual("no damage", UUnitTooltipWidget::GetIntentDamage(MakeView(1, {})), 0);
		TestEqual("zero damage tiles", UUnitTooltipWidget::GetIntentDamage(MakeView(1, { 0, 0 })), 0);
	});

	It("writes the ability, the damage and the order of the intent", [this]()
	{
		FMonsterIntent Intent;
		Intent.bActive = true;
		Intent.AbilityIndex = 0;
		Monster->SetIntent(Intent);

		const TArray<FIntentView> Views = { MakeView(2, { 1, 3 }) };
		TestEqual("text", UUnitTooltipWidget::MakeIntentText(*Monster, Views).ToString(), FString(TEXT("Strike, урон 3, порядок 2")));
	});

	It("writes «no intent» for a monster without an intent or without a view", [this]()
	{
		const FString NoIntent = NSLOCTEXT("UnitTooltip", "NoIntent", "Нет намерения").ToString();
		TestEqual("no intent", UUnitTooltipWidget::MakeIntentText(*Monster, { MakeView(1, { 2 }) }).ToString(), NoIntent);

		FMonsterIntent Intent;
		Intent.bActive = true;
		Monster->SetIntent(Intent);
		TestEqual("an intent without a view", UUnitTooltipWidget::MakeIntentText(*Monster, {}).ToString(), NoIntent);
	});

	It("does not mix up the intents of different monsters", [this]()
	{
		TStrongObjectPtr<AMonsterUnit> Other(NewObject<AMonsterUnit>());
		Other->InitializeUnit(Definition.Get(), FIntPoint(5, 5), 6);
		FMonsterIntent Intent;
		Intent.bActive = true;
		Intent.AbilityIndex = 0;
		Monster->SetIntent(Intent);

		FIntentView OtherView = MakeView(1, { 7 });
		OtherView.Monster = Other.Get();
		const TArray<FIntentView> Views = { OtherView, MakeView(2, { 1 }) };
		TestEqual("text", UUnitTooltipWidget::MakeIntentText(*Monster, Views).ToString(), FString(TEXT("Strike, урон 1, порядок 2")));
	});
}

// RestartBattle: тот же конфиг и то же поле
BEGIN_DEFINE_SPEC(FBattleGameModeSpec, "Dungeon.Core.GameMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	TStrongObjectPtr<UBattleEventRecorder> Recorder;
	UBattleSubsystem* Battle = nullptr;
END_DEFINE_SPEC(FBattleGameModeSpec)

void FBattleGameModeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Battle = TestWorld.GetBattle();
		Recorder.Reset(NewObject<UBattleEventRecorder>());
		if (Battle)
		{
			Recorder->Subscribe(*Battle);
		}
	});

	AfterEach([this]()
	{
		Recorder.Reset();
		Battle = nullptr;
		TestWorld.Destroy();
	});

	It("restarts the battle on the same field with a new set of units", [this]()
	{
		if (!Battle) return;
		ADungeonBattleGameMode* GameMode = TestWorld.GetWorld()->SpawnActor<ADungeonBattleGameMode>();
		GameMode->BattlefieldConfig = TestWorld.GetConfig();
		GameMode->GridActor = TestWorld.GetGrid();

		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		Battle->EndPlayerTurn();
		Battle->EndPlayerTurn();
		TestEqual("turn before the restart", Battle->GetTurnNumber(), 3);
		const TArray<AHeroUnit*> OldHeroes = Battle->GetHeroes();
		TestEqual("heroes", OldHeroes.Num(), 3);
		Recorder->Reset();

		GameMode->RestartBattle();

		TestEqual("OnBattleStarted", Recorder->BattleStartedCount, 1);
		TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
		TestEqual("turn", Battle->GetTurnNumber(), 1);
		TestTrue("the same field", Battle->GetGrid() == TestWorld.GetGrid());
		TestEqual("new heroes", Battle->GetHeroes().Num(), 3);
		TestEqual("old heroes are gone", TestWorld.CountUnitActors(), 3);
		for (const AHeroUnit* OldHero : OldHeroes)
		{
			TestFalse("an old hero is destroyed", IsValid(OldHero));
		}
	});

	It("restarts a finished battle", [this]()
	{
		if (!Battle) return;
		ADungeonBattleGameMode* GameMode = TestWorld.GetWorld()->SpawnActor<ADungeonBattleGameMode>();
		GameMode->BattlefieldConfig = TestWorld.GetConfig();
		GameMode->GridActor = TestWorld.GetGrid();

		// Без волн и монстров бой заканчивается победой при входе в ход игрока
		TestWorld.SetWaves({}, 0);
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		TestTrue("finished", Battle->IsBattleOver());

		TestWorld.SetWaves({}, 100);
		GameMode->RestartBattle();
		TestFalse("not finished", Battle->IsBattleOver());
		TestEqual("phase", Battle->GetCurrentPhase(), EBattlePhase::PlayerTurn);
	});

	It("logs an error and does nothing without a config or a field", [this]()
	{
		AddExpectedMessage(TEXT("there is no battle to restart"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
		if (!Battle) return;
		ADungeonBattleGameMode* GameMode = TestWorld.GetWorld()->SpawnActor<ADungeonBattleGameMode>();
		GameMode->RestartBattle();
		TestEqual("no battle started", Recorder->BattleStartedCount, 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
