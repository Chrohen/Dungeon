#include "UI/UnitTooltipWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Data/AbilityDefinition.h"
#include "Data/UnitDefinition.h"
#include "UI/HealthPipsWidget.h"
#include "UI/UIUtils.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"
#include "Units/UnitHealthBarComponent.h"

void UUnitTooltipWidget::SetUnit(ADungeonUnit* InUnit)
{
	if (InUnit == nullptr || !InUnit->IsAlive())
	{
		Unit.Reset();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Unit = InUnit;
	Refresh();
	UpdatePosition();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

ADungeonUnit* UUnitTooltipWidget::GetUnit() const
{
	return Unit.Get();
}

void UUnitTooltipWidget::Refresh()
{
	const ADungeonUnit* Target = Unit.Get();
	if (Target == nullptr)
	{
		return;
	}

	// Герой - имя, монстр - тип (название определения)
	if (const AHeroUnit* Hero = Cast<AHeroUnit>(Target))
	{
		NameText->SetText(Hero->GetDisplayName());
	}
	else
	{
		NameText->SetText(Target->Definition != nullptr ? Target->Definition->DisplayName : FText::GetEmpty());
	}

	const UUnitHealthBarComponent* HealthBar = Target->HealthBar;
	const FLinearColor Color = HealthBar == nullptr ? FLinearColor::White
		: Target->Team == EUnitTeam::Hero ? HealthBar->HeroColor : HealthBar->MonsterColor;
	Pips->SetValues(Target->GetHealth(), Target->GetMaxHealth(), Color);
	HealthText->SetText(FText::Format(NSLOCTEXT("UnitTooltip", "Health", "{0}/{1}"),
		FText::AsNumber(Target->GetHealth()), FText::AsNumber(Target->GetMaxHealth())));

	// Намерение есть только у монстров
	const AMonsterUnit* Monster = Cast<AMonsterUnit>(Target);
	const UBattleSubsystem* Battle = BoundBattle.Get();
	if (Monster != nullptr && Battle != nullptr)
	{
		IntentText->SetText(MakeIntentText(*Monster, Battle->GetIntentViews()));
		IntentText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		IntentText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

int32 UUnitTooltipWidget::GetIntentDamage(const FIntentView& View)
{
	int32 Damage = 0;
	for (const FIntentTileDamage& TileDamage : View.TileDamage)
	{
		Damage = FMath::Max(Damage, TileDamage.Damage);
	}
	return Damage;
}

FText UUnitTooltipWidget::MakeIntentText(const AMonsterUnit& Monster, const TArray<FIntentView>& Views)
{
	const FIntentView* View = Monster.HasIntent()
		? Views.FindByPredicate([&Monster](const FIntentView& Candidate) { return Candidate.Monster.Get() == &Monster; })
		: nullptr;
	if (View == nullptr)
	{
		return NSLOCTEXT("UnitTooltip", "NoIntent", "Нет намерения");
	}

	FText AbilityName;
	if (Monster.GetAbilities().IsValidIndex(Monster.Intent.AbilityIndex))
	{
		if (const UAbilityDefinition* Definition = Monster.GetAbilities()[Monster.Intent.AbilityIndex].Ability.Definition)
		{
			AbilityName = Definition->DisplayName;
		}
	}
	return FText::Format(NSLOCTEXT("UnitTooltip", "Intent", "{0}, урон {1}, порядок {2}"),
		AbilityName, FText::AsNumber(GetIntentDamage(*View)), FText::AsNumber(View->Order));
}

void UUnitTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UBattleSubsystem* Battle = DungeonUI::GetBattle(*this))
	{
		BoundBattle = Battle;
		Battle->OnUnitChanged.AddDynamic(this, &UUnitTooltipWidget::OnUnitChanged);
		Battle->OnIntentsChanged.AddDynamic(this, &UUnitTooltipWidget::OnIntentsChanged);
	}
	else
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("UUnitTooltipWidget::NativeConstruct: UBattleSubsystem is not available."));
	}

	// По умолчанию подсказка скрыта
	SetUnit(Unit.Get());
}

void UUnitTooltipWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnUnitChanged.RemoveDynamic(this, &UUnitTooltipWidget::OnUnitChanged);
		Battle->OnIntentsChanged.RemoveDynamic(this, &UUnitTooltipWidget::OnIntentsChanged);
	}
	BoundBattle.Reset();

	Super::NativeDestruct();
}

void UUnitTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Unit.IsValid())
	{
		UpdatePosition();
	}
}

void UUnitTooltipWidget::OnUnitChanged(ADungeonUnit* Changed)
{
	if (Changed == nullptr || Changed != Unit.Get())
	{
		return;
	}
	// Погибший юнит больше не подсказывается
	if (!Changed->IsAlive())
	{
		SetUnit(nullptr);
		return;
	}
	Refresh();
}

void UUnitTooltipWidget::OnIntentsChanged()
{
	Refresh();
}

void UUnitTooltipWidget::UpdatePosition()
{
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (CanvasSlot == nullptr)
	{
		return;
	}

	// Координаты окна с учётом масштаба DPI
	FVector2D Position = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) + CursorOffset;
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D Size = GetDesiredSize();
	Position.X = FMath::Clamp(Position.X, 0.0, FMath::Max(ViewportSize.X - Size.X, 0.0));
	Position.Y = FMath::Clamp(Position.Y, 0.0, FMath::Max(ViewportSize.Y - Size.Y, 0.0));
	CanvasSlot->SetPosition(Position);
}
