#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/AbilityTypes.h"
#include "UnitDefinition.generated.h"

class ADungeonUnit;
class UTexture2D;

// Описание типа юнита
UCLASS(BlueprintType)
class DUNGEON_API UUnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// Иконка класса для панелей UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Какой Blueprint спавнить (BP_HeroUnit_*, BP_MonsterUnit_*)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ADungeonUnit> UnitClass;

	// Максимальное здоровье
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxHealth = 1;

	// Очки перемещения
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MovePoints = 5;

	// Уровень юнита в начале боя
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Level = 1;

	// Способности: ссылка, переопределённые параметры, уровень способности
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FUnitAbilityEntry> Abilities;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
