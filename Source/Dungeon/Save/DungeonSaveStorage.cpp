#include "Save/DungeonSaveStorage.h"

#include "Battle/BattleSubsystem.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Save/DungeonSaveGame.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogDungeonSaveStorage, Log, All);

namespace
{
	constexpr uint32 EnvelopeMagic = 0x53474E44; // DNGS, little endian.
	constexpr uint32 EnvelopeVersion = 1;
	constexpr int32 PrefixSize = 20;
	constexpr int32 DigestSize = 20;
	constexpr int32 MaxPayloadSize = 16 * 1024 * 1024;
	constexpr int32 MaxEnvelopeSize = PrefixSize + MaxPayloadSize + DigestSize;
	constexpr int32 UserIndex = 0;
	bool bOperationActive = false;

	struct FSlotState
	{
		FString Name;
		FString Error;
		TStrongObjectPtr<UDungeonSaveGame> Data;
		bool bExists = false;
	};

	bool CheckSlotName(const FString& Name, FString& Error)
	{
		if (Name.IsEmpty() || Name.Len() > 64)
		{
			Error = TEXT("Save slot names must contain 1 to 64 ASCII letters, digits, underscores or hyphens.");
			return false;
		}
		for (const TCHAR Character : Name)
		{
			if (!((Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'))
				|| Character == TEXT('_') || Character == TEXT('-')))
			{
				Error = TEXT("The save slot name contains an invalid character. Paths are not allowed.");
				return false;
			}
		}
		return true;
	}

	bool CanStartOperation(const FString& SlotName, FString& Error)
	{
		Error.Reset();
		if (!IsInGameThread())
		{
			Error = TEXT("Save storage must run on the game thread.");
			return false;
		}
		if (bOperationActive)
		{
			Error = TEXT("Another save storage operation is active. Retry after it completes.");
			return false;
		}
		return CheckSlotName(SlotName, Error);
	}

	void AppendInteger(TArray<uint8>& Bytes, uint64 Value, int32 Size)
	{
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Bytes.Add(static_cast<uint8>(Value >> (Index * 8)));
		}
	}

	uint64 ReadInteger(const TArray<uint8>& Bytes, int32 Offset, int32 Size)
	{
		uint64 Value = 0;
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Value |= static_cast<uint64>(Bytes[Offset + Index]) << (Index * 8);
		}
		return Value;
	}

	bool ReadEnvelope(const FString& SlotName, TArray<uint8>& Payload, int64& Sequence, FString& Error)
	{
		// The generic desktop backend uses this path. Check its size before it allocates memory.
		// Native platform backends can use other storage. Check their returned buffer below.
		const FString LocalPath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
		const int64 LocalSize = IFileManager::Get().FileSize(*LocalPath);
		if (LocalSize > MaxEnvelopeSize)
		{
			Error = TEXT("The checkpoint exceeds the 16 MiB payload limit.");
			return false;
		}

		TArray<uint8> Bytes;
		// The raw slot API uses the platform save system without UObject deserialization.
		if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, UserIndex))
		{
			Error = TEXT("The platform could not read the checkpoint. Check storage access and free space.");
			return false;
		}
		if (Bytes.Num() <= PrefixSize + DigestSize || Bytes.Num() > MaxEnvelopeSize)
		{
			Error = TEXT("The checkpoint size is invalid or the file is truncated.");
			return false;
		}
		if (ReadInteger(Bytes, 0, 4) != EnvelopeMagic || ReadInteger(Bytes, 4, 4) != EnvelopeVersion)
		{
			Error = TEXT("The checkpoint header or storage version is invalid.");
			return false;
		}
		const uint64 StoredSequence = ReadInteger(Bytes, 8, 8);
		const uint64 PayloadSize = ReadInteger(Bytes, 16, 4);
		if (StoredSequence == 0 || StoredSequence > static_cast<uint64>(MAX_int64)
			|| PayloadSize == 0 || PayloadSize > MaxPayloadSize
			|| PayloadSize != static_cast<uint64>(Bytes.Num() - PrefixSize - DigestSize))
		{
			Error = TEXT("The checkpoint sequence or payload size is invalid.");
			return false;
		}
		uint8 Digest[DigestSize];
		FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num() - DigestSize, Digest);
		if (FMemory::Memcmp(Digest, Bytes.GetData() + Bytes.Num() - DigestSize, DigestSize) != 0)
		{
			Error = TEXT("The checkpoint checksum does not match. The file is damaged or incomplete.");
			return false;
		}

		Sequence = static_cast<int64>(StoredSequence);
		Payload.Append(Bytes.GetData() + PrefixSize, static_cast<int32>(PayloadSize));
		return true;
	}

	void ReadSlot(const FString& Name, FSlotState& Out)
	{
		Out.Name = Name;
		Out.bExists = UGameplayStatics::DoesSaveGameExist(Name, UserIndex);
		if (!Out.bExists)
		{
			Out.Error = TEXT("The checkpoint does not exist.");
			return;
		}
		TArray<uint8> Payload;
		int64 Sequence = 0;
		if (!ReadEnvelope(Name, Payload, Sequence, Out.Error))
		{
			return;
		}

		Out.Data.Reset(Cast<UDungeonSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload)));
		if (!Out.Data.IsValid())
		{
			Out.Error = TEXT("The checkpoint does not contain a Dungeon save game.");
			return;
		}
		if (Out.Data->Sequence != Sequence || Out.Data->Version != UDungeonSaveGame::CurrentVersion)
		{
			Out.Error = TEXT("The checkpoint schema version or sequence does not match.");
			Out.Data.Reset();
			return;
		}
		if (!UBattleSubsystem::ValidateSave(*Out.Data, Out.Error))
		{
			Out.Data.Reset();
		}
	}

	FSlotState* GetNewest(FSlotState& A, FSlotState& B)
	{
		if (!A.Data.IsValid())
		{
			return B.Data.IsValid() ? &B : nullptr;
		}
		return B.Data.IsValid() && B.Data->Sequence > A.Data->Sequence ? &B : &A;
	}
}

bool FDungeonSaveStorage::Save(const FString& SlotName, UDungeonSaveGame& Save, FString& Error)
{
	if (!CanStartOperation(SlotName, Error))
	{
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationActive, true);
	FSlotState A;
	FSlotState B;
	ReadSlot(SlotName + TEXT("_A"), A);
	ReadSlot(SlotName + TEXT("_B"), B);
	const FSlotState* Newest = GetNewest(A, B);
	const int64 PreviousSequence = Newest != nullptr ? Newest->Data->Sequence : 0;
	if (PreviousSequence == MAX_int64)
	{
		Error = TEXT("The checkpoint sequence is exhausted. Select a new save slot.");
		return false;
	}
	const int64 NextSequence = PreviousSequence + 1;
	const FString TargetName = Newest == &A ? B.Name : A.Name;

	TArray<uint8> Payload;
	{
		TGuardValue<int64> SequenceGuard(Save.Sequence, NextSequence);
		if (!UBattleSubsystem::ValidateSave(Save, Error))
		{
			return false;
		}
		if (!UGameplayStatics::SaveGameToMemory(&Save, Payload))
		{
			Error = TEXT("The checkpoint could not be serialized. The previous checkpoint was preserved.");
			return false;
		}
	}
	if (Payload.IsEmpty() || Payload.Num() > MaxPayloadSize)
	{
		Error = TEXT("The checkpoint payload is empty or exceeds 16 MiB. The previous checkpoint was preserved.");
		return false;
	}

	TArray<uint8> Bytes;
	Bytes.Reserve(PrefixSize + Payload.Num() + DigestSize);
	AppendInteger(Bytes, EnvelopeMagic, 4);
	AppendInteger(Bytes, EnvelopeVersion, 4);
	AppendInteger(Bytes, static_cast<uint64>(NextSequence), 8);
	AppendInteger(Bytes, Payload.Num(), 4);
	Bytes.Append(Payload);
	uint8 Digest[DigestSize];
	FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Digest);
	Bytes.Append(Digest, DigestSize);
	if (!UGameplayStatics::SaveDataToSlot(Bytes, TargetName, UserIndex))
	{
		Error = FString::Printf(TEXT("Could not write checkpoint '%s'. Check storage access and free space. The previous checkpoint was preserved."), *TargetName);
		return false;
	}

	FSlotState Written;
	ReadSlot(TargetName, Written);
	if (!Written.Data.IsValid() || Written.Data->Sequence != NextSequence)
	{
		Error = FString::Printf(TEXT("Checkpoint '%s' failed verification: %s The previous checkpoint was preserved."), *TargetName, *Written.Error);
		return false;
	}
	Save.Sequence = NextSequence;
	return true;
}

UDungeonSaveGame* FDungeonSaveStorage::Load(const FString& SlotName, FString& Error)
{
	if (!CanStartOperation(SlotName, Error))
	{
		return nullptr;
	}
	TGuardValue<bool> OperationGuard(bOperationActive, true);
	FSlotState A;
	FSlotState B;
	ReadSlot(SlotName + TEXT("_A"), A);
	ReadSlot(SlotName + TEXT("_B"), B);
	FSlotState* Newest = GetNewest(A, B);
	if (Newest == nullptr)
	{
		Error = FString::Printf(TEXT("No valid checkpoint in '%s'. A: %s B: %s"), *SlotName, *A.Error, *B.Error);
		return nullptr;
	}
	const FSlotState& Other = Newest == &A ? B : A;
	if (Other.bExists && !Other.Data.IsValid())
	{
		UE_LOG(LogDungeonSaveStorage, Warning, TEXT("Loaded checkpoint '%s'. Ignored '%s': %s"), *Newest->Name, *Other.Name, *Other.Error);
	}
	return Newest->Data.Get();
}

bool FDungeonSaveStorage::Exists(const FString& SlotName)
{
	FString Error;
	if (!CanStartOperation(SlotName, Error))
	{
		return false;
	}
	TGuardValue<bool> OperationGuard(bOperationActive, true);
	return UGameplayStatics::DoesSaveGameExist(SlotName + TEXT("_A"), UserIndex)
		|| UGameplayStatics::DoesSaveGameExist(SlotName + TEXT("_B"), UserIndex);
}
