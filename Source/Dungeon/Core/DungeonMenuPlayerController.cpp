#include "Core/DungeonMenuPlayerController.h"

#include "Battle/BattleSubsystem.h"
#include "Core/DungeonBattleGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Save/DungeonSaveGame.h"
#include "Save/DungeonSaveStorage.h"
#include "UI/MainMenuWidget.h"
#include "UObject/StrongObjectPtr.h"

ADungeonMenuPlayerController::ADungeonMenuPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/L_Battle.L_Battle")));
	MenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/L_MainMenu.L_MainMenu")));
	BattleGameModeClass = ADungeonBattleGameMode::StaticClass();
}

void ADungeonMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().AddUObject(this, &ADungeonMenuPlayerController::HandleTravelFailure);
	}
	if (const AGameModeBase* Mode = GetWorld()->GetAuthGameMode())
	{
		if (UGameplayStatics::HasOption(Mode->OptionsString, TEXT("DungeonLoadFailed")))
		{
			LastMenuError = NSLOCTEXT("DungeonMenu", "LoadFailed", "Не удалось загрузить сохранение. Файлы сохранения не изменены.");
		}
	}
	bShowMouseCursor = true;
	if (MenuWidgetClass == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("DungeonMenuPlayerController: MenuWidgetClass is not set."));
		return;
	}
	MenuWidget = CreateWidget<UMainMenuWidget>(this, MenuWidgetClass);
	if (MenuWidget == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("DungeonMenuPlayerController: the menu widget could not be created."));
		return;
	}
	MenuWidget->AddToViewport();
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	RefreshMenu();
	MenuWidget->FocusDefaultButton();
}

void ADungeonMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	if (MenuWidget != nullptr)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

FString ADungeonMenuPlayerController::GetSaveSlot() const
{
	const ADungeonBattleGameMode* BattleDefaults = BattleGameModeClass != nullptr
		? BattleGameModeClass->GetDefaultObject<ADungeonBattleGameMode>() : nullptr;
	return BattleDefaults != nullptr
		? ADungeonBattleGameMode::ResolveSaveSlot(GetWorld(), BattleDefaults->SaveSlotName) : FString();
}

bool ADungeonMenuPlayerController::HasExistingSave() const
{
	const FString Slot = GetSaveSlot();
	return !Slot.IsEmpty() && FDungeonSaveStorage::Exists(Slot);
}

bool ADungeonMenuPlayerController::CanContinueGame()
{
	if (bContinueValidationError)
	{
		LastMenuError = FText::GetEmpty();
		bContinueValidationError = false;
	}
	if (bTravelPending || !HasExistingSave())
	{
		return false;
	}
	if (!ValidateTravelMaps())
	{
		bContinueValidationError = true;
		return false;
	}
	FString Error;
	TStrongObjectPtr<UDungeonSaveGame> Save(FDungeonSaveStorage::Load(GetSaveSlot(), Error));
	if (!Save.IsValid())
	{
		LastMenuError = NSLOCTEXT("DungeonMenu", "InvalidSave", "Сохранение недоступно или повреждено. Можно начать новую игру.");
		bContinueValidationError = true;
		UE_LOG(LogDungeonBattle, Warning, TEXT("Dungeon menu: %s"), *Error);
		return false;
	}
	if (Save->MapPackage != FPackageName::GetShortName(BattleMap.ToSoftObjectPath().GetLongPackageName()))
	{
		LastMenuError = NSLOCTEXT("DungeonMenu", "DifferentMap", "Сохранение относится к другой карте. Продолжение недоступно.");
		bContinueValidationError = true;
		return false;
	}
	return true;
}

bool ADungeonMenuPlayerController::ValidateTravelMaps()
{
	const FString BattlePackage = BattleMap.ToSoftObjectPath().GetLongPackageName();
	const FString MenuPackage = MenuMap.ToSoftObjectPath().GetLongPackageName();
	if (BattleGameModeClass == nullptr || BattlePackage.IsEmpty() || MenuPackage.IsEmpty()
		|| !FPackageName::DoesPackageExist(BattlePackage) || !FPackageName::DoesPackageExist(MenuPackage))
	{
		LastMenuError = NSLOCTEXT("DungeonMenu", "MissingMap", "Не найдена карта игры или меню. Проверьте установку игры.");
		return false;
	}
	const ADungeonBattleGameMode* BattleDefaults = BattleGameModeClass->GetDefaultObject<ADungeonBattleGameMode>();
	if (BattleDefaults->BattlefieldConfig == nullptr || BattleDefaults->GridActorClass == nullptr || GetSaveSlot().IsEmpty())
	{
		LastMenuError = NSLOCTEXT("DungeonMenu", "MissingBattleConfig", "Параметры боя не настроены. Не удалось начать игру.");
		return false;
	}
	return true;
}

void ADungeonMenuPlayerController::StartNewGame()
{
	if (bTravelPending)
	{
		return;
	}
	LastMenuError = FText::GetEmpty();
	bContinueValidationError = false;
	if (!ValidateTravelMaps())
	{
		RefreshMenu();
		return;
	}
	TravelToBattle(false);
}

void ADungeonMenuPlayerController::ContinueGame()
{
	if (bTravelPending)
	{
		return;
	}
	LastMenuError = FText::GetEmpty();
	bContinueValidationError = false;
	if (!CanContinueGame())
	{
		if (LastMenuError.IsEmpty())
		{
			LastMenuError = NSLOCTEXT("DungeonMenu", "NoSave", "Нет доступного сохранения для продолжения.");
		}
		RefreshMenu();
		return;
	}
	TravelToBattle(true);
}

void ADungeonMenuPlayerController::TravelToBattle(bool bContinue)
{
	bTravelPending = true;
	const FString Options = FString(bContinue ? TEXT("DungeonContinue") : TEXT("DungeonNewGame"))
		+ TEXT("?DungeonMenu=") + MenuMap.ToSoftObjectPath().GetLongPackageName()
		+ TEXT("?game=") + BattleGameModeClass->GetPathName();
	UGameplayStatics::OpenLevel(this, FName(*BattleMap.ToSoftObjectPath().GetLongPackageName()), true, Options);
}

void ADungeonMenuPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ADungeonMenuPlayerController::RefreshMenu()
{
	if (MenuWidget != nullptr)
	{
		MenuWidget->RefreshMenu();
	}
}

void ADungeonMenuPlayerController::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (World != nullptr && World != GetWorld())
	{
		return;
	}
	bTravelPending = false;
	bContinueValidationError = false;
	LastMenuError = NSLOCTEXT("DungeonMenu", "TravelFailed", "Не удалось открыть карту игры. Сохранение не изменено.");
	UE_LOG(LogDungeonBattle, Warning, TEXT("Dungeon menu travel failed: %s"), *Error);
	RefreshMenu();
}
