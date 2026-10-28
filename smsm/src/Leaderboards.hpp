#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Challenge mode leaderboards from board.portal2.sr. Each board is fetched once by a background
// worker and cached for the rest of the session.
namespace Leaderboards {

struct Entry {
    uint64_t steamId;
    std::string name;
    int centiseconds;
    int rank;
};

enum class State {
    Idle,
    Loading,
    Ready,
    Failed,
};

// Queues every chamber's board to be fetched, in order.
void Prefetch(const std::vector<int>& chamberIds);
// Moves a chamber to the front of the queue (queuing it if needed) unless it's cached or loading.
void Request(int chamberId);
// The chamber's state; fills entries (best first) once it's Ready.
State Get(int chamberId, std::vector<Entry>& entries);
// Waits for in-flight requests so their threads don't outlive the plugin.
void Shutdown();

}
