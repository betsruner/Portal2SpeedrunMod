#include "SpeedrunMenus.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#include "Cheats.hpp"
#include "Leaderboards.hpp"
#include "Modules/Engine.hpp"

namespace {
// vgui ButtonCode_t for the controller A button and the Steam Controller's, which the footer's
// primary button also sends.
constexpr int KeyXButtonA = 114;
constexpr int KeySteamControllerA = 306;

bool IsConfirmKey(int code)
{
    return code == KeyXButtonA || code == KeySteamControllerA;
}

template <typename T>
Menus::Panel* Make()
{
    return new T();
}

// Single player challenge mode maps, by chapter, with their board.portal2.sr chamber (Steam leaderboard) ids.
struct ChallengeMap {
    const char* name;
    const char* bsp;
    int chamberId;
};

const ChallengeMap ChapterOneMaps[] = {
    { "Container Ride", "sp_a1_intro1", 62761 }, { "Portal Carousel", "sp_a1_intro2", 62758 },
    { "Portal Gun", "sp_a1_intro3", 47458 }, { "Smooth Jazz", "sp_a1_intro4", 47455 },
    { "Cube Momentum", "sp_a1_intro5", 47452 }, { "Future Starter", "sp_a1_intro6", 47106 },
    { "Secret Panel", "sp_a1_intro7", 62763 }, { "Wakeup", "sp_a1_wakeup", 62759 },
    { "Incinerator", "sp_a2_intro", 47735 },
};
const ChallengeMap ChapterTwoMaps[] = {
    { "Laser Intro", "sp_a2_laser_intro", 62765 }, { "Laser Stairs", "sp_a2_laser_stairs", 47736 },
    { "Dual Lasers", "sp_a2_dual_lasers", 47738 }, { "Laser Over Goo", "sp_a2_laser_over_goo", 47742 },
    { "Catapult Intro", "sp_a2_catapult_intro", 62767 }, { "Trust Fling", "sp_a2_trust_fling", 47744 },
    { "Pit Flings", "sp_a2_pit_flings", 47465 }, { "Fizzler Intro", "sp_a2_fizzler_intro", 47746 },
};
const ChallengeMap ChapterThreeMaps[] = {
    { "Ceiling Catapult", "sp_a2_sphere_peek", 47748 }, { "Ricochet", "sp_a2_ricochet", 47751 },
    { "Bridge Intro", "sp_a2_bridge_intro", 47752 }, { "Bridge the Gap", "sp_a2_bridge_the_gap", 47755 },
    { "Turret Intro", "sp_a2_turret_intro", 47756 }, { "Laser Relays", "sp_a2_laser_relays", 47759 },
    { "Turret Blocker", "sp_a2_turret_blocker", 47760 }, { "Laser vs Turret", "sp_a2_laser_vs_turret", 47763 },
    { "Pull the Rug", "sp_a2_pull_the_rug", 47764 },
};
const ChallengeMap ChapterFourMaps[] = {
    { "Column Blocker", "sp_a2_column_blocker", 47766 }, { "Laser Chaining", "sp_a2_laser_chaining", 47768 },
    { "Triple Laser", "sp_a2_triple_laser", 47770 }, { "Jail Break", "sp_a2_bts1", 47773 },
    { "Escape", "sp_a2_bts2", 47774 },
};
const ChallengeMap ChapterFiveMaps[] = {
    { "Turret Factory", "sp_a2_bts3", 47776 }, { "Turret Sabotage", "sp_a2_bts4", 47779 },
    { "Neurotoxin Sabotage", "sp_a2_bts5", 47780 }, { "Core", "sp_a2_core", 62771 },
};
const ChallengeMap ChapterSixMaps[] = {
    { "Underground", "sp_a3_01", 47783 }, { "Cave Johnson", "sp_a3_03", 47784 },
    { "Repulsion Intro", "sp_a3_jump_intro", 47787 }, { "Bomb Flings", "sp_a3_bomb_flings", 47468 },
    { "Crazy Box", "sp_a3_crazy_box", 47469 }, { "PotatOS", "sp_a3_transition01", 47472 },
};
const ChallengeMap ChapterSevenMaps[] = {
    { "Propulsion Intro", "sp_a3_speed_ramp", 47791 }, { "Propulsion Flings", "sp_a3_speed_flings", 47793 },
    { "Conversion Intro", "sp_a3_portal_intro", 47795 }, { "Three Gels", "sp_a3_end", 47798 },
};
const ChallengeMap ChapterEightMaps[] = {
    { "Test", "sp_a4_intro", 88350 }, { "Funnel Intro", "sp_a4_tb_intro", 47800 },
    { "Ceiling Button", "sp_a4_tb_trust_drop", 47802 }, { "Wall Button", "sp_a4_tb_wall_button", 47804 },
    { "Polarity", "sp_a4_tb_polarity", 47806 }, { "Funnel Catch", "sp_a4_tb_catch", 47808 },
    { "Stop the Box", "sp_a4_stop_the_box", 47811 }, { "Laser Catapult", "sp_a4_laser_catapult", 47813 },
    { "Laser Platform", "sp_a4_laser_platform", 47815 }, { "Propulsion Catch", "sp_a4_speed_tb_catch", 47817 },
    { "Repulsion Polarity", "sp_a4_jump_polarity", 47819 },
};
const ChallengeMap ChapterNineMaps[] = {
    { "Finale 1", "sp_a4_finale1", 62776 }, { "Finale 2", "sp_a4_finale2", 47821 },
    { "Finale 3", "sp_a4_finale3", 47824 }, { "Finale 4", "sp_a4_finale4", 47456 },
};

struct Chapter {
    const ChallengeMap* maps;
    int count;
};

#define CHAPTER(maps) { maps, static_cast<int>(std::size(maps)) }
const Chapter Chapters[] = {
    CHAPTER(ChapterOneMaps), CHAPTER(ChapterTwoMaps), CHAPTER(ChapterThreeMaps),
    CHAPTER(ChapterFourMaps), CHAPTER(ChapterFiveMaps), CHAPTER(ChapterSixMaps),
    CHAPTER(ChapterSevenMaps), CHAPTER(ChapterEightMaps), CHAPTER(ChapterNineMaps),
};
constexpr int ChapterCount = static_cast<int>(std::size(Chapters));
// board.portal2.sr returns the top 500; building that many rows at once hitches the menu.
constexpr int MaxLeaderboardEntries = 200;

// Leaderboard categories. Any% is what board.portal2.sr serves; the rest are mock-ups until real
// category data exists (see MockCategory).
const char* const Categories[] = { "ANY%", "INBOUNDS", "NO SLA", "GLITCHLESS" };
constexpr int CategoryCount = static_cast<int>(std::size(Categories));

// Fake board for a mock category: the Any% runners, some of them dropped, with slower times.
std::vector<Leaderboards::Entry> MockCategory(const std::vector<Leaderboards::Entry>& anyPercent, int category)
{
    std::vector<Leaderboards::Entry> entries;
    for (const auto& entry : anyPercent) {
        if ((entry.steamId >> category) % 4 == 0)
            continue;
        Leaderboards::Entry mock = entry;
        mock.centiseconds = entry.centiseconds * (100 + 9 * category) / 100 + static_cast<int>(entry.steamId % 97) * category;
        entries.push_back(mock);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.centiseconds < b.centiseconds; });
    for (size_t i = 0; i < entries.size(); ++i)
        entries[i].rank = i && entries[i].centiseconds == entries[i - 1].centiseconds ? entries[i - 1].rank : static_cast<int>(i) + 1;
    return entries;
}

// The stock map list's selected row: HybridButton.CursorColor and HybridButton.FocusColor in basemodui_scheme.res.
constexpr Menus::Color SelectedRowColor { 0, 0, 0, 240 };
constexpr Menus::Color SelectedTextColor { 240, 240, 240, 255 };
constexpr Menus::Color RowColor { 0, 0, 0, 0 };
constexpr Menus::Color TextColor { 0, 0, 0, 255 };

// Colors a Menus::List or Menus::Grid like the stock map list. Runtime colors don't survive a
// scheme reload, so this runs every tick.
template <typename Items>
void HighlightSelected(Items* items, const char* label)
{
    const int selected = items->Selected();
    for (int i = 0; i < items->Count(); ++i) {
        items->SetItemBackground(i, i == selected ? SelectedRowColor : RowColor);
        items->SetItemTextColor(i, label, i == selected ? SelectedTextColor : TextColor);
    }
}

// Challenge Mode: chapter selector, the chapter's maps, and the selected map's board.portal2.sr leaderboard.
class ChallengeModePanel : public Menus::Panel {
public:
    void OnCreate() override
    {
        this->maps = this->CreateList("MapList");
        this->leaderboard = this->CreateList("LeaderboardList");
        this->categories = this->CreateSelector("CategorySelector");
    }

    void OnOpen() override
    {
        if (this->categories) {
            this->categories->SetOptions(Categories, CategoryCount);
            this->categories->Select(0);
        }
        std::vector<int> chambers;
        for (auto& chapter : Chapters) {
            for (int i = 0; i < chapter.count; ++i)
                chambers.push_back(chapter.maps[i].chamberId);
        }
        Leaderboards::Prefetch(chambers);
        this->ShowChapter(0);
    }

    bool OnCommand(const char* command) override
    {
        int chapter = 0;
        if (std::sscanf(command, "SelectedChapter:%d", &chapter) == 1) {
            this->ShowChapter(chapter - 1);
            return true;
        }
        return false;
    }

    bool OnKeyCodePressed(int code) override
    {
        if (!IsConfirmKey(code))
            return false;
        const int map = this->maps ? this->maps->Selected() : -1;
        if (map >= 0) {
            char command[64];
            std::snprintf(command, sizeof(command), "map %s\n", Chapters[this->chapter].maps[map].bsp);
            engine->Cbuf_AddText(0, command, 0);
        }
        return true;
    }

    void OnTick() override
    {
        if (!this->maps)
            return;
        const int selected = this->maps->Selected();
        const int category = this->categories ? this->categories->Selected() : 0;
        if (selected != this->shownMap || category != this->shownCategory)
            this->ShowLeaderboard(selected);
        if (this->shownMap >= 0 && this->filledMap != this->shownMap)
            this->TryFillLeaderboard();
        HighlightSelected(this->maps, "LblMapName");
    }

private:
    void ShowChapter(int chapter)
    {
        if (!this->maps || chapter < 0 || chapter >= ChapterCount)
            return;
        this->chapter = chapter;
        this->maps->Clear();
        for (int i = 0; i < Chapters[chapter].count; ++i) {
            const int item = this->maps->Add("P2SMChallengeMapItem");
            this->maps->SetItemText(item, "LblMapName", Chapters[chapter].maps[i].name);
        }
        this->maps->Select(0);
        this->ShowLeaderboard(this->maps->Selected());
    }

    void ShowLeaderboard(int map)
    {
        this->shownMap = map;
        this->shownCategory = this->categories ? this->categories->Selected() : 0;
        this->filledMap = -1;
        if (this->leaderboard)
            this->leaderboard->Clear();
        this->SetControlText("LblLeaderboardStatus", map >= 0 ? "Loading leaderboard..." : "");
        if (map >= 0)
            Leaderboards::Request(Chapters[this->chapter].maps[map].chamberId);
    }

    void TryFillLeaderboard()
    {
        std::vector<Leaderboards::Entry> entries;
        switch (Leaderboards::Get(Chapters[this->chapter].maps[this->shownMap].chamberId, entries)) {
        case Leaderboards::State::Failed:
            this->filledMap = this->shownMap;
            this->SetControlText("LblLeaderboardStatus", "Couldn't reach board.portal2.sr.");
            return;
        case Leaderboards::State::Ready:
            break;
        default:
            return;
        }

        this->filledMap = this->shownMap;
        if (this->shownCategory > 0)
            entries = MockCategory(entries, this->shownCategory);
        this->SetControlText("LblLeaderboardStatus", entries.empty() ? "No times yet." : "");
        if (!this->leaderboard)
            return;
        const int count = std::min(static_cast<int>(entries.size()), MaxLeaderboardEntries);
        for (int i = 0; i < count; ++i) {
            const Leaderboards::Entry& entry = entries[i];
            char time[16], rank[16];
            std::snprintf(time, sizeof(time), "%d:%02d.%02d", entry.centiseconds / 6000, entry.centiseconds / 100 % 60, entry.centiseconds % 100);
            std::snprintf(rank, sizeof(rank), "#%d", entry.rank);

            const int item = this->leaderboard->Add("P2SMLeaderboardEntry");
            this->leaderboard->SetItemText(item, "LblPlayerName", entry.name.c_str());
            this->leaderboard->SetItemText(item, "LblTime", time);
            this->leaderboard->SetItemText(item, "LblRank", rank);
            this->leaderboard->SetItemAvatar(item, "ImgAvatar", entry.steamId);
        }
    }

    Menus::List* maps = nullptr;
    Menus::List* leaderboard = nullptr;
    Menus::Selector* categories = nullptr;
    int chapter = 0;
    int shownMap = -1;
    int shownCategory = 0;
    int filledMap = -1;
};

// Full Game: a grid of the playable categories. Credits, the leaderboard link and the like live
// in the stock Extras panel (scripts/extras.txt) instead.
struct FullGameCategory {
    const char* name;
    const char* image; // relative to materials/vgui/
    int mode; // sm_mode
    const char* map;
};

const FullGameCategory FullGameCategories[] = {
    { "Normal", "../srmod/menu_modes/normal", 0, "sp_a1_intro1" },
    { "Celeste Mode", "../srmod/menu_modes/celeste", 2, "sp_a1_intro1" },
    { "Reverse Mod", "../srmod/menu_modes/reverse", 4, "sp_a4_finale4" },
    { "Floor is Lava", "../srmod/menu_modes/floorislava", 5, "sp_a1_intro1" },
    { "Odyssey Mod", "../srmod/menu_modes/smo", 6, "sp_a1_intro1" },
    { "Fog% (beta)", "../srmod/menu_modes/fog", 1, "sp_a1_intro1" },
};

class FullGamePanel : public Menus::Panel {
public:
    void OnCreate() override
    {
        this->grid = this->CreateGrid("CategoryGrid", 3);
    }

    void OnOpen() override
    {
        if (!this->grid)
            return;
        for (const FullGameCategory& category : FullGameCategories) {
            const int tile = this->grid->Add("P2SMCategoryTile");
            this->grid->SetItemText(tile, "LblCategory", category.name);
            this->grid->SetItemImage(tile, "ImgCategory", category.image);
        }
        this->grid->Select(0);
    }

    bool OnKeyCodePressed(int code) override
    {
        if (!IsConfirmKey(code))
            return false;
        if (this->grid)
            this->Start(this->grid->Selected());
        return true;
    }

    void OnItemActivated(const char* control, int index) override
    {
        this->Start(index);
    }

    void OnTick() override
    {
        if (this->grid)
            HighlightSelected(this->grid, "LblCategory");
    }

private:
    void Start(int index)
    {
        if (index < 0 || index >= static_cast<int>(std::size(FullGameCategories)))
            return;
        char command[64];
        std::snprintf(command, sizeof(command), "sm_mode %d;map %s\n", FullGameCategories[index].mode, FullGameCategories[index].map);
        engine->Cbuf_AddText(0, command, 0);
    }

    Menus::Grid* grid = nullptr;
};

// Options > Speedrun Mod rows: stock on/off list buttons, each backed by an archived cvar.
// Must match the "list" blocks in P2SMSettings.res.
struct SettingToggle {
    const char* control;
    Variable* variable;
    const char* enableCommand;
    const char* disableCommand;
};

const SettingToggle SettingToggles[] = {
    { "DrpDialogue", &sm_dialogue, "DialogueEnabled", "DialogueDisabled" },
    { "DrpFastStart", &sm_fast_start, "FastStartEnabled", "FastStartDisabled" },
};

class SettingsPanel : public Menus::Panel {
public:
    void OnActivate() override
    {
        for (const SettingToggle& toggle : SettingToggles)
            this->SetListSelection(toggle.control, toggle.variable->GetBool() ? "#L4D360UI_Enabled" : "#L4D360UI_Disabled");
    }

    bool OnCommand(const char* command) override
    {
        for (const SettingToggle& toggle : SettingToggles) {
            if (!std::strcmp(command, toggle.enableCommand) || !std::strcmp(command, toggle.disableCommand)) {
                toggle.variable->SetValue(!std::strcmp(command, toggle.enableCommand) ? 1 : 0);
                return true;
            }
        }
        return false;
    }
};

}

void RegisterSpeedrunMenus(Menus::System* menus)
{
    // Main menu "Speedrun Mod" button -> pick Full Game or Challenge Mode. Its buttons only open
    // other panels, which .res commands do by themselves.
    menus->Register({ "p2sm_mode_select", "P2SMModeSelect", "Speedrun Mod", Make<Menus::Panel> });
    menus->Register({ "p2sm_full_game", "P2SMFullGame", "Full Game", Make<FullGamePanel>, true, Menus::Layer::Page, "#L4D360UI_Start" });
    // The stock leaderboard's strings live in portal2_dlc1, which the mod doesn't search, so use plain text.
    menus->Register({ "p2sm_challenge_mode", "P2SMChallengeMode", "Leaderboards", Make<ChallengeModePanel>, true, Menus::Layer::Page, "#PORTAL2_PlaySelectedMap" });
    // Options -> Speedrun Mod settings.
    menus->Register({ "p2sm_settings", "P2SMSettings", "Speedrun Mod", Make<SettingsPanel> });
}
