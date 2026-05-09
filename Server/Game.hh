#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <set>

class Client;

class GameInstance {
    std::set<Client *> clients;
    TeamManager team_manager;
public:
    Simulation simulation;
    // Monotonic game-tick counter for this instance. Used by Client to gate
    // chat-rate-limiting against game-time rather than wall-clock.
    uint64_t tick_count = 0;
    // Wave / round timer — counts game-ticks since the current round began.
    // At WAVE_TICKS_PER_ROUND we end the round (broadcast kRoundEnd, kill
    // every flower, wipe every inventory) and rewind to 0. The mob-spawn
    // rarity tier is `wave_tick * kNumRarities / WAVE_TICKS_PER_ROUND`,
    // ramping linearly from Common at round start to Unique at round end.
    uint32_t wave_tick = 0;
    // True once at least one flower has been alive in this round.
    // Used to fire end_round() early when every bot has died — without
    // it, the no-flower check would also fire at the moment of round
    // start (between kRoundEnd and the bots' kClientSpawn packets) and
    // loop the round forever.
    uint8_t any_flower_this_round = 0;
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);
    // Send the same byte buffer to every connected, verified client in this
    // instance. Used by chat broadcast.
    void broadcast(uint8_t const *packet, size_t len);
private:
    // End-of-round: pick the top-scoring living flower as the winner,
    // broadcast kRoundEnd { winner_name, winner_score } to all clients,
    // then kill every flower and clear every camera's inventory.
    void end_round();
};