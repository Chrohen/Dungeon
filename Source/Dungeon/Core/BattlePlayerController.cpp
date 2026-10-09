#include "Core/BattlePlayerController.h"
#include "Battle/BattleSubsystem.h"
#include "Core/DungeonCollision.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Grid/DungeonGridActor.h"
#include "Input/BattleSelectionComponent.h"
#include "UI/BattleHUDWidget.h"
#include "Units/DungeonUnit.h"

namespace
{
	// Длина луча из курсора
	constexpr float CursorTraceDistance = 100000.f;
}

ABattlePlayerController::ABattlePlayerController()
{
	Selection = CreateDefaultSubobject<UBattleSelectionComponent>(TEXT("Selection"));
}

void ABattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	if (InputContext != nullptr)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(InputContext, 0);
		}
	}
	else
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("ABattlePlayerController::BeginPlay: InputContext is not set."));
	}

	if (HUDWidgetClass != nullptr)
	{
		HUD = CreateWidget<UBattleHUDWidget>(this, HUDWidgetClass);
		if (HUD != nullptr)
		{
			HUD->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("ABattlePlayerController::BeginPlay: HUDWidgetClass is not set."));
	}
}

void ABattlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ABattlePlayerController::SetupInputComponent: the input component is not UEnhancedInputComponent."));
		return;
	}

	// Триггер Pressed задаётся в ассетах IA_*
	if (SelectAction != nullptr)
	{
		EnhancedInput->BindAction(SelectAction, ETriggerEvent::Triggered, this, &ABattlePlayerController::OnSelectTriggered);
	}
	if (ConfirmAction != nullptr)
	{
		EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Triggered, this, &ABattlePlayerController::OnConfirmTriggered);
	}
	if (CancelAction != nullptr)
	{
		EnhancedInput->BindAction(CancelAction, ETriggerEvent::Triggered, this, &ABattlePlayerController::OnCancelTriggered);
	}
}

void ABattlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController() || Selection == nullptr)
	{
		return;
	}

	const FCursorTarget Target = ResolveCursorTarget();
	const bool bTileChanged = Target.bOnGrid != LastHoverTarget.bOnGrid
		|| (Target.bOnGrid && Target.Tile != LastHoverTarget.Tile);
	if (bTileChanged)
	{
		LastHoverTarget = Target;
		Selection->HandleHover(Target);
	}
}

FCursorTarget ABattlePlayerController::ResolveCursorTarget() const
{
	FCursorTarget Result;

	const UWorld* World = GetWorld();
	const UBattleSubsystem* Battle = World != nullptr ? World->GetSubsystem<UBattleSubsystem>() : nullptr;
	const ADungeonGridActor* Grid = Battle != nullptr ? Battle->GetGrid() : nullptr;
	if (Grid == nullptr)
	{
		return Result;
	}

	FVector Origin;
	FVector Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction))
	{
		return Result;
	}
	const FVector End = Origin + Direction * CursorTraceDistance;

	FHitResult Hit;

	// Клетка берётся только с плоскости поля, модели юнитов её не закрывают
	if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Grid))
	{
		FIntPoint Tile;
		if (Grid->WorldToGrid(Hit.ImpactPoint, Tile))
		{
			Result.bOnGrid = true;
			Result.Tile = Tile;
		}
	}

	// Сначала юнит по модели, затем юнит на клетке под курсором
	if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Unit))
	{
		Result.Unit = Cast<ADungeonUnit>(Hit.GetActor());
	}
	else if (Result.bOnGrid)
	{
		if (const FGridOccupant* Occupant = Grid->GetModel().GetOccupant(Result.Tile))
		{
			Result.Unit = Occupant->Unit;
		}
	}

	return Result;
}

void ABattlePlayerController::OnSelectTriggered(const FInputActionValue& Value)
{
	if (Selection != nullptr)
	{
		Selection->HandleSelect(ResolveCursorTarget());
	}
}

void ABattlePlayerController::OnConfirmTriggered(const FInputActionValue& Value)
{
	if (Selection != nullptr)
	{
		Selection->HandleConfirm(ResolveCursorTarget());
	}
}

void ABattlePlayerController::OnCancelTriggered(const FInputActionValue& Value)
{
	if (Selection != nullptr)
	{
		Selection->HandleCancel();
	}
}
