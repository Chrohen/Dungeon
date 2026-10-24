#include "Units/MonsterUnit.h"
#include "Data/MonsterDefinition.h"

AMonsterUnit::AMonsterUnit()
{
	Team = EUnitTeam::Monster;
}

const UMonsterDefinition* AMonsterUnit::GetMonsterDefinition() const
{
	return Cast<UMonsterDefinition>(Definition);
}

int32 AMonsterUnit::GetExperienceReward() const
{
	const UMonsterDefinition* MonsterDefinition = GetMonsterDefinition();
	return MonsterDefinition != nullptr ? MonsterDefinition->ExperienceReward : 0;
}

FMonsterBehaviorProfile AMonsterUnit::GetBehaviorProfile() const
{
	const UMonsterDefinition* MonsterDefinition = GetMonsterDefinition();
	return MonsterDefinition != nullptr ? MonsterDefinition->BehaviorProfile : FMonsterBehaviorProfile();
}

void AMonsterUnit::SetIntent(const FMonsterIntent& InIntent)
{
	Intent = InIntent;
	RefreshVisuals();
}

void AMonsterUnit::ClearIntent()
{
	Intent = FMonsterIntent();
	RefreshVisuals();
}

bool AMonsterUnit::HasIntent() const
{
	return Intent.bActive;
}
