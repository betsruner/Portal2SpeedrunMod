#include "CustomMenus.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "Native.hpp"
#include "Support.hpp"

namespace Menus {

using namespace Native;

namespace {
constexpr int MainMenuWindowType = 20;
// Resource/UI/BaseModUI/%s.res is formatted into a 64 byte buffer by the frame constructor.
constexpr size_t MaxResourceLength = 37;

constexpr const char* OpenCommand = "menus_open";
constexpr const char* OpenNativeCommand = "menus_open_native";
constexpr const char* BackCommand = "menus_back";

struct Registration {
    std::string resource;
    std::string title;
    PanelFactory factory;
    bool footer;
    Layer layer;
    std::string footerPrimary;
};

bool ValidName(const char* value, size_t maxLength)
{
    if (!value || !value[0] || std::strlen(value) > maxLength)
        return false;
    for (const char* c = value; *c; ++c) {
        const bool alphaNumeric = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9');
        if (!alphaNumeric && *c != '_' && *c != '-' && *c != '.')
            return false;
    }
    return true;
}

constexpr int MouseLeft = 107;

bool SetChildText(void* panel, const char* control, const char* text)
{
    void* child = panel && control ? functions.findChildByName(panel, control, true) : nullptr;
    if (!child)
        return false;
    Virtual<Functions::LabelSetText>(child, layout.labelSetText)(child, text ? text : "");
    return true;
}

constexpr int AvatarAddReference = 1;
constexpr int AvatarRelease = 2;
constexpr int AvatarSize = 2; // what stock leaderboard rows ask for
// Cleared items are only deleted by vgui later, and may paint once more, so their avatar
// references are released a few ticks after the clear.
constexpr int AvatarReleaseDelay = 3;
std::vector<std::pair<uint64_t, int>> pendingAvatarReleases; // steam id, ticks left
// Avatars that aren't downloaded yet are retried this often.
constexpr int AvatarRetryTicks = 30;
std::unordered_map<uint64_t, bool> requestedUsers;

// Steam only fetches avatars of players who aren't friends once asked about them.
void RequestUser(uint64_t steamId)
{
    if (!functions.steamApiContext || !*functions.steamApiContext || requestedUsers.count(steamId))
        return;
    void* steamFriends = static_cast<void**>(*functions.steamApiContext)[2];
    if (!steamFriends)
        return;
    Virtual<Functions::RequestUserInformation>(steamFriends, layout.steamFriendsRequestUserInformation)(steamFriends, steamId, false);
    requestedUsers[steamId] = true;
}

void ReleasePendingAvatars()
{
    void* cache = functions.getUIGameData();
    for (auto it = pendingAvatarReleases.begin(); it != pendingAvatarReleases.end();) {
        if (--it->second > 0) {
            ++it;
            continue;
        }
        if (cache)
            functions.accessAvatarImage(cache, it->first, AvatarRelease, AvatarSize);
        it = pendingAvatarReleases.erase(it);
    }
}

constexpr const char* HybridButtonClassName = ".?AVBaseModHybridButton@BaseModUI@@";

// The class name MSVC's RTTI records for a polymorphic object, like HybridButtonClassName.
const char* ClassName(void* object)
{
#ifdef _WIN32
    // The vtable is preceded by the RTTI complete object locator, whose fourth field points to
    // the type descriptor; the decorated name starts 8 bytes into it.
    auto locator = reinterpret_cast<const uint32_t*>((*static_cast<uintptr_t**>(object))[-1]);
    return reinterpret_cast<const char*>(locator[3]) + 8;
#else
    return "";
#endif
}

// vgui::Color is four bytes passed by value.
int NativeColor(Color color)
{
    return color.r | color.g << 8 | color.b << 16 | color.a << 24;
}

class ListImpl;
class GridImpl;
class SelectorImpl;

// List items are plain EditablePanels holding Labels, ImagePanels and so on, and selectors are
// plain Panels; none of them do anything when clicked or painted. OnMousePressed,
// OnMouseDoublePressed and Paint are patched once per class, and the hooks only act on panels we own.
std::unordered_map<void*, ListImpl*> listItems; // item -> the list that owns it
std::unordered_map<void*, GridImpl*> gridRows;  // row -> the grid that owns it
std::unordered_map<void*, SelectorImpl*> selectors;
std::unordered_map<uintptr_t*, uintptr_t> mouseVtables; // patched vtable -> original OnMousePressed
std::unordered_map<uintptr_t*, uintptr_t> doubleVtables; // patched vtable -> original OnMouseDoublePressed
std::unordered_map<uintptr_t*, uintptr_t> paintVtables; // patched vtable -> original Paint
void SelectItem(ListImpl* list, void* item);
void ClickGridTile(GridImpl* grid, void* tile, bool doubleClick);
void ClickSelector(SelectorImpl* selector);
void PaintSelector(SelectorImpl* selector);
MENUS_DECL_DETOUR(void, ItemOnMousePressed, int code);
MENUS_DECL_DETOUR(void, ItemOnMouseDoublePressed, int code);
MENUS_DECL_DETOUR(void, PanelPaint);

MENUS_DETOUR(void, PanelPaint)
{
    auto original = paintVtables.find(*reinterpret_cast<uintptr_t**>(thisptr));
    if (original != paintVtables.end())
        reinterpret_cast<_PanelPaint>(original->second)(thisptr);
    auto selector = selectors.find(thisptr);
    if (selector != selectors.end())
        PaintSelector(selector->second);
}

// A left click on one of our panels, or anything inside a list item or grid tile. Returns false
// if it wasn't ours. vgui sends the second press of a double-click to OnMouseDoublePressed.
bool HandleClick(void* clicked, bool doubleClick)
{
    auto selector = selectors.find(clicked);
    if (selector != selectors.end()) {
        ClickSelector(selector->second);
        return true;
    }
    void* panel = clicked;
    void* child = nullptr;
    for (int depth = 0; panel && depth < 4; ++depth) {
        auto owner = listItems.find(panel);
        if (owner != listItems.end()) {
            SelectItem(owner->second, panel);
            return true;
        }
        // A grid row's children are its tiles. A double-click activates the tile, and the panel
        // can close itself in response, so nothing may be touched afterwards.
        auto grid = gridRows.find(panel);
        if (grid != gridRows.end()) {
            if (child)
                ClickGridTile(grid->second, child, doubleClick);
            return true;
        }
        child = panel;
        panel = Virtual<Functions::PanelGetParent>(panel, layout.getParent)(panel);
    }
    return false;
}

MENUS_DETOUR(void, ItemOnMousePressed, int code)
{
    if (code == MouseLeft && HandleClick(thisptr, false))
        return;
    auto original = mouseVtables.find(*reinterpret_cast<uintptr_t**>(thisptr));
    if (original != mouseVtables.end())
        reinterpret_cast<_ItemOnMousePressed>(original->second)(thisptr, code);
}

MENUS_DETOUR(void, ItemOnMouseDoublePressed, int code)
{
    if (code == MouseLeft && HandleClick(thisptr, true))
        return;
    auto original = doubleVtables.find(*reinterpret_cast<uintptr_t**>(thisptr));
    if (original != doubleVtables.end())
        reinterpret_cast<_ItemOnMouseDoublePressed>(original->second)(thisptr, code);
}

void PatchVirtual(void* panel, int index, uintptr_t hook, std::unordered_map<uintptr_t*, uintptr_t>& originals)
{
    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(panel);
    uintptr_t* entry = &vtable[index];
    if (originals.count(vtable) || *entry == hook)
        return;
    Support::UnProtect(entry, sizeof(uintptr_t));
    originals[vtable] = *entry;
    *entry = hook;
}

void PatchMouse(void* panel)
{
    PatchVirtual(panel, layout.onMousePressed, reinterpret_cast<uintptr_t>(ItemOnMousePressed_Hook), mouseVtables);
    PatchVirtual(panel, layout.onMouseDoublePressed, reinterpret_cast<uintptr_t>(ItemOnMouseDoublePressed_Hook), doubleVtables);
}

// Patches the panel's class and the classes of everything inside it.
void PatchMouseTree(void* panel)
{
    PatchMouse(panel);
    void* panelInterface = *functions.panelInterface;
    const uintptr_t vpanel = VPanelOf(panel);
    const int count = Virtual<Functions::PanelGetChildCount>(panelInterface, layout.panelGetChildCount)(panelInterface, vpanel);
    for (int i = 0; i < count; ++i) {
        const uintptr_t child = Virtual<Functions::PanelGetChild>(panelInterface, layout.panelGetChild)(panelInterface, vpanel, i);
        void* childPanel = Virtual<Functions::PanelGetPanel>(panelInterface, layout.panelGetPanel)(panelInterface, child, functions.controlsModuleName());
        if (childPanel)
            PatchMouseTree(childPanel);
    }
}

void RestoreMouse()
{
    for (auto& item : mouseVtables)
        item.first[layout.onMousePressed] = item.second;
    for (auto& item : doubleVtables)
        item.first[layout.onMouseDoublePressed] = item.second;
    for (auto& item : paintVtables)
        item.first[layout.paint] = item.second;
    mouseVtables.clear();
    doubleVtables.clear();
    paintVtables.clear();
    listItems.clear();
    gridRows.clear();
    selectors.clear();
}

// The stock list button colors (HybridButton.ListButtonActiveColor / InactiveColor).
constexpr Color SelectorActiveColor { 255, 255, 255, 255 };
constexpr Color SelectorInactiveColor { 255, 255, 255, 60 };
constexpr const char* SelectorSeparator = "/";

class SelectorImpl final : public Selector {
public:
    SelectorImpl(void* panel, const char* fontName)
        : panel(panel)
        , fontName(fontName)
    {
    }

    void SetOptions(const char* const* options, int count) override
    {
        this->options.assign(options, options + (count > 0 ? count : 0));
        this->selected = this->options.empty() ? -1 : std::min(std::max(this->selected, 0), this->Count() - 1);
    }

    int Count() override { return static_cast<int>(this->options.size()); }
    int Selected() override { return this->selected; }

    void Select(int index) override
    {
        const int count = this->Count();
        if (count)
            this->selected = (index % count + count) % count;
    }

    void ApplyScheme(void* scheme)
    {
        this->font = Virtual<Functions::SchemeGetFont>(scheme, layout.schemeGetFont)(scheme, this->fontName.c_str(), true);
    }

    void Paint()
    {
        if (!this->font)
            return;
        void* surface = functions.surface;
        void* panelInterface = *functions.panelInterface;
        int wide = 0, tall = 0;
        Virtual<Functions::PanelGetInts>(panelInterface, layout.panelGetSize)(panelInterface, VPanelOf(this->panel), wide, tall);
        const int fontTall = Virtual<Functions::SurfaceGetFontTall>(surface, layout.surfaceGetFontTall)(surface, this->font);
        auto setColor = Virtual<Functions::SurfaceDrawSetColor>(surface, layout.surfaceDrawSetColor);
        auto fillRect = Virtual<Functions::SurfaceDrawFilledRect>(surface, layout.surfaceDrawFilledRect);
        auto drawText = Virtual<Functions::SurfaceDrawColoredText>(surface, layout.surfaceDrawColoredText);
        auto getTextSize = Virtual<Functions::SurfaceGetTextSize>(surface, layout.surfaceGetTextSize);
        auto textWidth = [&](const char* text) {
            std::wstring wide(text, text + std::strlen(text)); // option names are expected to be ASCII
            int textWide = 0, textTall = 0;
            getTextSize(surface, this->font, wide.c_str(), textWide, textTall);
            return textWide;
        };

        // Arrow triangles, tips pointing outwards.
        const int arrow = std::max(3, fontTall / 3);
        const int inset = arrow * 2;
        const int middle = tall / 2;
        setColor(surface, SelectorActiveColor.r, SelectorActiveColor.g, SelectorActiveColor.b, SelectorActiveColor.a);
        for (int column = 0; column < arrow; ++column) {
            fillRect(surface, inset + column, middle - column, inset + column + 1, middle + column + 1);
            fillRect(surface, wide - inset - column - 1, middle - column, wide - inset - column, middle + column + 1);
        }
        this->leftArrowEnd = inset + arrow + inset / 2;
        this->rightArrowStart = wide - this->leftArrowEnd;

        // Options in a row, separated like the stock "PORTALS / TIME". When they don't all fit
        // between the arrows, show the run around the selected one that does.
        this->spans.assign(this->options.size(), { 0, 0 });
        if (this->selected < 0)
            return;
        const int gap = std::max(2, fontTall / 3); // drawn text skips leading spaces, so pad explicitly
        const int slashWide = textWidth(SelectorSeparator);
        const int separatorWide = slashWide + gap * 2;
        std::vector<int> widths;
        for (auto& option : this->options)
            widths.push_back(textWidth(option.c_str()));

        const int available = this->rightArrowStart - this->leftArrowEnd;
        int first = this->selected, last = this->selected, total = widths[this->selected];
        for (bool grew = true; grew;) {
            grew = false;
            if (last + 1 < this->Count() && total + separatorWide + widths[last + 1] <= available) {
                total += separatorWide + widths[++last];
                grew = true;
            }
            if (first > 0 && total + separatorWide + widths[first - 1] <= available) {
                total += separatorWide + widths[--first];
                grew = true;
            }
        }

        int x = (wide - total) / 2;
        const int y = (tall - fontTall) / 2;
        for (int i = first; i <= last; ++i) {
            const Color& color = i == this->selected ? SelectorActiveColor : SelectorInactiveColor;
            drawText(surface, this->font, x, y, color.r, color.g, color.b, color.a, "%s", this->options[i].c_str());
            this->spans[i] = { x, x + widths[i] };
            x += widths[i];
            if (i < last) {
                const Color& inactive = SelectorInactiveColor;
                drawText(surface, this->font, x + gap, y, inactive.r, inactive.g, inactive.b, inactive.a, "%s", SelectorSeparator);
                x += separatorWide;
            }
        }
    }

    void Click()
    {
        int cursorX = 0, cursorY = 0, panelX = 0, panelY = 0;
        Virtual<Functions::InputGetCursorPos>(functions.input, layout.inputGetCursorPos)(functions.input, cursorX, cursorY);
        void* panelInterface = *functions.panelInterface;
        Virtual<Functions::PanelGetInts>(panelInterface, layout.panelGetAbsPos)(panelInterface, VPanelOf(this->panel), panelX, panelY);
        const int x = cursorX - panelX;

        if (x < this->leftArrowEnd) {
            this->Select(this->selected - 1);
        } else if (x >= this->rightArrowStart) {
            this->Select(this->selected + 1);
        } else {
            for (int i = 0; i < static_cast<int>(this->spans.size()); ++i) {
                if (x >= this->spans[i].first && x < this->spans[i].second)
                    this->Select(i);
            }
        }
    }

private:
    void* panel;
    std::string fontName;
    unsigned long font = 0;
    std::vector<std::string> options;
    int selected = -1;
    // Where things were drawn last paint, for clicks.
    std::vector<std::pair<int, int>> spans;
    int leftArrowEnd = 0;
    int rightArrowStart = 0;
};

void ClickSelector(SelectorImpl* selector)
{
    selector->Click();
}

void PaintSelector(SelectorImpl* selector)
{
    selector->Paint();
}

class ListImpl final : public List {
public:
    ListImpl(void* frame, void* list)
        : frame(frame)
        , list(list)
    {
    }

    ~ListImpl()
    {
        this->Forget();
    }

    void Clear() override
    {
        Virtual<Functions::FrameVoid>(this->list, layout.listRemoveAllItems)(this->list);
        this->Forget();
    }

    int Add(const char* resource) override
    {
        if (!ValidName(resource, MaxResourceLength))
            return -1;
        // Same factory the .res loader uses, so any registered control could be an item.
        void* item = Virtual<Functions::CreateControlByName>(this->frame, layout.createControlByName)(this->frame, "EditablePanel");
        if (!item)
            return -1;

        char path[64];
        std::snprintf(path, sizeof(path), "Resource/UI/BaseModUI/%s.res", resource);
        functions.utlStringSet(&Field<char>(item, layout.panelName), resource);
        Virtual<Functions::FrameBool>(item, layout.setProportional)(item, true);
        Virtual<Functions::LoadControlSettings>(item, layout.loadControlSettings)(item, path, nullptr, nullptr, nullptr);
        Virtual<Functions::ListAddItem>(this->list, layout.listAddItem)(this->list, item, true);
        PatchMouseTree(item);
        listItems[item] = this;
        this->items.push_back(item);
        return static_cast<int>(this->items.size()) - 1;
    }

    int Count() override { return static_cast<int>(this->items.size()); }

    bool SetItemText(int index, const char* control, const char* text) override
    {
        return index >= 0 && index < this->Count() && SetChildText(this->items[index], control, text);
    }

    bool SetItemAvatar(int index, const char* control, unsigned long long steamId) override
    {
        if (index < 0 || index >= this->Count() || !control || !steamId)
            return false;
        if (!this->LoadAvatar(this->items[index], control, steamId)) {
            RequestUser(steamId);
            this->pendingAvatars.push_back({ this->items[index], control, steamId });
        }
        return true;
    }

    void RetryAvatars()
    {
        auto loaded = [this](const PendingAvatar& avatar) { return this->LoadAvatar(avatar.item, avatar.control.c_str(), avatar.steamId); };
        this->pendingAvatars.erase(std::remove_if(this->pendingAvatars.begin(), this->pendingAvatars.end(), loaded), this->pendingAvatars.end());
    }

    bool SetItemBackground(int index, Color color) override
    {
        if (index < 0 || index >= this->Count())
            return false;
        Virtual<Functions::FrameInt>(this->items[index], layout.setBgColor)(this->items[index], NativeColor(color));
        return true;
    }

    bool SetItemTextColor(int index, const char* control, Color color) override
    {
        void* child = index >= 0 && index < this->Count() && control ? functions.findChildByName(this->items[index], control, true) : nullptr;
        if (!child)
            return false;
        Virtual<Functions::FrameInt>(child, layout.setFgColor)(child, NativeColor(color));
        return true;
    }

    int Selected() override
    {
        void* selected = Field<void*>(this->list, layout.listSelectedItem);
        auto found = std::find(this->items.begin(), this->items.end(), selected);
        return found == this->items.end() || !selected ? -1 : static_cast<int>(found - this->items.begin());
    }

    void Select(int index) override
    {
        if (index >= 0 && index < this->Count())
            Virtual<Functions::ListSelectItem>(this->list, layout.listSelectItem)(this->list, static_cast<unsigned short>(index), 1, true, true, false);
    }

    void Select(void* item)
    {
        auto found = std::find(this->items.begin(), this->items.end(), item);
        if (found != this->items.end())
            this->Select(static_cast<int>(found - this->items.begin()));
    }

    void ApplyScheme()
    {
        Virtual<Functions::FrameBool>(this->list, layout.listSetScrollBarVisible)(this->list, true);
        Virtual<Functions::FrameBool>(this->list, layout.listSetScrollArrowsVisible)(this->list, false);
    }

private:
    struct PendingAvatar {
        void* item;
        std::string control;
        uint64_t steamId;
    };

    bool LoadAvatar(void* item, const char* control, uint64_t steamId)
    {
        void* imagePanel = functions.findChildByName(item, control, true);
        void* cache = imagePanel ? functions.getUIGameData() : nullptr;
        void* image = cache ? functions.accessAvatarImage(cache, steamId, AvatarAddReference, AvatarSize) : nullptr;
        if (!image)
            return false;
        Virtual<Functions::FrameVoidPointer>(imagePanel, layout.imagePanelSetImage)(imagePanel, image);
        this->avatars.push_back(steamId);
        return true;
    }

    void Forget()
    {
        for (void* item : this->items)
            listItems.erase(item);
        this->items.clear();
        this->pendingAvatars.clear();
        for (uint64_t steamId : this->avatars)
            pendingAvatarReleases.emplace_back(steamId, AvatarReleaseDelay);
        this->avatars.clear();
    }

    void* frame;
    void* list;
    std::vector<void*> items;
    std::vector<uint64_t> avatars; // references this list holds in the avatar cache
    std::vector<PendingAvatar> pendingAvatars;
};

void SelectItem(ListImpl* list, void* item)
{
    list->Select(item);
}

// ButtonCode_t values that move a grid's selection within a row: the arrow keys, and the first
// controller's d-pad and left stick. The list itself handles Up and Down.
constexpr int KeyLeft = 89;
constexpr int KeyRight = 91;
constexpr int KeyXButtonRight = 243;
constexpr int KeyXButtonLeft = 245;
constexpr int KeyXStick1Right = 258;
constexpr int KeyXStick1Left = 259;
constexpr const char* ImagePanelClassName = ".?AVImagePanel@vgui@@";

// Each list item is a row: a bare EditablePanel the tiles are reparented into and positioned in.
class GridImpl final : public Grid {
public:
    GridImpl(Panel* owner, const char* name, void* frame, void* list, int columns)
        : owner(owner)
        , name(name)
        , frame(frame)
        , list(list)
        , columns(columns)
    {
    }

    ~GridImpl()
    {
        this->Forget();
    }

    void Clear() override
    {
        Virtual<Functions::FrameVoid>(this->list, layout.listRemoveAllItems)(this->list);
        this->Forget();
    }

    int Add(const char* resource) override
    {
        if (!ValidName(resource, MaxResourceLength))
            return -1;
        auto create = Virtual<Functions::CreateControlByName>(this->frame, layout.createControlByName);
        if (this->Count() % this->columns == 0) {
            void* row = create(this->frame, "EditablePanel");
            if (!row)
                return -1;
            Virtual<Functions::ListAddItem>(this->list, layout.listAddItem)(this->list, row, true);
            PatchMouse(row);
            gridRows[row] = this;
            this->rows.push_back(row);
        }
        void* tile = create(this->frame, "EditablePanel");
        if (!tile)
            return -1;

        char path[64];
        std::snprintf(path, sizeof(path), "Resource/UI/BaseModUI/%s.res", resource);
        functions.utlStringSet(&Field<char>(tile, layout.panelName), resource);
        Virtual<Functions::FrameBool>(tile, layout.setProportional)(tile, true);
        Virtual<Functions::LoadControlSettings>(tile, layout.loadControlSettings)(tile, path, nullptr, nullptr, nullptr);
        void* panelInterface = *functions.panelInterface;
        Virtual<Functions::PanelSetParent>(panelInterface, layout.panelSetParent)(panelInterface, VPanelOf(tile), VPanelOf(this->rows.back()));
        PatchMouseTree(tile);
        this->tiles.push_back(tile);
        this->LayoutRow(static_cast<int>(this->rows.size()) - 1);
        return this->Count() - 1;
    }

    int Count() override { return static_cast<int>(this->tiles.size()); }

    bool SetItemText(int index, const char* control, const char* text) override
    {
        return index >= 0 && index < this->Count() && SetChildText(this->tiles[index], control, text);
    }

    bool SetItemImage(int index, const char* control, const char* image) override
    {
        void* child = index >= 0 && index < this->Count() && control && image ? functions.findChildByName(this->tiles[index], control, true) : nullptr;
        if (!child || std::strcmp(ClassName(child), ImagePanelClassName) != 0)
            return false;
        Virtual<Functions::ImagePanelSetImageName>(child, layout.imagePanelSetImageName)(child, image);
        return true;
    }

    bool SetItemBackground(int index, Color color) override
    {
        if (index < 0 || index >= this->Count())
            return false;
        Virtual<Functions::FrameInt>(this->tiles[index], layout.setBgColor)(this->tiles[index], NativeColor(color));
        return true;
    }

    bool SetItemTextColor(int index, const char* control, Color color) override
    {
        void* child = index >= 0 && index < this->Count() && control ? functions.findChildByName(this->tiles[index], control, true) : nullptr;
        if (!child)
            return false;
        Virtual<Functions::FrameInt>(child, layout.setFgColor)(child, NativeColor(color));
        return true;
    }

    int Selected() override { return this->selected >= 0 && this->selected < this->Count() ? this->selected : -1; }

    void Select(int index) override
    {
        if (index < 0 || index >= this->Count())
            return;
        this->selected = index;
        // Selecting the row scrolls it into view and moves focus there for Up and Down.
        Virtual<Functions::ListSelectItem>(this->list, layout.listSelectItem)(this->list, static_cast<unsigned short>(index / this->columns), 1, true, true, false);
    }

    // The list moves between rows by itself (Up and Down); keep the column when it does.
    void Sync()
    {
        void* row = Field<void*>(this->list, layout.listSelectedItem);
        auto found = std::find(this->rows.begin(), this->rows.end(), row);
        if (!row || found == this->rows.end())
            return;
        const int rowIndex = static_cast<int>(found - this->rows.begin());
        const int column = this->selected >= 0 ? this->selected % this->columns : 0;
        if (this->selected < 0 || this->selected / this->columns != rowIndex)
            this->selected = std::min(rowIndex * this->columns + column, this->Count() - 1);
    }

    bool HandleKey(int code)
    {
        const bool left = code == KeyLeft || code == KeyXButtonLeft || code == KeyXStick1Left;
        const bool right = code == KeyRight || code == KeyXButtonRight || code == KeyXStick1Right;
        if ((!left && !right) || this->Selected() < 0)
            return false;
        const int column = this->selected % this->columns;
        if (left && column > 0)
            this->Select(this->selected - 1);
        else if (right && column + 1 < this->columns)
            this->Select(this->selected + 1);
        return true;
    }

    // Selects the tile, and activates it on a double-click. Activating calls into the panel, which
    // can close and so free this grid: it has to be the last thing done.
    void Click(void* tile, bool doubleClick)
    {
        auto found = std::find(this->tiles.begin(), this->tiles.end(), tile);
        if (found == this->tiles.end())
            return;
        const int index = static_cast<int>(found - this->tiles.begin());
        this->Select(index);
        if (doubleClick)
            this->owner->OnItemActivated(this->name.c_str(), index);
    }

    void ApplyScheme()
    {
        Virtual<Functions::FrameBool>(this->list, layout.listSetScrollBarVisible)(this->list, true);
        Virtual<Functions::FrameBool>(this->list, layout.listSetScrollArrowsVisible)(this->list, false);
    }

private:
    // Packs the row's tiles left to right at their own widths, and sizes the row around them.
    void LayoutRow(int row)
    {
        void* panelInterface = *functions.panelInterface;
        auto getSize = Virtual<Functions::PanelGetInts>(panelInterface, layout.panelGetSize);
        auto setPos = Virtual<Functions::PanelSetInts>(panelInterface, layout.panelSetPos);
        auto setSize = Virtual<Functions::PanelSetInts>(panelInterface, layout.panelSetSize);
        int x = 0, rowTall = 0;
        const int last = std::min((row + 1) * this->columns, this->Count());
        for (int i = row * this->columns; i < last; ++i) {
            int wide = 0, tall = 0;
            getSize(panelInterface, VPanelOf(this->tiles[i]), wide, tall);
            setPos(panelInterface, VPanelOf(this->tiles[i]), x, 0);
            x += wide;
            rowTall = std::max(rowTall, tall);
        }
        // The row is as wide as a full one, so partial rows still line up.
        int tileWide = 0, tileTall = 0;
        getSize(panelInterface, VPanelOf(this->tiles[row * this->columns]), tileWide, tileTall);
        setSize(panelInterface, VPanelOf(this->rows[row]), std::max(x, tileWide * this->columns), rowTall);
    }

    void Forget()
    {
        for (void* row : this->rows)
            gridRows.erase(row);
        this->rows.clear();
        this->tiles.clear();
        this->selected = -1;
    }

    Panel* owner;
    std::string name;
    void* frame;
    void* list;
    int columns;
    std::vector<void*> rows;
    std::vector<void*> tiles;
    int selected = -1;
};

void ClickGridTile(GridImpl* grid, void* tile, bool doubleClick)
{
    grid->Click(tile, doubleClick);
}

// What a panel's helpers (Panel::Close and so on) call into.
class FrameImpl final : public Frame {
public:
    explicit FrameImpl(void* frame)
        : frame(frame)
    {
    }

    void* Native() override { return this->frame; }
    void Close() override;
    bool SetControlText(const char* control, const char* text) override;
    List* CreateList(const char* name) override;
    Selector* CreateSelector(const char* name, const char* font) override;
    bool SetListSelection(const char* control, const char* item) override;
    Grid* CreateGrid(const char* name, int columns) override;

private:
    void* frame;
};

struct Instance {
    Panel* panel;
    Support::VtableHook* hooks;
    FrameImpl* access;
    Layer layer;
    // vgui::PHandle, so frames the game deletes without closing them can be noticed.
    int handle;
    bool opened;
    // The frame loads its .res when its scheme is first applied, which can come after it opens;
    // panels are only told they're open once their controls exist.
    bool schemeApplied;
    bool activationPending;
    std::vector<ListImpl*> lists;
    std::vector<GridImpl*> grids;
    std::vector<SelectorImpl*> selectors;
};

// Returns the argument if command is "<name> <argument>", otherwise nullptr.
const char* CommandArgument(const char* command, const char* name)
{
    const size_t length = std::strlen(name);
    if (std::strncmp(command, name, length) != 0 || (command[length] != ' ' && command[length] != '\t'))
        return nullptr;
    const char* argument = command + length;
    while (*argument == ' ' || *argument == '\t')
        ++argument;
    return argument;
}

int SlotFor(Layer layer)
{
    return layer == Layer::Popup ? PopupSlot : PageSlot;
}
}

class MenuSystem {
public:
    static bool loaded;
    static std::unordered_map<std::string, Registration> registrations;
    static std::unordered_map<void*, Instance> instances;
    static std::vector<void*> stacks[2];
    // Stock frame vtables whose OnCommand we patched, and the original it pointed to.
    static std::unordered_map<uintptr_t*, uintptr_t> patchedVtables;
    static uintptr_t stockOnCommand[WindowTypeCount];

    // Custom frames
    MENUS_DECL_DETOUR(void, ApplySchemeSettings, void* scheme);
    MENUS_DECL_DETOUR(void, OnCommand, const char* command);
    MENUS_DECL_DETOUR(void, OnKeyCodePressed, int code);
    MENUS_DECL_DETOUR(void, OnOpen);
    MENUS_DECL_DETOUR(void, OnClose);
    // Stock frames
    MENUS_DECL_DETOUR(void, StockOnCommand, const char* command);

    static void* Manager() { return functions.getPanelSingleton(); }
    static void* SlotHandle(void* manager, int windowType) { return &Field<int>(manager, layout.frames + windowType * sizeof(int)); }
    static void* GetWindow(void* manager, int windowType) { return functions.handleGet(SlotHandle(manager, windowType)); }
    static void SetWindow(void* manager, int windowType, void* frame) { functions.handleSet(SlotHandle(manager, windowType), frame); }
    static std::vector<void*>& StackFor(Layer layer) { return stacks[layer == Layer::Popup ? 1 : 0]; }

    // Mirrors CBaseModPanel::GetActiveWindowType: the highest priority visible active frame.
    static void* ActiveWindow(void* manager)
    {
        for (int priority = WindowPriorityCount - 1; priority >= 0; --priority) {
            const int windowType = Field<int>(manager, layout.activeWindows + priority * sizeof(int));
            if (windowType <= 0 || windowType >= WindowTypeCount)
                continue;
            void* frame = GetWindow(manager, windowType);
            if (frame && Virtual<Functions::FrameIsVisible>(frame, layout.isVisible)(frame))
                return frame;
        }
        return GetWindow(manager, MainMenuWindowType);
    }

    static bool OpenPanel(const char* id, void* caller)
    {
        auto found = registrations.find(id);
        if (found == registrations.end()) {
            Support::Warning("menus: No panel is registered as '%s'.\n", id);
            return false;
        }
        void* manager = Manager();
        if (!manager)
            return false;
        Prune();

        const Registration& registration = found->second;
        Panel* panel = registration.factory();
        if (!panel)
            return false;

        void* frame = functions.allocate(layout.frameSize);
        if (!frame) {
            panel->Release();
            return false;
        }
        functions.frameConstructor(frame, manager, registration.resource.c_str(), true, true, true, true, true);
        // Same constructor tail as every stock BaseModUI frame.
        Virtual<Functions::FrameBool>(frame, layout.setAutoDelete)(frame, true);
        Virtual<Functions::FrameBool>(frame, layout.setProportional)(frame, true);
        functions.setDeleteSelfOnClose(frame, true);
        if (!registration.title.empty())
            functions.setDialogTitle(frame, registration.title.c_str(), nullptr, 0, 0, 0, 0);

        const int slot = SlotFor(registration.layer);
        Field<int>(frame, layout.frameWindowPriority) = CustomWindowPriority;
        Field<int>(frame, layout.frameWindowType) = slot;
        Field<bool>(frame, layout.frameTransitionFlag) = Field<bool>(manager, layout.transitionFlag);
        Virtual<Functions::FrameBool>(frame, layout.setVisible)(frame, false);

        if (registration.footer) {
            void* footer = functions.handleGet(&Field<int>(manager, layout.footer));
            if (footer) {
                // Footer button flags: 1 is A (primary), 2 is B (Back).
                const bool primary = !registration.footerPrimary.empty();
                functions.footerSetButtons(footer, primary ? 3 : 2, 1, 0, 0);
                if (primary)
                    functions.footerSetButtonText(footer, 1, registration.footerPrimary.c_str(), false, 0);
                functions.footerSetButtonText(footer, 2, "#L4D360UI_Back", false, 0);
            }
        }

        auto hooks = new Support::VtableHook(frame, layout.vtableSize);
        hooks->Hook(layout.applySchemeSettings, ApplySchemeSettings_Hook, ApplySchemeSettings);
        hooks->Hook(layout.onCommand, OnCommand_Hook, OnCommand);
        hooks->Hook(layout.onKeyCodePressed, OnKeyCodePressed_Hook, OnKeyCodePressed);
        hooks->Hook(layout.onOpen, OnOpen_Hook, OnOpen);
        hooks->Hook(layout.onClose, OnClose_Hook, OnClose);

        auto access = new FrameImpl(frame);
        panel->frame = access;
        Instance& instance = instances.emplace(frame, Instance { panel, hooks, access, registration.layer, -1, false, false, false, {}, {}, {} }).first->second;
        functions.handleSet(&instance.handle, frame);
        panel->OnCreate();
        StackFor(registration.layer).push_back(frame);

        // OpenWindow only builds frames for the types it knows. With the slot already holding
        // ours it skips that and just activates it.
        SetWindow(manager, slot, frame);
        int windowType = slot;
        void* opened = functions.openWindow(manager, &windowType, caller, registration.layer == Layer::Page, nullptr);
        if (opened != frame) {
            Support::Warning("menus: The game refused to open '%s'.\n", id);
            Virtual<Functions::FrameVoid>(frame, layout.onClose)(frame);
            return false;
        }
        return true;
    }

    static bool OpenNativeWindow(int windowType, void* caller)
    {
        void* manager = Manager();
        if (!manager || windowType <= 0 || windowType >= WindowTypeCount || windowType == PageSlot || windowType == PopupSlot)
            return false;
        return functions.openWindow(manager, &windowType, caller, true, nullptr) != nullptr;
    }

    // Tells the panel it's the active window (and that it's open, the first time).
    static void Activate(void* frame)
    {
        auto found = instances.find(frame);
        if (found != instances.end() && !found->second.opened) {
            found->second.opened = true;
            found->second.panel->OnOpen();
            found = instances.find(frame); // OnOpen may have closed it
        }
        if (found != instances.end())
            found->second.panel->OnActivate();
    }

    static void NavigateBack(void* frame)
    {
        Virtual<Functions::FrameVoid>(frame, layout.navigateBack)(frame);
    }

    static bool DispatchCommand(void* caller, const char* command)
    {
        if (!command)
            return false;
        if (const char* id = CommandArgument(command, OpenCommand)) {
            if (ValidName(id, 63))
                OpenPanel(id, caller);
            return true;
        }
        if (const char* windowType = CommandArgument(command, OpenNativeCommand)) {
            OpenNativeWindow(std::atoi(windowType), caller);
            return true;
        }
        if (!std::strcmp(command, BackCommand)) {
            NavigateBack(caller);
            return true;
        }
        return false;
    }

    static void Destroy(void* frame, bool frameAlive)
    {
        auto found = instances.find(frame);
        if (found == instances.end())
            return;
        Instance instance = found->second;
        instances.erase(found);

        instance.panel->OnClose();
        instance.panel->frame = nullptr;
        instance.panel->Release();
        delete instance.access;
        // The native lists and selector panels are children of the frame and go with it.
        for (ListImpl* list : instance.lists)
            delete list;
        for (GridImpl* grid : instance.grids)
            delete grid;
        for (SelectorImpl* selector : instance.selectors) {
            for (auto it = selectors.begin(); it != selectors.end();)
                it = it->second == selector ? selectors.erase(it) : std::next(it);
            delete selector;
        }
        if (!frameAlive)
            instance.hooks->Forget();
        delete instance.hooks;
    }

    // Forgets frames the game deleted without closing them.
    static void Prune()
    {
        std::vector<void*> dead;
        for (auto& item : instances) {
            if (!functions.handleGet(&item.second.handle))
                dead.push_back(item.first);
        }
        for (void* frame : dead) {
            for (auto& stack : stacks)
                stack.erase(std::remove(stack.begin(), stack.end(), frame), stack.end());
            Destroy(frame, false);
        }
    }

    // Forgets every panel without touching its (already deleted) frame.
    static void Abandon()
    {
        std::vector<void*> frames;
        for (auto& item : instances)
            frames.push_back(item.first);
        for (void* frame : frames)
            Destroy(frame, false);
        for (auto& stack : stacks)
            stack.clear();
    }

    // Presses Back on custom panels until a stock one is active again, so closing them doesn't
    // leave the menus with nothing showing.
    static void BackOut()
    {
        void* manager = Manager();
        const size_t open = instances.size();
        for (size_t presses = 0; manager && presses < open; ++presses) {
            void* active = ActiveWindow(manager);
            if (!active || !instances.count(active))
                break;
            NavigateBack(active);
        }
    }

    static void CloseAll()
    {
        for (auto& stack : stacks) {
            const std::vector<void*> frames = stack;
            for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
                if (instances.count(*it))
                    Virtual<Functions::FrameVoid>(*it, layout.onClose)(*it);
            }
        }
    }

    // Lets "menus_open" work from stock panels. Their OnCommand overrides swallow commands they
    // don't know (Options does), so each class's vtable is patched the first time one is seen.
    static void PatchStockWindows(void* manager)
    {
        for (int windowType = 0; windowType < WindowTypeCount; ++windowType) {
            void* frame = GetWindow(manager, windowType);
            if (!frame || instances.count(frame))
                continue;

            uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(frame);
            auto patched = patchedVtables.find(vtable);
            if (patched != patchedVtables.end()) {
                stockOnCommand[windowType] = patched->second;
                continue;
            }

            uintptr_t* entry = &vtable[layout.onCommand];
            Support::UnProtect(entry, sizeof(uintptr_t));
            patchedVtables[vtable] = stockOnCommand[windowType] = *entry;
            *entry = reinterpret_cast<uintptr_t>(StockOnCommand_Hook);
        }
    }

    static void RestoreStockWindows()
    {
        for (auto& item : patchedVtables)
            item.first[layout.onCommand] = item.second;
        patchedVtables.clear();
        std::fill(std::begin(stockOnCommand), std::end(stockOnCommand), 0);
    }
};

bool MenuSystem::loaded = false;
std::unordered_map<std::string, Registration> MenuSystem::registrations;
std::unordered_map<void*, Instance> MenuSystem::instances;
std::vector<void*> MenuSystem::stacks[2];
std::unordered_map<uintptr_t*, uintptr_t> MenuSystem::patchedVtables;
uintptr_t MenuSystem::stockOnCommand[WindowTypeCount] = {};

MENUS_REDECL(MenuSystem::ApplySchemeSettings);
MENUS_REDECL(MenuSystem::OnCommand);
MENUS_REDECL(MenuSystem::OnKeyCodePressed);
MENUS_REDECL(MenuSystem::OnOpen);
MENUS_REDECL(MenuSystem::OnClose);
MENUS_REDECL(MenuSystem::StockOnCommand);

MENUS_DETOUR(void, MenuSystem::ApplySchemeSettings, void* scheme)
{
    // This is also where the frame loads its .res, styling the lists created in OnCreate.
    ApplySchemeSettings(thisptr, scheme);
    auto found = instances.find(thisptr);
    if (found != instances.end()) {
        for (ListImpl* list : found->second.lists)
            list->ApplyScheme();
        for (GridImpl* grid : found->second.grids)
            grid->ApplyScheme();
        for (SelectorImpl* selector : found->second.selectors)
            selector->ApplyScheme(scheme);
        if (!found->second.schemeApplied) {
            found->second.schemeApplied = true;
            if (found->second.activationPending) {
                found->second.activationPending = false;
                Activate(thisptr);
            }
        }
    }
}

MENUS_DETOUR(void, MenuSystem::OnCommand, const char* command)
{
    if (DispatchCommand(thisptr, command))
        return;
    auto found = instances.find(thisptr);
    if (found != instances.end() && found->second.panel->OnCommand(command))
        return;
    OnCommand(thisptr, command);
}

MENUS_DETOUR(void, MenuSystem::OnKeyCodePressed, int code)
{
    auto found = instances.find(thisptr);
    if (found != instances.end()) {
        for (GridImpl* grid : found->second.grids) {
            if (grid->HandleKey(code))
                return;
        }
        if (found->second.panel->OnKeyCodePressed(code))
            return;
    }
    OnKeyCodePressed(thisptr, code);
}

MENUS_DETOUR(void, MenuSystem::OnOpen)
{
    OnOpen(thisptr);
    auto found = instances.find(thisptr);
    if (found == instances.end())
        return;
    if (found->second.schemeApplied)
        Activate(thisptr);
    else
        found->second.activationPending = true;
}

MENUS_DETOUR(void, MenuSystem::OnClose)
{
    auto found = instances.find(thisptr);
    void* manager = Manager();
    if (found == instances.end() || !manager) {
        OnClose(thisptr);
        Destroy(thisptr, true);
        return;
    }

    const Layer layer = found->second.layer;
    const int slot = SlotFor(layer);
    auto& stack = StackFor(layer);
    stack.erase(std::remove(stack.begin(), stack.end(), thisptr), stack.end());

    bool& closingAll = Field<bool>(manager, layout.closingAllWindows);
    const bool wasClosingAll = closingAll;
    void* owner = GetWindow(manager, slot);
    if (owner == thisptr || wasClosingAll) {
        OnClose(thisptr);
    } else {
        // A hidden panel closing on its own. OnFrameClosed would clear the slot and the active
        // window out from under the panel that owns them, unless it thinks everything is closing.
        SetWindow(manager, slot, thisptr);
        closingAll = true;
        OnClose(thisptr);
        closingAll = false;
        SetWindow(manager, slot, owner);
    }
    Destroy(thisptr, true);

    if (wasClosingAll) {
        // CloseAllWindows only reaches the panel bound to each slot; close the hidden ones too.
        CloseAll();
    } else if (owner == thisptr && !stack.empty()) {
        // NavigateBack activates the previous panel right after this returns, through the slot.
        SetWindow(manager, slot, stack.back());
    }
}

MENUS_DETOUR(void, MenuSystem::StockOnCommand, const char* command)
{
    if (DispatchCommand(thisptr, command)) {
        functions.releaseActiveMenuControl(0);
        return;
    }

    uintptr_t original = 0;
    auto patched = patchedVtables.find(*reinterpret_cast<uintptr_t**>(thisptr));
    if (patched != patchedVtables.end()) {
        original = patched->second;
    } else {
        // Someone gave this frame its own vtable copy after we patched its class.
        const int windowType = Field<int>(thisptr, layout.frameWindowType);
        if (windowType >= 0 && windowType < WindowTypeCount)
            original = stockOnCommand[windowType];
    }
    if (original)
        reinterpret_cast<_StockOnCommand>(original)(thisptr, command);
}

void FrameImpl::Close()
{
    if (this->frame && MenuSystem::loaded)
        MenuSystem::NavigateBack(this->frame);
}

bool FrameImpl::SetListSelection(const char* control, const char* item)
{
    void* button = MenuSystem::loaded && control && item ? functions.findChildByName(this->frame, control, true) : nullptr;
    if (!button || std::strcmp(ClassName(button), HybridButtonClassName) != 0)
        return false;
    functions.hybridButtonSetCurrentSelection(button, item);
    return true;
}

bool FrameImpl::SetControlText(const char* control, const char* text)
{
    return MenuSystem::loaded && SetChildText(this->frame, control, text);
}

List* FrameImpl::CreateList(const char* name)
{
    auto found = this->frame ? MenuSystem::instances.find(this->frame) : MenuSystem::instances.end();
    if (found == MenuSystem::instances.end() || found->second.opened || !ValidName(name, 63))
        return nullptr;
    void* list = functions.allocate(layout.listSize);
    if (!list)
        return nullptr;
    // Same arguments the stock leaderboard uses for its lists.
    functions.listConstructor(list, this->frame, name, 5, true);
    ListImpl* wrapper = new ListImpl(this->frame, list);
    found->second.lists.push_back(wrapper);
    return wrapper;
}

Grid* FrameImpl::CreateGrid(const char* name, int columns)
{
    auto found = this->frame ? MenuSystem::instances.find(this->frame) : MenuSystem::instances.end();
    if (found == MenuSystem::instances.end() || found->second.opened || !ValidName(name, 63) || columns < 1)
        return nullptr;
    void* list = functions.allocate(layout.listSize);
    if (!list)
        return nullptr;
    functions.listConstructor(list, this->frame, name, 5, true);
    GridImpl* grid = new GridImpl(found->second.panel, name, this->frame, list, columns);
    found->second.grids.push_back(grid);
    return grid;
}

Selector* FrameImpl::CreateSelector(const char* name, const char* font)
{
    auto found = this->frame ? MenuSystem::instances.find(this->frame) : MenuSystem::instances.end();
    if (found == MenuSystem::instances.end() || found->second.opened || !ValidName(name, 63) || !font)
        return nullptr;
    void* panel = Virtual<Functions::CreateControlByName>(this->frame, layout.createControlByName)(this->frame, "Panel");
    if (!panel)
        return nullptr;

    // Named and parented now so the frame's .res finds it and applies its settings.
    functions.utlStringSet(&Field<char>(panel, layout.panelName), name);
    Virtual<Functions::FrameBool>(panel, layout.setProportional)(panel, true);
    void* panelInterface = *functions.panelInterface;
    Virtual<Functions::PanelSetParent>(panelInterface, layout.panelSetParent)(panelInterface, VPanelOf(panel), VPanelOf(this->frame));
    PatchVirtual(panel, layout.paint, reinterpret_cast<uintptr_t>(PanelPaint_Hook), paintVtables);
    PatchMouse(panel);

    SelectorImpl* selector = new SelectorImpl(panel, font);
    selectors[panel] = selector;
    found->second.selectors.push_back(selector);
    return selector;
}

namespace {
class SystemImpl final : public System {
public:
    bool Init() override;
    void Shutdown() override;
    bool IsAvailable() override;
    void Tick() override;
    bool Register(const PanelDesc& desc) override;
    bool Unregister(const char* id) override;
    bool Open(const char* id) override;
    bool OpenNative(int windowType) override;
    bool Back() override;
    void Dump() override;
};

SystemImpl menuSystem;
}

bool SystemImpl::Init()
{
    if (MenuSystem::loaded)
        return true;
    if (!Native::Resolve(MENUS_MODULE("client"))) {
        Support::Warning("menus: BaseModUI integration is unavailable on this game build; custom panels are disabled.\n");
        return false;
    }
    return MenuSystem::loaded = true;
}

void SystemImpl::Shutdown()
{
    if (!MenuSystem::loaded)
        return;
    if (*functions.vgui) {
        MenuSystem::Prune();
        MenuSystem::BackOut();
        MenuSystem::CloseAll();
    } else {
        // The game is quitting: vgui shut down and deleted every window before unloading plugins,
        // so there's nothing left to close and nothing in vgui to call.
        MenuSystem::Abandon();
    }
    MenuSystem::RestoreStockWindows();
    RestoreMouse();
    // Frames closed just now may still paint, so their avatars are left referenced.
    pendingAvatarReleases.clear();
    MenuSystem::registrations.clear();
    MenuSystem::loaded = false;
}

bool SystemImpl::IsAvailable()
{
    return MenuSystem::loaded;
}

void SystemImpl::Tick()
{
    if (!MenuSystem::loaded)
        return;
    void* manager = MenuSystem::Manager();
    if (!manager)
        return;
    MenuSystem::Prune();
    MenuSystem::PatchStockWindows(manager);
    ReleasePendingAvatars();

    static int avatarTicks = 0;
    if (++avatarTicks >= AvatarRetryTicks) {
        avatarTicks = 0;
        for (auto& item : MenuSystem::instances) {
            for (ListImpl* list : item.second.lists)
                list->RetryAvatars();
        }
    }
    for (auto& item : MenuSystem::instances) {
        for (GridImpl* grid : item.second.grids)
            grid->Sync();
    }

    // A panel's OnTick can close panels, so walk a copy.
    std::vector<void*> frames;
    for (auto& item : MenuSystem::instances)
        frames.push_back(item.first);
    for (void* frame : frames) {
        auto found = MenuSystem::instances.find(frame);
        if (found != MenuSystem::instances.end() && found->second.opened)
            found->second.panel->OnTick();
    }
}

bool SystemImpl::Register(const PanelDesc& desc)
{
    const char* resource = desc.resource ? desc.resource : desc.id;
    if (!MenuSystem::loaded || !ValidName(desc.id, 63) || !ValidName(resource, MaxResourceLength) || !desc.factory)
        return false;
    return MenuSystem::registrations.emplace(desc.id, Registration { resource, desc.title ? desc.title : "", desc.factory, desc.footer, desc.layer, desc.footerPrimary ? desc.footerPrimary : "" }).second;
}

bool SystemImpl::Unregister(const char* id)
{
    return id && MenuSystem::registrations.erase(id) != 0;
}

bool SystemImpl::Open(const char* id)
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    return manager && id && MenuSystem::OpenPanel(id, MenuSystem::ActiveWindow(manager));
}

bool SystemImpl::OpenNative(int windowType)
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    return manager && MenuSystem::OpenNativeWindow(windowType, MenuSystem::ActiveWindow(manager));
}

bool SystemImpl::Back()
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    void* active = manager ? MenuSystem::ActiveWindow(manager) : nullptr;
    if (!active)
        return false;
    MenuSystem::NavigateBack(active);
    return true;
}

void SystemImpl::Dump()
{
    void* manager = MenuSystem::loaded ? MenuSystem::Manager() : nullptr;
    if (!manager) {
        Support::Print("menus: unavailable\n");
        return;
    }

    Support::Print("menus: active window %p, closing all %d\n", MenuSystem::ActiveWindow(manager), Field<bool>(manager, layout.closingAllWindows));
    Support::Print("  list items %d, mouse-patched classes %d\n", static_cast<int>(listItems.size()), static_cast<int>(mouseVtables.size()));
    for (int priority = 0; priority < WindowPriorityCount; ++priority) {
        const int windowType = Field<int>(manager, layout.activeWindows + priority * sizeof(int));
        if (windowType)
            Support::Print("  active[%d] = type %d\n", priority, windowType);
    }
    for (int windowType = 0; windowType < WindowTypeCount; ++windowType) {
        void* frame = MenuSystem::GetWindow(manager, windowType);
        if (frame)
            Support::Print("  slot %2d: %p%s\n", windowType, frame, MenuSystem::instances.count(frame) ? " (custom)" : "");
    }
    const char* names[] = { "pages", "popups" };
    for (int layer = 0; layer < 2; ++layer) {
        Support::Print("  %s:", names[layer]);
        for (void* frame : MenuSystem::stacks[layer])
            Support::Print(" %p", frame);
        Support::Print("\n");
    }
}


}

MENUS_EXPORT Menus::System* CreateMenuSystem(const char* version)
{
    return version && !std::strcmp(version, Menus::InterfaceVersion) ? &Menus::menuSystem : nullptr;
}
