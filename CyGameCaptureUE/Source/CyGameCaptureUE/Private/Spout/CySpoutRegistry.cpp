// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "Spout/CySpoutRegistry.h"
#include "Spout/CySpoutIncludes.h"
#include "RHI/CyRHICompat.h"
#include "CyGameCaptureLog.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"

namespace
{
#if CYGC_WITH_SPOUT
	// Shared enumeration instance; spoutSenderNames keeps shared memory handles, serialize access.
	FCriticalSection& EnumLock()
	{
		static FCriticalSection Lock;
		return Lock;
	}
	spoutSenderNames& EnumNames()
	{
		static spoutSenderNames Names;
		return Names;
	}

	FString NarrowToFString(const char* Text)
	{
		return Text != nullptr ? FString(ANSI_TO_TCHAR(Text)) : FString();
	}

	bool ReadInfo(spoutSenderNames& Names, const FString& Name, FCySpoutSenderInfo& OutInfo, void*& OutHandle)
	{
		SharedTextureInfo Info = {};
		const FTCHARToUTF8 NameUtf8(*Name);
		if (!Names.getSharedInfo(NameUtf8.Get(), &Info))
		{
			return false;
		}
		OutInfo.Name = Name;
		OutInfo.Width = static_cast<int32>(Info.width);
		OutInfo.Height = static_cast<int32>(Info.height);
		OutInfo.DxgiFormat = static_cast<int32>(Info.format);
		OutInfo.FormatName = CyDxgiFormatName(Info.format);
		char Description[257] = {};
		FMemory::Memcpy(Description, Info.description, 256);
		OutInfo.OwnerExecutable = NarrowToFString(Description);
		OutInfo.bOwnedByThisProcess = !OutInfo.OwnerExecutable.IsEmpty() && OutInfo.OwnerExecutable.Equals(CySpout::ThisProcessExecutable(), ESearchCase::IgnoreCase);
		OutInfo.bIsCyGameCaptureSender = CySpout::IsCyGameCaptureName(Name);
		// Spout stores the 32 bit handle value; receivers sign-extend it like Spout does (LongToHandle)
		OutHandle = reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int32>(Info.shareHandle)));
		return true;
	}
#endif
}

namespace CySpout
{
	bool IsCompiledIn()
	{
#if CYGC_WITH_SPOUT
		return true;
#else
		return false;
#endif
	}

	FString ThisProcessExecutable()
	{
		static const FString Path = []()
		{
			// Same source as Spout (GetModuleFileNameA / QueryFullProcessImageName): the running executable
			return FString(FPlatformProcess::ExecutablePath());
		}();
		return Path;
	}

	bool IsCyGameCaptureName(const FString& Name)
	{
		return Name.StartsWith(TEXT("CyGameCaptureUE::")) || Name.StartsWith(TEXT("CyGameCaptureRS::")) || Name.StartsWith(TEXT("CyGameCapture"));
	}

	int32 MaxSenders()
	{
#if CYGC_WITH_SPOUT
		FScopeLock Lock(&EnumLock());
		return static_cast<int32>(EnumNames().GetMaxSenders());
#else
		return 0;
#endif
	}

	int32 ActiveSenderCount()
	{
#if CYGC_WITH_SPOUT
		FScopeLock Lock(&EnumLock());
		return static_cast<int32>(EnumNames().GetSenderCount());
#else
		return 0;
#endif
	}

	bool CanRegisterNewSender(FString& OutReason)
	{
#if CYGC_WITH_SPOUT
		const int32 Max = MaxSenders();
		const int32 Count = ActiveSenderCount();
		if (Count < Max)
		{
			return true;
		}
		OutReason = FString::Printf(
			TEXT("Spout's sender table is full (%d of %d senders on this machine, all applications included). ")
			TEXT("Raise HKCU\\Software\\Leading Edge\\Spout\\MaxSenders or stop a sender."), Count, Max);
		return false;
#else
		OutReason = TEXT("Spout is not available on this platform");
		return false;
#endif
	}

	TArray<FCySpoutSenderInfo> EnumerateSenders()
	{
		TArray<FCySpoutSenderInfo> Result;
#if CYGC_WITH_SPOUT
		FScopeLock Lock(&EnumLock());
		spoutSenderNames& Names = EnumNames();
		std::set<std::string> NameSet;
		if (!Names.GetSenderNames(&NameSet))
		{
			return Result;
		}
		for (const std::string& NameStd : NameSet)
		{
			FCySpoutSenderInfo Info;
			void* Handle = nullptr;
			const FString Name = NarrowToFString(NameStd.c_str());
			if (ReadInfo(Names, Name, Info, Handle))
			{
				Result.Add(MoveTemp(Info));
			}
		}
		Result.Sort([](const FCySpoutSenderInfo& A, const FCySpoutSenderInfo& B) { return A.Name < B.Name; });
#endif
		return Result;
	}

	bool GetSenderInfo(const FString& Name, FCySpoutSenderInfo& OutInfo, void*& OutShareHandle)
	{
		OutShareHandle = nullptr;
#if CYGC_WITH_SPOUT
		if (Name.IsEmpty())
		{
			return false;
		}
		FScopeLock Lock(&EnumLock());
		spoutSenderNames& Names = EnumNames();
		const FTCHARToUTF8 NameUtf8(*Name);
		if (!Names.FindSenderName(NameUtf8.Get()))
		{
			return false;
		}
		return ReadInfo(Names, Name, OutInfo, OutShareHandle);
#else
		return false;
#endif
	}

	bool SenderExists(const FString& Name)
	{
#if CYGC_WITH_SPOUT
		if (Name.IsEmpty())
		{
			return false;
		}
		FScopeLock Lock(&EnumLock());
		const FTCHARToUTF8 NameUtf8(*Name);
		return EnumNames().FindSenderName(NameUtf8.Get());
#else
		return false;
#endif
	}
}

// ------------------------------------------------------------------------------------------------
FCySpoutSenderRegistration::FCySpoutSenderRegistration()
{
#if CYGC_WITH_SPOUT
	Names = MakeUnique<spoutSenderNames>();
	Frame = MakeUnique<spoutFrameCount>();
#endif
}

FCySpoutSenderRegistration::~FCySpoutSenderRegistration()
{
	Release();
}

bool FCySpoutSenderRegistration::Create(const FString& InName, uint32 Width, uint32 Height, void* ShareHandle, uint32 DxgiFormat, FString& OutError)
{
#if CYGC_WITH_SPOUT
	Release();

	// Spout silently refuses new names once its machine-wide table is full: say so explicitly
	FString CapacityReason;
	if (!CySpout::CanRegisterNewSender(CapacityReason))
	{
		OutError = CapacityReason;
		return false;
	}

	char NameBuffer[256] = {};
	const FTCHARToUTF8 NameUtf8(*InName);
	FCStringAnsi::Strncpy(NameBuffer, NameUtf8.Get(), sizeof(NameBuffer));

	if (!Names->CreateSender(NameBuffer, Width, Height, static_cast<HANDLE>(ShareHandle), static_cast<DWORD>(DxgiFormat)))
	{
		OutError = FString::Printf(TEXT("Spout sender registration failed for '%s'"), *InName);
		return false;
	}
	Frame->CreateAccessMutex(NameBuffer);
	Frame->EnableFrameCount(NameBuffer);
	Name = InName;
	bCreated = true;
	bOpen = true;
	CYGC_LOG(Log, TEXT("Spout sender registered: '%s' %ux%u %s handle=0x%p"), *InName, Width, Height, *CyDxgiFormatName(DxgiFormat), ShareHandle);
	return true;
#else
	OutError = TEXT("Spout is not available on this platform");
	return false;
#endif
}

bool FCySpoutSenderRegistration::Update(uint32 Width, uint32 Height, void* ShareHandle, uint32 DxgiFormat)
{
#if CYGC_WITH_SPOUT
	if (!bCreated)
	{
		return false;
	}
	const FTCHARToUTF8 NameUtf8(*Name);
	if (!Names->UpdateSender(NameUtf8.Get(), Width, Height, static_cast<HANDLE>(ShareHandle), static_cast<DWORD>(DxgiFormat)))
	{
		return false;
	}
	CYGC_LOG(Log, TEXT("Spout sender updated: '%s' %ux%u %s"), *Name, Width, Height, *CyDxgiFormatName(DxgiFormat));
	return true;
#else
	return false;
#endif
}

bool FCySpoutSenderRegistration::OpenForReceiving(const FString& InName)
{
#if CYGC_WITH_SPOUT
	Release();
	char NameBuffer[256] = {};
	const FTCHARToUTF8 NameUtf8(*InName);
	FCStringAnsi::Strncpy(NameBuffer, NameUtf8.Get(), sizeof(NameBuffer));
	Frame->CreateAccessMutex(NameBuffer);
	Frame->EnableFrameCount(NameBuffer);
	Name = InName;
	bOpen = true;
	return true;
#else
	return false;
#endif
}

void FCySpoutSenderRegistration::Release()
{
#if CYGC_WITH_SPOUT
	if (!bOpen && !bCreated)
	{
		return;
	}
	Frame->CleanupFrameCount();
	if (bCreated)
	{
		const FTCHARToUTF8 NameUtf8(*Name);
		Names->ReleaseSenderName(NameUtf8.Get());
		CYGC_LOG(Log, TEXT("Spout sender released: '%s'"), *Name);
	}
#endif
	bCreated = false;
	bOpen = false;
	Name.Reset();
}

bool FCySpoutSenderRegistration::BeginAccess()
{
#if CYGC_WITH_SPOUT
	return bOpen ? Frame->CheckAccess() : false;
#else
	return false;
#endif
}

void FCySpoutSenderRegistration::EndAccess(bool bSignalNewFrame)
{
#if CYGC_WITH_SPOUT
	if (!bOpen)
	{
		return;
	}
	if (bSignalNewFrame)
	{
		Frame->SetNewFrame();
	}
	Frame->AllowAccess();
#endif
}
