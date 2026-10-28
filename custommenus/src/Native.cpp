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
constexpr int FindChildPanelInterfaceOffset = 0x15;
constexpr int FindChildModuleNameCallOffset = 0x66;
constexpr const char* UtlStringSetSignature = "55 8B EC 56 57 8B 7D 08 8B F1 85 FF 74 1B 57 E8 ? ? ? ? 83 C4 04 40 50 57 8B CE E8 ? ? ? ? 5F 8B C6 5E 5D C2 04 00 33 C0 50 57 8B CE E8";
constexpr const char* FindChildByNameSignature = "55 8B EC 83 EC 10 53 56 89 4D F8 C7 45 FC 00 00 00 00 57 8B 1D ? ? ? ? 85 DB 74 24 8B 41 04 8B 50 04 8B 44 0A 04 8B 33 8D 4C 0A 04 8B 10 FF D2 50 8B 46 44 8B CB FF D0 8B 1D ? ? ? ? EB 02 33 C0 39 45 FC 0F 8D";

constexpr Layout PlatformLayout {
    // CBaseModFrame
    0x838, 264, 0x274, 0x278, 0x700,
    // CBaseModPanel
    0x328, 0x464, 0x4AE, 0x458, 0x578,
    // CBaseModFrame virtuals
    5, 6, 60, 68, 93, 106, 207, 214, 253, 254,
    // vgui::Panel
    0x54, 11, 31, 32, 77, 100,
    // ISteamFriends017
    37,
    // vgui::IPanel
    5, 10, 16, 17, 18, 55,
    // ISurface, IInput, IScheme
    14, 15, 72, 160, 77, 8, 3,
    // EditablePanel / Label / ImagePanel virtuals
    183, 189, 184, 184,
    // GenericPanelList
    548, 504, 183, 187, 188, 195, 196,
    // Added later: ImagePanel::SetImage(const char*), IPanel::SetPos / SetSize, Panel::OnMouseDoublePressed
    183, 2, 4, 78,
};
#else
// Linux values carried over from the first prototype. The vtable indices are the Windows ones
// shifted by 29 and the CBaseModPanel fields past m_Frames are unverified, so the menu system
// stays disabled on Linux until someone checks them against client.so.
constexpr const char* GetPanelSingletonSignature = "A1 ? ? ? ? C3 8D B4 26 ? ? ? ? 8D 76 ? A1 ? ? ? ? C3 8D B4 26 ? ? ? ? 8D 76 ? C3 90 8D B4 26 ? ? ? ? 8D B4 26 ? ? ? ? 8B 44 24 ?";
constexpr const char* AllocateSignature = "83 EC ? A1 ? ? ? ? 8B 10 FF 74 24 ? 50 FF 12 83 C4 ? C3 8D B4 26 ? ? ? ? 8D 74 26 ? A1 ? ? ? ? 8B 4C 24 ? 8B 10 89 44 24 ? 89 4C 24 ? FF 62 ? 8D B4 26 ? ? ? ? 8D 76 ? 83 EC ? A1 ? ? ? ? 8B 10 FF 74 24 ? 50 FF 52 ? 83 C4 ? C3 8D B4 26 ? ? ? ? 8D 76 ? 83 EC";
constexpr const char* FrameConstructorSignature = "55 89 E5 57 56 53 83 EC ? 8B 5D ? 8B 45 ? 8B 7D ? C7 03 ? ? ? ? 6A";
constexpr const char* OpenWindowSignature = "55 66 0F EF C0 89 E5 57 56 53 83 EC ? 8B 75";
constexpr const char* HandleGetSignature = "57 56 53 8B 44 24 ? 8B 00";
constexpr const char* HandleSetSignature = "57 56 53 8B 5C 24 ? 85 DB 74 ? 8B 35";
constexpr const char* SetDeleteSelfOnCloseSignature = "8B 44 24 ? 8B 54 24 ? 88 90 ? ? ? ? C3 90 83 EC";
constexpr const char* FooterSetButtonsSignature = "8B 54 24 ? 8B 4C 24 ? 6B 44 24 ? ? 01 D0 89 88 ? ? ? ? 8B 4C 24 ? 89 88 ? ? ? ? 8B 4C 24";
constexpr const char* FooterSetButtonTextSignature = "55 57 56 53 83 EC ? 8B 5C 24 ? 8B 7C 24 ? 8B 44 24 ? 8B 6C 24 ? 8B 74 24";
constexpr const char* ReleaseActiveMenuControlSignature = "A1 ? ? ? ? 85 C0 74 ? 83 EC ? 8B 10 50 FF 92 ? ? ? ? 83 C4 ? 84 C0 75 ? 83 C4";

constexpr Layout PlatformLayout {
    0xC68, 293, 0x270, 0x274, 0xAFC,
    0x2E0, -1, -1, 0x410, 0x52C,
    34, 35, 89, 97, 122, 135, 236, 243, 282, 283,
    -1, -1, -1, -1, -1, -1,
    -1,
    -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1,
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
#ifndef _WIN32
    return false;
#else
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

    // FindChildByName starts with "mov ebx, [g_pVGuiPanel]" and calls GetControlsModuleName.
    const auto findChild = reinterpret_cast<uintptr_t>(functions.findChildByName);
    functions.panelInterface = *reinterpret_cast<void***>(findChild + FindChildPanelInterfaceOffset);
    functions.controlsModuleName = Support::ReadRelative<Functions::ControlsModuleName>(findChild + FindChildModuleNameCallOffset);

    // Without it, avatars of players who aren't Steam friends just never load.
    const uintptr_t setAvatar = Support::Scan(clientModule, SetAvatarXuidSignature);
    functions.steamApiContext = setAvatar ? *reinterpret_cast<void***>(setAvatar + SetAvatarXuidContextOffset) : nullptr;

    functions.surface = Support::GetInterface(MENUS_MODULE("vguimatsurface"), "VGUI_Surface031");
    functions.input = Support::GetInterface(MENUS_MODULE("vgui2"), "VGUI_Input005");
    functions.vgui = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(functions.handleGet) + HandleGetVguiOffset);
    return functions.panelInterface && functions.controlsModuleName && functions.surface && functions.input && functions.vgui;
#endif
}

}
