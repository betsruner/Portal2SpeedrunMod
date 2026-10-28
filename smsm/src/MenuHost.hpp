#pragma once

#include "CustomMenus.hpp"

// The custom BaseModUI menu system, or nullptr if it isn't loaded or doesn't support this game build.
extern Menus::System* menus;

// Loads custommenus next to this plugin and registers the Speedrun Mod panels.
bool LoadMenus();
void UnloadMenus();
