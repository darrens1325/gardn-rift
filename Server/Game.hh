#pragma once

#include <Server/TeamManager.hh>

#include <Shared/Simulation.hh>

#include <atomic>
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
    // Peak flower count seen this round. Used to gate the "last man
    // standing" reset: if the peak was ≥ 2 and the live count drops to
    // 1, end the round (the last alive flower is the winner). Without
    // this latch, a solo-bot session would reset every tick because
    // alive_flowers would always equal 1.
    uint32_t max_flowers_this_round = 0;
    // Debug stdin override. Negative = no override; ≥ 0 = the next
    // tick() should set wave_tick to this value (and re-derive the
    // wave rarity from it). Written by the stdin reader thread, read
    // by the main game-loop thread — atomic to keep the cross-thread
    // hand-off honest. See Native.cc for the reader loop.
    std::atomic<int64_t> stdin_wave_tick_override{-1};
    // Sync-mode bookkeeping. When `sync_mode` is true the wall-clock
    // timer in Native.cc is left unstarted, and ticks fire only when
    // every verified client has sent Serverbound::kStep. `pending_step`
    // is the set of clients that still need to step before the next
    // tick can run — populated when a client first verifies, cleared
    // entry-by-entry as `client_requested_step()` is called, and
    // drained wholesale at the end of every tick.
    bool sync_mode = false;
    std::set<Client *> pending_step;
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);
    // Send the same byte buffer to every connected, verified client in this
    // instance. Used by chat broadcast.
    void broadcast(uint8_t const *packet, size_t len);
    // Sync-mode step request. No-op outside sync mode. Marks `client`
    // as ready; if every verified client is now ready, calls tick()
    // once and resets the pending set.
    void client_requested_step(Client *client);
private:
    // End-of-round: pick the top-scoring living flower as the winner,
    // broadcast kRoundEnd { winner_name, winner_score } to all clients,
    // then kill every flower and clear every camera's inventory.
    void end_round();
};