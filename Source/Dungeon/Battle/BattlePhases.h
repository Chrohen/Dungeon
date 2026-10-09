#pragma once

#include "CoreMinimal.h"
#include "Battle/BattlePhase.h"
#include "BattlePhases.generated.h"

// Спавнит героев по Config->HeroPlacements и сразу вызывает AdvancePhase().
// //CURRENT-STAGE: расстановки игроком пока нет
UCLASS()
class DUNGEON_API UPreparationPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::Preparation; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersMove; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// TurnNumber++, затем сразу AdvancePhase().
// //CURRENT-STAGE: перемещения монстров нет
UCLASS()
class DUNGEON_API UMonstersMovePhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::MonstersMove; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersIntent; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// Сразу AdvancePhase(). //CURRENT-STAGE: намерений монстров нет
UCLASS()
class DUNGEON_API UMonstersIntentPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::MonstersIntent; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::PlayerTurn; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// ResetTurnState() у всех героев и OnTurnStarted. Ждёт EndPlayerTurn().
// //CURRENT-STAGE: проверки победы нет
UCLASS()
class DUNGEON_API UPlayerTurnPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::PlayerTurn; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::ResolveIntents; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// Сразу AdvancePhase(). //CURRENT-STAGE: атак монстров и проверки поражения нет
UCLASS()
class DUNGEON_API UResolveIntentsPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::ResolveIntents; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::WaveSpawn; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};

// Сразу AdvancePhase(). //CURRENT-STAGE: волн нет
UCLASS()
class DUNGEON_API UWaveSpawnPhase : public UBattlePhase
{
	GENERATED_BODY()

public:
	virtual EBattlePhase GetPhaseType() const override { return EBattlePhase::WaveSpawn; }
	virtual EBattlePhase GetNextPhase() const override { return EBattlePhase::MonstersMove; }
	virtual void Enter(UBattleSubsystem& Battle) override;
};
