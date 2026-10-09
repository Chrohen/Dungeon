#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BattleCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

// Ортографическая камера боя. В спринте 1 без управления: поворот, длина SpringArm
// и OrthoWidth задаются в Blueprint-наследнике
UCLASS(Blueprintable)
class DUNGEON_API ABattleCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ABattleCameraPawn();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;
};
