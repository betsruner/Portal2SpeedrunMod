#include "BaseModUI.hpp"

#include <cctype>
#include <cstring>

#include "Console.hpp"
#include "Offsets.hpp"

namespace {
constexpr int CustomWindowTypes[] = { 25, 27 };
constexpr int CustomWindowPriority = 2;
constexpr int MainMenuWindowType = 20;

#ifdef _WIN32
constexpr const char* GetPanelSingletonSignature = "A1 ? ? ? ? C3 CC CC CC CC CC CC CC CC CC CC 8A 81";
constexpr const char* AllocateSignature = "55 8B EC A1 ? ? ? ? 8B 08 8B 11 8B 45 ? 8B 52 ? 50 FF D2 5D C3 CC CC CC CC CC CC CC CC CC 55 8B EC 53";
constexpr const char* FrameConstructorSignature = "55 8B EC 53 56 33 DB 57 8B F1 39 5D ? 74 ? C7 46 ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? 8B 45 ? 8B 4D ? 53 53 6A";
constexpr const char* OpenWindowSignature = "55 8B EC 83 EC ? 53 56 8B F1 F3 0F 10 86 ? ? ? ? 0F 2E 05";
constexpr const char* PanelHandleGetSignature = "8B 01 56 83 F8 ? 74 ? 8B 0D";
constexpr const char* PanelHandleSetSignature = "55 8B EC 51 56 8B 75 ? 89 4D ? 85 F6 74";
constexpr const char* ReleaseActiveMenuControlSignature = "55 8B EC 8B 0D ? ? ? ? 85 C9 74 ? 8B 01 8B 50 ? FF D2";
constexpr const char* SetDeleteSelfOnCloseSignature = "55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 56";
constexpr const char* FooterSetButtonsSignature = "55 8B EC 8B 45 ? 8B D0 C1 E2 ? 2B D0 8D 84 D1 ? ? ? ? 8B 55 ? 89 50 ? 8B 55 ? 89 50 ? 8B 55";
constexpr const char* FooterSetButtonTextSignature = "55 8B EC 8B 45 ? 53 8B D9 56";
#else
constexpr const char* GetPanelSingletonSignature = "A1 ? ? ? ? C3 8D B4 26 ? ? ? ? 8D 76 ? A1 ? ? ? ? C3 8D B4 26 ? ? ? ? 8D 76 ? C3 90 8D B4 26 ? ? ? ? 8D B4 26 ? ? ? ? 8B 44 24 ?";
constexpr const char* AllocateSignature = "83 EC ? A1 ? ? ? ? 8B 10 FF 74 24 ? 50 FF 12 83 C4 ? C3 8D B4 26 ? ? ? ? 8D 74 26 ? A1 ? ? ? ? 8B 4C 24 ? 8B 10 89 44 24 ? 89 4C 24 ? FF 62 ? 8D B4 26 ? ? ? ? 8D 76 ? 83 EC ? A1 ? ? ? ? 8B 10 FF 74 24 ? 50 FF 52 ? 83 C4 ? C3 8D B4 26 ? ? ? ? 8D 76 ? 83 EC";
constexpr const char* FrameConstructorSignature = "55 89 E5 57 56 53 83 EC ? 8B 5D ? 8B 45 ? 8B 7D ? C7 03 ? ? ? ? 6A";
constexpr const char* OpenWindowSignature = "55 66 0F EF C0 89 E5 57 56 53 83 EC ? 8B 75";
constexpr const char* PanelHandleGetSignature = "57 56 53 8B 44 24 ? 8B 00";
constexpr const char* PanelHandleSetSignature = "57 56 53 8B 5C 24 ? 85 DB 74 ? 8B 35";
constexpr const char* ReleaseActiveMenuControlSignature = "A1 ? ? ? ? 85 C0 74 ? 83 EC ? 8B 10 50 FF 92 ? ? ? ? 83 C4 ? 84 C0 75 ? 83 C4";
constexpr const char* SetDeleteSelfOnCloseSignature = "8B 44 24 ? 8B 54 24 ? 88 90 ? ? ? ? C3 90 83 EC";
constexpr const char* FooterSetButtonsSignature = "8B 54 24 ? 8B 4C 24 ? 6B 44 24 ? ? 01 D0 89 88 ? ? ? ? 8B 4C 24 ? 89 88 ? ? ? ? 8B 4C 24";
constexpr const char* FooterSetButtonTextSignature = "55 57 56 53 83 EC ? 8B 5C 24 ? 8B 7C 24 ? 8B 44 24 ? 8B 6C 24 ? 8B 74 24";
#endif
}

BaseModUIExtension* basemodui = nullptr;

REDECL(BaseModUIExtension::MainMenuOnCommand);
REDECL(BaseModUIExtension::PanelOnCommand);
REDECL(BaseModUIExtension::PanelApplySchemeSettings);
REDECL(BaseModUIExtension::PanelOnKeyCodePressed);
REDECL(BaseModUIExtension::PanelOnOpen);
REDECL(BaseModUIExtension::PanelClose);

bool BaseModUI::RegisterPanel(const PanelRegistration& registration)
{
    return basemodui && basemodui->Register(registration);
}

bool BaseModUI::UnregisterPanel(const char* id)
{
    return basemodui && basemodui->Unregister(id);
}

bool BaseModUI::OpenPanel(const char* id)
{
    return basemodui && basemodui->Open(id, nullptr);
}

void BaseModUI::Panel::Close()
{
    if (basemodui && this->nativePanel)
        basemodui->Close(this->nativePanel);
}

bool BaseModUIExtension::ValidName(const char* value, size_t maxLength)
{
    if (!value || !value[0])
        return false;

    const size_t length = std::strlen(value);
    if (length > maxLength)
        return false;

    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        const bool asciiAlphaNumeric = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9');
        if (!asciiAlphaNumeric && c != '_' && c != '-' && c != '.')
            return false;
    }
    return true;
}

bool BaseModUIExtension::Register(const BaseModUI::PanelRegistration& registration)
{
    if (!this->hasLoaded || !ValidName(registration.id, 63) || !registration.factory)
        return false;

    const char* resourceName = registration.resourceName ? registration.resourceName : registration.id;
    if (!ValidName(resourceName, 37))
        return false;

    Registration stored {
        resourceName,
        registration.factory,
        registration.showFooter,
        registration.useActiveControl,
    };
    return this->registrations.emplace(registration.id, std::move(stored)).second;
}

bool BaseModUIExtension::Unregister(const char* id)
{
    return id && this->registrations.erase(id) != 0;
}

bool BaseModUIExtension::Init()
{
    this->getPanelSingleton = reinterpret_cast<GetPanelSingletonFn>(Memory::Scan(this->Name(), GetPanelSingletonSignature));
    this->allocate = reinterpret_cast<AllocateFn>(Memory::Scan(this->Name(), AllocateSignature));
    this->frameConstructor = reinterpret_cast<FrameConstructorFn>(Memory::Scan(this->Name(), FrameConstructorSignature));
    this->openWindow = reinterpret_cast<OpenWindowFn>(Memory::Scan(this->Name(), OpenWindowSignature));
    this->panelHandleGet = reinterpret_cast<PanelHandleGetFn>(Memory::Scan(this->Name(), PanelHandleGetSignature));
    this->panelHandleSet = reinterpret_cast<PanelHandleSetFn>(Memory::Scan(this->Name(), PanelHandleSetSignature));
    this->setDeleteSelfOnClose = reinterpret_cast<FrameBoolFn>(Memory::Scan(this->Name(), SetDeleteSelfOnCloseSignature));
    this->releaseActiveMenuControl = reinterpret_cast<ReleaseActiveMenuControlFn>(Memory::Scan(this->Name(), ReleaseActiveMenuControlSignature));
    this->footerSetButtons = reinterpret_cast<FooterSetButtonsFn>(Memory::Scan(this->Name(), FooterSetButtonsSignature));
    this->footerSetButtonText = reinterpret_cast<FooterSetButtonTextFn>(Memory::Scan(this->Name(), FooterSetButtonTextSignature));

    if (!this->getPanelSingleton || !this->allocate || !this->frameConstructor || !this->openWindow
        || !this->panelHandleGet || !this->panelHandleSet || !this->setDeleteSelfOnClose
        || !this->releaseActiveMenuControl || !this->footerSetButtons || !this->footerSetButtonText) {
        console->Warning("smsm: Native BaseModUI integration did not resolve; custom panels are disabled.\n");
        return this->hasLoaded = false;
    }

    this->hasLoaded = true;
    this->EnsureMainMenuHook();
    return true;
}

void BaseModUIExtension::Shutdown()
{
    Interface::Delete(this->mainMenuInterface);
    this->mainMenuInterface = nullptr;

    void* manager = this->getPanelSingleton ? this->getPanelSingleton() : nullptr;

    for (auto& item : this->instances) {
        void* frame = item.first;
        Instance& instance = item.second;
        if (!instance.closing)
            instance.panel->OnClose();
        instance.panel->nativePanel = nullptr;
        delete instance.panel;

        const bool isManaged = manager && this->GetManagedFrame(manager, instance.windowType) == frame;
        Interface::Delete(instance.nativeInterface);
        instance.nativeInterface = nullptr;
        if (isManaged && frame && !instance.closing && PanelClose)
            PanelClose(frame);
    }

    this->instances.clear();
    this->registrations.clear();
}

void BaseModUIExtension::EnsureMainMenuHook()
{
    if (!this->hasLoaded || this->mainMenuInterface || this->mainMenuHookFailed)
        return;

    void* manager = this->getPanelSingleton();
    void* mainMenu = manager ? this->GetManagedFrame(manager, MainMenuWindowType) : nullptr;
    if (!mainMenu)
        return;

    Interface* nativeInterface = Interface::Create(mainMenu, true, true, Offsets::BaseModUIVtableSize);
    if (!nativeInterface
        || !nativeInterface->Hook(MainMenuOnCommand_Hook, MainMenuOnCommand, Offsets::BaseModUIOnCommand)) {
        Interface::Delete(nativeInterface);
        this->mainMenuHookFailed = true;
        console->Warning("smsm: Failed to hook the native BaseModUI main menu; resource commands are disabled.\n");
        return;
    }
    this->mainMenuInterface = nativeInterface;
}

void* BaseModUIExtension::HandleAddress(void* manager, int windowType) const
{
    return static_cast<unsigned char*>(manager) + Offsets::BaseModUIManagedHandle + windowType * sizeof(int);
}

void* BaseModUIExtension::GetManagedFrame(void* manager, int windowType) const
{
    return this->panelHandleGet(this->HandleAddress(manager, windowType));
}

void BaseModUIExtension::SetManagedFrame(void* manager, int windowType, void* frame) const
{
    this->panelHandleSet(this->HandleAddress(manager, windowType), frame);
}

BaseModUIExtension::Instance* BaseModUIExtension::FindInstance(void* frame)
{
    if (!basemodui)
        return nullptr;
    auto found = basemodui->instances.find(frame);
    return found == basemodui->instances.end() ? nullptr : &found->second;
}

void BaseModUIExtension::DestroyStaleInstances(void* manager)
{
    for (auto it = basemodui->instances.begin(); it != basemodui->instances.end();) {
        Instance& instance = it->second;
        if (basemodui->GetManagedFrame(manager, instance.windowType)) {
            ++it;
            continue;
        }

        if (!instance.closing)
            instance.panel->OnClose();
        instance.panel->nativePanel = nullptr;
        delete instance.panel;
        it = basemodui->instances.erase(it);
    }
}

bool BaseModUIExtension::Open(const char* id, void* caller)
{
    if (!this->hasLoaded || !id)
        return false;

    auto registration = this->registrations.find(id);
    if (registration == this->registrations.end())
        return false;

    void* manager = this->getPanelSingleton();
    if (!manager)
        return false;

    DestroyStaleInstances(manager);

    int windowType = 0;
    for (int candidate : CustomWindowTypes) {
        if (!this->GetManagedFrame(manager, candidate)) {
            windowType = candidate;
            break;
        }
    }
    if (!windowType) {
        console->Warning("smsm: Cannot open BaseModUI panel '%s': native custom window slots are in use.\n", id);
        return false;
    }

    BaseModUI::Panel* panel = registration->second.factory();
    if (!panel)
        return false;

    void* frame = this->allocate(Offsets::BaseModUIFrameSize);
    if (!frame) {
        delete panel;
        return false;
    }

#ifdef _WIN32
    this->frameConstructor(frame, manager, registration->second.resourceName.c_str(),
        registration->second.showFooter, registration->second.useActiveControl, true, true, true);
#else
    this->frameConstructor(frame, manager, registration->second.resourceName.c_str(),
        registration->second.useActiveControl, registration->second.showFooter);
#endif

    void** frameVtable = *reinterpret_cast<void***>(frame);
    // Mirror the constructor tail shared by every stock derived BaseMod frame.
    reinterpret_cast<FrameBoolFn>(frameVtable[Offsets::BaseModUISetAutoDelete])(frame, true);
    reinterpret_cast<FrameBoolFn>(frameVtable[Offsets::BaseModUISetProportional])(frame, true);
    this->setDeleteSelfOnClose(frame, true);

    // Pre-populating the manager handle bypasses OpenWindow's new-frame block.
    auto frameBytes = static_cast<unsigned char*>(frame);
    auto managerBytes = static_cast<unsigned char*>(manager);
    *reinterpret_cast<int*>(frameBytes + Offsets::BaseModUIFrameWindowPriority) = CustomWindowPriority;
    *reinterpret_cast<int*>(frameBytes + Offsets::BaseModUIFrameWindowType) = windowType;
    frameBytes[Offsets::BaseModUIFrameTransitionFlag] = managerBytes[Offsets::BaseModUIManagerTransitionFlag];
    reinterpret_cast<FrameBoolFn>(frameVtable[Offsets::BaseModUISetVisible])(frame, false);

    if (registration->second.showFooter) {
        void* footer = this->panelHandleGet(managerBytes + Offsets::BaseModUIFooterHandle);
        if (footer) {
            this->footerSetButtons(footer, 2, 1, 0, 0);
            this->footerSetButtonText(footer, 2, "#L4D360UI_Back", false, 0);
        }
    }

    Interface* nativeInterface = Interface::Create(frame, true, true, Offsets::BaseModUIVtableSize);
    const bool hooked = nativeInterface
        && nativeInterface->Hook(PanelApplySchemeSettings_Hook, PanelApplySchemeSettings, Offsets::BaseModUIApplySchemeSettings)
        && nativeInterface->Hook(PanelOnCommand_Hook, PanelOnCommand, Offsets::BaseModUIOnCommand)
        && nativeInterface->Hook(PanelOnKeyCodePressed_Hook, PanelOnKeyCodePressed, Offsets::BaseModUIOnKeyCodePressed)
        && nativeInterface->Hook(PanelOnOpen_Hook, PanelOnOpen, Offsets::BaseModUIOnOpen)
        && nativeInterface->Hook(PanelClose_Hook, PanelClose, Offsets::BaseModUIClose);
    if (!hooked) {
        Interface::Delete(nativeInterface);
        delete panel;
        reinterpret_cast<_PanelClose>(frameVtable[Offsets::BaseModUIClose])(frame);
        return false;
    }

    panel->nativePanel = frame;
    this->instances.emplace(frame, Instance { panel, nativeInterface, id, windowType, false });
    this->SetManagedFrame(manager, windowType, frame);

    int nativeWindowType = windowType;
    void* opened = this->openWindow(manager, &nativeWindowType, caller, true, nullptr);
    if (opened != frame) {
        if (this->GetManagedFrame(manager, windowType) == frame)
            this->SetManagedFrame(manager, windowType, nullptr);
        auto instance = this->instances.find(frame);
        if (instance != this->instances.end()) {
            instance->second.panel->nativePanel = nullptr;
            delete instance->second.panel;
            Interface::Delete(instance->second.nativeInterface);
            this->instances.erase(instance);
        }
        PanelClose(frame);
        return false;
    }
    return true;
}

bool BaseModUIExtension::DispatchOpenCommand(void* caller, const char* command)
{
    if (!command || std::strncmp(command, "smsm_open", 9) != 0)
        return false;

    const char* id = command + 9;
    if (*id != ' ' && *id != '\t')
        return false;
    while (*id == ' ' || *id == '\t')
        ++id;

    const char* end = id + std::strlen(id);
    while (end > id && (end[-1] == ' ' || end[-1] == '\t'))
        --end;
    std::string stableId(id, end);
    if (!ValidName(stableId.c_str(), 63))
        return true;

    if (!this->Open(stableId.c_str(), caller))
        console->Warning("smsm: Unknown or unavailable BaseModUI panel '%s'.\n", stableId.c_str());
    return true;
}

void BaseModUIExtension::Close(void* frame)
{
    Instance* instance = FindInstance(frame);
    if (!instance || instance->closing)
        return;
    instance->closing = true;
    instance->panel->OnClose();
    Interface::Delete(instance->nativeInterface);
    instance->nativeInterface = nullptr;
    PanelClose(frame);
}

DETOUR_T(void, BaseModUIExtension::MainMenuOnCommand, const char* command)
{
    if (basemodui && basemodui->DispatchOpenCommand(thisptr, command)) {
        basemodui->releaseActiveMenuControl(0);
        return;
    }
    MainMenuOnCommand(thisptr, command);
}

DETOUR_T(void, BaseModUIExtension::PanelOnCommand, const char* command)
{
    Instance* instance = FindInstance(thisptr);
    if (basemodui && basemodui->DispatchOpenCommand(thisptr, command))
        return;
    if (instance && instance->panel->OnCommand(command))
        return;
    PanelOnCommand(thisptr, command);
}

DETOUR_T(void, BaseModUIExtension::PanelApplySchemeSettings, void* scheme)
{
    PanelApplySchemeSettings(thisptr, scheme);
    Instance* instance = FindInstance(thisptr);
    if (instance)
        instance->panel->ApplySchemeSettings();
}

DETOUR_T(void, BaseModUIExtension::PanelOnKeyCodePressed, int code)
{
    Instance* instance = FindInstance(thisptr);
    if (!instance || !instance->panel->OnKeyCodePressed(code))
        PanelOnKeyCodePressed(thisptr, code);
}

DETOUR_T(void, BaseModUIExtension::PanelOnOpen, void* parameters)
{
    PanelOnOpen(thisptr, parameters);
    Instance* instance = FindInstance(thisptr);
    if (instance)
        instance->panel->OnOpen();
}

DETOUR_T(void, BaseModUIExtension::PanelClose)
{
    if (basemodui)
        basemodui->Close(thisptr);
}
