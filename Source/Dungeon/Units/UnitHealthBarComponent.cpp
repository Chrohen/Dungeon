#include "Units/UnitHealthBarComponent.h"
#include "Units/DungeonUnit.h"
#include "UI/UnitHealthBarWidget.h"

UUnitHealthBarComponent::UUnitHealthBarComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	// Нижний центр виджета стоит в якоре, деления растут вверх (отчёт задачи 0.1)
	SetPivot(FVector2D(0.5, 1.0));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void UUnitHealthBarComponent::InitWidget()
{
	Super::InitWidget();

	if (const ADungeonUnit* Unit = LastUnit.Get())
	{
		Refresh(*Unit);
	}
}

void UUnitHealthBarComponent::Refresh(const ADungeonUnit& Unit)
{
	LastUnit = &Unit;
	SetVisibility(Unit.IsAlive());

	if (UUnitHealthBarWidget* BarWidget = GetHealthBarWidget())
	{
		BarWidget->SetValues(Unit.GetHealth(), Unit.GetMaxHealth(), Unit.Team == EUnitTeam::Hero ? HeroColor : MonsterColor);
	}
}

void UUnitHealthBarComponent::SetPreview(int32 LostPips, bool bWillDie)
{
	if (UUnitHealthBarWidget* BarWidget = GetHealthBarWidget())
	{
		BarWidget->SetPreview(LostPips, bWillDie);
	}
}

void UUnitHealthBarComponent::ClearPreview()
{
	if (UUnitHealthBarWidget* BarWidget = GetHealthBarWidget())
	{
		BarWidget->ClearPreview();
	}
}

void UUnitHealthBarComponent::SetOrderNumber(int32 Order)
{
	if (UUnitHealthBarWidget* BarWidget = GetHealthBarWidget())
	{
		BarWidget->SetOrderNumber(Order);
	}
}

UUnitHealthBarWidget* UUnitHealthBarComponent::GetHealthBarWidget() const
{
	return Cast<UUnitHealthBarWidget>(GetUserWidgetObject());
}
