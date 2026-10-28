#include "SpeedrunMenus.hpp"

#include <cstdlib>

#include "Command.hpp"
#include "Menus/Menus.hpp"
#include "Modules/Console.hpp"

namespace {
// Placeholder until the real panels exist: logs its lifecycle so menu navigation can be traced in the console.
class PlaceholderPanel : public Menus::Panel {
public:
    explicit PlaceholderPanel(const char* id)
        : id(id)
    {
    }

    void OnOpen() override { console->Print("[menus] %s opened\n", this->id); }
    void OnActivate() override { console->Print("[menus] %s activated\n", this->id); }
    void OnClose() override { console->Print("[menus] %s closed\n", this->id); }

private:
    const char* id;
};

#define PLACEHOLDER(name) +[]() -> Menus::Panel* { return new PlaceholderPanel(name); }
}

void RegisterSpeedrunMenus()
{
    // Main menu "Speedrun Mod" button -> pick Full Game or Challenge Mode.
    Menus::Register({ "p2sm_mode_select", "P2SMModeSelect", "Speedrun Mod", PLACEHOLDER("p2sm_mode_select") });
    Menus::Register({ "p2sm_full_game", "P2SMFullGame", "Full Game", PLACEHOLDER("p2sm_full_game") });
    Menus::Register({ "p2sm_challenge_mode", "P2SMChallengeMode", "Challenge Mode", PLACEHOLDER("p2sm_challenge_mode") });
    // Options -> Speedrun Mod settings.
    Menus::Register({ "p2sm_settings", "P2SMSettings", "Speedrun Mod", PLACEHOLDER("p2sm_settings") });

    // Navigation tests: a third level of pages and a popup.
    Menus::Register({ "p2sm_test_page", "P2SMTestPage", "Third Page", PLACEHOLDER("p2sm_test_page") });
    Menus::Register({ "p2sm_test_popup", "P2SMTestPopup", "Popup", PLACEHOLDER("p2sm_test_popup"), false, Menus::Layer::Popup });
}

CON_COMMAND(menus_open, "menus_open <id> : Opens a custom menu panel over the active one.\n")
{
    if (args.ArgC() != 2)
        return console->Print(menus_open.ThisPtr()->m_pszHelpString);
    if (!Menus::Open(args[1]))
        console->Print("Could not open panel '%s'.\n", args[1]);
}

CON_COMMAND(menus_open_native, "menus_open_native <window type> : Opens a stock BaseModUI panel over the active one.\n")
{
    if (args.ArgC() != 2)
        return console->Print(menus_open_native.ThisPtr()->m_pszHelpString);
    if (!Menus::OpenNative(std::atoi(args[1])))
        console->Print("Could not open window type %s.\n", args[1]);
}

CON_COMMAND(menus_back, "Navigates back from the active menu panel.\n")
{
    Menus::Back();
}

CON_COMMAND(menus_dump, "Prints the menu system's native window slots and custom panel stacks.\n")
{
    Menus::Dump();
}
