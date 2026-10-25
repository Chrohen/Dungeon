#include "Battle/BattleSubsystem.h"
#include "Abilities/AbilityRules.h"
#include "Battle/BattleAction.h"
#include "Battle/BattlePhases.h"
#include "Data/BattleRulesConfig.h"
#include "Data/BattlefieldConfig.h"
#include "Data/MonsterDefinition.h"
#include "Data/UnitDefinition.h"
#include "Grid/DungeonGridActor.h"
#include "Units/DungeonUnit.h"
#include "Units/HeroUnit.h"
#include "Units/MonsterUnit.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogDungeonBattle);

namespace
{
	// Шаг на одну клетку от опорной клетки к позиции по оси с большей разностью координат.
	// (0,0), если клетки совпадают или разности равны (TD, раздел 13, п. 10). Нужен только стрелкам намерений:
	// само смещение считает FAbilityRules::Simulate
	FIntPoint IntentStepAwayFrom(FIntPoint Reference, FIntPoint Position)
	{
		const FIntPoint Delta = Position - Reference;
		const int32 AbsX = FMath::Abs(Delta.X);
		const int32 AbsY = FMath::Abs(Delta.Y);
		if (AbsX == AbsY)
		{
			return FIntPoint::ZeroValue;
		}
		return AbsX > AbsY ? FIntPoint(FMath::Sign(Delta.X), 0) : FIntPoint(0, FMath::Sign(Delta.Y));
	}

	int32 SumCooldowns(const ADungeonUnit& Unit)
	{
		int32 Sum = 0;
		for (const FUnitAbilityState& State : Unit.GetAbilities())
		{
			Sum += State.RemainingCooldown;
		}
		return Sum;
	}
}

bool UBattleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBattleSubsystem::StartBattle(UBattlefieldConfig* InConfig, ADungeonGridActor* InGrid)
{
	if (InConfig == nullptr || InGrid == nullptr)
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("StartBattle: Config and Grid must be set."));
		return;
	}

	// Прошлый бой: фаза сбрасывает свой таймер, юниты уничтожаются, их клетки освобождаются
	if (const TObjectPtr<UBattlePhase>* OldPhase = Phases.Find(CurrentPhase))
	{
		if (*OldPhase != nullptr)
		{
			(*OldPhase)->Exit(*this);
		}
	}
	for (ADungeonUnit* Unit : Units)
	{
		if (Unit != nullptr)
		{
			if (Grid != nullptr)
			{
				Grid->GetMutableModel().RemoveUnit(Unit->GridPosition);
			}
			Unit->Destroy();
		}
	}
	Units.Reset();
	Phases.Reset();
	IntentViews.Reset();

	Config = InConfig;
	Grid = InGrid;
	Rules = InConfig->Rules != nullptr ? InConfig->Rules.Get() : GetMutableDefault<UBattleRulesConfig>();
	CurrentPhase = EBattlePhase::None;
	TurnNumber = 0;
	CurrentWave = 0;
	TotalWaves = InConfig->WaveCount;
	Result = EBattleResult::None;
	bIsTransitioning = false;
	bTransitionPending = false;
	bFinishPending = false;
	NextUnitId = 0;
	Random.Initialize(InConfig->RandomSeed != 0 ? InConfig->RandomSeed : FMath::RandRange(1, MAX_int32 - 1));

	const TSubclassOf<UBattlePhase> PhaseClasses[] = {
		UPreparationPhase::StaticClass(),
		UMonstersMovePhase::StaticClass(),
		UMonstersIntentPhase::StaticClass(),
		UPlayerTurnPhase::StaticClass(),
		UResolveIntentsPhase::StaticClass(),
		UWaveSpawnPhase::StaticClass(),
	};
	for (const TSubclassOf<UBattlePhase>& PhaseClass : PhaseClasses)
	{
		UBattlePhase* Phase = NewObject<UBattlePhase>(this, PhaseClass);
		Phases.Add(Phase->GetPhaseType(), Phase);
	}

	OnBattleStarted.Broadcast();
	TransitionTo(EBattlePhase::Preparation);
}

EBattleActionResult UBattleSubsystem::TryExecute(UBattleAction* Action)
{
	if (Action == nullptr)
	{
		UE_LOG(LogDungeonBattle, Warning, TEXT("TryExecute: Action is null."));
		return EBattleActionResult::InvalidUnit;
	}

	const EBattleActionResult CheckResult = Action->CanExecute(*this);
	if (CheckResult == EBattleActionResult::Success)
	{
		Action->Execute(*this);
		CheckBattleEnd();
	}
	return CheckResult;
}

void UBattleSubsystem::EndPlayerTurn()
{
	if (IsPlayerTurn())
	{
		AdvancePhase();
	}
}

void UBattleSubsystem::AdvancePhase()
{
	if (Result != EBattleResult::None)
	{
		return; // бой завершён или завершается
	}
	if (bIsTransitioning)
	{
		bTransitionPending = true; // вызов из Enter(): флаг подхватит цикл в TransitionTo
		return;
	}

	const TObjectPtr<UBattlePhase>* Current = Phases.Find(CurrentPhase);
	if (Current == nullptr || *Current == nullptr)
	{
		return; // у фаз None и Finished нет объекта
	}
	TransitionTo((*Current)->GetNextPhase());
}

void UBattleSubsystem::TransitionTo(EBattlePhase Next)
{
	bIsTransitioning = true;
	SetPhase(Next);
	while (bTransitionPending || bFinishPending)
	{
		if (bFinishPending)
		{
			bFinishPending = false;
			bTransitionPending = false;
			SetPhase(EBattlePhase::Finished);
			OnBattleEnded.Broadcast(Result);
			break;
		}
		bTransitionPending = false;
		const TObjectPtr<UBattlePhase>* Current = Phases.Find(CurrentPhase);
		if (Current == nullptr || *Current == nullptr)
		{
			break;
		}
		SetPhase((*Current)->GetNextPhase());
	}
	bIsTransitioning = false;
}

void UBattleSubsystem::SetPhase(EBattlePhase Next)
{
	const EBattlePhase OldPhase = CurrentPhase;

	if (const TObjectPtr<UBattlePhase>* Old = Phases.Find(OldPhase))
	{
		if (*Old != nullptr)
		{
			(*Old)->Exit(*this);
		}
	}

	CurrentPhase = Next;

	if (const TObjectPtr<UBattlePhase>* New = Phases.Find(Next))
	{
		if (*New != nullptr)
		{
			(*New)->Enter(*this);
		}
	}

	OnPhaseChanged.Broadcast(OldPhase, Next);
}

void UBattleSubsystem::FinishBattle(EBattleResult InResult)
{
	if (Result != EBattleResult::None || CurrentPhase == EBattlePhase::None)
	{
		return;
	}
	Result = InResult;
	ClearAllIntents();
	if (bIsTransitioning)
	{
		bFinishPending = true; // вызов из Enter(): цикл в TransitionTo завершит бой после текущего SetPhase
		return;
	}
	SetPhase(EBattlePhase::Finished);
	OnBattleEnded.Broadcast(Result);
}

bool UBattleSubsystem::CheckBattleEnd()
{
	if (CurrentPhase == EBattlePhase::None || CurrentPhase == EBattlePhase::Finished)
	{
		return IsBattleOver();
	}
	if (IsBattleOver())
	{
		return true; // бой уже завершается
	}

	// Поражение проверяется первым: при одновременной гибели всех героев и всех монстров это поражение
	if (!HasLivingHeroes())
	{
		FinishBattle(EBattleResult::Defeat);
		return true;
	}
	if (GetLivingMonsters().IsEmpty() && CurrentWave >= TotalWaves)
	{
		FinishBattle(EBattleResult::Victory);
		return true;
	}
	return false;
}

ADungeonUnit* UBattleSubsystem::SpawnUnit(UUnitDefinition* Def, FIntPoint Coord)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Grid == nullptr || Def == nullptr || Def->UnitClass == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADungeonUnit* Unit = World->SpawnActor<ADungeonUnit>(Def->UnitClass, FTransform(Grid->GridToWorld(Coord)), SpawnParams);
	if (Unit == nullptr)
	{
		return nullptr;
	}

	Unit->InitializeUnit(Def, Coord, NextUnitId);

	FGridOccupant Occupant;
	Occupant.Unit = Unit;
	Occupant.Team = Unit->Team;
	Occupant.UnitId = Unit->UnitId;
	if (!Grid->GetMutableModel().PlaceUnit(Occupant, Coord))
	{
		Unit->Destroy();
		return nullptr;
	}

	// Номер расходуется только на юнита, который появился
	++NextUnitId;
	Unit->SetGridPosition(Coord, Grid->GridToWorld(Coord));
	Units.Add(Unit);
	OnUnitSpawned.Broadcast(Unit);
	return Unit;
}

void UBattleSubsystem::SpawnWave()
{
	if (Config == nullptr || Grid == nullptr)
	{
		return;
	}

	const FGridArea& Area = Config->MonsterSpawnArea;
	for (UMonsterDefinition* Def : Config->WaveComposition)
	{
		if (Def == nullptr)
		{
			UE_LOG(LogDungeonBattle, Warning, TEXT("WaveComposition: monster definition is not set."));
			continue;
		}

		// Свободные клетки пересчитываются для каждого монстра: предыдущий уже занял свою
		const FGridModel& Model = Grid->GetModel();
		TArray<FIntPoint> FreeTiles;
		for (int32 Y = Area.Top; Y < Area.Top + Area.Height; ++Y)
		{
			for (int32 X = Area.Left; X < Area.Left + Area.Width; ++X)
			{
				const FIntPoint Tile(X, Y);
				if (Model.IsPassable(Tile) && !Model.IsOccupied(Tile))
				{
					FreeTiles.Add(Tile);
				}
			}
		}
		if (FreeTiles.IsEmpty())
		{
			UE_LOG(LogDungeonBattle, Warning, TEXT("SpawnWave: no free passable tile in MonsterSpawnArea for %s, the monster does not appear."), *GetNameSafe(Def));
			continue;
		}

		const FIntPoint Tile = FreeTiles[Random.RandRange(0, FreeTiles.Num() - 1)];
		if (SpawnUnit(Def, Tile) == nullptr)
		{
			UE_LOG(LogDungeonBattle, Warning, TEXT("SpawnWave: could not spawn %s at (%d, %d)."), *GetNameSafe(Def), Tile.X, Tile.Y);
		}
	}
}

bool UBattleSubsystem::MoveUnitInternal(ADungeonUnit* Unit, FIntPoint To)
{
	if (Unit == nullptr || Grid == nullptr)
	{
		return false;
	}

	if (!Grid->GetMutableModel().MoveUnit(Unit->GridPosition, To))
	{
		return false;
	}
	Unit->SetGridPosition(To, Grid->GridToWorld(To));
	return true;
}

FBattleSimState UBattleSubsystem::MakeSimState() const
{
	FBattleSimState State;
	if (Grid != nullptr)
	{
		State.Grid = Grid->GetModel();
	}
	for (const ADungeonUnit* Unit : Units)
	{
		if (Unit == nullptr || !Unit->IsAlive())
		{
			continue;
		}
		FSimUnit SimUnit;
		SimUnit.UnitId = Unit->UnitId;
		SimUnit.Team = Unit->Team;
		SimUnit.Position = Unit->GridPosition;
		SimUnit.Health = Unit->GetHealth();
		SimUnit.MaxHealth = Unit->GetMaxHealth();
		SimUnit.bAlive = true;
		State.Units.Add(SimUnit);
	}
	State.CollisionDamage = GetRules().CollisionDamage;
	return State;
}

bool UBattleSubsystem::GetSelectableTiles(const ADungeonUnit* Unit, int32 AbilityIndex, TArray<FIntPoint>& Out) const
{
	Out.Reset();
	if (Unit == nullptr || !Unit->IsAlive() || !Unit->GetAbilities().IsValidIndex(AbilityIndex))
	{
		return false;
	}
	FAbilityRules::GetSelectableTiles(MakeSimState(), Unit->UnitId, Unit->GetAbilities()[AbilityIndex].Ability, Out);
	return true;
}

bool UBattleSubsystem::PreviewAbility(const ADungeonUnit* Unit, int32 AbilityIndex, FIntPoint SelectedTile, FAbilityOutcome& Out) const
{
	Out = FAbilityOutcome();
	if (Unit == nullptr || !Unit->IsAlive() || !Unit->GetAbilities().IsValidIndex(AbilityIndex))
	{
		return false;
	}

	const FBattleSimState Sim = MakeSimState();
	const FResolvedAbility& Ability = Unit->GetAbilities()[AbilityIndex].Ability;
	FAbilityCast Cast;
	if (!FAbilityRules::MakeCast(Sim, Unit->UnitId, Ability, SelectedTile, Cast))
	{
		return false;
	}
	return FAbilityRules::Preview(Sim, Ability, Cast, Out);
}

void UBattleSubsystem::ExecuteAbilityInternal(ADungeonUnit* Source, int32 AbilityIndex, const FAbilityCast& Cast)
{
	if (Source == nullptr || !Source->GetAbilities().IsValidIndex(AbilityIndex))
	{
		UE_LOG(LogDungeonBattle, Error, TEXT("ExecuteAbilityInternal: no source unit or ability index %d is out of range."), AbilityIndex);
		return;
	}

	// 1. Результат считают только правила способностей
	FBattleSimState Sim = MakeSimState();
	FAbilityOutcome Outcome;
	FAbilityRules::Simulate(Sim, Source->GetAbilities()[AbilityIndex].Ability, Cast, Outcome);

	// 2. Перезарядка и признак "использовал способность" у героя
	Source->StartCooldown(AbilityIndex, TurnNumber);
	if (Source->Team == EUnitTeam::Hero)
	{
		Source->MarkActed();
	}

	// 3. События результата
	ApplyOutcome(Outcome);

	// 4.
	OnUnitChanged.Broadcast(Source);
	OnAbilityUsed.Broadcast(Source, AbilityIndex, Cast.TargetPoint);

	// 5. Конец боя проверяет TryExecute после Execute действия
	RebuildIntentViews();
}

void UBattleSubsystem::ApplyOutcome(const FAbilityOutcome& Outcome)
{
	for (const FAbilityEvent& Event : Outcome.Events)
	{
		ADungeonUnit* Unit = FindUnitById(Event.UnitId);
		if (Unit == nullptr)
		{
			UE_LOG(LogDungeonBattle, Error, TEXT("ApplyOutcome: no unit with UnitId %d, the event is skipped."), Event.UnitId);
			continue;
		}

		switch (Event.Type)
		{
		case EAbilityEventType::Damage:
			Unit->SetHealth(Unit->GetHealth() - Event.Amount);
			OnUnitChanged.Broadcast(Unit);
			break;
		case EAbilityEventType::Heal:
			Unit->SetHealth(Unit->GetHealth() + Event.Amount);
			OnUnitChanged.Broadcast(Unit);
			break;
		case EAbilityEventType::Push:
		case EAbilityEventType::SourceMove:
		{
			// Симуляция и бой совпадают, поэтому перемещение не должно проваливаться
			const FIntPoint From = Unit->GridPosition;
			if (!MoveUnitInternal(Unit, Event.To))
			{
				UE_LOG(LogDungeonBattle, Error, TEXT("ApplyOutcome: could not move unit %d from (%d, %d) to (%d, %d), the event is skipped."),
					Event.UnitId, From.X, From.Y, Event.To.X, Event.To.Y);
				break;
			}
			OnUnitMoved.Broadcast(Unit, From, Event.To);
			break;
		}
		case EAbilityEventType::Collision:
			// Изменений нет: событие нужно предпросмотру
			break;
		case EAbilityEventType::Death:
			KillUnit(Unit, FindUnitById(Event.OtherUnitId));
			break;
		}
	}
}

void UBattleSubsystem::KillUnit(ADungeonUnit* Unit, ADungeonUnit* Killer)
{
	if (Unit == nullptr || !Unit->IsAlive())
	{
		return;
	}

	if (Grid != nullptr)
	{
		Grid->GetMutableModel().RemoveUnit(Unit->GridPosition);
	}
	Unit->MarkDead();

	if (AMonsterUnit* Monster = Cast<AMonsterUnit>(Unit))
	{
		ClearIntent(Monster);

		// Опыт получает только герой, нанёсший смертельный удар, в том числе столкновением.
		// Монстр, погибший от способности монстра, опыта не даёт
		if (AHeroUnit* Hero = Cast<AHeroUnit>(Killer))
		{
			Hero->AddExperience(Monster->GetExperienceReward());
			OnUnitChanged.Broadcast(Hero);
		}
	}

	OnUnitDied.Broadcast(Unit, Killer);
	OnUnitChanged.Broadcast(Unit);
}

void UBattleSubsystem::SetIntent(AMonsterUnit* Monster, const FMonsterIntent& Intent)
{
	if (Monster == nullptr)
	{
		return;
	}
	FMonsterIntent Active = Intent;
	Active.bActive = true;
	Monster->SetIntent(Active);
}

void UBattleSubsystem::ClearIntent(AMonsterUnit* Monster)
{
	if (Monster != nullptr)
	{
		Monster->ClearIntent();
	}
}

void UBattleSubsystem::ClearAllIntents()
{
	for (ADungeonUnit* Unit : Units)
	{
		if (AMonsterUnit* Monster = Cast<AMonsterUnit>(Unit))
		{
			Monster->ClearIntent();
		}
	}
	// Показанные намерения больше не соответствуют монстрам. Делегат вызывает RebuildIntentViews
	IntentViews.Reset();
}

bool UBattleSubsystem::GetIntentCast(const AMonsterUnit* Monster, FAbilityCast& Out) const
{
	if (Monster == nullptr || !Monster->IsAlive() || !Monster->HasIntent()
		|| !Monster->GetAbilities().IsValidIndex(Monster->Intent.AbilityIndex))
	{
		return false;
	}
	return FAbilityRules::MakeIntentCast(MakeSimState(), Monster->UnitId,
		Monster->GetAbilities()[Monster->Intent.AbilityIndex].Ability, Monster->Intent, Out);
}

void UBattleSubsystem::RebuildIntentViews()
{
	IntentViews.Reset();

	// Живые монстры с намерением по номеру порядка; при равенстве - в порядке появления
	TArray<AMonsterUnit*> Monsters = GetLivingMonsters();
	Monsters.RemoveAll([](const AMonsterUnit* Monster) { return !Monster->HasIntent(); });
	Monsters.StableSort([](const AMonsterUnit& A, const AMonsterUnit& B) { return A.Intent.Order < B.Intent.Order; });

	const FBattleSimState Sim = MakeSimState();
	for (AMonsterUnit* Monster : Monsters)
	{
		FIntentView View;
		View.Monster = Monster;
		View.Order = Monster->Intent.Order;
		if (!GetIntentCast(Monster, View.Cast))
		{
			continue;
		}
		const FResolvedAbility& Ability = Monster->GetAbilities()[Monster->Intent.AbilityIndex].Ability;
		const FAbilityCast& IntentCast = View.Cast;

		// Число на клетке - оценка намерения, а не предпросмотр: сумма урона эффектов по частям области
		// без фильтра целей и урона столкновения. Запись есть для каждой клетки области, в том числе с уроном 0
		// (на ней действуют другие эффекты: смещение, лечение)
		for (const FIntPoint& Tile : IntentCast.Area.GetAllTiles())
		{
			int32 Damage = 0;
			for (const FAbilityEffect& Effect : Ability.Effects)
			{
				if (Effect.Type == EAbilityEffectType::Damage && IntentCast.Area.Contains(Tile, Effect.AreaPart))
				{
					Damage += Effect.Amount;
				}
			}
			FIntentTileDamage Entry;
			Entry.Tile = Tile;
			Entry.Damage = Damage;
			View.TileDamage.Add(Entry);
		}

		// Стрелки смещения
		for (const FAbilityEffect& Effect : Ability.Effects)
		{
			if (Effect.Type != EAbilityEffectType::Push || Effect.PushDistance <= 0)
			{
				continue;
			}
			if (Effect.PushDirection == EPushDirection::AbilityDirection)
			{
				if (IntentCast.Direction != EGridDirection::None)
				{
					FIntentPushMark Mark;
					Mark.Tile = IntentCast.TargetPoint;
					Mark.Direction = IntentCast.Direction;
					View.PushMarks.Add(Mark);
				}
				continue;
			}

			TArray<FIntPoint> PartTiles;
			IntentCast.Area.GetTiles(Effect.AreaPart, PartTiles);
			for (const FIntPoint& Tile : PartTiles)
			{
				if (Sim.FindUnitAt(Tile) == nullptr)
				{
					continue;
				}
				FIntPoint Step = FIntPoint::ZeroValue;
				switch (Effect.PushDirection)
				{
				case EPushDirection::AwayFromSource:
					Step = IntentStepAwayFrom(IntentCast.SourcePosition, Tile);
					break;
				case EPushDirection::TowardSource:
					Step = FIntPoint::ZeroValue - IntentStepAwayFrom(IntentCast.SourcePosition, Tile);
					break;
				case EPushDirection::AwayFromCenter:
					Step = IntentStepAwayFrom(IntentCast.TargetPoint, Tile);
					break;
				default:
					break;
				}
				if (Step != FIntPoint::ZeroValue)
				{
					FIntentPushMark Mark;
					Mark.Tile = Tile;
					Mark.Direction = FAbilityRules::GetDirection(Tile, Tile + Step);
					View.PushMarks.Add(Mark);
				}
			}
		}

		// Каждое намерение считается отдельно, без учёта других намерений
		FAbilityRules::Preview(Sim, Ability, IntentCast, View.Preview);
		IntentViews.Add(View);
	}

	OnIntentsChanged.Broadcast();
}

void UBattleSubsystem::TickCooldowns()
{
	for (ADungeonUnit* Unit : Units)
	{
		if (Unit == nullptr || !Unit->IsAlive())
		{
			continue;
		}
		const int32 Before = SumCooldowns(*Unit);
		Unit->TickCooldowns(TurnNumber);
		if (SumCooldowns(*Unit) != Before)
		{
			OnUnitChanged.Broadcast(Unit);
		}
	}
}

const UBattleRulesConfig& UBattleSubsystem::GetRules() const
{
	return Rules != nullptr ? *Rules : *GetDefault<UBattleRulesConfig>();
}

TArray<AHeroUnit*> UBattleSubsystem::GetHeroes() const
{
	TArray<AHeroUnit*> Heroes;
	for (ADungeonUnit* Unit : Units)
	{
		if (AHeroUnit* Hero = Cast<AHeroUnit>(Unit))
		{
			Heroes.Add(Hero);
		}
	}
	return Heroes;
}

TArray<AMonsterUnit*> UBattleSubsystem::GetLivingMonsters() const
{
	TArray<AMonsterUnit*> Monsters;
	for (ADungeonUnit* Unit : Units)
	{
		if (AMonsterUnit* Monster = Cast<AMonsterUnit>(Unit))
		{
			if (Monster->IsAlive())
			{
				Monsters.Add(Monster);
			}
		}
	}
	return Monsters;
}

bool UBattleSubsystem::HasLivingHeroes() const
{
	for (const ADungeonUnit* Unit : Units)
	{
		if (Unit != nullptr && Unit->Team == EUnitTeam::Hero && Unit->IsAlive())
		{
			return true;
		}
	}
	return false;
}

ADungeonUnit* UBattleSubsystem::FindUnitById(int32 UnitId) const
{
	for (ADungeonUnit* Unit : Units)
	{
		if (Unit != nullptr && Unit->UnitId == UnitId)
		{
			return Unit;
		}
	}
	return nullptr;
}
