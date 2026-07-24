#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "Interface.hpp"
#include "Module.hpp"
#include "Utils.hpp"

class BaseModUIExtension;

namespace BaseModUI {

class Panel {
public:
    virtual ~Panel() = default;

    virtual void OnOpen() {}
    virtual bool OnCommand(const char* command) { return false; }
    virtual void ApplySchemeSettings() {}
    virtual bool OnKeyCodePressed(int code) { return false; }
    virtual void OnClose() {}

    void Close();

private:
    friend class ::BaseModUIExtension;
    void* nativePanel = nullptr;
};

using PanelFactory = Panel* (*)();

struct PanelRegistration {
    const char* id = nullptr;
    const char* resourceName = nullptr;
    PanelFactory factory = nullptr;
    bool showFooter = false;
    bool useActiveControl = true;
};

bool RegisterPanel(const PanelRegistration& registration);
bool UnregisterPanel(const char* id);
bool OpenPanel(const char* id);

}

class BaseModUIExtension : public Module {
public:
    BaseModUIExtension() { this->hasLoaded = false; }
    bool Init() override;
    void Shutdown() override;
    const char* Name() override { return MODULE("client"); }

    bool Register(const BaseModUI::PanelRegistration& registration);
    bool Unregister(const char* id);
    bool Open(const char* id, void* caller);
    bool DispatchOpenCommand(void* caller, const char* command);
    void EnsureMainMenuHook();
    void Close(void* frame);

private:
    struct Registration {
        std::string resourceName;
        BaseModUI::PanelFactory factory;
        bool showFooter;
        bool useActiveControl;
    };

    struct Instance {
        BaseModUI::Panel* panel;
        Interface* nativeInterface;
        std::string id;
        int windowType;
        bool closing;
    };

    using GetPanelSingletonFn = void* (__cdecl*)();
    using AllocateFn = void* (__cdecl*)(size_t);

#ifdef _WIN32
    using FrameConstructorFn = void (__thiscall*)(void*, void*, const char*, bool, bool, bool, bool, bool);
#else
    using FrameConstructorFn = void (__cdecl*)(void*, void*, const char*, bool, bool);
#endif

    using OpenWindowFn = void* (__func*)(void*, int*, void*, bool, void*);
    using PanelHandleGetFn = void* (__func*)(void*);
    using PanelHandleSetFn = void (__func*)(void*, void*);
    using FrameBoolFn = void (__func*)(void*, bool);
    using FooterSetButtonsFn = void (__func*)(void*, int, int, int, int);
    using FooterSetButtonTextFn = void (__func*)(void*, int, const char*, bool, int);
    using ReleaseActiveMenuControlFn = void (__cdecl*)(int);

    DECL_DETOUR_T(void, MainMenuOnCommand, const char* command);
    DECL_DETOUR_T(void, PanelOnCommand, const char* command);
    DECL_DETOUR_T(void, PanelApplySchemeSettings, void* scheme);
    DECL_DETOUR_T(void, PanelOnKeyCodePressed, int code);
    DECL_DETOUR_T(void, PanelOnOpen, void* parameters);
    DECL_DETOUR_T(void, PanelClose);

    static bool ValidName(const char* value, size_t maxLength);
    static Instance* FindInstance(void* frame);
    static void DestroyStaleInstances(void* manager);

    void* HandleAddress(void* manager, int windowType) const;
    void* GetManagedFrame(void* manager, int windowType) const;
    void SetManagedFrame(void* manager, int windowType, void* frame) const;

    GetPanelSingletonFn getPanelSingleton = nullptr;
    AllocateFn allocate = nullptr;
    FrameConstructorFn frameConstructor = nullptr;
    OpenWindowFn openWindow = nullptr;
    PanelHandleGetFn panelHandleGet = nullptr;
    PanelHandleSetFn panelHandleSet = nullptr;
    FrameBoolFn setDeleteSelfOnClose = nullptr;
    ReleaseActiveMenuControlFn releaseActiveMenuControl = nullptr;
    FooterSetButtonsFn footerSetButtons = nullptr;
    FooterSetButtonTextFn footerSetButtonText = nullptr;

    Interface* mainMenuInterface = nullptr;
    bool mainMenuHookFailed = false;
    std::unordered_map<std::string, Registration> registrations;
    std::unordered_map<void*, Instance> instances;
};

extern BaseModUIExtension* basemodui;
