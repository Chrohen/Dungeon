#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "AbilityTypes.generated.h"

class UAbilityDefinition;

// Форма области, в которой можно выбрать клетку цели
UENUM(BlueprintType)
enum class ETargetingShape : uint8
{
	Diamond,
	Cross
};

// Траектория: мгновенная или снаряд, который останавливается на первом препятствии
UENUM(BlueprintType)
enum class ETrajectory : uint8
{
	Instant,
	Projectile
};

// Требование к выбранной клетке
UENUM(BlueprintType)
enum class ETargetRequirement : uint8
{
	AnyTile,
	Unit,
	Enemy,
	Ally,
	FreePassable
};

// Тип области действия
UENUM(BlueprintType)
enum class EAreaType : uint8
{
	Tile,
	Diamond,
	Line,
	CrossLine,
	Path,
	Pattern
};

// Какую часть области затрагивает эффект
UENUM(BlueprintType)
enum class EAreaPart : uint8
{
	All,
	Center,
	Edge
};

// На каких юнитов в области действует эффект
UENUM(BlueprintType)
enum class ETargetFilter : uint8
{
	AllUnits,
	Enemies,
	Allies
};

UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
	Damage,
	Heal,
	Push,
	MoveSource,
	UnitStatus,
	TileStatus
};

UENUM(BlueprintType)
enum class EPushDirection : uint8
{
	AbilityDirection,
	AwayFromSource,
	TowardSource,
	AwayFromCenter
};

// Способ перемещения источника: рывок или прыжок
UENUM(BlueprintType)
enum class ESourceMoveMode : uint8
{
	Dash,
	Jump
};

// Параметр способности, который меняют уровни и переопределения
UENUM(BlueprintType)
enum class EAbilityParam : uint8
{
	Cooldown,
	MinRange,
	MaxRange,
	AreaSize,
	EffectAmount,
	PushDistance,
	StatusDuration
};

UENUM(BlueprintType)
enum class EAbilityEventType : uint8
{
	Damage,
	Heal,
	Push,
	Collision,
	SourceMove,
	Death
};

// Наведение: где можно выбрать клетку цели
USTRUCT(BlueprintType)
struct FAbilityTargeting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETargetingShape Shape = ETargetingShape::Diamond;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinRange = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxRange = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETrajectory Trajectory = ETrajectory::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETargetRequirement Requirement = ETargetRequirement::AnyTile;
};

// Область действия. Size - радиус для Diamond, длина для Line, полуширина для CrossLine,
// для остальных типов не используется. Offsets - только для Pattern
USTRUCT(BlueprintType)
struct FAbilityAreaSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAreaType Type = EAreaType::Tile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Size = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntPoint> Offsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAffectsSource = false;
};

// Эффект способности. Поля, не нужные типу эффекта, скрыты в редакторе.
// Status и Duration - задел для состояний
USTRUCT(BlueprintType)
struct FAbilityEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityEffectType Type = EAbilityEffectType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type != EAbilityEffectType::MoveSource", EditConditionHides))
	EAreaPart AreaPart = EAreaPart::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::Damage || Type == EAbilityEffectType::Heal || Type == EAbilityEffectType::Push || Type == EAbilityEffectType::UnitStatus", EditConditionHides))
	ETargetFilter Filter = ETargetFilter::AllUnits;

	// Величина урона или лечения
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::Damage || Type == EAbilityEffectType::Heal", EditConditionHides))
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::Push", EditConditionHides))
	EPushDirection PushDirection = EPushDirection::AbilityDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::Push", EditConditionHides))
	int32 PushDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::MoveSource", EditConditionHides))
	ESourceMoveMode MoveMode = ESourceMoveMode::Dash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::UnitStatus || Type == EAbilityEffectType::TileStatus", EditConditionHides))
	TObjectPtr<UObject> Status = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Type == EAbilityEffectType::UnitStatus || Type == EAbilityEffectType::TileStatus", EditConditionHides))
	int32 Duration = 0;
};

// Адрес параметра. Для параметров эффекта (EffectAmount, PushDistance, StatusDuration)
// EffectIndex - индекс в Effects, для остальных не используется
USTRUCT(BlueprintType)
struct FAbilityParamKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityParam Param = EAbilityParam::Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EffectIndex = INDEX_NONE;
};

// Переопределение (значение заменяет) или изменение уровня (значение прибавляется)
USTRUCT(BlueprintType)
struct FAbilityParamValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityParamKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;
};

// Изменения уровня относительно предыдущего
USTRUCT(BlueprintType)
struct FAbilityLevel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityParamValue> Changes;
};

// Запись в списке способностей определения юнита
USTRUCT(BlueprintType)
struct FUnitAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAbilityDefinition> Ability = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityParamValue> Overrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;
};

// Способность с итоговыми значениями параметров. Все правила работают только с ней.
// Definition нужен UI (название, описание, иконка)
USTRUCT(BlueprintType)
struct FResolvedAbility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAbilityDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityTargeting Targeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityAreaSpec Area;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityEffect> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 0;
};

// Способность юнита в бою: итоговые значения, уровень, оставшаяся перезарядка и ход использования
USTRUCT(BlueprintType)
struct FUnitAbilityState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResolvedAbility Ability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RemainingCooldown = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UsedOnTurn = INDEX_NONE;

	bool IsReady() const
	{
		return RemainingCooldown == 0;
	}
};

// Затронутые тайлы. bHasCenter ложно, если центр вне поля, это тайл источника без bAffectsSource
// или шаблон без смещения (0,0)
USTRUCT(BlueprintType)
struct FAffectedArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Center = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasCenter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntPoint> Edge;

	bool Contains(const FIntPoint& Tile, EAreaPart Part) const
	{
		const bool bInCenter = bHasCenter && Tile == Center;
		switch (Part)
		{
		case EAreaPart::Center:
			return bInCenter;
		case EAreaPart::Edge:
			return Edge.Contains(Tile);
		default:
			return bInCenter || Edge.Contains(Tile);
		}
	}

	// Заменяет содержимое Out тайлами части области. Для All центр идёт первым
	void GetTiles(EAreaPart Part, TArray<FIntPoint>& Out) const
	{
		Out.Reset();
		if (Part != EAreaPart::Edge && bHasCenter)
		{
			Out.Add(Center);
		}
		if (Part != EAreaPart::Center)
		{
			for (const FIntPoint& Tile : Edge)
			{
				Out.AddUnique(Tile);
			}
		}
	}

	TArray<FIntPoint> GetAllTiles() const
	{
		TArray<FIntPoint> Result;
		GetTiles(EAreaPart::All, Result);
		return Result;
	}
};

// Одно применение: выбранная клетка, фактическая точка цели (для снаряда отличается),
// направление и область, построенная один раз
USTRUCT(BlueprintType)
struct FAbilityCast
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SourceId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint SourcePosition = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint SelectedTile = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint TargetPoint = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGridDirection Direction = EGridDirection::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAffectedArea Area;
};

// Одно изменение боя. Значения полей по типам событий - TD 5.3
USTRUCT(BlueprintType)
struct FAbilityEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityEventType Type = EAbilityEventType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UnitId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OtherUnitId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint From = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint To = FIntPoint::ZeroValue;

	// Для Damage: урон столкновения
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EffectIndex = INDEX_NONE;
};

// Полный результат способности в порядке событий
USTRUCT(BlueprintType)
struct FAbilityOutcome
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityCast Cast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityEvent> Events;

	// Суммарный урон юниту, включая столкновения
	int32 GetDamageTo(int32 UnitId) const
	{
		int32 Total = 0;
		for (const FAbilityEvent& Event : Events)
		{
			if (Event.Type == EAbilityEventType::Damage && Event.UnitId == UnitId)
			{
				Total += Event.Amount;
			}
		}
		return Total;
	}

	bool IsKilled(int32 UnitId) const
	{
		return Events.ContainsByPredicate([UnitId](const FAbilityEvent& Event)
		{
			return Event.Type == EAbilityEventType::Death && Event.UnitId == UnitId;
		});
	}

	// Клетка после последнего перемещения юнита. Ложь, если юнит не перемещался
	bool GetFinalPosition(int32 UnitId, FIntPoint& OutPosition) const
	{
		for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
		{
			const FAbilityEvent& Event = Events[Index];
			if (Event.UnitId == UnitId
				&& (Event.Type == EAbilityEventType::Push || Event.Type == EAbilityEventType::SourceMove))
			{
				OutPosition = Event.To;
				return true;
			}
		}
		return false;
	}
};
