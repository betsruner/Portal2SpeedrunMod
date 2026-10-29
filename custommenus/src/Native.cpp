#include "Native.hpp"


namespace Menus::Native {

Layout layout;
Functions functions;

namespace {
#ifdef _WIN32
constexpr const char* GetPanelSingletonSignature = "A1 ? ? ? ? C3 CC CC CC CC CC CC CC CC CC CC 8A 81";
constexpr const char* AllocateSignature = "55 8B EC A1 ? ? ? ? 8B 08 8B 11 8B 45 ? 8B 52 ? 50 FF D2 5D C3 CC CC CC CC CC CC CC CC CC 55 8B EC 53";
constexpr const char* FrameConstructorSignature = "55 8B EC 53 56 33 DB 57 8B F1 39 5D ? 74 ? C7 46 ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? 8B 45 ? 8B 4D ? 53 53 6A";
constexpr const char* SetDialogTitleSignature = "55 8B EC 83 EC 44 53 57 8B 7D 08 8B D9 89 5D FC 85 FF 0F 84 ? ? ? ? 8B 0D ? ? ? ? 8B 01 8B 50 28 56 57 FF D2 8D B3 A4 02 00 00";
constexpr const char* OpenWindowSignature = "55 8B EC 83 EC ? 53 56 8B F1 F3 0F 10 86 ? ? ? ? 0F 2E 05";
constexpr const char* HandleGetSignature = "8B 01 56 83 F8 ? 74 ? 8B 0D";
constexpr const char* HandleSetSignature = "55 8B EC 51 56 8B 75 ? 89 4D ? 85 F6 74";
constexpr const char* SetDeleteSelfOnCloseSignature = "55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 55 8B EC 8A 45 ? 88 81 ? ? ? ? 5D C2 ? ? 56";
constexpr const char* FooterSetButtonsSignature = "55 8B EC 8B 45 ? 8B D0 C1 E2 ? 2B D0 8D 84 D1 ? ? ? ? 8B 55 ? 89 50 ? 8B 55 ? 89 50 ? 8B 55";
constexpr const char* FooterSetButtonTextSignature = "55 8B EC 8B 45 ? 53 8B D9 56";
constexpr const char* ReleaseActiveMenuControlSignature = "55 8B EC 8B 0D ? ? ? ? 85 C9 74 ? 8B 01 8B 50 ? FF D2";

// The GenericPanelList constructor's prologue matches another class's, so find it by its field setup.
constexpr const char* ListConstructorSignature = "8B 45 10 89 9E 60 01 00 00 89 9E 64 01 00 00 89 9E 68 01 00 00 89 9E 6C 01 00 00 89 9E 70 01 00 00 33 C9 66 89 8E 14 02 00 00";
constexpr int ListConstructorSignatureOffset = -0x102;
constexpr const char* HybridButtonSetCurrentSelectionSignature = "55 8B EC 53 56 8B F1 8B 86 40 02 00 00 57 83 F8 02 0F 85";
constexpr const char* GetUIGameDataSignature = "A1 ? ? ? ? 85 C0 75 2D 38 05 ? ? ? ? 75 25 68 50 01 00 00 E8";
constexpr const char* AccessAvatarImageSignature = "55 8B EC 83 EC 14 83 7D 10 00 56 8B F1 75 09 33 C0 5E 8B E5 5D C2 10 00 8B 45 0C 53 8B 5D 08 8B CB 0B C8 75 0A";
// CGameUiAvatarImage::SetAvatarXUID, which starts by loading the client's CSteamAPIContext pointer.
constexpr const char* SetAvatarXuidSignature = "55 8B EC 0F 57 C0 83 EC 0C 53 56 8B F1 C6 46 1C 00 F3 0F 11 46 20 A1 ? ? ? ? 83 78 08 00 57";
constexpr int SetAvatarXuidContextOffset = 0x17;
constexpr int HandleGetVguiOffset = 10; // "8B 0D <g_pVGui>" in PHandle::Get
constexpr const char* UtlStringSetSignature = "55 8B EC 56 57 8B 7D 08 8B F1 85 FF 74 1B 57 E8 ? ? ? ? 83 C4 04 40 50 57 8B CE E8 ? ? ? ? 5F 8B C6 5E 5D C2 04 00 33 C0 50 57 8B CE E8";
// FindChildByName starts with "mov ebx, [g_pVGuiPanel]" and calls GetControlsModuleName.
constexpr const char* FindChildByNameSignature = "55 8B EC 83 EC 10 53 56 89 4D F8 C7 45 FC 00 00 00 00 57 8B 1D ? ? ? ? 85 DB 74 24 8B 41 04 8B 50 04 8B 44 0A 04 8B 33 8D 4C 0A 04 8B 10 FF D2 50 8B 46 44 8B CB FF D0 8B 1D ? ? ? ? EB 02 33 C0 39 45 FC 0F 8D";
constexpr int PanelInterfaceOffset = 0x15;
constexpr int ModuleNameCallOffset = 0x66;

constexpr Layout PlatformLayout {
    // CBaseModFrame
    0x838, 264, 0x274, 0x278, 0x700,
    // CBaseModPanel
    0x328, 0x464, 0x4AE, 0x458, 0x578,
    // CBaseModFrame virtuals
    5, 6, 60, 68, 93, 106, 207, 214, 253, 254,
    // vgui::Panel
    0x54, 11, 31, 32, 77, 78, 100,
    // ISteamFriends017
    37,
    // vgui::IPanel
    2, 4, 5, 10, 16, 17, 18, 55,
    // ISurface, IInput, IScheme
    14, 15, 72, 160, 77, 8, 3,
    // EditablePanel / Label / ImagePanel virtuals
    183, 189, 184, 184, 183,
    // GenericPanelList
    548, 504, 183, 187, 188, 195, 196,
};
#else
// client.so is built without -fPIC, so globals are absolute addresses the loader patches at load
// time (text relocations); the signatures leave those bytes as wildcards.
constexpr const char* GetPanelSingletonSignature = "A1 ? ? ? ? C3 8D B4 26 00 00 00 00 8D 76 00 A1 ? ? ? ? C3 8D B4 26 00 00 00 00 8D 76 00 C3";
// There are two identical copies; either does.
constexpr const char* AllocateSignature = "83 EC 14 A1 ? ? ? ? 8B 10 FF 74 24 18 50 FF 12 83 C4 1C C3";
// The complete object constructor, which takes no VTT. Its arguments stop at the Cancel button flag.
constexpr const char* FrameConstructorSignature = "55 89 E5 57 56 53 83 EC 24 8B 5D 08 8B 45 14";
constexpr const char* SetDialogTitleSignature = "55 57 56 53 83 EC 4C 8B 74 24 64 85 F6 0F 84 ? ? ? ? A1 ? ? ? ? 83 EC 08 8B 10 56 50 FF 52 28 8B 4C 24 70 83 C4 10 8D 99 A0 02 00 00";
constexpr const char* OpenWindowSignature = "55 66 0F EF C0 89 E5 57 56 53 83 EC 3C";
constexpr const char* HandleGetSignature = "57 56 53 8B 44 24 10 8B 00";
constexpr const char* HandleSetSignature = "57 56 53 8B 5C 24 14 85 DB 74 35";
constexpr const char* SetDeleteSelfOnCloseSignature = "8B 44 24 04 8B 54 24 08 88 90 24 02 00 00 C3 90 83 EC 14";
constexpr const char* FooterSetButtonsSignature = "8B 54 24 04 8B 4C 24 0C 6B 44 24 10 78";
constexpr const char* FooterSetButtonTextSignature = "55 57 56 53 83 EC 0C 8B 5C 24 24 8B 7C 24 20 8B 44 24 28";
constexpr const char* ReleaseActiveMenuControlSignature = "A1 ? ? ? ? 85 C0 74 3F 83 EC 18 8B 10 50 FF 92 8C 00 00 00";
// Same arguments as on Windows, minus the trailing most-derived flag. Found by its field setup too.
constexpr const char* ListConstructorSignature = "C7 83 68 01 00 00 00 00 00 00 89 83 0C 02 00 00 31 C0";
constexpr int ListConstructorSignatureOffset = -0xBE;
constexpr const char* HybridButtonSetCurrentSelectionSignature = "55 57 56 53 83 EC 0C 8B 7C 24 20 8B 6C 24 24 8B 87 3C 02 00 00";
constexpr const char* GetUIGameDataSignature = "55 89 E5 56 53 8B 1D ? ? ? ? 85 DB 74 11 8D 65 F8 89 D8 5B 5E 5D C3 8D B4 26 00 00 00 00 90 80 3D ? ? ? ? 00 75 E6 83 EC 0C 68 64 01 00 00 E8";
constexpr const char* AccessAvatarImageSignature = "55 89 E5 57 56 53 83 EC 3C F3 0F 7E 45 0C";
constexpr const char* SetAvatarXuidSignature = "57 31 D2 56 53 83 EC 10 A1 ? ? ? ?";
constexpr int SetAvatarXuidContextOffset = 9;
constexpr int HandleGetVguiOffset = 0x10; // "8B 15 <g_pVGui>" in PHandle::Get
constexpr const char* UtlStringSetSignature = "57 56 53 8B 7C 24 14 8B 74 24 10 85 FF";
// FindChildByName gets each child through Panel::GetChild, which loads g_pVGuiPanel and calls
// GetControlsModuleName.
constexpr const char* FindChildByNameSignature = "55 57 56 53 31 DB 83 EC 0C 8B 74 24 20 0F B6 6C 24 28";
constexpr int FindChildGetChildCallOffset = 0x31;
constexpr int PanelInterfaceOffset = 9;
constexpr int ModuleNameCallOffset = 0x1A;

// GCC gives every vgui::Panel two destructor slots and keeps overloads in declaration order, so
// most indices sit 29 above MSVC's, but not all of them. Panel fields sit 4 lower, with no vbptr.
// The interfaces deriving IBaseInterface are one above, for its second destructor slot.
constexpr Layout PlatformLayout {
    // CBaseModFrame
    0xC68, 293, 0x270, 0x274, 0xAFC,
    // CBaseModPanel
    0x2E0, 0x41C, 0x466, 0x410, 0x52C,
    // CBaseModFrame virtuals
    34, 35, 89, 97, 122, 135, 236, 243, 282, 283,
    // vgui::Panel
    0x50, 39, 60, 61, 106, 107, 129,
    // ISteamFriends017
    37,
    // vgui::IPanel
    3, 5, 6, 11, 17, 18, 19, 56,
    // ISurface, IInput, IScheme
    13, 15, 72, 160, 77, 9, 4,
    // EditablePanel / Label / ImagePanel virtuals
    212, 218, 212, 212, 213,
    // GenericPanelList
    0x218, 0x1F4, 212, 216, 217, 224, 225,
};
#endif

template <typename T>
bool Find(const char* module, const char* signature, T& out, int offset = 0)
{
    out = reinterpret_cast<T>(Support::Scan(module, signature, offset));
    return out != nullptr;
}
}

bool Resolve(const char* clientModule)
{
    layout = PlatformLayout;
    const bool found = Find(clientModule, GetPanelSingletonSignature, functions.getPanelSingleton)
        && Find(clientModule, AllocateSignature, functions.allocate)
        && Find(clientModule, FrameConstructorSignature, functions.frameConstructor)
        && Find(clientModule, SetDialogTitleSignature, functions.setDialogTitle)
        && Find(clientModule, ListConstructorSignature, functions.listConstructor, ListConstructorSignatureOffset)
        && Find(clientModule, FindChildByNameSignature, functions.findChildByName)
        && Find(clientModule, HybridButtonSetCurrentSelectionSignature, functions.hybridButtonSetCurrentSelection)
        && Find(clientModule, UtlStringSetSignature, functions.utlStringSet)
        && Find(clientModule, GetUIGameDataSignature, functions.getUIGameData)
        && Find(clientModule, AccessAvatarImageSignature, functions.accessAvatarImage)
        && Find(clientModule, OpenWindowSignature, functions.openWindow)
        && Find(clientModule, HandleGetSignature, functions.handleGet)
        && Find(clientModule, HandleSetSignature, functions.handleSet)
        && Find(clientModule, SetDeleteSelfOnCloseSignature, functions.setDeleteSelfOnClose)
        && Find(clientModule, FooterSetButtonsSignature, functions.footerSetButtons)
        && Find(clientModule, FooterSetButtonTextSignature, functions.footerSetButtonText)
        && Find(clientModule, ReleaseActiveMenuControlSignature, functions.releaseActiveMenuControl);
    if (!found)
        return false;

    // Both read out of the code that walks a panel's children.
    const auto findChild = reinterpret_cast<uintptr_t>(functions.findChildByName);
#ifdef _WIN32
    const uintptr_t children = findChild;
#else
    const uintptr_t children = Support::ReadRelative(findChild + FindChildGetChildCallOffset);
#endif
    functions.panelInterface = *reinterpret_cast<void***>(children + PanelInterfaceOffset);
    functions.controlsModuleName = Support::ReadRelative<Functions::ControlsModuleName>(children + ModuleNameCallOffset);

    // Without it, avatars of players who aren't Steam friends just never load.
    const uintptr_t setAvatar = Support::Scan(clientModule, SetAvatarXuidSignature);
    functions.steamApiContext = setAvatar ? *reinterpret_cast<void***>(setAvatar + SetAvatarXuidContextOffset) : nullptr;

    functions.surface = Support::GetInterface(MENUS_MODULE("vguimatsurface"), "VGUI_Surface031");
    functions.input = Support::GetInterface(MENUS_MODULE("vgui2"), "VGUI_Input005");
    functions.vgui = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(functions.handleGet) + HandleGetVguiOffset);
    return functions.panelInterface && functions.controlsModuleName && functions.surface && functions.input && functions.vgui;
}

}
