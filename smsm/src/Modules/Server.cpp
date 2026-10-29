#include "Server.hpp"

#include "Command.hpp"
#include "Interface.hpp"
#include "Offsets.hpp"
#include "Utils.hpp"

#include "Console.hpp"
#include "SMSM.hpp"
#include "CelesteMoveset.hpp"


// CGameMovement::ProcessMovement
REDECL(Server::ProcessMovement);
DETOUR(Server::ProcessMovement, void* pPlayer, CMoveData* pMove) {
    celesteMoveset.PreProcessMovement(pPlayer, pMove);

    auto result = Server::ProcessMovement(thisptr, pPlayer, pMove);
    server->tickBase = *reinterpret_cast<int*>((uintptr_t)pPlayer + Offsets::m_nTickBase);

    celesteMoveset.ProcessMovement(pPlayer, pMove);
    return result;
}

// Ultimate Speedrun mode's movement, built in so it needs neither SAR nor sv_cheats.
constexpr float UltimateSpeedrunAirAccelerate = 1000.0f;

// CPortalGameMovement::AirMove
REDECL(Server::AirMove);
DETOUR(Server::AirMove) {
    if (smsm.GetMode() != UltimateSpeedrun)
        return Server::AirMove(thisptr);

    // CGameMovement::AirMove: Source's own air control, without Portal 2's limits on steering
    // while flung, and with its own air acceleration (Portal 2 uses sv_paintairacceleration).
    auto player = *reinterpret_cast<uintptr_t*>((uintptr_t)thisptr + Offsets::player);
    auto mv = *reinterpret_cast<CMoveData**>((uintptr_t)thisptr + Offsets::mv);

    Vector forward, right;
    Math::AngleVectors(mv->m_vecViewAngles, &forward, &right, nullptr);
    forward.z = 0;
    right.z = 0;
    Math::VectorNormalize(forward);
    Math::VectorNormalize(right);

    Vector wishdir(forward.x * mv->m_flForwardMove + right.x * mv->m_flSideMove,
        forward.y * mv->m_flForwardMove + right.y * mv->m_flSideMove, 0);
    float wishspeed = Math::VectorNormalize(wishdir);
    if (wishspeed > mv->m_flMaxSpeed)
        wishspeed = mv->m_flMaxSpeed;

    Memory::VMT<_AirAccelerate>(thisptr, Offsets::AirAccelerate)(thisptr, &wishdir, wishspeed, UltimateSpeedrunAirAccelerate);

    auto& baseVelocity = *reinterpret_cast<Vector*>(player + Offsets::m_vecBaseVelocity);
    mv->m_vecVelocity = mv->m_vecVelocity + baseVelocity;
    Memory::VMT<_TryPlayerMove>(thisptr, Offsets::TryPlayerMove)(thisptr, nullptr, nullptr);
    mv->m_vecVelocity = mv->m_vecVelocity + baseVelocity * -1;
    return 0;
}

// CPortalGameMovement::CheckJumpButton
REDECL(Server::CheckJumpButton);
DETOUR_T(bool, Server::CheckJumpButton) {
    // Autojump: the game won't jump while the button was already held last tick, so forget it was.
    if (smsm.GetMode() == UltimateSpeedrun) {
        auto mv = *reinterpret_cast<CMoveData**>((uintptr_t)thisptr + Offsets::mv);
        mv->m_nOldButtons &= ~IN_JUMP;
    }
    return Server::CheckJumpButton(thisptr);
}

void* Server::GetPlayer(int index) {
    return this->UTIL_PlayerByIndex(index);
}

void* Server::GetEntityHandleByIndex(int index) {
    auto size = sizeof(CEntInfo2);
    CEntInfo* info = reinterpret_cast<CEntInfo*>((uintptr_t)server->m_EntPtrArray + size * index);
    return info->m_pEntity;
}

Server::Server()
    : Module() {
}
bool Server::Init() {

    if (auto g_ServerTools = Interface::Create(this->Name(), "VSERVERTOOLS001")) {
        auto GetIServerEntity = g_ServerTools->Original(Offsets::GetIServerEntity);
#ifdef _WIN32
        Memory::Deref(GetIServerEntity + Offsets::m_EntPtrArray, &this->m_EntPtrArray);
#else
        this->m_EntPtrArray = (CEntInfo *)(GetIServerEntity + 12 + *(uint32_t *)(GetIServerEntity + 14) + *(uint32_t *)(GetIServerEntity + 54) + 4);
#endif
        Interface::Delete(g_ServerTools);
    }

    this->g_GameMovement = Interface::Create(this->Name(), "GameMovement001");
    if (this->g_GameMovement) {
        this->g_GameMovement->Hook(Server::ProcessMovement_Hook, Server::ProcessMovement, Offsets::ProcessMovement);
        // Only found for Windows so far
        if (Offsets::AirMove) {
            this->g_GameMovement->Hook(Server::AirMove_Hook, Server::AirMove, Offsets::AirMove);
            this->g_GameMovement->Hook(Server::CheckJumpButton_Hook, Server::CheckJumpButton, Offsets::CheckJumpButton);
        }
    }

    this->g_ServerGameDLL = Interface::Create(this->Name(), "ServerGameDLL005");

    if (this->g_ServerGameDLL) {
        auto Think = this->g_ServerGameDLL->Original(Offsets::Think);
        Memory::Read<_UTIL_PlayerByIndex>(Think + Offsets::UTIL_PlayerByIndex, &this->UTIL_PlayerByIndex);
        Memory::DerefDeref<CGlobalVars*>((uintptr_t)this->UTIL_PlayerByIndex + Offsets::gpGlobals, &this->gpGlobals);
    }

    return this->hasLoaded = this->g_GameMovement && this->g_ServerGameDLL;
}
void Server::Shutdown() {
    if (this->g_GameMovement) {
        Interface::Delete(this->g_GameMovement);
    }

    if (this->g_ServerGameDLL) {
        Interface::Delete(this->g_ServerGameDLL);
    }
}

Server* server;
