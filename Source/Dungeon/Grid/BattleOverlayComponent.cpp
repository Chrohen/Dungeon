#include "Grid/BattleOverlayComponent.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/DungeonGridActor.h"
#include "Grid/GridHighlightComponent.h"
#include "Materials/MaterialInterface.h"
#include "UI/TileLabelWidget.h"
#include "Units/DungeonUnit.h"
#include "Units/MonsterUnit.h"
#include "Units/UnitHealthBarComponent.h"

namespace
{
	// Приоритет сортировки полупрозрачных стрелок: выше слоёв подсветки (0..5)
	constexpr int32 IntentArrowSortPriority = 10;
	constexpr int32 PreviewArrowSortPriority = 11;
}

UBattleOverlayComponent::UBattleOverlayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBattleOverlayComponent::OnRegister()
{
	Super::OnRegister();
	EnsureSubcomponents();
}

void UBattleOverlayComponent::BeginPlay()
{
	Super::BeginPlay();

	UBattleSubsystem* Battle = GetBattle();
	if (Battle == nullptr)
	{
		return;
	}

	BoundBattle = Battle;
	Battle->OnBattleStarted.AddDynamic(this, &UBattleOverlayComponent::HandleBattleStarted);
	Battle->OnPhaseChanged.AddDynamic(this, &UBattleOverlayComponent::HandlePhaseChanged);
	Battle->OnIntentsChanged.AddDynamic(this, &UBattleOverlayComponent::HandleIntentsChanged);

	// Бой мог начаться раньше, чем компонент подписался
	RefreshFromBattle();
}

void UBattleOverlayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBattleSubsystem* Battle = BoundBattle.Get())
	{
		Battle->OnBattleStarted.RemoveDynamic(this, &UBattleOverlayComponent::HandleBattleStarted);
		Battle->OnPhaseChanged.RemoveDynamic(this, &UBattleOverlayComponent::HandlePhaseChanged);
		Battle->OnIntentsChanged.RemoveDynamic(this, &UBattleOverlayComponent::HandleIntentsChanged);
	}
	BoundBattle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBattleOverlayComponent::ShowIntents(const TArray<FIntentView>& InViews)
{
	EnsureSubcomponents();
	Views = InViews;

	RebuildThreat();
	RebuildIntentArrows();
	RebuildIntentLabels();
	RebuildOrderNumbers();
	RebuildFocus();
	RefreshLabels();
}

void UBattleOverlayComponent::HideIntents()
{
	FocusMonster.Reset();
	FocusTile.Reset();
	ShowIntents(TArray<FIntentView>());
}

void UBattleOverlayComponent::SetFocus(AMonsterUnit* Monster, TOptional<FIntPoint> Tile)
{
	const TOptional<FIntPoint> NewTile = Monster != nullptr ? TOptional<FIntPoint>() : Tile;
	if (FocusMonster.Get() == Monster && FocusTile == NewTile)
	{
		return;
	}

	FocusMonster = Monster;
	FocusTile = NewTile;
	RebuildThreat();
	RebuildFocus();
}

void UBattleOverlayComponent::ShowAbilityPreview(const FAbilityOutcome& Outcome, bool bProjectile)
{
	EnsureSubcomponents();
	ResetAbilityPreviewState();
	bAbilityPreviewShown = true;

	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		TArray<FIntPoint> Center;
		if (Outcome.Cast.Area.bHasCenter)
		{
			Center.Add(Outcome.Cast.Area.Center);
		}
		Highlight->SetLayerTiles(EGridHighlightLayer::AreaCenter, Center);
		Highlight->SetLayerTiles(EGridHighlightLayer::AreaEdge, Outcome.Cast.Area.Edge);
	}

	if (const ADungeonGridActor* Grid = GetGrid())
	{
		const bool bCanDrawArrows = PreviewArrows != nullptr && PrepareArrows(PreviewArrows, PreviewArrowMaterial);

		// Линия полёта снаряда до фактической точки цели
		if (bProjectile && bCanDrawArrows)
		{
			AddArrowBetween(PreviewArrows, Outcome.Cast.SourcePosition, Outcome.Cast.TargetPoint);
		}

		for (const FAbilityEvent& Event : Outcome.Events)
		{
			switch (Event.Type)
			{
			case EAbilityEventType::Push:
				if (bCanDrawArrows)
				{
					AddArrowBetween(PreviewArrows, Event.From, Event.To);
				}
				break;
			case EAbilityEventType::Collision:
			{
				// To может быть вне поля: GridToWorld границ не проверяет
				FOverlayTileLabel Label;
				Label.Location = (Grid->GridToWorld(Event.From) + Grid->GridToWorld(Event.To)) * 0.5 + FVector(0.0, 0.0, LabelHeight);
				Label.Value = Event.Amount;
				Label.bCollision = true;
				PreviewLabels.Add(Label);
				break;
			}
			case EAbilityEventType::SourceMove:
				ShowSilhouette(Event);
				break;
			default:
				break;
			}
		}
	}

	CollectPreviews(Outcome, AbilityPreviews);
	RefreshHealthPreviews();
	RefreshLabels();
}

void UBattleOverlayComponent::ClearAbilityPreview()
{
	if (!bAbilityPreviewShown)
	{
		return;
	}
	bAbilityPreviewShown = false;

	if (UGridHighlightComponent* Highlight = GetHighlight())
	{
		Highlight->ClearLayer(EGridHighlightLayer::AreaCenter);
		Highlight->ClearLayer(EGridHighlightLayer::AreaEdge);
	}
	ResetAbilityPreviewState();
	RefreshHealthPreviews();
	RefreshLabels();
}

int32 UBattleOverlayComponent::GetVisibleLabelCount() const
{
	int32 Count = 0;
	for (const UWidgetComponent* Component : LabelPool)
	{
		if (Component != nullptr && Component->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

int32 UBattleOverlayComponent::GetShownOrder(AMonsterUnit* Monster) const
{
	const int32* Order = ShownOrders.Find(Monster);
	return Order != nullptr ? FMath::Max(*Order, 0) : 0;
}

bool UBattleOverlayComponent::GetShownPreview(ADungeonUnit* Unit, int32& OutLostPips, bool& bOutWillDie) const
{
	const FUnitPreview* Preview = AppliedPreviews.Find(Unit);
	if (Preview == nullptr)
	{
		return false;
	}
	OutLostPips = Preview->LostPips;
	bOutWillDie = Preview->bWillDie;
	return true;
}

void UBattleOverlayComponent::EnsureSubcomponents()
{
	if (!IsRegistered() || GetOwner() == nullptr)
	{
		return;
	}

	if (IntentArrows == nullptr)
	{
		IntentArrows = CreateArrows(TEXT("IntentArrows"), IntentArrowSortPriority);
	}
	if (PreviewArrows == nullptr)
	{
		PreviewArrows = CreateArrows(TEXT("PreviewArrows"), PreviewArrowSortPriority);
	}
	if (Silhouette == nullptr)
	{
		Silhouette = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("AbilitySilhouette"), RF_Transient);
		Silhouette->PrimaryComponentTick.bCanEverTick = false;
		Silhouette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Silhouette->SetGenerateOverlapEvents(false);
		Silhouette->SetCanEverAffectNavigation(false);
		Silhouette->SetCastShadow(false);
		Silhouette->SetVisibility(false);
		Silhouette->SetupAttachment(this);
		Silhouette->RegisterComponent();
	}
}

UInstancedStaticMeshComponent* UBattleOverlayComponent::CreateArrows(const TCHAR* Name, int32 SortPriority)
{
	UInstancedStaticMeshComponent* Arrows = NewObject<UInstancedStaticMeshComponent>(GetOwner(), Name, RF_Transient);
	Arrows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Arrows->SetGenerateOverlapEvents(false);
	Arrows->SetCanEverAffectNavigation(false);
	Arrows->SetCastShadow(false);
	Arrows->SetTranslucentSortPriority(SortPriority);
	Arrows->SetupAttachment(this);
	Arrows->RegisterComponent();
	return Arrows;
}

bool UBattleOverlayComponent::PrepareArrows(UInstancedStaticMeshComponent* Arrows, UMaterialInterface* Material)
{
	// Меш и материал задаются полями компонента, а они могут появиться после OnRegister: подставляем при показе
	if (ArrowMesh == nullptr)
	{
		if (!bWarnedNoArrowMesh)
		{
			bWarnedNoArrowMesh = true;
			UE_LOG(LogTemp, Warning, TEXT("UBattleOverlayComponent: ArrowMesh is not set, intent and preview arrows are not drawn."));
		}
		return false;
	}
	if (Arrows->GetStaticMesh() != ArrowMesh)
	{
		Arrows->SetStaticMesh(ArrowMesh);
	}
	if (Arrows->GetMaterial(0) != Material)
	{
		Arrows->SetMaterial(0, Material);
	}
	return true;
}

ADungeonGridActor* UBattleOverlayComponent::GetGrid() const
{
	return Cast<ADungeonGridActor>(GetOwner());
}

UGridHighlightComponent* UBattleOverlayComponent::GetHighlight() const
{
	const ADungeonGridActor* Grid = GetGrid();
	return Grid != nullptr ? Grid->GetHighlight() : nullptr;
}

UBattleSubsystem* UBattleOverlayComponent::GetBattle() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
}

void UBattleOverlayComponent::AddArrow(UInstancedStaticMeshComponent* Arrows, FIntPoint From, const FVector& WorldDirection, float LengthTiles) const
{
	const ADungeonGridActor* Grid = GetGrid();
	if (Arrows == nullptr || Grid == nullptr || WorldDirection.IsNearlyZero() || LengthTiles <= 0.f)
	{
		return;
	}

	// Меш стрелки лежит вдоль +X с опорой у хвоста, длина в один тайл: масштаб X равен длине в тайлах
	const FVector Location = Grid->GridToWorld(From) + FVector(0.0, 0.0, ArrowHeight);
	const FQuat Rotation = FRotationMatrix::MakeFromX(WorldDirection).ToQuat();
	Arrows->AddInstance(FTransform(Rotation, Location, FVector(LengthTiles, 1.0, 1.0)), /*bWorldSpace=*/true);
}

void UBattleOverlayComponent::AddArrowBetween(UInstancedStaticMeshComponent* Arrows, FIntPoint From, FIntPoint To) const
{
	const ADungeonGridActor* Grid = GetGrid();
	if (Grid == nullptr || From == To)
	{
		return;
	}
	const FVector Direction = Grid->GridToWorld(To) - Grid->GridToWorld(From);
	const float LengthTiles = static_cast<float>(FVector2D(To.X - From.X, To.Y - From.Y).Size());
	AddArrow(Arrows, From, Direction, LengthTiles);
}

FVector UBattleOverlayComponent::DirectionToWorld(EGridDirection Direction) const
{
	const ADungeonGridActor* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return FVector::ZeroVector;
	}
	const FIntPoint Step = FAbilityRules::DirectionToOffset(Direction);
	return Grid->GetActorTransform().TransformVectorNoScale(FVector(Step.X, Step.Y, 0.0));
}

void UBattleOverlayComponent::RefreshFromBattle()
{
	const UBattleSubsystem* Battle = BoundBattle.Get();
	if (Battle == nullptr)
	{
		return;
	}

	if (Battle->IsPlayerTurn())
	{
		ShowIntents(Battle->GetIntentViews());
	}
	else
	{
		HideIntents();
		ClearAbilityPreview();
	}
}

void UBattleOverlayComponent::HandleBattleStarted()
{
	// Юниты прошлого боя уничтожены: слабые ссылки на них уже недействительны. Своё состояние очищаем,
	// затем читаем новое
	HideIntents();
	ClearAbilityPreview();
	RefreshFromBattle();
}

void UBattleOverlayComponent::HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase)
{
	RefreshFromBattle();
}

void UBattleOverlayComponent::HandleIntentsChanged()
{
	RefreshFromBattle();
}

void UBattleOverlayComponent::RebuildThreat()
{
	UGridHighlightComponent* Highlight = GetHighlight();
	if (Highlight == nullptr)
	{
		return;
	}

	TArray<FIntPoint> Tiles;
	for (const FIntentView& View : Views)
	{
		for (const FIntPoint& Tile : View.Cast.Area.GetAllTiles())
		{
			Tiles.AddUnique(Tile);
		}
	}
	if (Tiles.IsEmpty())
	{
		Highlight->ClearLayer(EGridHighlightLayer::Threat);
		return;
	}

	// В фокусе одно намерение: его клетки яркие, остальные приглушены
	const FIntentView* Focused = FindView(FocusMonster.Get());
	TArray<float> Brightness;
	Brightness.Init(1.f, Tiles.Num());
	if (Focused != nullptr)
	{
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			if (!Focused->Cast.Area.Contains(Tiles[Index], EAreaPart::All))
			{
				Brightness[Index] = DimmedBrightness;
			}
		}
	}
	Highlight->SetLayerTiles(EGridHighlightLayer::Threat, Tiles, &Brightness);
}

void UBattleOverlayComponent::RebuildIntentArrows()
{
	if (IntentArrows == nullptr)
	{
		return;
	}
	if (IntentArrows->GetInstanceCount() > 0)
	{
		IntentArrows->ClearInstances();
	}
	if (Views.IsEmpty() || !PrepareArrows(IntentArrows, IntentArrowMaterial))
	{
		return;
	}

	for (const FIntentView& View : Views)
	{
		AddArrowBetween(IntentArrows, View.Cast.SourcePosition, View.Cast.TargetPoint);
		for (const FIntentPushMark& Mark : View.PushMarks)
		{
			AddArrow(IntentArrows, Mark.Tile, DirectionToWorld(Mark.Direction), PushMarkLength);
		}
	}
}

void UBattleOverlayComponent::RebuildIntentLabels()
{
	IntentLabels.Reset();

	const ADungeonGridActor* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return;
	}

	// Суммарный урон по клетке среди всех намерений (GDD). Клетки без урона подписи не получают
	TArray<FIntPoint> Tiles;
	TMap<FIntPoint, int32> Sums;
	for (const FIntentView& View : Views)
	{
		for (const FIntentTileDamage& TileDamage : View.TileDamage)
		{
			if (TileDamage.Damage <= 0)
			{
				continue;
			}
			int32& Sum = Sums.FindOrAdd(TileDamage.Tile);
			if (Sum == 0)
			{
				Tiles.Add(TileDamage.Tile);
			}
			Sum += TileDamage.Damage;
		}
	}

	for (const FIntPoint& Tile : Tiles)
	{
		FOverlayTileLabel Label;
		Label.Location = Grid->GridToWorld(Tile) + FVector(0.0, 0.0, LabelHeight);
		Label.Value = Sums[Tile];
		IntentLabels.Add(Label);
	}
}

void UBattleOverlayComponent::RebuildOrderNumbers()
{
	TMap<TWeakObjectPtr<AMonsterUnit>, int32> Desired;
	for (const FIntentView& View : Views)
	{
		if (AMonsterUnit* Monster = View.Monster.Get())
		{
			Desired.Add(Monster, View.Order);
		}
	}

	// У монстров без намерения номер снимается
	for (const TPair<TWeakObjectPtr<AMonsterUnit>, int32>& Shown : ShownOrders)
	{
		AMonsterUnit* Monster = Shown.Key.Get();
		if (Monster != nullptr && !Desired.Contains(Shown.Key) && Monster->HealthBar != nullptr)
		{
			Monster->HealthBar->SetOrderNumber(0);
		}
	}
	for (const TPair<TWeakObjectPtr<AMonsterUnit>, int32>& Pair : Desired)
	{
		if (AMonsterUnit* Monster = Pair.Key.Get(); Monster != nullptr && Monster->HealthBar != nullptr)
		{
			Monster->HealthBar->SetOrderNumber(Pair.Value);
		}
	}
	ShownOrders = MoveTemp(Desired);
}

void UBattleOverlayComponent::RebuildFocus()
{
	// Контур: монстры, чьи области содержат клетку под курсором
	TSet<TWeakObjectPtr<AMonsterUnit>> Outlined;
	if (FocusTile.IsSet())
	{
		for (const FIntentView& View : Views)
		{
			if (AMonsterUnit* Monster = View.Monster.Get(); Monster != nullptr && View.Cast.Area.Contains(FocusTile.GetValue(), EAreaPart::All))
			{
				Outlined.Add(Monster);
			}
		}
	}
	for (const TWeakObjectPtr<AMonsterUnit>& Old : OutlinedMonsters)
	{
		if (AMonsterUnit* Monster = Old.Get(); Monster != nullptr && !Outlined.Contains(Old))
		{
			Monster->SetFocused(false);
		}
	}
	for (const TWeakObjectPtr<AMonsterUnit>& New : Outlined)
	{
		if (AMonsterUnit* Monster = New.Get(); Monster != nullptr && !OutlinedMonsters.Contains(New))
		{
			Monster->SetFocused(true);
		}
	}
	OutlinedMonsters = MoveTemp(Outlined);

	// Предпросмотр урона от намерения монстра в фокусе
	FocusPreviews.Reset();
	if (const FIntentView* Focused = FindView(FocusMonster.Get()))
	{
		CollectPreviews(Focused->Preview, FocusPreviews);
	}
	RefreshHealthPreviews();
}

const FIntentView* UBattleOverlayComponent::FindView(AMonsterUnit* Monster) const
{
	if (Monster == nullptr)
	{
		return nullptr;
	}
	return Views.FindByPredicate([Monster](const FIntentView& View)
	{
		return View.Monster.Get() == Monster;
	});
}

void UBattleOverlayComponent::CollectPreviews(const FAbilityOutcome& Outcome, FPreviewMap& Out) const
{
	const UBattleSubsystem* Battle = GetBattle();
	if (Battle == nullptr)
	{
		return;
	}

	// Предпросмотр для всех юнитов области, включая союзников (GDD)
	TArray<int32> UnitIds;
	for (const FAbilityEvent& Event : Outcome.Events)
	{
		if (Event.Type == EAbilityEventType::Damage || Event.Type == EAbilityEventType::Death)
		{
			UnitIds.AddUnique(Event.UnitId);
		}
	}
	for (const int32 UnitId : UnitIds)
	{
		ADungeonUnit* Unit = Battle->FindUnitById(UnitId);
		if (Unit == nullptr || !Unit->IsAlive())
		{
			continue;
		}
		FUnitPreview Preview;
		Preview.LostPips = Outcome.GetDamageTo(UnitId);
		Preview.bWillDie = Outcome.IsKilled(UnitId);
		if (Preview.LostPips > 0 || Preview.bWillDie)
		{
			Out.Add(Unit, Preview);
		}
	}
}

void UBattleOverlayComponent::RefreshHealthPreviews()
{
	// Предпросмотр способности героя важнее фокуса на монстре
	FPreviewMap Desired = FocusPreviews;
	for (const TPair<TWeakObjectPtr<ADungeonUnit>, FUnitPreview>& Pair : AbilityPreviews)
	{
		Desired.Add(Pair.Key, Pair.Value);
	}

	for (auto It = AppliedPreviews.CreateIterator(); It; ++It)
	{
		if (!Desired.Contains(It.Key()))
		{
			if (ADungeonUnit* Unit = It.Key().Get(); Unit != nullptr && Unit->HealthBar != nullptr)
			{
				Unit->HealthBar->ClearPreview();
			}
			It.RemoveCurrent();
		}
	}

	// Не повторяем вызов для тех же значений: мигание делений не сбрасывается при каждом движении курсора
	for (const TPair<TWeakObjectPtr<ADungeonUnit>, FUnitPreview>& Pair : Desired)
	{
		ADungeonUnit* Unit = Pair.Key.Get();
		const FUnitPreview* Applied = AppliedPreviews.Find(Pair.Key);
		if (Unit == nullptr || (Applied != nullptr && *Applied == Pair.Value))
		{
			continue;
		}
		if (Unit->HealthBar != nullptr)
		{
			Unit->HealthBar->SetPreview(Pair.Value.LostPips, Pair.Value.bWillDie);
		}
		AppliedPreviews.Add(Pair.Key, Pair.Value);
	}
}

void UBattleOverlayComponent::ResetAbilityPreviewState()
{
	if (PreviewArrows != nullptr && PreviewArrows->GetInstanceCount() > 0)
	{
		PreviewArrows->ClearInstances();
	}
	PreviewLabels.Reset();
	AbilityPreviews.Reset();
	HideSilhouette();
}

void UBattleOverlayComponent::ShowSilhouette(const FAbilityEvent& Event)
{
	HideSilhouette();

	const UBattleSubsystem* Battle = GetBattle();
	const ADungeonGridActor* Grid = GetGrid();
	if (Silhouette == nullptr || Battle == nullptr || Grid == nullptr || GhostMaterial == nullptr)
	{
		return;
	}
	const ADungeonUnit* Source = Battle->FindUnitById(Event.UnitId);
	USkeletalMesh* SourceMesh = Source != nullptr && Source->Mesh != nullptr ? Source->Mesh->GetSkeletalMeshAsset() : nullptr;
	if (SourceMesh == nullptr)
	{
		return;
	}

	// Меш источника в его позе и масштабе, сдвинутый с клетки From на клетку To
	Silhouette->SetSkeletalMeshAsset(SourceMesh);
	for (int32 Slot = 0; Slot < Silhouette->GetNumMaterials(); ++Slot)
	{
		Silhouette->SetMaterial(Slot, GhostMaterial);
	}
	FTransform Transform = Source->Mesh->GetComponentTransform();
	Transform.AddToTranslation(Grid->GridToWorld(Event.To) - Grid->GridToWorld(Event.From));
	Silhouette->SetWorldTransform(Transform);
	Silhouette->SetVisibility(true);
}

void UBattleOverlayComponent::HideSilhouette()
{
	if (Silhouette != nullptr)
	{
		Silhouette->SetVisibility(false);
	}
}

void UBattleOverlayComponent::RefreshLabels()
{
	Labels = IntentLabels;
	Labels.Append(PreviewLabels);

	// Без класса виджета подписи пропускаются
	if (TileLabelClass == nullptr)
	{
		for (UWidgetComponent* Component : LabelPool)
		{
			if (Component != nullptr)
			{
				Component->SetVisibility(false);
			}
		}
		return;
	}

	// Компоненты пула переиспользуются, лишние скрываются
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		UWidgetComponent* Component = AcquireLabelComponent(Index);
		if (Component == nullptr)
		{
			continue;
		}
		const FOverlayTileLabel& Label = Labels[Index];
		Component->SetWorldLocation(Label.Location);
		Component->SetVisibility(true);

		// Виджет создаётся при BeginPlay компонента, но компонент мог зарегистрироваться раньше
		Component->InitWidget();
		if (UTileLabelWidget* Widget = Cast<UTileLabelWidget>(Component->GetUserWidgetObject()))
		{
			if (Label.bCollision)
			{
				Widget->ShowCollision(Label.Value);
			}
			else
			{
				Widget->ShowDamage(Label.Value);
			}
		}
	}
	for (int32 Index = Labels.Num(); Index < LabelPool.Num(); ++Index)
	{
		if (LabelPool[Index] != nullptr)
		{
			LabelPool[Index]->SetVisibility(false);
		}
	}
}

UWidgetComponent* UBattleOverlayComponent::AcquireLabelComponent(int32 Index)
{
	if (LabelPool.IsValidIndex(Index) && LabelPool[Index] != nullptr)
	{
		UWidgetComponent* Existing = LabelPool[Index];
		if (Existing->GetWidgetClass() != TileLabelClass)
		{
			Existing->SetWidgetClass(TileLabelClass);
		}
		return Existing;
	}
	if (Index != LabelPool.Num() || GetOwner() == nullptr || !IsRegistered())
	{
		return nullptr;
	}

	UWidgetComponent* Component = NewObject<UWidgetComponent>(GetOwner(), NAME_None, RF_Transient);
	Component->SetWidgetSpace(EWidgetSpace::Screen);
	Component->SetDrawAtDesiredSize(true);
	Component->SetPivot(FVector2D(0.5, 0.5));
	Component->SetWidgetClass(TileLabelClass);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetupAttachment(this);
	Component->RegisterComponent();
	LabelPool.Add(Component);
	return Component;
}
