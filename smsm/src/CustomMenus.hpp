#pragma once

#include "Menus/Menus.hpp"

// The custom BaseModUI menu system, or nullptr if it isn't loaded or doesn't support this game build.
extern Menus::System* menus;

bool LoadMenus();
void UnloadMenus();
