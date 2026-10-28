#include "CustomMenus.hpp"

#include <cstdlib>

#include "Command.hpp"
#include "Modules/Console.hpp"
#include "SpeedrunMenus.hpp"

// Built into this plugin for now; see Menus/Menus.hpp.
extern "C" Menus::System* CreateMenuSystem(const char* version);

Menus::System* menus = nullptr;

bool LoadMenus()
{
    if (menus)
        return true;
    Menus::System* system = CreateMenuSystem(Menus::InterfaceVersion);
    if (!system || !system->Init())
        return false;
    menus = system;
    RegisterSpeedrunMenus(menus);
    return true;
}

void UnloadMenus()
{
    if (!menus)
        return;
    menus->Shutdown();
    menus = nullptr;
}

CON_COMMAND(menus_open, "menus_open <id> : Opens a custom menu panel over the active one.\n")
{
    if (args.ArgC() != 2)
        return console->Print(menus_open.ThisPtr()->m_pszHelpString);
    if (!menus || !menus->Open(args[1]))
        console->Print("Could not open panel '%s'.\n", args[1]);
}

CON_COMMAND(menus_open_native, "menus_open_native <window type> : Opens a stock BaseModUI panel over the active one.\n")
{
    if (args.ArgC() != 2)
        return console->Print(menus_open_native.ThisPtr()->m_pszHelpString);
    if (!menus || !menus->OpenNative(std::atoi(args[1])))
        console->Print("Could not open window type %s.\n", args[1]);
}

CON_COMMAND(menus_back, "Navigates back from the active menu panel.\n")
{
    if (menus)
        menus->Back();
}

CON_COMMAND(menus_dump, "Prints the menu system's native window slots and custom panel stacks.\n")
{
    if (menus)
        menus->Dump();
    else
        console->Print("menus: not loaded\n");
}
