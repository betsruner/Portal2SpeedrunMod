#include "Menus.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "Interface.hpp"
#include "Modules/Console.hpp"
#include "Native.hpp"
#include "Utils.hpp"

namespace Menus {

using namespace Native;

namespace {
constexpr int MainMenuWindowType = 20;
// Resource/UI/BaseModUI/%s.res is formatted into a 64 byte buffer by the frame constructor.
constexpr size_t MaxResourceLength = 37;

constexpr const char* OpenCommand = "menus_open";
constexpr const char* OpenNativeCommand = "menus_open_native";
constexpr const char* BackCommand = "menus_back";

struct Registration {
    std::string resource;
    PanelFactory factory;
    bool footer;
    Layer layer;
};

struct Instance {
    Panel* panel;
    Interface* hooks;
    Layer layer;
    // vgui::PHandle, so frames the game deletes without closing them can be noticed.
    int handle;
    bool opened;
};

bool ValidName(const char* value, size_t maxLength)
{
    if (!value || !value[0] || std::strlen(value) > maxLength)
        return false;
    for (const char* c = value; *c; ++c) {
        const bool alphaNumeric = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9');
        if (!alphaNumeric && *c != '_' && *c != '-' && *c != '.')
            return false;
    }
    return true;
}

// Returns the argument if command is "<name> <argument>", otherwise nullptr.
const char* CommandArgument(const char* command, const char* name)
{
    const size_t length = std::strlen(name);
    if (std::strncmp(command, name, length) != 0 || (command[length] != ' ' && command[length] != '\t'))
        return nullptr;
    const char* argument = command + length;
    while (*argument == ' ' || *argument == '\t')
        ++argument;
    return argument;
}

int SlotFor(Layer layer)
{
    return layer == Layer::Popup ? PopupSlot : PageSlot;
}
}

class MenuSystem {
public:
    static bool loaded;
    static std::unordered_map<std::string, Registration> registrations;
    static std::unordered_map<void*, Instance> instances;
    static std::vector<void*> stacks[2];
    // Stock frame vtables whose OnCommand we patched, and the original it pointed to.
    static std::unordered_map<uintptr_t*, uintptr_t> patchedVtables;
    static uintptr_t stockOnCommand[WindowTypeCount];

    // Custom frames
    DECL_DETOUR_T(void, OnCommand, const char* command);
    DECL_DETOUR_T(void, OnOpen);
    DECL_DETOUR_T(void, OnClose);
    // Stock frames
    DECL_DETOUR_T(void, StockOnCommand, const char* command);

    static void* Manager() { return functions.getPanelSingleton(); }
    static void* SlotHandle(void* manager, int windowType) { return &Field<int>(manager, layout.frames + windowType * sizeof(int)); }
    static void* GetWindow(void* manager, int windowType) { return functions.handleGet(SlotHandle(manager, windowType)); }
    static void SetWindow(void* manager, int windowType, void* frame) { functions.handleSet(SlotHandle(manager, windowType), frame); }
    static std::vector<void*>& StackFor(Layer layer) { return stacks[layer == Layer::Popup ? 1 : 0]; }

    // Mirrors CBaseModPanel::GetActiveWindowType: the highest priority visible active frame.
    static void* ActiveWindow(void* manager)
    {
        for (int priority = WindowPriorityCount - 1; priority >= 0; --priority) {
            const int windowType = Field<int>(manager, layout.activeWindows + priority * sizeof(int));
            if (windowType <= 0 || windowType >= WindowTypeCount)
                continue;
            void* frame = GetWindow(manager, windowType);
            if (frame && Virtual<Functions::FrameIsVisible>(frame, layout.isVisible)(frame))
                return frame;
        }
        return GetWindow(manager, MainMenuWindowType);
    }

    static bool OpenPanel(const char* id, void* caller)
    {
        auto found = registrations.find(id);
        if (found == registrations.end()) {
            console->Warning("menus: No panel is registered as '%s'.\n", id);
            return false;
        }
        void* manager = Manager();
        if (!manager)
            return false;
        Prune();

        const Registration& registration = found->second;
        Panel* panel = registration.factory();
        if (!panel)
            return false;

        void* frame = functions.allocate(layout.frameSize);
        if (!frame) {
            panel->Release();
            return false;
        }
        functions.frameConstructor(frame, manager, registration.resource.c_str(), true, true, true, true, true);
        // Same constructor tail as every stock BaseModUI frame.
        Virtual<Functions::FrameBool>(frame, layout.setAutoDelete)(frame, true);
        Virtual<Functions::FrameBool>(frame, layout.setProportional)(frame, true);
        functions.setDeleteSelfOnClose(frame, true);

        const int slot = SlotFor(registration.layer);
        Field<int>(frame, layout.frameWindowPriority) = CustomWindowPriority;
        Field<int>(frame, layout.frameWindowType) = slot;
        Field<bool>(frame, layout.frameTransitionFlag) = Field<bool>(manager, layout.transitionFlag);
        Virtual<Functions::FrameBool>(frame, layout.setVisible)(frame, false);

        if (registration.footer) {
            void* footer = functions.handleGet(&Field<int>(manager, layout.footer));
            if (footer) {
                functions.footerSetButtons(footer, 2, 1, 0, 0);
                functions.footerSetButtonText(footer, 2, "#L4D360UI_Back", false, 0);
            }
        }

        Interface* hooks = Interface::Create(frame, true, true, layout.vtableSize);
        hooks->Hook(OnCommand_Hook, OnCommand, layout.onCommand);
        hooks->Hook(OnOpen_Hook, OnOpen, layout.onOpen);
        hooks->Hook(OnClose_Hook, OnClose, layout.onClose);

        panel->frame = frame;
        Instance& instance = instances.emplace(frame, Instance { panel, hooks, registration.layer, -1, false }).first->second;
        functions.handleSet(&instance.handle, frame);
        StackFor(registration.layer).push_back(frame);

        // OpenWindow only builds frames for the types it knows. With the slot already holding
        // ours it skips that and just activates it.
        SetWindow(manager, slot, frame);
        int windowType = slot;
        void* opened = functions.openWindow(manager, &windowType, caller, registration.layer == Layer::Page, nullptr);
        if (opened != frame) {
            console->Warning("menus: The game refused to open '%s'.\n", id);
            Virtual<Functions::FrameVoid>(frame, layout.onClose)(frame);
            return false;
        }
        return true;
    }

    static bool OpenNativeWindow(int windowType, void* caller)
    {
        void* manager = Manager();
        if (!manager || windowType <= 0 || windowType >= WindowTypeCount || windowType == PageSlot || windowType == PopupSlot)
            return false;
        return functions.openWindow(manager, &windowType, caller, true, nullptr) != nullptr;
    }

    static void NavigateBack(void* frame)
    {
        Virtual<Functions::FrameVoid>(frame, layout.navigateBack)(frame);
    }

    static bool DispatchCommand(void* caller, const char* command)
    {
        if (!command)
            return false;
        if (const char* id = CommandArgument(command, OpenCommand)) {
            if (ValidName(id, 63))
                OpenPanel(id, caller);
            return true;
        }
        if (const char* windowType = CommandArgument(command, OpenNativeCommand)) {
            OpenNativeWindow(std::atoi(windowType), caller);
            return true;
        }
        if (!std::strcmp(command, BackCommand)) {
            NavigateBack(caller);
            return true;
        }
        return false;
    }

    static void Destroy(void* frame, bool frameAlive)
    {
        auto found = instances.find(frame);
        if (found == instances.end())
            return;
        Instance instance = found->second;
        instances.erase(found);

        instance.panel->OnClose();
        instance.panel->frame = nullptr;
        instance.panel->Release();
        if (!frameAlive)
            instance.hooks->Forget();
        Interface::Delete(instance.hooks);
    }

    // Forgets frames the game deleted without closing them.
    static void Prune()
    {
        std::vector<void*> dead;
        for (auto& item : instances) {
            if (!functions.handleGet(&item.second.handle))
                dead.push_back(item.first);
        }
        for (void* frame : dead) {
            for (auto& stack : stacks)
                stack.erase(std::remove(stack.begin(), stack.end(), frame), stack.end());
            Destroy(frame, false);
        }
    }

    static void CloseAll()
    {
        for (auto& stack : stacks) {
            const std::vector<void*> frames = stack;
            for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
                if (instances.count(*it))
                    Virtual<Functions::FrameVoid>(*it, layout.onClose)(*it);
            }
        }
    }

    // Lets "menus_open" work from stock panels. Their OnCommand overrides swallow commands they
    // don't know (Options does), so each class's vtable is patched the first time one is seen.
    static void PatchStockWindows(void* manager)
    {
        for (int windowType = 0; windowType < WindowTypeCount; ++windowType) {
            void* frame = GetWindow(manager, windowType);
            if (!frame || instances.count(frame))
                continue;

            uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(frame);
            auto patched = patchedVtables.find(vtable);
            if (patched != patchedVtables.end()) {
                stockOnCommand[windowType] = patched->second;
                continue;
            }

            uintptr_t* entry = &vtable[layout.onCommand];
            Memory::UnProtect(entry, sizeof(uintptr_t));
            patchedVtables[vtable] = stockOnCommand[windowType] = *entry;
            *entry = reinterpret_cast<uintptr_t>(StockOnCommand_Hook);
        }
    }

    static void RestoreStockWindows()
    {
        for (auto& item : patchedVtables)
            item.first[layout.onCommand] = item.second;
        patchedVtables.clear();
        std::fill(std::begin(stockOnCommand), std::end(stockOnCommand), 0);
    }
};

bool MenuSystem::loaded = false;
std::unordered_map<std::string, Registration> MenuSystem::registrations;
std::unordered_map<void*, Instance> MenuSystem::instances;
std::vector<void*> MenuSystem::stacks[2];
std::unordered_map<uintptr_t*, uintptr_t> MenuSystem::patchedVtables;
uintptr_t MenuSystem::stockOnCommand[WindowTypeCount] = {};

REDECL(MenuSystem::OnCommand);
REDECL(MenuSystem::OnOpen);
REDECL(MenuSystem::OnClose);
REDECL(MenuSystem::StockOnCommand);

DETOUR_T(void, MenuSystem::OnCommand, const char* command)
{
    if (DispatchCommand(thisptr, command))
        return;
    auto found = instances.find(thisptr);
    if (found != instances.end() && found->second.panel->OnCommand(command))
        return;
    OnCommand(thisptr, command);
}

DETOUR_T(void, MenuSystem::OnOpen)
{
    OnOpen(thisptr);
    auto found = instances.find(thisptr);
    if (found == instances.end())
        return;
    if (!found->second.opened) {
        found->second.opened = true;
        found->second.panel->OnOpen();
    }
    found->second.panel->OnActivate();
}

DETOUR_T(void, MenuSystem::OnClose)
{
    auto found = instances.find(thisptr);
    void* manager = Manager();
    if (found == instances.end() || !manager) {
        OnClose(thisptr);
        Destroy(thisptr, true);
        return;
    }

    const Layer layer = found->second.layer;
    const int slot = SlotFor(layer);
    auto& stack = StackFor(layer);
    stack.erase(std::remove(stack.begin(), stack.end(), thisptr), stack.end());

    bool& closingAll = Field<bool>(manager, layout.closingAllWindows);
    const bool wasClosingAll = closingAll;
    void* owner = GetWindow(manager, slot);
    if (owner == thisptr || wasClosingAll) {
        OnClose(thisptr);
    } else {
        // A hidden panel closing on its own. OnFrameClosed would clear the slot and the active
        // window out from under the panel that owns them, unless it thinks everything is closing.
        SetWindow(manager, slot, thisptr);
        closingAll = true;
        OnClose(thisptr);
        closingAll = false;
        SetWindow(manager, slot, owner);
    }
    Destroy(thisptr, true);

    if (wasClosingAll) {
        // CloseAllWindows only reaches the panel bound to each slot; close the hidden ones too.
        CloseAll();
    } else if (owner == thisptr && !stack.empty()) {
        // NavigateBack activates the previous panel right after this returns, through the slot.
        SetWindow(manager, slot, stack.back());
    }
}

DETOUR_T(void, MenuSystem::StockOnCommand, const char* command)
{
    if (DispatchCommand(thisptr, command)) {
        functions.releaseActiveMenuControl(0);
        return;
    }

    uintptr_t original = 0;
    auto patched = patchedVtables.find(*reinterpret_cast<uintptr_t**>(thisptr));
    if (patched != patchedVtables.end()) {
        original = patched->second;
    } else {
        // Someone gave this frame its own vtable copy after we patched its class.
        const int windowType = Field<int>(thisptr, layout.frameWindowType);
        if (windowType >= 0 && windowType < WindowTypeCount)
            original = stockOnCommand[windowType];
    }
    if (original)
        reinterpret_cast<_StockOnCommand>(original)(thisptr, command);
}

void Panel::Close()
{
    if (this->frame && MenuSystem::loaded)
        MenuSystem::NavigateBack(this->frame);
}

bool Init()
{
    if (MenuSystem::loaded)
        return true;
    if (!Native::Resolve(MODULE("client"))) {
        console->Warning("menus: BaseModUI integration is unavailable on this game build; custom panels are disabled.\n");
        return false;
    }
    return MenuSystem::loaded = true;
}

void Shutdown()
{
    if (!MenuSystem::loaded)
        return;
    MenuSystem::CloseAll();
    MenuSystem::RestoreStockWindows();
    MenuSystem::registrations.clear();
    MenuSystem::loaded = false;
}

bool IsAvailable()
{
    return MenuSystem::loaded;
}

void Tick()
{
    if (!MenuSystem::loaded)
        return;
    void* manager = MenuSystem::Manager();
    if (!manager)
        return;
    MenuSystem::Prune();
    MenuSystem::PatchStockWindows(manager);
}

bool Register(const PanelDesc& desc)
{
    const char* resource = desc.resource ? desc.resource : desc.id;
    if (!MenuSystem::loaded || !ValidName(desc.id, 63) || !ValidName(resource, MaxResourceLength) || !desc.factory)
        return false;
    return MenuSystem::registrations.emplace(desc.id, Registration { resource, desc.factory, desc.footer, desc.layer }).second;
}

bool Unregister(const char* id)
{
    return id && MenuSystem::registrations.erase(id) != 0;
}

bool Open(const char* id)
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    return manager && id && MenuSystem::OpenPanel(id, MenuSystem::ActiveWindow(manager));
}

bool OpenNative(int windowType)
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    return manager && MenuSystem::OpenNativeWindow(windowType, MenuSystem::ActiveWindow(manager));
}

bool Back()
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    void* active = manager ? MenuSystem::ActiveWindow(manager) : nullptr;
    if (!active)
        return false;
    MenuSystem::NavigateBack(active);
    return true;
}

void Dump()
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    if (!manager) {
        console->Print("menus: unavailable\n");
        return;
    }

    console->Print("menus: active window %p, closing all %d\n", MenuSystem::ActiveWindow(manager), Field<bool>(manager, layout.closingAllWindows));
    for (int priority = 0; priority < WindowPriorityCount; ++priority) {
        const int windowType = Field<int>(manager, layout.activeWindows + priority * sizeof(int));
        if (windowType)
            console->Print("  active[%d] = type %d\n", priority, windowType);
    }
    for (int windowType = 0; windowType < WindowTypeCount; ++windowType) {
        void* frame = MenuSystem::GetWindow(manager, windowType);
        if (frame)
            console->Print("  slot %2d: %p%s\n", windowType, frame, MenuSystem::instances.count(frame) ? " (custom)" : "");
    }
    const char* names[] = { "pages", "popups" };
    for (int layer = 0; layer < 2; ++layer) {
        console->Print("  %s:", names[layer]);
        for (void* frame : MenuSystem::stacks[layer])
            console->Print(" %p", frame);
        console->Print("\n");
    }
}

}
