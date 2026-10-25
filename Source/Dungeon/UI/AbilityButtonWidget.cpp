#include "UI/AbilityButtonWidget.h"
#include "Battle/BattleSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/AbilityDefinition.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/UIUtils.h"
#include "Units/HeroUnit.h"

namespace
{
	FText GetAreaPartText(EAreaPart Part)
	{
		switch (Part)
		{
		case EAreaPart::Center:
			return NSLOCTEXT("AbilityButton", "PartCenter", "центр");
		case EAreaPart::Edge:
			return NSLOCTEXT("AbilityButton", "PartEdge", "край");
		default:
			return NSLOCTEXT("AbilityButton", "PartAll", "вся область");
		}
	}
}

EAbilityButtonState UAbilityButtonWidget::GetButtonState(const AHeroUnit& Hero, int32 Index, int32 SelectedIndex, bool bIsPlayerTurn)
{
	const TArray<FUnitAbilityState>& Abilities = Hero.GetAbilities();
	if (!Abilities.IsValidIndex(Index))
	{
		return EAbilityButtonState::Unavailable;
	}
	if (Index == SelectedIndex)
	{
		return EAbilityButtonState::Selected;
	}
	// Перезарядка важнее «Недоступна»: число ходов информативнее (TD 13, п. 21)
	if (Abilities[Index].RemainingCooldown > 0)
	{
		return EAbilityButtonState::Cooldown;
	}
	if (!Hero.CanAct() || !bIsPlayerTurn)
	{
		return EAbilityButtonState::Unavailable;
	}
	return EAbilityButtonState::Available;
}

FText UAbilityButtonWidget::MakeTooltipText(const FResolvedAbility& Ability)
{
	TArray<FText> Lines;
	if (const UAbilityDefinition* Definition = Ability.Definition)
	{
		Lines.Add(Definition->DisplayName);
		if (!Definition->Description.IsEmpty())
		{
			Lines.Add(Definition->Description);
		}
	}
	for (const FAbilityEffect& Effect : Ability.Effects)
	{
		if (Effect.Type == EAbilityEffectType::Damage)
		{
			Lines.Add(FText::Format(NSLOCTEXT("AbilityButton", "Damage", "Урон: {0} ({1})"), FText::AsNumber(Effect.Amount), GetAreaPartText(Effect.AreaPart)));
		}
	}
	Lines.Add(FText::Format(NSLOCTEXT("AbilityButton", "Range", "Дальность: {0}–{1}"), FText::AsNumber(Ability.Targeting.MinRange), FText::AsNumber(Ability.Targeting.MaxRange)));
	Lines.Add(FText::Format(NSLOCTEXT("AbilityButton", "Cooldown", "Перезарядка: {0}"), FText::AsNumber(Ability.Cooldown)));
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

void UAbilityButtonWidget::SetAbility(AHeroUnit* InHero, int32 InIndex)
{
	Hero = InHero;
	AbilityIndex = InIndex;
	Refresh();
}

AHeroUnit* UAbilityButtonWidget::GetHero() const
{
	return Hero.Get();
}

void UAbilityButtonWidget::Refresh()
{
	const AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr || !Unit->GetAbilities().IsValidIndex(AbilityIndex))
	{
		State = EAbilityButtonState::Unavailable;
		Button->SetIsEnabled(false);
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		SelectedFrame->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	const FUnitAbilityState& Ability = Unit->GetAbilities()[AbilityIndex];
	State = ComputeState();

	const bool bClickable = State == EAbilityButtonState::Available || State == EAbilityButtonState::Selected;
	Button->SetIsEnabled(bClickable);
	Icon->SetColorAndOpacity(bClickable ? FLinearColor::White : DimmedColor);
	DungeonUI::SetIcon(Icon, Ability.Ability.Definition != nullptr ? Ability.Ability.Definition->Icon.Get() : nullptr);
	SelectedFrame->SetVisibility(State == EAbilityButtonState::Selected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	if (State == EAbilityButtonState::Cooldown)
	{
		CooldownText->SetText(FText::AsNumber(Ability.RemainingCooldown));
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}

	Button->SetToolTipText(MakeTooltipText(Ability.Ability));
}

void UAbilityButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button->OnClicked.AddUniqueDynamic(this, &UAbilityButtonWidget::OnButtonClicked);
}

void UAbilityButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UBattleSubsystem* Battle = DungeonUI::GetBattle(*this);
	if (Battle != nullptr)
	{
		BoundBattle = Battle;
		Battle->OnUnitChanged.AddDynamic(this, &UAbilityButtonWidget::OnUnitChanged);
		Battle->OnPhaseChanged.AddDynamic(this, &UAbilityButtonWidget::OnPhaseChanged);
	}
	if (UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this))
	{
		BoundSelection = Selection;
		Selection->OnSelectionChanged.AddDynamic(this, &UAbilityButtonWidget::OnSelectionChanged);
	}

	// Кнопка создаётся посреди боя: сразу читаем текущее состояние
	Refresh();
}

void UAbilityButtonWidget::NativeDestruct()
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnUnitChanged.RemoveDynamic(this, &UAbilityButtonWidget::OnUnitChanged);
		Battle->OnPhaseChanged.RemoveDynamic(this, &UAbilityButtonWidget::OnPhaseChanged);
	}
	if (UBattleSelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged.RemoveDynamic(this, &UAbilityButtonWidget::OnSelectionChanged);
	}
	BoundBattle.Reset();
	BoundSelection.Reset();

	Super::NativeDestruct();
}

void UAbilityButtonWidget::OnButtonClicked()
{
	// Нажатие обрабатывается только в Available и Selected
	if (Hero.IsValid() && (State == EAbilityButtonState::Available || State == EAbilityButtonState::Selected))
	{
		if (UBattleSelectionComponent* Selection = DungeonUI::GetSelection(*this))
		{
			Selection->SelectAbility(AbilityIndex);
		}
	}
}

void UAbilityButtonWidget::OnUnitChanged(ADungeonUnit* Unit)
{
	if (Unit != nullptr && Unit == Hero.Get())
	{
		Refresh();
	}
}

void UAbilityButtonWidget::OnPhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	Refresh();
}

void UAbilityButtonWidget::OnSelectionChanged(ESelectionState SelectionState, AHeroUnit* SelectedHero, int32 SelectedAbility)
{
	Refresh();
}

int32 UAbilityButtonWidget::GetSelectedIndex() const
{
	const UBattleSelectionComponent* Selection = BoundSelection.Get();
	if (Selection != nullptr && Selection->GetState() == ESelectionState::Aiming && Selection->GetSelectedHero() == Hero.Get())
	{
		return Selection->GetSelectedAbility();
	}
	return INDEX_NONE;
}

EAbilityButtonState UAbilityButtonWidget::ComputeState() const
{
	const AHeroUnit* Unit = Hero.Get();
	if (Unit == nullptr)
	{
		return EAbilityButtonState::Unavailable;
	}
	const UBattleSubsystem* Battle = BoundBattle.Get();
	return GetButtonState(*Unit, AbilityIndex, GetSelectedIndex(), Battle != nullptr && Battle->IsPlayerTurn());
}
