#include "Cheats.hpp"

#include <cstring>

#include "Modules/Console.hpp"
#include "Modules/Engine.hpp"
#include "Modules/Client.hpp"

#include "SMSM.hpp"


Variable sv_transition_fade_time;
Variable ui_loadingscreen_transition_time;
Variable ui_loadingscreen_fadein_time;
Variable ui_loadingscreen_mintransition_time;
Variable sv_player_collide_with_laser;
Variable portalgun_fire_delay;

Variable sv_cheats;
Variable crosshair;
Variable viewmodel_offset_z;

// Options > Speedrun Mod
Variable sm_dialogue("sm_dialogue", "1", "Plays in-game dialogue.\n", FCVAR_ARCHIVE);
Variable sm_fast_start("sm_fast_start", "0", "Skips the slow wakeup at the start of the game.\n", FCVAR_ARCHIVE);

void Cheats::Init()
{
    sv_transition_fade_time = Variable("sv_transition_fade_time");
    ui_loadingscreen_transition_time = Variable("ui_loadingscreen_transition_time");
    ui_loadingscreen_fadein_time = Variable("ui_loadingscreen_fadein_time");
    ui_loadingscreen_mintransition_time = Variable("ui_loadingscreen_mintransition_time");
    sv_player_collide_with_laser = Variable("sv_player_collide_with_laser");
    portalgun_fire_delay = Variable("portalgun_fire_delay");

    sv_transition_fade_time.Modify(FCVAR_DEVELOPMENTONLY, FCVAR_CHEAT | FCVAR_HIDDEN);
    sv_transition_fade_time.SetValue(0);
    sv_transition_fade_time.ThisPtr()->m_pszDefaultValue = "0";
    ui_loadingscreen_transition_time.Modify(FCVAR_DEVELOPMENTONLY, FCVAR_CHEAT | FCVAR_HIDDEN);
    ui_loadingscreen_transition_time.SetValue(0);
    ui_loadingscreen_transition_time.ThisPtr()->m_pszDefaultValue = "0";
    ui_loadingscreen_fadein_time.Modify(FCVAR_DEVELOPMENTONLY, FCVAR_CHEAT | FCVAR_HIDDEN);
    ui_loadingscreen_fadein_time.SetValue(0);
    ui_loadingscreen_fadein_time.ThisPtr()->m_pszDefaultValue = "0";
    ui_loadingscreen_mintransition_time.Modify(FCVAR_DEVELOPMENTONLY, FCVAR_CHEAT | FCVAR_HIDDEN);
    ui_loadingscreen_mintransition_time.SetValue(0);
    ui_loadingscreen_mintransition_time.ThisPtr()->m_pszDefaultValue = "0";
    sv_player_collide_with_laser.Modify(FCVAR_CHEAT);

    sv_cheats = Variable("sv_cheats");
    crosshair = Variable("crosshair");
    viewmodel_offset_z = Variable("viewmodel_offset_z");


    Variable::RegisterAll();
    Command::RegisterAll();
}
void Cheats::Shutdown()
{
    // The forced default points into this DLL, so it can't outlive it.
    this->RevertUltimateSpeedrun();

    Variable::UnregisterAll();
    Command::UnregisterAll();
}

// Ultimate Speedrun mode forces portalgun_fire_delay 0 (its movement is in Server.cpp). The cvar stays
// cheat protected: setting it from here skips the sv_cheats check and, unlike a console command,
// isn't recorded in demos.
static const char* fireDelayDefault = nullptr; // The real default while the mode has replaced it

void Cheats::ApplyUltimateSpeedrun()
{
    // With sv_cheats 0 the engine reverts cheat cvars to their defaults while connecting to every map
    // or save, after this runs, so the forced value has to be the default as well.
    if (!fireDelayDefault) {
        fireDelayDefault = portalgun_fire_delay.ThisPtr()->m_pszDefaultValue;
        portalgun_fire_delay.ThisPtr()->m_pszDefaultValue = "0";
    }
    portalgun_fire_delay.SetValue("0");
}
void Cheats::RevertUltimateSpeedrun()
{
    if (!fireDelayDefault)
        return;
    portalgun_fire_delay.ThisPtr()->m_pszDefaultValue = fireDelayDefault;
    portalgun_fire_delay.SetValue(fireDelayDefault);
    fireDelayDefault = nullptr;
}


CON_COMMAND(sm_mode, "Variable used by Speedrun Mod to determine currently played mode.\n") {
    if (args.ArgC() != 2) {
        return console->Print("Current Speedrun Mod mode: %d\n", smsm.GetMode());
    }
    auto mode = std::atoi(args[1]);
    smsm.SetMode(mode);
}

CON_COMMAND(sm_param, "Allows to preview and manipulate mode-specific variables stored through the session.\n") {
    if (args.ArgC() < 2 || args.ArgC() > 3) {
        return console->Print("Incorrect command usage. Use syntax: sm_param [id] <value>");
    }
    auto paramId = std::atoi(args[1]);
    if (args.ArgC() == 2) {
        return console->Print("Mode-specific param id %d: %f\n", paramId, smsm.GetModeParam(paramId));
    }
    auto param = std::atof(args[2]);

    smsm.SetModeParam(paramId, param);
}
