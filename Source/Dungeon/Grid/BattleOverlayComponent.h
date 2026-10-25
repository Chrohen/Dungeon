#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Abilities/AbilityTypes.h"
#include "Battle/BattleTypes.h"
#include "BattleOverlayComponent.generated.h"

class ADungeonGridActor;
class ADungeonUnit;
class AMonsterUnit;
class UBattleSubsystem;
class UGridHighlightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMesh;
class UTileLabelWidget;
class UWidgetComponent;

// Подпись на клетке: число урона или значок удара с числом. Location - точка в мире
struct FOverlayTileLabel
{
	FVector Location = FVector::ZeroVector;
	int32 Value = 0;
	bool bCollision = false;
};

// Намерения монстров и предпросмотр способности героя на поле (GDD: «Намерение монстров», «Прицеливание героя»).
// Рамки угрозы и зона поражения - слои UGridHighlightComponent, остальное (стрелки, подписи, силуэт) - подкомпоненты,
// которые создаются в OnRegister. Деления здоровья и контур юнитов меняются через их компоненты.
// Намерения показываются только в PlayerTurn: компонент сам подписан на UBattleSubsystem
UCLASS(ClassGroup = (Dungeon), meta = (BlueprintSpawnableComponent))
class DUNGEON_API UBattleOverlayComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBattleOverlayComponent();

	// Слой Threat - все клетки областей; подпись урона - сумма TileDamage всех намерений по клетке; стрелка от монстра
	// к точке цели; короткие стрелки PushMarks; номер Order на делениях монстра, у монстров без намерения номер снимается.
	// Текущий фокус сохраняется и применяется к новому набору
	void ShowIntents(const TArray<FIntentView>& InViews);

	// Убирает рамки, подписи, стрелки, номера и фокус
	void HideIntents();

	// Монстр: рамки его намерения яркие, остальных приглушены, на делениях юнитов из Preview его намерения -
	// предпросмотр урона. Клетка: у монстров, чьи области содержат клетку, красный контур (SetFocused).
	// Пустой вызов снимает фокус, контур и предпросмотр. Если Monster задан, Tile игнорируется
	void SetFocus(AMonsterUnit* Monster, TOptional<FIntPoint> Tile);

	// Слои AreaCenter и AreaEdge, предпросмотр результата по событиям Outcome (TD 6.8), предыдущий предпросмотр заменяется.
	// bProjectile - способность с траекторией Projectile: показывается линия полёта от источника до фактической точки цели
	// (в Outcome траектории нет, её знает вызывающий)
	void ShowAbilityPreview(const FAbilityOutcome& Outcome, bool bProjectile = false);

	// Убирает зону, стрелки предпросмотра, подписи столкновений, силуэт и предпросмотр на делениях
	void ClearAbilityPreview();

	// Подписи, которые сейчас должны быть на поле: сначала намерения, затем предпросмотр. Заполняется и без
	// TileLabelClass, но виджеты при этом не создаются
	const TArray<FOverlayTileLabel>& GetLabels() const { return Labels; }

	// Сколько компонентов подписей создано и сколько из них показано
	int32 GetLabelPoolSize() const { return LabelPool.Num(); }
	int32 GetVisibleLabelCount() const;

	// ISM стрелок намерений и предпросмотра, силуэт. nullptr до регистрации компонента
	const UInstancedStaticMeshComponent* GetIntentArrows() const { return IntentArrows; }
	const UInstancedStaticMeshComponent* GetPreviewArrows() const { return PreviewArrows; }
	const USkeletalMeshComponent* GetSilhouette() const { return Silhouette; }

	// Номер порядка, показанный на делениях монстра, или 0
	int32 GetShownOrder(AMonsterUnit* Monster) const;

	// Предпросмотр, показанный на делениях юнита (фокус или способность). false, если его нет
	bool GetShownPreview(ADungeonUnit* Unit, int32& OutLostPips, bool& bOutWillDie) const;

	// WBP_TileLabel: число урона или значок удара с числом на клетке. Без класса подписи пропускаются
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	TSubclassOf<UTileLabelWidget> TileLabelClass;

	// SM_IntentArrow: плоская стрелка длиной в 1 тайл вдоль +X, точка опоры у хвоста. Без меша стрелки не рисуются
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	TObjectPtr<UStaticMesh> ArrowMesh;

	// M_IntentArrow (красная)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	TObjectPtr<UMaterialInterface> IntentArrowMaterial;

	// M_PreviewArrow (белая)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	TObjectPtr<UMaterialInterface> PreviewArrowMaterial;

	// M_Ghost: полупрозрачный силуэт источника. Без материала силуэт не показывается
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	// Высота подписей над полем
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	float LabelHeight = 20.f;

	// Высота стрелок над полем
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	float ArrowHeight = 12.f;

	// Длина короткой стрелки смещения (PushMarks) в тайлах
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	float PushMarkLength = 0.5f;

	// Яркость рамок Threat у намерений, которые не в фокусе (GDD: остальные приглушаются)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overlay")
	float DimmedBrightness = 0.35f;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Предпросмотр урона на делениях юнита
	struct FUnitPreview
	{
		int32 LostPips = 0;
		bool bWillDie = false;

		bool operator==(const FUnitPreview& Other) const
		{
			return LostPips == Other.LostPips && bWillDie == Other.bWillDie;
		}
	};

	using FPreviewMap = TMap<TWeakObjectPtr<ADungeonUnit>, FUnitPreview>;

	// Подкомпоненты создаются в OnRegister через NewObject: в конструкторе другого компонента они создаются
	// ненадёжно. Метод идемпотентен, его же вызывают показ намерений и предпросмотра
	void EnsureSubcomponents();

	UInstancedStaticMeshComponent* CreateArrows(const TCHAR* Name, int32 SortPriority);

	// Подставляет меш и материал в ISM стрелок. false (с одним предупреждением), если меша нет
	bool PrepareArrows(UInstancedStaticMeshComponent* Arrows, UMaterialInterface* Material);

	ADungeonGridActor* GetGrid() const;
	UGridHighlightComponent* GetHighlight() const;
	UBattleSubsystem* GetBattle() const;

	// Стрелка с хвостом в центре клетки From, вдоль мирового вектора WorldDirection, длиной LengthTiles тайлов
	void AddArrow(UInstancedStaticMeshComponent* Arrows, FIntPoint From, const FVector& WorldDirection, float LengthTiles) const;

	// Стрелка из центра клетки From в центр клетки To. Совпадающие клетки пропускаются
	void AddArrowBetween(UInstancedStaticMeshComponent* Arrows, FIntPoint From, FIntPoint To) const;

	// Мировое направление смещения по направлению на сетке
	FVector DirectionToWorld(EGridDirection Direction) const;

	// Подписка на подсистему: в PlayerTurn показывает намерения, в остальных фазах прячет всё
	void RefreshFromBattle();

	UFUNCTION()
	void HandleBattleStarted();

	UFUNCTION()
	void HandlePhaseChanged(EBattlePhase OldPhase, EBattlePhase NewPhase);

	UFUNCTION()
	void HandleIntentsChanged();

	// Части отображения намерений. Все читают Views, а рамки и контур - ещё и фокус
	void RebuildThreat();
	void RebuildIntentArrows();
	void RebuildIntentLabels();
	void RebuildOrderNumbers();
	void RebuildFocus();

	const FIntentView* FindView(AMonsterUnit* Monster) const;

	// Юниты с уроном или гибелью в Outcome и их предпросмотр
	void CollectPreviews(const FAbilityOutcome& Outcome, FPreviewMap& Out) const;

	// Приводит деления юнитов к сумме предпросмотров: предпросмотр способности важнее фокуса
	void RefreshHealthPreviews();

	// Сбрасывает всё, что показывает предпросмотр способности, кроме слоёв и делений
	void ResetAbilityPreviewState();

	void ShowSilhouette(const FAbilityEvent& Event);
	void HideSilhouette();

	// Собирает Labels и раскладывает их по пулу компонентов
	void RefreshLabels();

	UWidgetComponent* AcquireLabelComponent(int32 Index);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> IntentArrows;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> PreviewArrows;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Silhouette;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> LabelPool;

	TWeakObjectPtr<UBattleSubsystem> BoundBattle;

	// Показанные намерения и фокус
	TArray<FIntentView> Views;
	TWeakObjectPtr<AMonsterUnit> FocusMonster;
	TOptional<FIntPoint> FocusTile;
	TSet<TWeakObjectPtr<AMonsterUnit>> OutlinedMonsters;
	TMap<TWeakObjectPtr<AMonsterUnit>, int32> ShownOrders;

	// Предпросмотр делений от фокуса, от способности и то, что реально выставлено на делениях
	FPreviewMap FocusPreviews;
	FPreviewMap AbilityPreviews;
	FPreviewMap AppliedPreviews;

	TArray<FOverlayTileLabel> IntentLabels;
	TArray<FOverlayTileLabel> PreviewLabels;
	TArray<FOverlayTileLabel> Labels;

	bool bAbilityPreviewShown = false;
	bool bWarnedNoArrowMesh = false;
};
