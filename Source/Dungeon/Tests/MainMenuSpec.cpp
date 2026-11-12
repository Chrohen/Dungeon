#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/BattleSubsystem.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/DungeonBattleGameMode.h"
#include "Core/DungeonMenuPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Grid/DungeonGridActor.h"
#include "Kismet/GameplayStatics.h"
#include "Save/DungeonSaveGame.h"
#include "Save/DungeonSaveStorage.h"
#include "Tests/BattleTestHelpers.h"
#include "UI/MainMenuWidget.h"
#include "UI/MenuSettingsWidget.h"
#include "UObject/StrongObjectPtr.h"

BEGIN_DEFINE_SPEC(FMainMenuSpec, "Dungeon.UI.MainMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FBattleTestWorld TestWorld;
	ADungeonMenuPlayerController* Controller = nullptr;
	TStrongObjectPtr<ULocalPlayer> MenuPlayer;
	TStrongObjectPtr<UMainMenuWidget> Menu;
	TUniquePtr<TGuardValue<FString>> SlotOverride;
	TUniquePtr<TGuardValue<TObjectPtr<UBattlefieldConfig>>> ConfigOverride;
	TUniquePtr<TGuardValue<TSubclassOf<ADungeonGridActor>>> GridClassOverride;
	FString Slot;

	UButton* Button(const TCHAR* Name) const
	{
		return Menu.IsValid() ? Cast<UButton>(Menu->GetWidgetFromName(FName(Name))) : nullptr;
	}

	UWidget* Panel(const TCHAR* Name) const
	{
		return Menu.IsValid() ? Menu->GetWidgetFromName(FName(Name)) : nullptr;
	}

	FString TravelURL() const
	{
		return GEngine->GetWorldContextFromWorldChecked(TestWorld.GetWorld()).TravelURL;
	}

	bool CheckChild(UWidgetTree* Tree, const TCHAR* Name, UClass* Type)
	{
		UWidget* Child = Tree != nullptr ? Tree->FindWidget(FName(Name)) : nullptr;
		return TestTrue(FString::Printf(TEXT("required widget %s has the correct type"), Name), Child != nullptr && Child->IsA(Type));
	}

	bool CreateMenu()
	{
		UClass* MainClass = LoadClass<UMainMenuWidget>(nullptr, TEXT("/Game/UI/Menu/WBP_MainMenu.WBP_MainMenu_C"));
		UClass* SettingsClass = LoadClass<UMenuSettingsWidget>(nullptr, TEXT("/Game/UI/Menu/WBP_MenuSettings.WBP_MenuSettings_C"));
		if (!TestNotNull(TEXT("real main menu Blueprint"), MainClass)
			|| !TestNotNull(TEXT("real settings Blueprint"), SettingsClass)) return false;
		UWidgetBlueprintGeneratedClass* MainBlueprint = Cast<UWidgetBlueprintGeneratedClass>(MainClass);
		UWidgetBlueprintGeneratedClass* SettingsBlueprint = Cast<UWidgetBlueprintGeneratedClass>(SettingsClass);
		if (!TestNotNull(TEXT("main menu is a Widget Blueprint"), MainBlueprint)
			|| !TestNotNull(TEXT("settings is a Widget Blueprint"), SettingsBlueprint)) return false;
		UWidgetTree* MainTree = MainBlueprint->GetWidgetTreeArchetype();
		UWidgetTree* SettingsTree = SettingsBlueprint->GetWidgetTreeArchetype();
		if (!TestNotNull(TEXT("main menu has a layout"), MainTree)
			|| !TestNotNull(TEXT("settings has a layout"), SettingsTree)) return false;
		if (!TestNotNull(TEXT("main menu has a root widget"), MainTree->RootWidget.Get())
			|| !TestNotNull(TEXT("settings has a root widget"), SettingsTree->RootWidget.Get())) return false;

		// Check the bindings before native initialization uses the child widgets.
		bool bReady = CheckChild(MainTree, TEXT("MainPanel"), UWidget::StaticClass());
		bReady &= CheckChild(MainTree, TEXT("NewGameConfirmPanel"), UWidget::StaticClass());
		bReady &= CheckChild(MainTree, TEXT("StatusText"), UTextBlock::StaticClass());
		bReady &= CheckChild(MainTree, TEXT("SettingsPanel"), SettingsClass);
		for (const TCHAR* Name : { TEXT("NewGameButton"), TEXT("ContinueButton"), TEXT("SettingsButton"),
			TEXT("QuitButton"), TEXT("ConfirmNewGameButton"), TEXT("CancelNewGameButton") })
		{
			bReady &= CheckChild(MainTree, Name, UButton::StaticClass());
		}
		bReady &= CheckChild(SettingsTree, TEXT("BackButton"), UButton::StaticClass());
		if (!bReady) return false;

		Menu.Reset(CreateWidget<UMainMenuWidget>(Controller, MainClass));
		if (!TestNotNull(TEXT("real menu instance"), Menu.Get())) return false;
		Menu->ShowMainMenu();
		return true;
	}

	bool WriteCheckpoint(const FString& MapName = TEXT("L_Battle"))
	{
		UBattleSubsystem* Battle = TestWorld.GetBattle();
		Battle->StartBattle(TestWorld.GetConfig(), TestWorld.GetGrid());
		TStrongObjectPtr<UDungeonSaveGame> Save(NewObject<UDungeonSaveGame>());
		FString Error;
		if (!TestTrue(TEXT("capture test battle"), Battle->CaptureSave(*Save, Error))) return false;
		Save->MapPackage = MapName;
		const bool bWritten = FDungeonSaveStorage::Save(Slot, *Save, Error);
		return TestTrue(FString::Printf(TEXT("write isolated checkpoint: %s"), *Error), bWritten);
	}

	bool ReadFirstSlot(TArray<uint8>& Bytes)
	{
		return TestTrue(TEXT("read isolated slot"), UGameplayStatics::LoadDataFromSlot(Bytes, Slot + TEXT("_A"), 0));
	}

	void CheckTravel(const TCHAR* ExpectedOption, const TCHAR* RejectedOption)
	{
		const FString URL = TravelURL();
		const int32 OptionsStart = URL.Find(TEXT("?"));
		const FString Options = OptionsStart != INDEX_NONE ? URL.Mid(OptionsStart) : FString();
		TestTrue(TEXT("opens the battle map"), URL.StartsWith(TEXT("/Game/Maps/L_Battle?")));
		TestTrue(TEXT("sets the requested launch mode"), UGameplayStatics::HasOption(Options, ExpectedOption));
		TestFalse(TEXT("does not request the other launch mode"), UGameplayStatics::HasOption(Options, RejectedOption));
		TestEqual(TEXT("keeps a return route to the menu"), UGameplayStatics::ParseOption(Options, TEXT("DungeonMenu")), FString(TEXT("/Game/Maps/L_MainMenu")));
		TestEqual(TEXT("uses the configured battle game mode"), UGameplayStatics::ParseOption(Options, TEXT("game")), ADungeonBattleGameMode::StaticClass()->GetPathName());
	}
END_DEFINE_SPEC(FMainMenuSpec)

void FMainMenuSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld.Create();
		Slot = TEXT("DungeonMenuSpec_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ADungeonBattleGameMode* Defaults = GetMutableDefault<ADungeonBattleGameMode>();
		SlotOverride = MakeUnique<TGuardValue<FString>>(Defaults->SaveSlotName, Slot);
		ConfigOverride = MakeUnique<TGuardValue<TObjectPtr<UBattlefieldConfig>>>(Defaults->BattlefieldConfig, TestWorld.GetConfig());
		GridClassOverride = MakeUnique<TGuardValue<TSubclassOf<ADungeonGridActor>>>(Defaults->GridActorClass, ADungeonGridActor::StaticClass());

		// Use real objects without BeginPlay, a viewport, or a map transition.
		Controller = TestWorld.GetWorld()->SpawnActorDeferred<ADungeonMenuPlayerController>(
			ADungeonMenuPlayerController::StaticClass(), FTransform::Identity);
		if (!TestNotNull(TEXT("menu controller"), Controller)) return;
		MenuPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
		Controller->Player = MenuPlayer.Get();
		MenuPlayer->PlayerController = Controller;
		Controller->SetAsLocalPlayerController();
		TestWorld.GetWorld()->AddController(Controller);
	});

	AfterEach([this]()
	{
		// OpenLevel only queues travel. Clear this test world's request before teardown.
		GEngine->GetWorldContextFromWorldChecked(TestWorld.GetWorld()).TravelURL.Reset();
		Menu.Reset();
		if (Controller != nullptr)
		{
			Controller->Player = nullptr;
			TestWorld.GetWorld()->RemoveController(Controller);
			Controller->Destroy();
		}
		if (MenuPlayer.IsValid()) MenuPlayer->PlayerController = nullptr;
		MenuPlayer.Reset();
		Controller = nullptr;
		GridClassOverride.Reset();
		ConfigOverride.Reset();
		SlotOverride.Reset();
		UGameplayStatics::DeleteGameInSlot(Slot + TEXT("_A"), 0);
		UGameplayStatics::DeleteGameInSlot(Slot + TEXT("_B"), 0);
		TestWorld.Destroy();
	});

	It("opens the real settings panel and returns through its Back button", [this]()
	{
		if (!CreateMenu()) return;
		UMainMenuWidget* MainInstance = Menu.Get();
		UMenuSettingsWidget* Settings = Cast<UMenuSettingsWidget>(Panel(TEXT("SettingsPanel")));
		if (!TestNotNull(TEXT("settings instance has its native parent"), Settings)) return;
		UButton* Back = Cast<UButton>(Settings->GetWidgetFromName(TEXT("BackButton")));
		if (!TestNotNull(TEXT("real settings Back button"), Back)) return;
		TestEqual(TEXT("main panel starts visible"), Panel(TEXT("MainPanel"))->GetVisibility(), ESlateVisibility::Visible);
		TestEqual(TEXT("settings starts collapsed"), Settings->GetVisibility(), ESlateVisibility::Collapsed);
		TestFalse(TEXT("Continue is disabled without a save"), Button(TEXT("ContinueButton"))->GetIsEnabled());
		TestFalse(TEXT("the missing save has a status message"), CastChecked<UTextBlock>(Panel(TEXT("StatusText")))->GetText().IsEmpty());

		Button(TEXT("SettingsButton"))->OnClicked.Broadcast();
		TestEqual(TEXT("settings click hides the main panel"), Panel(TEXT("MainPanel"))->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("settings click shows settings"), Settings->GetVisibility(), ESlateVisibility::Visible);
		Back->OnClicked.Broadcast();
		TestEqual(TEXT("Back restores the main panel"), Panel(TEXT("MainPanel"))->GetVisibility(), ESlateVisibility::Visible);
		TestEqual(TEXT("Back hides settings"), Settings->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("navigation reuses the same menu instance"), Menu.Get() == MainInstance);
		TestTrue(TEXT("settings navigation does not travel"), TravelURL().IsEmpty());
		TestFalse(TEXT("settings navigation does not create a save"), FDungeonSaveStorage::Exists(Slot));
	});

	It("requires confirmation before New Game and requests a fresh battle without deleting the save", [this]()
	{
		if (!WriteCheckpoint() || !CreateMenu()) return;
		TArray<uint8> Before;
		if (!ReadFirstSlot(Before)) return;
		Button(TEXT("NewGameButton"))->OnClicked.Broadcast();
		TestEqual(TEXT("new game asks for confirmation"), Panel(TEXT("NewGameConfirmPanel"))->GetVisibility(), ESlateVisibility::Visible);
		TestTrue(TEXT("confirmation does not travel"), TravelURL().IsEmpty());
		Button(TEXT("CancelNewGameButton"))->OnClicked.Broadcast();
		TestEqual(TEXT("cancel hides confirmation"), Panel(TEXT("NewGameConfirmPanel"))->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("cancel does not travel"), TravelURL().IsEmpty());
		TArray<uint8> AfterCancel;
		if (!ReadFirstSlot(AfterCancel)) return;
		TestTrue(TEXT("cancel leaves the checkpoint unchanged"), Before == AfterCancel);

		Button(TEXT("NewGameButton"))->OnClicked.Broadcast();
		Button(TEXT("ConfirmNewGameButton"))->OnClicked.Broadcast();
		CheckTravel(TEXT("DungeonNewGame"), TEXT("DungeonContinue"));
		TArray<uint8> AfterConfirm;
		if (ReadFirstSlot(AfterConfirm)) TestTrue(TEXT("the old save survives until the new battle starts"), Before == AfterConfirm);
	});

	It("continues a valid checkpoint through the real Continue button", [this]()
	{
		if (!WriteCheckpoint() || !CreateMenu()) return;
		TestTrue(TEXT("Continue is enabled for a valid checkpoint"), Button(TEXT("ContinueButton"))->GetIsEnabled());
		TestTrue(TEXT("valid checkpoint has no error"), CastChecked<UTextBlock>(Panel(TEXT("StatusText")))->GetText().IsEmpty());
		TArray<uint8> Before;
		if (!ReadFirstSlot(Before)) return;
		Button(TEXT("ContinueButton"))->OnClicked.Broadcast();
		CheckTravel(TEXT("DungeonContinue"), TEXT("DungeonNewGame"));
		TArray<uint8> After;
		if (ReadFirstSlot(After)) TestTrue(TEXT("Continue does not replace the checkpoint"), Before == After);
	});

	It("blocks a corrupt checkpoint and enables Continue after a valid replacement", [this]()
	{
		if (!CreateMenu()) return;
		const TArray<uint8> CorruptBytes = { 0x11, 0x22, 0x33 };
		if (!TestTrue(TEXT("write isolated corrupt data"), UGameplayStatics::SaveDataToSlot(CorruptBytes, Slot + TEXT("_A"), 0))) return;
		AddExpectedMessage(TEXT("Dungeon menu:"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
		Menu->RefreshMenu();
		TestFalse(TEXT("corrupt checkpoint disables Continue"), Button(TEXT("ContinueButton"))->GetIsEnabled());
		TestFalse(TEXT("corruption has a visible reason"), CastChecked<UTextBlock>(Panel(TEXT("StatusText")))->GetText().IsEmpty());
		// The controller must also reject the action when input bypasses the disabled button.
		Button(TEXT("ContinueButton"))->OnClicked.Broadcast();
		TestTrue(TEXT("corrupt checkpoint cannot start travel"), TravelURL().IsEmpty());
		TArray<uint8> Unchanged;
		if (!ReadFirstSlot(Unchanged)) return;
		TestTrue(TEXT("failed Continue preserves the failed checkpoint"), Unchanged == CorruptBytes);

		if (!WriteCheckpoint()) return;
		Menu->RefreshMenu();
		TestTrue(TEXT("valid replacement enables Continue"), Button(TEXT("ContinueButton"))->GetIsEnabled());
		TestTrue(TEXT("validation clears the old error"), CastChecked<UTextBlock>(Panel(TEXT("StatusText")))->GetText().IsEmpty());
	});

	It("does not continue a checkpoint for another battle map", [this]()
	{
		if (!WriteCheckpoint(TEXT("DifferentBattleMap")) || !CreateMenu()) return;
		TestFalse(TEXT("another map disables Continue"), Button(TEXT("ContinueButton"))->GetIsEnabled());
		TestFalse(TEXT("another map has a reason"), Controller->LastMenuError.IsEmpty());
		Button(TEXT("ContinueButton"))->OnClicked.Broadcast();
		TestTrue(TEXT("another map cannot start travel"), TravelURL().IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
