#pragma once

#include <cstddef>
#include <cstdint>

#include "Utils/Platform.hpp"

// Everything the menu system knows about Portal 2's BaseModUI (CBaseModPanel / CBaseModFrame).
namespace Menus::Native {

constexpr int WindowTypeCount = 76;
constexpr int WindowPriorityCount = 7;
// WINDOW_TYPE values CBaseModPanel::OpenWindow has no case for. Custom frames borrow them.
constexpr int PageSlot = 25;
constexpr int PopupSlot = 27;
constexpr int CustomWindowPriority = 2;

struct Layout {
    // CBaseModFrame
    int frameSize;
    int vtableSize;
    int frameWindowType;
    int frameWindowPriority;
    int frameTransitionFlag;
    // CBaseModPanel
    int frames;            // vgui::PHandle m_Frames[WindowTypeCount]
    int activeWindows;     // WINDOW_TYPE m_ActiveWindow[WindowPriorityCount]
    int closingAllWindows; // bool, set while CloseAllWindows runs
    int footer;            // vgui::PHandle to CBaseModFooterPanel
    int transitionFlag;
    // CBaseModFrame virtuals
    int setVisible;
    int isVisible;
    int onCommand;
    int setProportional;
    int onClose;
    int setAutoDelete;
    int onOpen;
    int navigateBack;
};

struct Functions {
    using GetPanelSingleton = void*(__cdecl*)();
    using Allocate = void*(__cdecl*)(size_t);
    using FrameConstructor = void(__func*)(void* frame, void* parent, const char* name, bool okButton, bool cancelButton, bool splatter, bool doButton, bool mostDerived);
    // Localization token (or plain text if it isn't one), or a wide string when the token is null.
    using SetDialogTitle = void(__func*)(void* frame, const char* token, const wchar_t* text, int, int, int, int);
    using OpenWindow = void*(__func*)(void* manager, int* windowType, void* caller, bool hidePrevious, void* parameters);
    using HandleGet = void*(__func*)(void* handle);
    using HandleSet = void(__func*)(void* handle, void* panel);
    using FrameBool = void(__func*)(void* frame, bool value);
    using FrameVoid = void(__func*)(void* frame);
    using FrameIsVisible = bool(__func*)(void* frame);
    using FooterSetButtons = void(__func*)(void* footer, int, int, int, int);
    using FooterSetButtonText = void(__func*)(void* footer, int, const char*, bool, int);
    using ReleaseActiveMenuControl = void(__cdecl*)(int);

    GetPanelSingleton getPanelSingleton = nullptr;
    Allocate allocate = nullptr;
    FrameConstructor frameConstructor = nullptr;
    SetDialogTitle setDialogTitle = nullptr;
    OpenWindow openWindow = nullptr;
    HandleGet handleGet = nullptr;
    HandleSet handleSet = nullptr;
    FrameBool setDeleteSelfOnClose = nullptr;
    FooterSetButtons footerSetButtons = nullptr;
    FooterSetButtonText footerSetButtonText = nullptr;
    ReleaseActiveMenuControl releaseActiveMenuControl = nullptr;
};

extern Layout layout;
extern Functions functions;

// Resolves every signature. Returns false (and leaves the menu system disabled) if anything is missing.
bool Resolve(const char* clientModule);

template <typename T>
inline T& Field(void* object, int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}

template <typename T>
inline T Virtual(void* object, int index)
{
    return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[index]);
}

}
