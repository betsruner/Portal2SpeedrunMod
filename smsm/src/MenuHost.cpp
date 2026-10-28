#include "MenuHost.hpp"

#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "Command.hpp"
#include "Modules/Console.hpp"
#include "SpeedrunMenus.hpp"

Menus::System* menus = nullptr;

namespace {
#ifdef _WIN32
constexpr const char* LibraryName = "custommenus.dll";
#else
constexpr const char* LibraryName = "custommenus.so";
#endif

void* library = nullptr;

// The menu system ships next to this plugin, which isn't anywhere the game searches for libraries.
std::string LibraryPath()
{
#ifdef _WIN32
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&LibraryPath), &self);
    char buffer[MAX_PATH];
    std::string path(buffer, GetModuleFileNameA(self, buffer, sizeof(buffer)));
#else
    Dl_info info {};
    dladdr(reinterpret_cast<void*>(&LibraryPath), &info);
    std::string path = info.dli_fname ? info.dli_fname : "";
#endif
    const size_t slash = path.find_last_of("\\/");
    return (slash == std::string::npos ? std::string() : path.substr(0, slash + 1)) + LibraryName;
}

void FreeMenuLibrary()
{
    if (!library)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
    library = nullptr;
}
}

bool LoadMenus()
{
    if (menus)
        return true;

    const std::string path = LibraryPath();
#ifdef _WIN32
    library = LoadLibraryA(path.c_str());
    auto create = library ? reinterpret_cast<Menus::CreateMenuSystemFn>(GetProcAddress(static_cast<HMODULE>(library), Menus::CreateMenuSystemName)) : nullptr;
#else
    library = dlopen(path.c_str(), RTLD_NOW);
    auto create = library ? reinterpret_cast<Menus::CreateMenuSystemFn>(dlsym(library, Menus::CreateMenuSystemName)) : nullptr;
#endif
    Menus::System* system = create ? create(Menus::InterfaceVersion) : nullptr;
    if (!system) {
        console->Warning("smsm: Could not load the menu system (%s); custom menus are disabled.\n", path.c_str());
        FreeMenuLibrary();
        return false;
    }
    // Init explains itself when this game build isn't supported.
    if (!system->Init()) {
        FreeMenuLibrary();
        return false;
    }

    menus = system;
    RegisterSpeedrunMenus(menus);
    return true;
}

void UnloadMenus()
{
    if (menus) {
        menus->Shutdown();
        menus = nullptr;
    }
    FreeMenuLibrary();
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
