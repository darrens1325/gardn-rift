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
    GameInstance();
    void init();
    void tick();
    void add_client(Client *);
    void remove_client(Client *);
    // Send the same byte buffer to every connected, verified client in this
    // instance. Used by chat broadcast.
    void broadcast(uint8_t const *packet, size_t len);
};