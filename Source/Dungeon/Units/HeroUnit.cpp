#include "Units/HeroUnit.h"
#include "Data/UnitDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"

AHeroUnit::AHeroUnit()
{
	Team = EUnitTeam::Hero;
}

void AHeroUnit::SetHeroName(const FText& InName)
{
	HeroName = InName;
	RefreshVisuals();
}

FText AHeroUnit::GetDisplayName() const
{
	if (!HeroName.IsEmpty())
	{
		return HeroName;
	}
	return Definition != nullptr ? Definition->DisplayName : FText::GetEmpty();
}

void AHeroUnit::AddExperience(int32 Amount)
{
	Experience += FMath::Max(Amount, 0);
	RefreshVisuals();
}

void AHeroUnit::RefreshVisuals()
{
	Super::RefreshVisuals();

	const bool bInactive = TurnState.bHasMoved && TurnState.bHasActed;
	Mesh->SetOverlayMaterial(bInactive ? InactiveMaterial.Get() : nullptr);
}
