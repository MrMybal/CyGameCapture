// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include "CyAutomation.h"
#include "CyGameCaptureLog.h"
#include "CyGameCaptureSpoutReceiverComponent.h"
#include "CyGameCaptureSpoutSenderComponent.h"
#include "CyGameCaptureSubsystem.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#endif
namespace
{
using Json = TSharedPtr<FJsonObject>;
void *Library = nullptr;
void *Host = nullptr;
using CreateFn = void *(*)(const char *);
using AddFn = int (*)(void *, const char *, const char *(__cdecl *)(void *, const char *, const char *), void *);
using PumpFn = void (*)(void *);
using AllowFn = void (*)(void *, int);
using TextFn = const char *(*)(void *);
using ChatFn = int (*)(void *, const char *);
CreateFn Create = nullptr;
AddFn Add = nullptr;
PumpFn Pump = nullptr, Destroy = nullptr, Cancel = nullptr, Clear = nullptr;
AllowFn Allow = nullptr;
TextFn Connection = nullptr, Events = nullptr, History = nullptr, Access = nullptr;
using ErrorFn = const char *(*)();
ErrorFn LastError = nullptr;
ChatFn Send = nullptr;
int32 SelectedProfile = 0;
FAutoConsoleVariableRef AccessProfileCvar(
    TEXT("CyGameCapture.Automation.AccessProfile"), SelectedProfile,
    TEXT("MCP access profile on next start: 0=Free (default), 1=Local controlled, 2=Token only, 3=Protected"));
const FString &ProfileConfigFile()
{
#if WITH_EDITOR
    return GEditorPerProjectIni;
#else
    return GGameIni;
#endif
}
void SaveAccessProfile()
{
    if (GConfig)
    {
        GConfig->SetInt(TEXT("CyGameCapture.Automation"), TEXT("AccessProfile"), SelectedProfile, ProfileConfigFile());
        GConfig->Flush(false, ProfileConfigFile());
    }
}
void LoadAccessProfile()
{
    auto *Var = IConsoleManager::Get().FindConsoleVariable(TEXT("CyGameCapture.Automation.AccessProfile"));
    int32 Saved = 0;
    const bool HasSaved =
        GConfig && GConfig->GetInt(TEXT("CyGameCapture.Automation"), TEXT("AccessProfile"), Saved, ProfileConfigFile());
    if (HasSaved && Var && (Var->GetFlags() & ECVF_SetByMask) == ECVF_SetByConstructor)
        SelectedProfile = Saved;
    if (!HasSaved)
        SaveAccessProfile();
}
const TCHAR *ProfileLabel(int32 Profile)
{
    switch (Profile)
    {
    case 0:
        return TEXT("Free");
    case 1:
        return TEXT("Local controlled");
    case 2:
        return TEXT("Token only");
    case 3:
        return TEXT("Protected");
    default:
        return TEXT("Invalid");
    }
}
int32 Control = 0;
FAutoConsoleVariableRef ControlCvar(TEXT("CyGameCapture.Automation.AllowMutations"), Control,
                                    TEXT("For controlled profiles only: 0=read only, 1=allow capture mutations"));
int32 Compact = 0, Notifications = 1, CacheTTL = 1000, Performance = 0;
FAutoConsoleVariableRef CompactCvar(TEXT("CyGameCapture.Automation.Compact"), Compact,
                                    TEXT("Compact MCP catalogue, applied on next subsystem start"));
FAutoConsoleVariableRef NotificationsCvar(TEXT("CyGameCapture.Automation.Notifications"), Notifications,
                                          TEXT("MCP change notifications, applied on next subsystem start"));
FAutoConsoleVariableRef CacheCvar(TEXT("CyGameCapture.Automation.CacheTTL"), CacheTTL,
                                  TEXT("Discovery cache TTL in milliseconds, applied on next subsystem start"));
FAutoConsoleVariableRef PerformanceCvar(TEXT("CyGameCapture.Automation.Performance"), Performance,
                                        TEXT("Private metadata-only JSONL measures, applied on next subsystem start"));
FString Transcript, Partial;
bool ChatBusy = false;
TArray<ANSICHAR> Reply;
#if WITH_EDITOR
TSharedPtr<SMultiLineEditableTextBox> Input;
TWeakPtr<SWindow> AssistantWindow;
#endif
Json Object()
{
    return MakeShared<FJsonObject>();
}
FString Encode(Json Value)
{
    FString Text;
    auto Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Value.ToSharedRef(), Writer);
    return Text;
}
Json Decode(const FString &Text)
{
    Json Value;
    auto Reader = TJsonReaderFactory<>::Create(Text);
    FJsonSerializer::Deserialize(Reader, Value);
    return Value;
}
void Put(Json J, const FString &Name, const FString &Value)
{
    J->SetStringField(Name, Value);
}
TSharedPtr<FJsonValue> Value(Json J)
{
    return MakeShared<FJsonValueObject>(J);
}
Json Schema(const FString &Type)
{
    auto J = Object();
    Put(J, TEXT("type"), Type);
    if (Type == TEXT("string"))
        J->SetNumberField(TEXT("maxLength"), 2048);
    return J;
}
void Paginate(Json Result, const TCHAR *Key, Json Args);
const char *__cdecl Invoke(void *, const char *Name, const char *Arguments)
{
    const FString Op = UTF8_TO_TCHAR(Name);
    auto Args = Decode(UTF8_TO_TCHAR(Arguments));
    auto Result = Object();
    auto *System = UCyGameCaptureSubsystem::Get();
    FString Error;
    if (!System)
        Error = TEXT("Engine subsystem is not initialized");
    else if (Op == TEXT("app.status"))
    {
        Put(Result, TEXT("component"), TEXT("unreal"));
        Put(Result, TEXT("project"), FApp::GetProjectName());
        Put(Result, TEXT("renderer"), System->GetRHIName());
        Result->SetBoolField(TEXT("spout_supported"), System->IsSpoutSupported());
        Result->SetNumberField(TEXT("active_streams"), System->GetActiveStreamCount());
    }
    else if (Op == TEXT("senders.list"))
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto &Item : System->GetAvailableSpoutSenders(true))
        {
            auto Row = Object();
            Put(Row, TEXT("name"), Item.Name);
            Row->SetNumberField(TEXT("width"), Item.Width);
            Row->SetNumberField(TEXT("height"), Item.Height);
            Put(Row, TEXT("format"), Item.FormatName);
            Rows.Add(Value(Row));
        }
        Result->SetArrayField(TEXT("senders"), Rows);
    }
    else if (Op == TEXT("streams.list"))
    {
        TArray<FCyGameCaptureStreamStats> A, B;
        System->GetAllStreamStats(A, B);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto &Item : A)
        {
            Json Row = Object();
            FJsonObjectConverter::UStructToJsonObject(FCyGameCaptureStreamStats::StaticStruct(), &Item,
                                                      Row.ToSharedRef(), 0, 0);
            Put(Row, TEXT("direction"), TEXT("sender"));
            Rows.Add(Value(Row));
        }
        for (const auto &Item : B)
        {
            Json Row = Object();
            FJsonObjectConverter::UStructToJsonObject(FCyGameCaptureStreamStats::StaticStruct(), &Item,
                                                      Row.ToSharedRef(), 0, 0);
            Put(Row, TEXT("direction"), TEXT("receiver"));
            Rows.Add(Value(Row));
        }
        Result->SetArrayField(TEXT("streams"), Rows);
    }
    else if (Op == TEXT("components.list"))
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (TObjectIterator<UCyGameCaptureSpoutSenderComponent> It; It; ++It)
            if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld())
            {
                auto Row = Object();
                Put(Row, TEXT("path"), It->GetPathName());
                Put(Row, TEXT("type"), TEXT("sender"));
                Put(Row, TEXT("sender"), It->GetActiveSenderName());
                Rows.Add(Value(Row));
            }
        for (TObjectIterator<UCyGameCaptureSpoutReceiverComponent> It; It; ++It)
            if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld())
            {
                auto Row = Object();
                Put(Row, TEXT("path"), It->GetPathName());
                Put(Row, TEXT("type"), TEXT("receiver"));
                Rows.Add(Value(Row));
            }
        Result->SetArrayField(TEXT("components"), Rows);
    }
    else if (Op == TEXT("streams.stop_all"))
    {
        System->StopAllStreams();
        Result->SetBoolField(TEXT("stopped"), true);
    }
    else
    {
        FString Path;
        Args->TryGetStringField(TEXT("component"), Path);
        if (Op.StartsWith(TEXT("sender.")))
        {
            auto *Component = FindObject<UCyGameCaptureSpoutSenderComponent>(nullptr, *Path);
            if (!IsValid(Component) || !Component->GetWorld())
                Error = TEXT("Sender component must already exist in an open world");
            else if (Op == TEXT("sender.start"))
            {
                const bool Started = Component->StartSender();
                Result->SetBoolField(TEXT("started"), Started);
                if (!Started)
                    Error = TEXT("Sender could not start; check its source, world, renderer and component diagnostics");
            }
            else if (Op == TEXT("sender.stop"))
            {
                Component->StopSender();
                Result->SetBoolField(TEXT("stopped"), true);
            }
            else if (Op == TEXT("sender.name"))
            {
                Component->SetSenderName(Args->GetStringField(TEXT("name")));
                Put(Result, TEXT("name"), Args->GetStringField(TEXT("name")));
                Put(Result, TEXT("active_name"), Component->GetActiveSenderName());
            }
            else
                Error = TEXT("Unknown sender operation");
        }
        else if (Op.StartsWith(TEXT("receiver.")))
        {
            auto *Component = FindObject<UCyGameCaptureSpoutReceiverComponent>(nullptr, *Path);
            if (!IsValid(Component) || !Component->GetWorld())
                Error = TEXT("Receiver component must already exist in an open world");
            else if (Op == TEXT("receiver.start"))
            {
                const bool Started = Component->StartReceiver();
                Result->SetBoolField(TEXT("started"), Started);
                if (!Started)
                    Error = TEXT("Receiver could not start; check its source, renderer and component diagnostics");
            }
            else if (Op == TEXT("receiver.stop"))
            {
                Component->StopReceiver();
                Result->SetBoolField(TEXT("stopped"), true);
            }
            else if (Op == TEXT("receiver.sender"))
            {
                Component->SetSenderName(Args->GetStringField(TEXT("name")));
                Put(Result, TEXT("sender"), Args->GetStringField(TEXT("name")));
            }
            else
                Error = TEXT("Unknown receiver operation");
        }
        else
            Error = TEXT("Unknown operation");
    }
    if (Error.IsEmpty() && (Op == TEXT("senders.list") || Op == TEXT("streams.list") || Op == TEXT("components.list")))
    {
        const TCHAR *Key = Op == TEXT("senders.list")   ? TEXT("senders")
                           : Op == TEXT("streams.list") ? TEXT("streams")
                                                        : TEXT("components");
        Paginate(Result, Key, Args);
    }
    if (!Error.IsEmpty())
    {
        auto Details = Object();
        Put(Details, TEXT("code"), TEXT("OperationFailed"));
        Put(Details, TEXT("message"), Error);
        Result->SetObjectField(TEXT("error"), Details);
    }
    FTCHARToUTF8 Bytes(*Encode(Result));
    Reply.SetNumUninitialized(Bytes.Length() + 1);
    FMemory::Memcpy(Reply.GetData(), Bytes.Get(), Bytes.Length());
    Reply[Bytes.Length()] = 0;
    return Reply.GetData();
}
void Paginate(Json Result, const TCHAR *Key, Json Args)
{
    const auto All = Result->GetArrayField(Key);
    double OffsetValue = 0, LimitValue = 32;
    Args->TryGetNumberField(TEXT("offset"), OffsetValue);
    Args->TryGetNumberField(TEXT("limit"), LimitValue);
    const int32 Offset = static_cast<int32>(OffsetValue), Limit = static_cast<int32>(LimitValue);
    TArray<TSharedPtr<FJsonValue>> Page;
    for (int32 I = Offset; I < All.Num() && I < Offset + Limit; ++I)
        Page.Add(All[I]);
    Result->SetArrayField(Key, Page);
    Result->SetNumberField(TEXT("total"), All.Num());
    Result->SetField(TEXT("next_offset"),
                     Offset + Page.Num() < All.Num()
                         ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueNumber>(Offset + Page.Num()))
                         : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
}
Json PageSchema()
{
    auto P = Object();
    auto Offset = Schema(TEXT("integer"));
    Offset->SetNumberField(TEXT("minimum"), 0);
    Offset->SetNumberField(TEXT("maximum"), 1000000);
    P->SetObjectField(TEXT("offset"), Offset);
    auto Limit = Schema(TEXT("integer"));
    Limit->SetNumberField(TEXT("minimum"), 1);
    Limit->SetNumberField(TEXT("maximum"), 128);
    P->SetObjectField(TEXT("limit"), Limit);
    return P;
}
void Register(const FString &Name, const FString &Description, bool Read, Json Properties = Object(),
              TArray<FString> Required = {})
{
    auto Spec = Object();
    Put(Spec, TEXT("name"), Name);
    Put(Spec, TEXT("description"), Description);
    Spec->SetBoolField(TEXT("readOnly"), Read);
    auto InputSchema = Schema(TEXT("object"));
    InputSchema->SetObjectField(TEXT("properties"), Properties);
    InputSchema->SetBoolField(TEXT("additionalProperties"), false);
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const auto &Item : Required)
        Names.Add(MakeShared<FJsonValueString>(Item));
    InputSchema->SetArrayField(TEXT("required"), Names);
    Spec->SetObjectField(TEXT("inputSchema"), InputSchema);
    if (!Add(Host, TCHAR_TO_UTF8(*Encode(Spec)), Invoke, nullptr))
        CYGC_LOG(Warning, TEXT("MCP tool registration failed: %s"), *Name);
}
} // namespace
namespace CyAutomation
{
void Start()
{
#if PLATFORM_WINDOWS
    if (Host || FParse::Param(FCommandLine::Get(), TEXT("CyGameCaptureNoMCP")))
        return;
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CyGameCaptureUE"));
    if (!Plugin)
        return;
    const auto File =
        FPaths::Combine(Plugin->GetBaseDir(), TEXT("Binaries/ThirdParty/CyMCPSDK/Win64/CyGameCaptureMCP.dll"));
    Library = FPlatformProcess::GetDllHandle(*File);
    if (!Library)
    {
        CYGC_LOG(Warning, TEXT("CyMCPSDK runtime unavailable; capture remains usable"));
        return;
    }
    Create = reinterpret_cast<CreateFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_create")));
    Add = reinterpret_cast<AddFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_add")));
    Pump = reinterpret_cast<PumpFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_pump")));
    Destroy = reinterpret_cast<PumpFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_destroy")));
    Allow = reinterpret_cast<AllowFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_allow_mutations")));
    Access = reinterpret_cast<TextFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_access")));
    Connection = reinterpret_cast<TextFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_connection")));
    Send = reinterpret_cast<ChatFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_chat_send")));
    Events = reinterpret_cast<TextFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_chat_events")));
    Cancel = reinterpret_cast<PumpFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_chat_cancel")));
    Clear = reinterpret_cast<PumpFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_chat_clear")));
    LastError = reinterpret_cast<ErrorFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_error")));
    History = reinterpret_cast<TextFn>(FPlatformProcess::GetDllExport(Library, TEXT("cygc_automation_chat_history")));
    if (!Create || !Add || !Pump || !Destroy || !Allow || !Connection || !Send || !Events || !Cancel || !Clear ||
        !History || !LastError || !Access)
    {
        FPlatformProcess::FreeDllHandle(Library);
        Library = nullptr;
        return;
    }
    LoadAccessProfile();
    auto Options = Object();
    Options->SetNumberField(TEXT("access_profile"), SelectedProfile);
    Options->SetBoolField(TEXT("allow_mutations"), Control != 0);
    Options->SetBoolField(TEXT("compact"), Compact != 0);
    Options->SetBoolField(TEXT("notifications"), Notifications != 0);
    Options->SetBoolField(TEXT("performance"), Performance != 0);
    Options->SetNumberField(TEXT("cache_ttl_ms"), FMath::Max(0, CacheTTL));
    Put(Options, TEXT("component"), TEXT("unreal"));
    Put(Options, TEXT("scope"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Host = Create(TCHAR_TO_UTF8(*Encode(Options)));
    if (!Host)
        return;
    Register(TEXT("app.status"), TEXT("Read the running Unreal project's renderer and capture subsystem state."), true);
    Register(TEXT("senders.list"), TEXT("Discover actual local Spout senders visible to the Unreal subsystem."), true,
             PageSchema());
    Register(TEXT("streams.list"), TEXT("Read all real sender and receiver stream statistics."), true, PageSchema());
    Register(TEXT("components.list"),
             TEXT("Find capture component object paths in open worlds before operating on them."), true, PageSchema());
    Register(TEXT("streams.stop_all"),
             TEXT("Stop all capture streams owned by this plugin in the running Unreal instance."), false);
    for (const auto &Type : {FString(TEXT("sender")), FString(TEXT("receiver"))})
        for (const auto &Action : {FString(TEXT("start")), FString(TEXT("stop"))})
        {
            auto Props = Object();
            Props->SetObjectField(TEXT("component"), Schema(TEXT("string")));
            Register(Type + TEXT(".") + Action,
                     TEXT("Start or stop the specified existing capture component on the game thread."), false, Props,
                     {TEXT("component")});
        }
    for (const auto &Name : {FString(TEXT("sender.name")), FString(TEXT("receiver.sender"))})
    {
        auto Props = Object();
        Props->SetObjectField(TEXT("component"), Schema(TEXT("string")));
        Props->SetObjectField(TEXT("name"), Schema(TEXT("string")));
        Register(Name, TEXT("Change an existing capture component's sender name using its public API."), false, Props,
                 {TEXT("component"), TEXT("name")});
    }
    CYGC_LOG(Display, TEXT("CyMCPSDK 0.7.1 connection: %s"), UTF8_TO_TCHAR(Connection(Host)));
#endif
}
void Tick()
{
    if (!Host)
        return;
    Allow(Host, Control);
    Pump(Host);
#if WITH_EDITOR
    auto Batch = Decode(UTF8_TO_TCHAR(Events(Host)));
    if (Batch)
        ChatBusy = Batch->GetBoolField(TEXT("busy"));
    if (Batch)
        for (const auto &Item : Batch->GetArrayField(TEXT("events")))
        {
            auto Event = Item->AsObject();
            auto Data = Event->GetObjectField(TEXT("data"));
            const auto Type = Event->GetStringField(TEXT("type"));
            if (Type == TEXT("TextDelta"))
                Partial += Data->GetStringField(TEXT("text"));
            else if (Type == TEXT("HostReply"))
            {
                Transcript += TEXT("\nAssistant: ") + Data->GetStringField(TEXT("text")) + TEXT("\n");
                Partial.Empty();
            }
            else if (Type == TEXT("HostError"))
            {
                Transcript += TEXT("\nError: ") + Data->GetStringField(TEXT("message")) + TEXT("\n");
                Partial.Empty();
            }
        }
#endif
}
void Stop()
{
#if WITH_EDITOR
    if (auto Window = AssistantWindow.Pin())
    {
        if (FSlateApplication::IsInitialized())
            FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
    }
    AssistantWindow.Reset();
    Input.Reset();
#endif
    if (Host)
    {
        Destroy(Host);
        Host = nullptr;
    }
    if (Library)
    {
        FPlatformProcess::FreeDllHandle(Library);
        Library = nullptr;
    }
}
void OpenAssistant()
{
#if WITH_EDITOR
    if (!Host || !FSlateApplication::IsInitialized())
        return;
    if (auto Existing = AssistantWindow.Pin())
    {
        Existing->BringToFront();
        return;
    }
    if (Transcript.IsEmpty())
    {
        auto Saved = Decode(UTF8_TO_TCHAR(History(Host)));
        if (Saved)
            for (const auto &Item : Saved->GetArrayField(TEXT("messages")))
            {
                auto Message = Item->AsObject();
                Transcript += TEXT("\n") + Message->GetStringField(TEXT("role")) + TEXT(": ") +
                              Message->GetStringField(TEXT("text")) + TEXT("\n");
            }
    }
    auto Provider = MakeShared<FString>(TEXT("codex")), Model = MakeShared<FString>(),
         Endpoint = MakeShared<FString>(TEXT("http://127.0.0.1:1234/v1"));
    auto Attachment = MakeShared<FString>();
    auto Context = MakeShared<int32>(0);
    auto SavedSettings = Decode(UTF8_TO_TCHAR(History(Host)));
    if (SavedSettings && SavedSettings->HasTypedField<EJson::Object>(TEXT("settings")))
    {
        auto Settings = SavedSettings->GetObjectField(TEXT("settings"));
        Settings->TryGetStringField(TEXT("provider"), *Provider);
        Settings->TryGetStringField(TEXT("model"), *Model);
        Settings->TryGetStringField(TEXT("endpoint"), *Endpoint);
        double Budget = 0;
        if (Settings->TryGetNumberField(TEXT("context_bytes"), Budget))
            *Context = static_cast<int32>(Budget);
    }
    auto Window =
        SNew(SWindow).Title(FText::FromString(TEXT("CyGameCapture Assistant"))).ClientSize(FVector2D(700, 650));
    Window->SetContent(
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                              .Text(FText::FromString(
                                                  TEXT("Provider: codex or openai-compatible. Keys use "
                                                       "CYGC_AI_API_KEY. New conversation after changing provider.")))
                                              .AutoWrapText(true)] +
        SVerticalBox::Slot()
            .AutoHeight()[SNew(SEditableTextBox)
                              .Text(FText::FromString(*Provider))
                              .OnTextChanged_Lambda([Provider](const FText &Text) { *Provider = Text.ToString(); })] +
        SVerticalBox::Slot()
            .AutoHeight()[SNew(SEditableTextBox)
                              .Text_Lambda([Model] { return FText::FromString(*Model); })
                              .HintText(FText::FromString(TEXT("Exact model ID (empty = native/local default)")))
                              .OnTextChanged_Lambda([Model](const FText &Text) { *Model = Text.ToString(); })] +
        SVerticalBox::Slot()
            .AutoHeight()[SNew(SEditableTextBox)
                              .Text(FText::FromString(*Endpoint))
                              .OnTextChanged_Lambda([Endpoint](const FText &Text) { *Endpoint = Text.ToString(); })] +
        SVerticalBox::Slot().AutoHeight()
            [SNew(SEditableTextBox)
                 .HintText(FText::FromString(TEXT("Optional attachment: absolute path to an image or text file")))
                 .OnTextChanged_Lambda([Attachment](const FText &Text) { *Attachment = Text.ToString(); })] +
        SVerticalBox::Slot()
            .AutoHeight()[SNew(SEditableTextBox)
                              .HintText(FText::FromString(TEXT(
                                  "API context budget in bytes (0 = full history; CLI agents use native context)")))
                              .OnTextChanged_Lambda([Context](const FText &Text) {
                                  *Context = FMath::Clamp(FCString::Atoi(*Text.ToString()), 0, 10000000);
                              })] +
        SVerticalBox::Slot().AutoHeight()[SNew(SButton)
                                              .Text_Lambda([] {
                                                  return FText::FromString(
                                                      FString(TEXT("MCP profile (restart editor to apply): ")) +
                                                      ProfileLabel(SelectedProfile));
                                              })
                                              .OnClicked_Lambda([] {
                                                  SelectedProfile = (SelectedProfile + 1) % 4;
                                                  SaveAccessProfile();
                                                  return FReply::Handled();
                                              })] +
        SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                              .Text_Lambda([] {
                                                  if (!Host)
                                                      return FText::GetEmpty();
                                                  auto State = Decode(UTF8_TO_TCHAR(Access(Host)));
                                                  return FText::FromString(
                                                      State ? FString(TEXT("Active: ")) +
                                                                  State->GetStringField(TEXT("profile")) + TEXT(" · ") +
                                                                  State->GetStringField(TEXT("url"))
                                                            : FString());
                                              })
                                              .AutoWrapText(true)] +
        SVerticalBox::Slot().AutoHeight()
            [SNew(SCheckBox)
                 .IsEnabled_Lambda([] {
                     if (!Host)
                         return false;
                     auto State = Decode(UTF8_TO_TCHAR(Access(Host)));
                     return State && State->GetBoolField(TEXT("control_operations"));
                 })
                 .IsChecked_Lambda([] { return Control ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                 .OnCheckStateChanged_Lambda(
                     [](ECheckBoxState State) { Control = State == ECheckBoxState::Checked ? 1 : 0; })
                 .Content()[SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Allow capture mutations (controlled profiles only)")))]] +
        SVerticalBox::Slot().FillHeight(
            1)[SNew(SScrollBox) +
               SScrollBox::Slot()[SNew(STextBlock)
                                      .Text_Lambda([] { return FText::FromString(Transcript + Partial); })
                                      .AutoWrapText(true)]] +
        SVerticalBox::Slot().AutoHeight()[SAssignNew(Input, SMultiLineEditableTextBox)
                                              .HintText(FText::FromString(
                                                  TEXT("Ask about the current capture or request an action")))] +
        SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                    .Text(FText::FromString(TEXT("Send")))
                                                    .OnClicked_Lambda([Provider, Model, Endpoint, Attachment, Context] {
                                                        if (!Host || !Input)
                                                            return FReply::Handled();
                                                        auto Request = Object();
                                                        Put(Request, TEXT("provider"), *Provider);
                                                        Put(Request, TEXT("model"), *Model);
                                                        Put(Request, TEXT("endpoint"), *Endpoint);
                                                        Put(Request, TEXT("key_env"), TEXT("CYGC_AI_API_KEY"));
                                                        Put(Request, TEXT("text"), Input->GetText().ToString());
                                                        Request->SetNumberField(TEXT("context_bytes"), *Context);
                                                        if (!Attachment->IsEmpty())
                                                            Request->SetArrayField(
                                                                TEXT("attachments"),
                                                                {MakeShared<FJsonValueString>(*Attachment)});
                                                        if (Send(Host, TCHAR_TO_UTF8(*Encode(Request))))
                                                        {
                                                            Transcript += TEXT("\nYou: ") +
                                                                          Input->GetText().ToString() + TEXT("\n");
                                                            Input->SetText(FText());
                                                        }
                                                        else
                                                            Transcript += TEXT("\nError: ") +
                                                                          FString(UTF8_TO_TCHAR(LastError())) +
                                                                          TEXT("\n");
                                                        return FReply::Handled();
                                                    })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).OnClicked_Lambda([] {
                     if (Host)
                         Cancel(Host);
                     return FReply::Handled();
                 })] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("New conversation"))).OnClicked_Lambda([] {
                     if (ChatBusy)
                     {
                         Transcript += TEXT("\nCancel the current turn and wait before starting a new conversation.\n");
                         return FReply::Handled();
                     }
                     if (Host)
                         Clear(Host);
                     Transcript.Empty();
                     Partial.Empty();
                     return FReply::Handled();
                 })]] +
        SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                              .Text(FText::FromString(TEXT(
                                                  "CyMCPSDK 0.7.1 · local MCP · Free by default")))]);
    AssistantWindow = Window;
    FSlateApplication::Get().AddWindow(Window);
#endif
}
} // namespace CyAutomation
