// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include "CyUserGuide.h"
#include "CyGameCaptureLog.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "cydoc_link.hpp"
#if WITH_EDITOR
#include "ToolMenus.h"
#endif

namespace
{
    FString GuideUrl(const FString& RequestedLocale)
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CyGameCaptureUE"));
        if (!Plugin.IsValid()) return FString();
        FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Documentation/Documentation.html")));
        FPaths::NormalizeFilename(File);
        if (!IFileManager::Get().FileExists(*File))
        {
            CYGC_LOG(Error, TEXT("Offline guide is missing: %s"), *File);
            return FString();
        }
        // Framework path normalisation plus the upstream CyDoc UTF-8 encoder.
        // Preserve only path delimiters, after encoding spaces, %, # and Unicode.
        const FTCHARToUTF8 Utf8(*File);
        FString Encoded = UTF8_TO_TCHAR(cydoc_native::encode(std::string_view(Utf8.Get(), Utf8.Length())).c_str());
        Encoded.ReplaceInline(TEXT("%2F"), TEXT("/"));
        Encoded.ReplaceInline(TEXT("%3A"), TEXT(":"));
        const FString Base = (File.StartsWith(TEXT("//")) ? TEXT("file:") : TEXT("file:///")) + Encoded;
        const FString Locale = RequestedLocale.IsEmpty() ? FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() : RequestedLocale;
        return UTF8_TO_TCHAR(cydoc_native::link(TCHAR_TO_UTF8(*Base), Locale.StartsWith(TEXT("fr")) ? "fr" : "en", "unreal").c_str());
    }

    void HelpCommand(const TArray<FString>& Arguments)
    {
        const FString Url = GuideUrl(Arguments.Num() ? Arguments[0] : FString());
        if (Url.IsEmpty()) return;
        if (Arguments.Num() > 1 && Arguments[1] == TEXT("url"))
        {
            CYGC_LOG(Display, TEXT("CyDoc help URL: %s"), *Url);
            return;
        }
        FString Error;
        FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
        if (!Error.IsEmpty()) CYGC_LOG(Error, TEXT("Could not open local user guide: %s"), *Error);
    }

    FAutoConsoleCommand HelpConsole(TEXT("CyGameCapture.Help"),
        TEXT("Open the local CyDoc Unreal guide. Optional: en|fr, then url to print the address without opening a browser."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&HelpCommand));

#if WITH_EDITOR
    FDelegateHandle StartupHandle;
    void RegisterMenu()
    {
        if (!UToolMenus::IsToolMenuUIEnabled()) return;
        FToolMenuOwnerScoped Owner(TEXT("CyGameCaptureUE.Help"));
        const bool French = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("fr");
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Help"));
        Menu->FindOrAddSection(TEXT("CyGameCapture")).AddMenuEntry(TEXT("CyGameCaptureUserGuide"),
            FText::FromString(French ? TEXT("Guide utilisateur CyGameCapture") : TEXT("CyGameCapture user guide")),
            FText::FromString(French ? TEXT("Ouvrir la documentation locale CyDoc") : TEXT("Open the local CyDoc documentation")),
            FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([](){ HelpCommand(TArray<FString>()); })));
    }
#endif
}

void CyUserGuide::Register()
{
#if WITH_EDITOR
    StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenu));
#endif
}

void CyUserGuide::Unregister()
{
#if WITH_EDITOR
    if (StartupHandle.IsValid()) UToolMenus::UnRegisterStartupCallback(StartupHandle);
    UToolMenus::UnregisterOwner(TEXT("CyGameCaptureUE.Help"));
#endif
}
