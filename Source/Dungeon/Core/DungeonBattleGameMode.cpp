#include "Core/DungeonBattleGameMode.h"
#include "Battle/BattleSubsystem.h"
#include "Core/BattleCameraPawn.h"
#include "Data/BattlefieldConfig.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Grid/DungeonGridActor.h"
#include "Save/DungeonSaveStorage.h"

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
	GridActor = Grid;

	Battle->ConfigureAutoSave(GetSaveSlot(), bAutoSaveEnabled && !GIsAutomationTesting);
	if (!GIsAutomationTesting && bResumeSavedBattle && FDungeonSaveStorage::Exists(GetSaveSlot()))
	{
		if (Battle->LoadBattle(Grid))
		{
			CenterCamera();
			return;
		}
		// Keep a failed save until the player starts a new battle explicitly.
		Battle->ConfigureAutoSave(GetSaveSlot(), false);
		UE_LOG(LogDungeonBattle, Warning, TEXT("The saved battle could not be loaded. Autosave is disabled until RestartBattle."));
	}
	CenterCamera();
	Battle->StartBattle(BattlefieldConfig, Grid);
}

void ADungeonBattleGameMode::CenterCamera() const
{
	APlayerController* PlayerController = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* CameraPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (CameraPawn != nullptr && GridActor != nullptr)
	{
		CameraPawn->SetActorLocation(GridActor->GetGridCenter());
	}
	else
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("ADungeonBattleGameMode::StartPlay: the player has no pawn, the camera is not moved."));
	}

}

FString ADungeonBattleGameMode::GetSaveSlot() const
{
	return SaveSlotName + (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::PIE ? TEXT("_PIE") : TEXT(""));
}

bool ADungeonBattleGameMode::LoadSavedBattle()
{
	UBattleSubsystem* Battle = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBattleSubsystem>() : nullptr;
	if (Battle == nullptr || GridActor == nullptr)
	{
		return false;
	}
	if (!Battle->LoadBattle(GridActor))
	{
		return false;
	}
	Battle->ConfigureAutoSave(GetSaveSlot(), bAutoSaveEnabled && !GIsAutomationTesting);
	CenterCamera();
	return true;
}

void ADungeonBattleGameMode::RestartBattle()
{
	UWorld* World = GetWorld();
	UBattleSubsystem* Battle = World != nullptr ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
	if (Battle == nullptr || BattlefieldConfig == nullptr || GridActor == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ADungeonBattleGameMode::RestartBattle: there is no battle to restart (subsystem, BattlefieldConfig or GridActor is missing)."));
		return;
	}

	// //CURRENT-STAGE: следующий этаж и возвращение в город пока перезапускают тот же бой
	Battle->ConfigureAutoSave(GetSaveSlot(), bAutoSaveEnabled && !GIsAutomationTesting);
	GridActor->BuildFromConfig(BattlefieldConfig);
	Battle->StartBattle(BattlefieldConfig, GridActor);
}
