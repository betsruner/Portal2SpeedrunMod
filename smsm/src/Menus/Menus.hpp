#pragma once

// Custom BaseModUI panels for Portal 2.
//
// This is the whole interface between the menu system and the plugin using it, so that the
// menu system can live in its own module: the plugin gets a System from CreateMenuSystem and
// only ever talks to it, and the panels it gets back, through virtual calls. Nothing here uses
// STL types or needs anything from the other side to link.
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

// Bumped whenever anything in this header changes in a way that breaks plugins built against it.
constexpr const char* InterfaceVersion = "CustomMenus001";

struct Color {
    unsigned char r, g, b, a;
};

// A scrolling BaseModUI::GenericPanelList, created with Panel::CreateList. Its .res entry (matched
// by name) sets its position and look. Owned by the panel that created it.
class List {
public:
    virtual void Clear() = 0;
    // Adds an item built from Resource/UI/BaseModUI/<resource>.res, whose root block must be named
    // <resource>. Clicking an item selects it. Returns its index, or -1.
    virtual int Add(const char* resource) = 0;
    virtual int Count() = 0;
    // Sets the text of a Label or button named control inside the item.
    virtual bool SetItemText(int index, const char* control, const char* text) = 0;
    // Shows a Steam user's avatar in an ImagePanel named control inside the item. Avatars Steam
    // hasn't downloaded yet (anyone who isn't a friend) are requested and filled in once they arrive.
    virtual bool SetItemAvatar(int index, const char* control, unsigned long long steamId) = 0;
    // Runtime colors don't survive the item's scheme being reapplied, so set them every OnTick.
    virtual bool SetItemBackground(int index, Color color) = 0;
    virtual bool SetItemTextColor(int index, const char* control, Color color) = 0;
    // The selected item's index, or -1.
    virtual int Selected() = 0;
    virtual void Select(int index) = 0;

protected:
    ~List() = default;
};

// A horizontal option picker drawn like the stock leaderboard's "PORTALS / TIME" toggle, but for
// any number of options: arrows either side and every option in a row, the selected one bright.
// Clicking an option selects it; clicking an arrow steps (wrapping around). Created with
// Panel::CreateSelector; its .res entry (a "Panel", matched by name) sets the bar's position,
// size and background. Owned by the panel that created it.
class Selector {
public:
    // Option names are drawn as given (ASCII).
    virtual void SetOptions(const char* const* options, int count) = 0;
    virtual int Count() = 0;
    // The selected option's index, or -1 when there are none.
    virtual int Selected() = 0;
    virtual void Select(int index) = 0;

protected:
    ~Selector() = default;
};

// The menu system's side of an open panel. Panels reach it through their own helpers below.
class Frame {
public:
    virtual void* Native() = 0;
    virtual void Close() = 0;
    virtual bool SetControlText(const char* control, const char* text) = 0;
    virtual List* CreateList(const char* name) = 0;
    virtual Selector* CreateSelector(const char* name, const char* font) = 0;

protected:
    ~Frame() = default;
};

// Implemented by the plugin, one instance per open panel.
class Panel {
public:
    virtual ~Panel() = default;

    // The native frame exists but its .res hasn't loaded yet. Create lists here.
    virtual void OnCreate() {}
    // The first time the panel is shown.
    virtual void OnOpen() {}
    // Every time the panel becomes the active window, including after Back from a child panel.
    virtual void OnActivate() {}
    // Commands from the panel's .res controls. Return true if handled.
    virtual bool OnCommand(const char* command) { return false; }
    // vgui ButtonCode_t, including the footer's controller buttons. Return true if handled.
    virtual bool OnKeyCodePressed(int code) { return false; }
    // Once per frame while the panel is open.
    virtual void OnTick() {}
    virtual void OnClose() {}
    // The menu system calls this instead of delete, so the panel is freed by the module that made it.
    virtual void Release() { delete this; }

    // The CBaseModFrame, or nullptr once closed.
    void* NativeFrame() const { return this->frame ? this->frame->Native() : nullptr; }
    // Same as pressing Back on this panel.
    void Close()
    {
        if (this->frame)
            this->frame->Close();
    }
    // Sets the text of a Label or button named control anywhere in the panel.
    bool SetControlText(const char* control, const char* text) { return this->frame && this->frame->SetControlText(control, text); }

protected:
    // Only valid inside OnCreate.
    List* CreateList(const char* name) { return this->frame ? this->frame->CreateList(name) : nullptr; }
    // Only valid inside OnCreate. font is a font from the BaseModUI scheme.
    Selector* CreateSelector(const char* name, const char* font = "NewGameChapterName")
    {
        return this->frame ? this->frame->CreateSelector(name, font) : nullptr;
    }

private:
    friend class MenuSystem;
    Frame* frame = nullptr;
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
    // The dialog's title tab, like stock panels set in code. A localization token or plain text.
    const char* title = nullptr;
    PanelFactory factory = nullptr;
    bool footer = true;
    Layer layer = Layer::Page;
    // Text for the footer's primary (A) button, shown next to Back. Reaches OnKeyCodePressed when pressed.
    const char* footerPrimary = nullptr;
};

class System {
public:
    // Finds everything the menu system needs in the game. Returns false (and every other call
    // does nothing) if this game build isn't supported.
    virtual bool Init() = 0;
    // Closes every custom panel, forgets every registration and undoes every hook. Call it
    // before the plugin unloads, since registrations point into the plugin.
    virtual void Shutdown() = 0;
    virtual bool IsAvailable() = 0;
    // Call once per frame from the plugin.
    virtual void Tick() = 0;

    virtual bool Register(const PanelDesc& desc) = 0;
    virtual bool Unregister(const char* id) = 0;

    // These open over whichever panel is active, or the main menu.
    virtual bool Open(const char* id) = 0;
    virtual bool OpenNative(int windowType) = 0;
    virtual bool Back() = 0;

    // Prints the native window slots and the custom panel stacks to the console.
    virtual void Dump() = 0;

protected:
    ~System() = default;
};

// The menu system's one entry point, CreateInterface style: returns the System if version is one
// it implements, otherwise nullptr.
using CreateMenuSystemFn = System* (*)(const char* version);
constexpr const char* CreateMenuSystemName = "CreateMenuSystem";

}
