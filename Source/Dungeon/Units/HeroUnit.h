#pragma once

#include "CoreMinimal.h"
#include "Units/DungeonUnit.h"
#include "HeroUnit.generated.h"

class UMaterialInterface;

// Герой игрока
UCLASS(Blueprintable)
class DUNGEON_API AHeroUnit : public ADungeonUnit
{
	GENERATED_BODY()

public:
	AHeroUnit();

	// Имя героя
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero")
	FText HeroName;

	// Текущий опыт
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero")
	int32 Experience = 0;

	// Нераспределённые очки прокачки. В спринте 2 всегда 0
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero")
	int32 UnspentPoints = 0;

	// M_Unit_Inactive: полупрозрачный серый overlay героя, выполнившего оба действия.
	// Задаётся в BP_HeroUnit_*
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hero")
	TObjectPtr<UMaterialInterface> InactiveMaterial;

	// Вызывает UPreparationPhase сразу после SpawnUnit
	void SetHeroName(const FText& InName);

	// HeroName, а если оно пустое - название класса из Definition (//CURRENT-STAGE). Вызывает любой
	FText GetDisplayName() const;

	// Прибавляет опыт. Уровень не повышается: пороги уровней в GDD не заданы (TD §12).
	// Вызывает UBattleSubsystem
	void AddExperience(int32 Amount);

	// Базовые деления здоровья и обесцвечивание: герой с bHasMoved и bHasActed получает
	// overlay InactiveMaterial, иначе overlay снимается
	virtual void RefreshVisuals() override;
};
