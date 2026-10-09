#include "Core/DungeonBattleGameMode.h"
#include "Battle/BattleSubsystem.h"
#include "Core/BattleCameraPawn.h"
#include "Data/BattlefieldConfig.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Grid/DungeonGridActor.h"

ADungeonBattleGameMode::ADungeonBattleGameMode()
{
	DefaultPawnClass = ABattleCameraPawn::StaticClass();
}

void ADungeonBattleGameMode::StartPlay()
{
	Super::StartPlay();

	if (BattlefieldConfig == nullptr || GridActorClass == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ADungeonBattleGameMode::StartPlay: BattlefieldConfig and GridActorClass must be set. The battle is not started."));
		return;
	}

	UWorld* World = GetWorld();
	UBattleSubsystem* Battle = World != nullptr ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
	if (Battle == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ADungeonBattleGameMode::StartPlay: UBattleSubsystem is not available. The battle is not started."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADungeonGridActor* Grid = World->SpawnActor<ADungeonGridActor>(GridActorClass, FTransform::Identity, SpawnParams);
	if (Grid == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ADungeonBattleGameMode::StartPlay: could not spawn the grid actor. The battle is not started."));
		return;
	}
	Grid->BuildFromConfig(BattlefieldConfig);

	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* CameraPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (CameraPawn != nullptr)
	{
		CameraPawn->SetActorLocation(Grid->GetGridCenter());
	}
	else
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("ADungeonBattleGameMode::StartPlay: the player has no pawn, the camera is not moved."));
	}

	Battle->StartBattle(BattlefieldConfig, Grid);
}
