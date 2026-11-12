#include "Core/DungeonMenuGameMode.h"
#include "Core/DungeonMenuPlayerController.h"

ADungeonMenuGameMode::ADungeonMenuGameMode()
{
	PlayerControllerClass = ADungeonMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}
