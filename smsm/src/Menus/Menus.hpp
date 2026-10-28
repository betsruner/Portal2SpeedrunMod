#pragma once

// Custom BaseModUI panels for Portal 2.
//
// This folder is meant to move into its own project later, so it only depends on
// Utils/Memory, Interface and the console module, and this header uses no STL types.
//
// Panels are real CBaseModFrames built from Resource/UI/BaseModUI/<resource>.res, so they get
// the game's transitions, footer and controller navigation. Any number of panels can be
// registered and nested: open custom pages share one borrowed native window slot and open
// popups share another, with the hidden ones kept in a stack and swapped into the slot as you
// navigate back.
//
// .res buttons (in custom panels and in any stock BaseModUI panel) can use these commands:
//   "menus_open <id>"         open a registered panel
//   "menus_open_native <n>"   open a stock panel by WINDOW_TYPE
//   "menus_back"              same as pressing Back
namespace Menus {

class Panel {
public:
    virtual ~Panel() = default;

    // The first time the panel is shown.
    virtual void OnOpen() {}
    // Every time the panel becomes the active window, including after Back from a child panel.
    virtual void OnActivate() {}
    // Commands from the panel's .res controls. Return true if handled.
    virtual bool OnCommand(const char* command) { return false; }
    virtual void OnClose() {}
    // The menu system calls this instead of delete, so the panel is freed by the module that made it.
    virtual void Release() { delete this; }

    void* NativeFrame() const { return this->frame; }
    // Same as pressing Back on this panel.
    void Close();

private:
    friend class MenuSystem;
    void* frame = nullptr;
};

enum class Layer {
    // Replaces the panel that opened it.
    Page,
    // Shown over the panel that opened it.
    Popup,
};

using PanelFactory = Panel* (*)();

struct PanelDesc {
    const char* id = nullptr;
    // Resource/UI/BaseModUI/<resource>.res. Defaults to the id.
    const char* resource = nullptr;
    PanelFactory factory = nullptr;
    bool footer = true;
    Layer layer = Layer::Page;
};

bool Init();
void Shutdown();
bool IsAvailable();
// Call once per frame from the host plugin.
void Tick();

bool Register(const PanelDesc& desc);
bool Unregister(const char* id);

// These open over whichever panel is active, or the main menu.
bool Open(const char* id);
bool OpenNative(int windowType);
bool Back();

// Prints the native window slots and the custom panel stacks to the console.
void Dump();

}
