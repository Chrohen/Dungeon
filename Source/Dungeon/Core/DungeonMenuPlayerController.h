#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerController.h"
#include "DungeonMenuPlayerController.generated.h"

class ADungeonBattleGameMode;
class UMainMenuWidget;

UCLASS(Blueprintable)
class DUNGEON_API ADungeonMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADungeonMenuPlayerController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UMainMenuWidget> MenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSoftObjectPtr<UWorld> BattleMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSoftObjectPtr<UWorld> MenuMap;

	// Use the same class that configures the battle map.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<ADungeonBattleGameMode> BattleGameModeClass;

	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText LastMenuError;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	bool CanContinueGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	bool HasExistingSave() const;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartNewGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ContinueGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FString GetSaveSlot() const;
	bool ValidateTravelMaps();
	void TravelToBattle(bool bContinue);
	void RefreshMenu();
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MenuWidget;

	bool bTravelPending = false;
	bool bContinueValidationError = false;
};
