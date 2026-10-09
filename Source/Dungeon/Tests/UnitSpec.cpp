#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/DungeonCollision.h"
#include "Data/UnitDefinition.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"

// Юниты проверяются без уровня и без ассетов: актёры создаются вне мира
BEGIN_DEFINE_SPEC(FUnitSpec, "Dungeon.Units.Unit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UUnitDefinition> Definition;
	TStrongObjectPtr<AHeroUnit> Hero;
END_DEFINE_SPEC(FUnitSpec)

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
		Hero->InitializeUnit(Definition.Get(), FIntPoint(2, 1));
		TestEqual("MovePoints", Hero->GetMovePoints(), 4);
		TestEqual("GridPosition", Hero->GridPosition, FIntPoint(2, 1));
		TestTrue("Definition", Hero->Definition == Definition.Get());
	});

	It("tracks the turn state", [this]()
	{
		TestTrue("CanMove at the start", Hero->CanMove());
		Hero->MarkMoved();
		TestFalse("CanMove after MarkMoved", Hero->CanMove());
		TestTrue("bHasMoved", Hero->TurnState.bHasMoved);
		Hero->ResetTurnState();
		TestTrue("CanMove after ResetTurnState", Hero->CanMove());
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
