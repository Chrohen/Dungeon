#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/AbilityTypes.h"
#include "AbilityDefinition.generated.h"

class UTexture2D;

// Описание способности. Собирается из модулей: наведение, область, эффекты, перезарядка
UCLASS(BlueprintType)
class DUNGEON_API UAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FAbilityTargeting Targeting;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FAbilityAreaSpec Area;

	// Эффекты применяются по порядку
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityEffect> Effects;

	// Перезарядка в ходах
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Cooldown = 0;

	// Изменения уровней 2, 3 и т.д.: Levels[0] - переход на уровень 2
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityLevel> Levels;

	int32 GetMaxLevel() const { return Levels.Num() + 1; }

	// Есть ли у способности параметр с таким адресом. Для параметров эффекта
	// (EffectAmount, PushDistance, StatusDuration) EffectIndex должен быть в пределах Effects
	bool HasParam(const FAbilityParamKey& Key) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
