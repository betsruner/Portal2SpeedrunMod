#pragma once

#include <cstddef>
#include <cstdint>

#include "Support.hpp"

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
    int applySchemeSettings;
    int onCommand;
    int onKeyCodePressed;
    int setProportional;
    int onClose;
    int setAutoDelete;
    int onOpen;
    int navigateBack;
    // vgui::Panel
    int panelName; // CUtlString; the .res root block only applies to a panel whose name matches it
    int getParent;
    int setBgColor;
    int setFgColor;
    int onMousePressed;
    int paint;
    // ISteamFriends017
    int steamFriendsRequestUserInformation;
    // vgui::IPanel
    int panelGetSize;
    int panelGetAbsPos;
    int panelSetParent;
    int panelGetChildCount;
    int panelGetChild;
    int panelGetPanel;
    // vgui::ISurface, vgui::IInput, vgui::IScheme
    int surfaceDrawSetColor;
    int surfaceDrawFilledRect;
    int surfaceGetFontTall;
    int surfaceDrawColoredText;
    int surfaceGetTextSize;
    int inputGetCursorPos;
    int schemeGetFont;
    // vgui::EditablePanel / vgui::Label / vgui::ImagePanel virtuals
    int loadControlSettings;
    int createControlByName;
    int labelSetText;
    int imagePanelSetImage; // SetImage(IImage*)
    // BaseModUI::GenericPanelList
    int listSize;
    int listSelectedItem; // Panel* of the selected item
    int listAddItem;
    int listRemoveAllItems;
    int listSelectItem;
    int listSetScrollBarVisible;
    int listSetScrollArrowsVisible;
};

struct Functions {
    using GetPanelSingleton = void*(MENUS_CDECL*)();
    using Allocate = void*(MENUS_CDECL*)(size_t);
    using FrameConstructor = void(MENUS_THISCALL*)(void* frame, void* parent, const char* name, bool okButton, bool cancelButton, bool splatter, bool doButton, bool mostDerived);
    // Localization token (or plain text if it isn't one), or a wide string when the token is null.
    using SetDialogTitle = void(MENUS_THISCALL*)(void* frame, const char* token, const wchar_t* text, int, int, int, int);
    using OpenWindow = void*(MENUS_THISCALL*)(void* manager, int* windowType, void* caller, bool hidePrevious, void* parameters);
    using HandleGet = void*(MENUS_THISCALL*)(void* handle);
    using HandleSet = void(MENUS_THISCALL*)(void* handle, void* panel);
    using FrameBool = void(MENUS_THISCALL*)(void* frame, bool value);
    using FrameVoid = void(MENUS_THISCALL*)(void* frame);
    using FrameIsVisible = bool(MENUS_THISCALL*)(void* frame);
    using FrameInt = void(MENUS_THISCALL*)(void* frame, int value);
    using FrameVoidPointer = void(MENUS_THISCALL*)(void* frame, void* value);
    using PanelGetParent = void*(MENUS_THISCALL*)(void* panel);
    using UtlStringSet = void(MENUS_THISCALL*)(void* string, const char* value);
    using GetVPanel = uintptr_t(MENUS_THISCALL*)(void* clientPanel);
    using PanelGetInts = void(MENUS_THISCALL*)(void* panelInterface, uintptr_t vpanel, int& a, int& b);
    using PanelSetParent = void(MENUS_THISCALL*)(void* panelInterface, uintptr_t vpanel, uintptr_t parent);
    using SurfaceDrawSetColor = void(MENUS_THISCALL*)(void* surface, int r, int g, int b, int a);
    using SurfaceDrawFilledRect = void(MENUS_THISCALL*)(void* surface, int x0, int y0, int x1, int y1);
    using SurfaceGetFontTall = int(MENUS_THISCALL*)(void* surface, unsigned long font);
    using SurfaceDrawColoredText = int(MENUS_CDECL*)(void* surface, unsigned long font, int x, int y, int r, int g, int b, int a, const char* format, ...);
    using SurfaceGetTextSize = void(MENUS_THISCALL*)(void* surface, unsigned long font, const wchar_t* text, int& wide, int& tall);
    using InputGetCursorPos = void(MENUS_THISCALL*)(void* input, int& x, int& y);
    using SchemeGetFont = unsigned long(MENUS_THISCALL*)(void* scheme, const char* name, bool proportional);
    using PanelGetChildCount = int(MENUS_THISCALL*)(void* panelInterface, uintptr_t vpanel);
    using PanelGetChild = uintptr_t(MENUS_THISCALL*)(void* panelInterface, uintptr_t vpanel, int index);
    using PanelGetPanel = void*(MENUS_THISCALL*)(void* panelInterface, uintptr_t vpanel, const char* moduleName);
    using ControlsModuleName = const char*(MENUS_CDECL*)();
    using GetUIGameData = void*(MENUS_CDECL*)();
    // mode 1 adds a reference (loading the avatar if needed), mode 2 releases one.
    using AccessAvatarImage = void*(MENUS_THISCALL*)(void* uiGameData, uint64_t steamId, int mode, int size);
    using RequestUserInformation = bool(MENUS_THISCALL*)(void* steamFriends, uint64_t steamId, bool nameOnly);
    using FindChildByName = void*(MENUS_THISCALL*)(void* panel, const char* name, bool recurse);
    using LoadControlSettings = void(MENUS_THISCALL*)(void* panel, const char* resource, const char* pathId, void* keyValues, void* conditions);
    using CreateControlByName = void*(MENUS_THISCALL*)(void* panel, const char* controlName);
    using LabelSetText = void(MENUS_THISCALL*)(void* label, const char* text);
    // Selects a DialogListButton style button's entry by its display text.
    using HybridButtonSetCurrentSelection = void(MENUS_THISCALL*)(void* button, const char* text);
    using ListConstructor = void(MENUS_THISCALL*)(void* list, void* parent, const char* name, int selectionModifier, bool mostDerived);
    using ListAddItem = int(MENUS_THISCALL*)(void* list, void* item, bool needsInvalidateScheme);
    using ListSelectItem = bool(MENUS_THISCALL*)(void* list, unsigned short index, int direction, bool scrollToItem, bool allowStealFocus, bool skipDisabled);
    using FooterSetButtons = void(MENUS_THISCALL*)(void* footer, int, int, int, int);
    using FooterSetButtonText = void(MENUS_THISCALL*)(void* footer, int, const char*, bool, int);
    using ReleaseActiveMenuControl = void(MENUS_CDECL*)(int);

    GetPanelSingleton getPanelSingleton = nullptr;
    Allocate allocate = nullptr;
    FrameConstructor frameConstructor = nullptr;
    SetDialogTitle setDialogTitle = nullptr;
    ListConstructor listConstructor = nullptr;
    FindChildByName findChildByName = nullptr;
    HybridButtonSetCurrentSelection hybridButtonSetCurrentSelection = nullptr;
    UtlStringSet utlStringSet = nullptr;
    GetUIGameData getUIGameData = nullptr;
    AccessAvatarImage accessAvatarImage = nullptr;
    // The client's CSteamAPIContext*, read out of the avatar loader. Its SteamFriends() is at +8.
    void** steamApiContext = nullptr;
    void* surface = nullptr; // VGUI_Surface031
    void* input = nullptr; // VGUI_Input005
    // The client's vgui::ivgui() pointer, read out of PHandle::Get. Null once vgui has shut down,
    // which happens before plugins are unloaded when the game quits.
    void** vgui = nullptr;
    // Both read out of FindChildByName, which walks children the same way.
    void** panelInterface = nullptr;
    ControlsModuleName controlsModuleName = nullptr;
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

// A vgui::Panel's IClientPanel lives behind its virtual base; its first virtual is GetVPanel.
inline uintptr_t VPanelOf(void* panel)
{
    auto bytes = static_cast<unsigned char*>(panel);
    void* clientPanel = bytes + *reinterpret_cast<int*>(*reinterpret_cast<unsigned char**>(bytes + 4) + 4) + 4;
    return Virtual<Functions::GetVPanel>(clientPanel, 0)(clientPanel);
}

}
