#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/DungeonCollision.h"
#include "Data/AbilityDefinition.h"
#include "Data/MonsterDefinition.h"
#include "Data/UnitDefinition.h"
#include "Materials/Material.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"
#include "Units/UnitHealthBarComponent.h"

// Юниты проверяются без уровня и без ассетов: актёры создаются вне мира
BEGIN_DEFINE_SPEC(FUnitSpec, "Dungeon.Units.Unit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UUnitDefinition> Definition;
	TStrongObjectPtr<AHeroUnit> Hero;

	// Способность с перезарядкой, добавленная в Definition
	UAbilityDefinition* AddAbility(int32 Cooldown);
	TArray<TStrongObjectPtr<UAbilityDefinition>> AbilityAssets;
END_DEFINE_SPEC(FUnitSpec)

UAbilityDefinition* FUnitSpec::AddAbility(int32 Cooldown)
{
	UAbilityDefinition* Ability = NewObject<UAbilityDefinition>();
	Ability->Cooldown = Cooldown;
	AbilityAssets.Emplace(Ability);

	FUnitAbilityEntry Entry;
	Entry.Ability = Ability;
	Definition->Abilities.Add(Entry);
	return Ability;
}

void FUnitSpec::Define()
{
	BeforeEach([this]()
	{
		Definition.Reset(NewObject<UUnitDefinition>());
		Hero.Reset(NewObject<AHeroUnit>());
	});

	AfterEach([this]()
	{
		Hero.Reset();
		Definition.Reset();
		AbilityAssets.Reset();
	});

	It("sets the team in the subclasses", [this]()
	{
		TStrongObjectPtr<AMonsterUnit> Monster(NewObject<AMonsterUnit>());
		TestEqual("hero", Hero->Team, EUnitTeam::Hero);
		TestEqual("monster", Monster->Team, EUnitTeam::Monster);
	});

	It("uses the move points of the definition", [this]()
	{
		TestEqual("without Definition", Hero->GetMovePoints(), 0);
		Definition->MovePoints = 4;
		Hero->InitializeUnit(Definition.Get(), FIntPoint(2, 1), 7);
		TestEqual("MovePoints", Hero->GetMovePoints(), 4);
		TestEqual("GridPosition", Hero->GridPosition, FIntPoint(2, 1));
		TestTrue("Definition", Hero->Definition == Definition.Get());
	});

	It("tracks the turn state", [this]()
	{
		TestTrue("CanMove at the start", Hero->CanMove());
		TestTrue("CanAct at the start", Hero->CanAct());
		Hero->MarkMoved();
		TestFalse("CanMove after MarkMoved", Hero->CanMove());
		TestTrue("bHasMoved", Hero->TurnState.bHasMoved);
		TestTrue("CanAct after MarkMoved", Hero->CanAct());
		Hero->MarkActed();
		TestFalse("CanAct after MarkActed", Hero->CanAct());
		TestTrue("bHasActed", Hero->TurnState.bHasActed);
		Hero->ResetTurnState();
		TestTrue("CanMove after ResetTurnState", Hero->CanMove());
		TestTrue("CanAct after ResetTurnState", Hero->CanAct());
		TestFalse("bHasActed", Hero->TurnState.bHasActed);
	});

	It("places the bottom of the capsule at the given point", [this]()
	{
		Hero->SetGridPosition(FIntPoint(2, 3), FVector(250.0, 350.0, 0.0));
		TestEqual("GridPosition", Hero->GridPosition, FIntPoint(2, 3));

		const double HalfHeight = Hero->Capsule->GetScaledCapsuleHalfHeight();
		TestTrue("location", Hero->GetActorLocation().Equals(FVector(250.0, 350.0, HalfHeight), 0.01));
	});

	It("blocks only the Unit trace channel with the capsule", [this]()
	{
		TestTrue("Capsule is the root", Hero->GetRootComponent() == Hero->Capsule);
		TestEqual("collision", Hero->Capsule->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		TestEqual("Unit", Hero->Capsule->GetCollisionResponseToChannel(ECC_Unit), ECR_Block);
		TestEqual("Grid", Hero->Capsule->GetCollisionResponseToChannel(ECC_Grid), ECR_Ignore);
		TestEqual("Visibility", Hero->Capsule->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
		TestEqual("Pawn", Hero->Capsule->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		TestTrue("Mesh is attached to the capsule", Hero->Mesh->GetAttachParent() == Hero->Capsule);
		TestEqual("Mesh collision", Hero->Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	});

	It("toggles Custom Depth with stencil 1 on SetSelected", [this]()
	{
		TestFalse("initially", Hero->Mesh->bRenderCustomDepth);
		Hero->SetSelected(true);
		TestTrue("selected", Hero->Mesh->bRenderCustomDepth);
		TestEqual("stencil", Hero->Mesh->CustomDepthStencilValue, 1);
		Hero->SetSelected(false);
		TestFalse("deselected", Hero->Mesh->bRenderCustomDepth);
	});

	It("toggles Custom Depth with stencil 2 on SetFocused, and SetSelected restores stencil 1", [this]()
	{
		Hero->SetFocused(true);
		TestTrue("focused", Hero->Mesh->bRenderCustomDepth);
		TestEqual("focus stencil", Hero->Mesh->CustomDepthStencilValue, 2);
		Hero->SetFocused(false);
		TestFalse("unfocused", Hero->Mesh->bRenderCustomDepth);

		Hero->SetSelected(true);
		TestEqual("selection stencil", Hero->Mesh->CustomDepthStencilValue, 1);
	});

	Describe("health and life", [this]()
	{
		It("sets the health to the maximum and the unit alive in InitializeUnit", [this]()
		{
			Definition->MaxHealth = 6;
			Definition->Level = 3;
			Hero->SetHealth(2);
			Hero->MarkDead();

			Hero->InitializeUnit(Definition.Get(), FIntPoint(1, 1), 4);
			TestEqual("Health", Hero->GetHealth(), 6);
			TestEqual("MaxHealth", Hero->GetMaxHealth(), 6);
			TestTrue("alive", Hero->IsAlive());
			TestEqual("Level", Hero->Level, 3);
			TestEqual("UnitId", Hero->UnitId, 4);
		});

		It("keeps the turn state in InitializeUnit", [this]()
		{
			Hero->MarkMoved();
			Hero->InitializeUnit(Definition.Get(), FIntPoint(1, 1), 0);
			TestTrue("bHasMoved", Hero->TurnState.bHasMoved);
		});

		It("treats a unit without a definition as having 1 maximum health", [this]()
		{
			TestEqual("MaxHealth", Hero->GetMaxHealth(), 1);
			TestEqual("UnitId before InitializeUnit", Hero->UnitId, static_cast<int32>(INDEX_NONE));
		});

		It("clamps SetHealth to [0, max] and does not kill the unit", [this]()
		{
			Definition->MaxHealth = 5;
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);

			Hero->SetHealth(3);
			TestEqual("3", Hero->GetHealth(), 3);
			Hero->SetHealth(99);
			TestEqual("above the maximum", Hero->GetHealth(), 5);
			Hero->SetHealth(-4);
			TestEqual("below zero", Hero->GetHealth(), 0);
			TestTrue("still alive", Hero->IsAlive());
		});

		It("hides the actor, disables the capsule collision and hides the health bar in MarkDead", [this]()
		{
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			TestTrue("health bar is visible", Hero->HealthBar->IsVisible());
			TestTrue("CanMove", Hero->CanMove());

			Hero->MarkDead();
			TestFalse("alive", Hero->IsAlive());
			TestTrue("hidden", Hero->IsHidden());
			TestEqual("capsule collision", Hero->Capsule->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse("health bar", Hero->HealthBar->IsVisible());
			TestFalse("CanMove", Hero->CanMove());
			TestFalse("CanAct", Hero->CanAct());
		});

		It("has a screen-space health bar that does nothing without a widget class", [this]()
		{
			TestEqual("Space", Hero->HealthBar->GetWidgetSpace(), EWidgetSpace::Screen);
			TestTrue("DrawAtDesiredSize", Hero->HealthBar->GetDrawAtDesiredSize());
			TestNull("no widget", Hero->HealthBar->GetUserWidgetObject());

			Hero->HealthBar->SetPreview(2, true);
			Hero->HealthBar->SetOrderNumber(3);
			Hero->HealthBar->ClearPreview();
			Hero->SetHealth(1);
		});
	});

	Describe("abilities and cooldowns", [this]()
	{
		It("resolves the abilities of the definition in InitializeUnit", [this]()
		{
			AddAbility(2);
			AddAbility(0);
			Definition->Abilities[0].Overrides = { FAbilityParamValue{ FAbilityParamKey{ EAbilityParam::Cooldown, INDEX_NONE }, 5 } };
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);

			const TArray<FUnitAbilityState>& Abilities = Hero->GetAbilities();
			TestEqual("count", Abilities.Num(), 2);
			if (Abilities.Num() == 2)
			{
				TestEqual("override applied", Abilities[0].Ability.Cooldown, 5);
				TestEqual("second cooldown", Abilities[1].Ability.Cooldown, 0);
				TestTrue("Definition kept", Abilities[0].Ability.Definition == Definition->Abilities[0].Ability);
				TestEqual("level", Abilities[0].Level, 1);
				TestEqual("remaining", Abilities[0].RemainingCooldown, 0);
				TestEqual("used on turn", Abilities[0].UsedOnTurn, static_cast<int32>(INDEX_NONE));
				TestTrue("ready", Abilities[0].IsReady());
			}
		});

		It("resets the cooldowns when InitializeUnit is called again", [this]()
		{
			AddAbility(2);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			Hero->StartCooldown(0, 3);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			TestEqual("remaining", Hero->GetAbilities()[0].RemainingCooldown, 0);
			TestEqual("used on turn", Hero->GetAbilities()[0].UsedOnTurn, static_cast<int32>(INDEX_NONE));
		});

		It("starts the cooldown and does not reduce it on the turn of use", [this]()
		{
			AddAbility(2);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);

			Hero->StartCooldown(0, 3);
			TestEqual("remaining", Hero->GetAbilities()[0].RemainingCooldown, 2);
			TestEqual("used on turn", Hero->GetAbilities()[0].UsedOnTurn, 3);
			TestFalse("not ready", Hero->GetAbilities()[0].IsReady());

			Hero->TickCooldowns(3);
			TestEqual("end of the turn of use", Hero->GetAbilities()[0].RemainingCooldown, 2);
		});

		It("reduces the cooldown by 1 per turn down to 0", [this]()
		{
			AddAbility(2);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			Hero->StartCooldown(0, 3);

			Hero->TickCooldowns(3);
			Hero->TickCooldowns(4);
			TestEqual("after turn 4", Hero->GetAbilities()[0].RemainingCooldown, 1);
			Hero->TickCooldowns(5);
			TestEqual("after turn 5", Hero->GetAbilities()[0].RemainingCooldown, 0);
			TestTrue("ready on turn 6", Hero->GetAbilities()[0].IsReady());
			Hero->TickCooldowns(6);
			TestEqual("stays at 0", Hero->GetAbilities()[0].RemainingCooldown, 0);
		});

		It("makes a cooldown 1 ability ready on turn T + 2", [this]()
		{
			AddAbility(1);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			Hero->StartCooldown(0, 3);

			Hero->TickCooldowns(3);
			TestEqual("after turn 3", Hero->GetAbilities()[0].RemainingCooldown, 1);
			Hero->TickCooldowns(4);
			TestEqual("after turn 4", Hero->GetAbilities()[0].RemainingCooldown, 0);
		});

		It("keeps a zero cooldown ability ready and ignores a bad index", [this]()
		{
			AddAbility(0);
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			Hero->StartCooldown(0, 1);
			TestTrue("ready right after use", Hero->GetAbilities()[0].IsReady());

			Hero->StartCooldown(5, 1);
			Hero->StartCooldown(-1, 1);
			TestEqual("count", Hero->GetAbilities().Num(), 1);
		});
	});

	Describe("hero", [this]()
	{
		It("uses the hero name, and the class name when the name is empty", [this]()
		{
			Definition->DisplayName = FText::FromString(TEXT("Knight"));
			TestTrue("without Definition", Hero->GetDisplayName().IsEmpty());

			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			TestEqual("class name", Hero->GetDisplayName().ToString(), FString(TEXT("Knight")));

			Hero->SetHeroName(FText::FromString(TEXT("Arthur")));
			TestEqual("hero name", Hero->GetDisplayName().ToString(), FString(TEXT("Arthur")));
		});

		It("adds experience without raising the level", [this]()
		{
			Definition->Level = 2;
			Hero->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			Hero->AddExperience(30);
			Hero->AddExperience(25);
			TestEqual("Experience", Hero->Experience, 55);
			TestEqual("Level", Hero->Level, 2);
			TestEqual("UnspentPoints", Hero->UnspentPoints, 0);
		});

		It("sets the inactive overlay only when both actions are done", [this]()
		{
			TStrongObjectPtr<UMaterial> Inactive(NewObject<UMaterial>());
			Hero->InactiveMaterial = Inactive.Get();

			TestNull("initially", Hero->Mesh->GetOverlayMaterial());
			Hero->MarkMoved();
			TestNull("only moved", Hero->Mesh->GetOverlayMaterial());
			Hero->MarkActed();
			TestTrue("moved and acted", Hero->Mesh->GetOverlayMaterial() == Inactive.Get());
			Hero->ResetTurnState();
			TestNull("next turn", Hero->Mesh->GetOverlayMaterial());

			Hero->MarkActed();
			TestNull("only acted", Hero->Mesh->GetOverlayMaterial());
			Hero->InactiveMaterial = nullptr;
			Hero->MarkMoved();
			TestNull("without the material", Hero->Mesh->GetOverlayMaterial());
		});
	});

	Describe("monster", [this]()
	{
		It("stores and clears the intent", [this]()
		{
			TStrongObjectPtr<AMonsterUnit> Monster(NewObject<AMonsterUnit>());
			TestFalse("initially", Monster->HasIntent());

			FMonsterIntent Intent;
			Intent.bActive = true;
			Intent.AbilityIndex = 1;
			Intent.Order = 2;
			Monster->SetIntent(Intent);
			TestTrue("set", Monster->HasIntent());
			TestEqual("AbilityIndex", Monster->Intent.AbilityIndex, 1);
			TestEqual("Order", Monster->Intent.Order, 2);

			Monster->ClearIntent();
			TestFalse("cleared", Monster->HasIntent());
			TestEqual("AbilityIndex after clear", Monster->Intent.AbilityIndex, static_cast<int32>(INDEX_NONE));
		});

		It("reads the reward and the behavior profile from the monster definition", [this]()
		{
			TStrongObjectPtr<AMonsterUnit> Monster(NewObject<AMonsterUnit>());
			TestNull("no definition", Monster->GetMonsterDefinition());
			TestEqual("reward without definition", Monster->GetExperienceReward(), 0);
			TestEqual("default profile", Monster->GetBehaviorProfile().PreferredDistance, FMonsterBehaviorProfile().PreferredDistance);

			// Обычное определение юнита не даёт ни опыта, ни профиля
			Monster->InitializeUnit(Definition.Get(), FIntPoint(0, 0), 0);
			TestNull("plain definition", Monster->GetMonsterDefinition());
			TestEqual("reward of a plain definition", Monster->GetExperienceReward(), 0);

			TStrongObjectPtr<UMonsterDefinition> MonsterDefinition(NewObject<UMonsterDefinition>());
			MonsterDefinition->ExperienceReward = 15;
			MonsterDefinition->BehaviorProfile.PreferredDistance = 4;
			MonsterDefinition->MaxHealth = 3;
			Monster->InitializeUnit(MonsterDefinition.Get(), FIntPoint(0, 0), 1);
			TestTrue("definition", Monster->GetMonsterDefinition() == MonsterDefinition.Get());
			TestEqual("reward", Monster->GetExperienceReward(), 15);
			TestEqual("profile", Monster->GetBehaviorProfile().PreferredDistance, 4);
			TestEqual("health", Monster->GetHealth(), 3);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
