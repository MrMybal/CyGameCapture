// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Spout2 sender registry helpers (names, shared-memory sender info, access mutex).
//
// Only the CPU side of Spout is used here: sender names (shared memory map every Spout application
// reads), per-sender info (share handle, size, DXGI format, owner executable), the named access mutex
// and the optional frame counter. GPU texture sharing is done by the backends.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "CyGameCaptureTypes.h"

class spoutSenderNames;
class spoutFrameCount;

namespace CySpout
{
	/** Spout2 is compiled into this build (Win64). */
	bool IsCompiledIn();

	/** Every sender visible on the machine. Game thread (or any thread; internally serialized). */
	TArray<FCySpoutSenderInfo> EnumerateSenders();

	/** Live lookup of a sender's published information. */
	bool GetSenderInfo(const FString& Name, FCySpoutSenderInfo& OutInfo, void*& OutShareHandle);

	bool SenderExists(const FString& Name);

	/** Full path of this process' executable, as Spout writes it in the sender description. */
	FString ThisProcessExecutable();

	bool IsCyGameCaptureName(const FString& Name);

	/** Size of Spout's machine-wide sender name table (64 unless raised in the Spout registry key). */
	int32 MaxSenders();

	/** Senders currently registered on the machine, all applications included. */
	int32 ActiveSenderCount();

	/**
	 * The only real ceiling on simultaneous senders. Spout's name table is shared by every Spout
	 * application on the machine and CreateSender fails silently once it is full, so it is checked
	 * explicitly and reported with an actionable message.
	 */
	bool CanRegisterNewSender(FString& OutReason);
}

/**
 * Registration of one Spout sender name (sender side) or attachment to an existing one (receiver side).
 * Owns the Spout access mutex used around every GPU copy. All methods except BeginAccess/EndAccess are
 * game/render thread; BeginAccess/EndAccess must be called on the same thread (RHI thread or worker).
 */
class FCySpoutSenderRegistration
{
public:
	FCySpoutSenderRegistration();
	~FCySpoutSenderRegistration();

	FCySpoutSenderRegistration(const FCySpoutSenderRegistration&) = delete;
	FCySpoutSenderRegistration& operator=(const FCySpoutSenderRegistration&) = delete;

	/** Sender side: publish name + shared texture description. */
	bool Create(const FString& Name, uint32 Width, uint32 Height, void* ShareHandle, uint32 DxgiFormat, FString& OutError);
	bool Update(uint32 Width, uint32 Height, void* ShareHandle, uint32 DxgiFormat);

	/** Receiver side: only attach to the access mutex / frame counter of an existing sender. */
	bool OpenForReceiving(const FString& Name);

	void Release();

	/** Take the named access mutex (Spout waits up to 67 ms). Returns false when a peer holds it too long: skip the frame. */
	bool BeginAccess();
	/** Release the mutex; bSignalNewFrame increments the Spout frame counter (sender side). */
	void EndAccess(bool bSignalNewFrame);

	bool IsCreated() const { return bCreated; }
	bool IsOpen() const { return bOpen; }
	const FString& GetName() const { return Name; }

private:
	TUniquePtr<spoutSenderNames> Names;
	TUniquePtr<spoutFrameCount> Frame;
	FString Name;
	bool bCreated = false;
	bool bOpen = false;
};
