#pragma once

#include "CoreMinimal.h"

class UDungeonSaveGame;

// Stores two checkpoints. Each write preserves the last valid checkpoint.
class DUNGEON_API FDungeonSaveStorage
{
public:
	// These synchronous operations must run on the game thread.
	static bool Save(const FString& SlotName, UDungeonSaveGame& Save, FString& Error);
	static UDungeonSaveGame* Load(const FString& SlotName, FString& Error);

	// True when either slot exists, including a slot with invalid data.
	static bool Exists(const FString& SlotName);
};
