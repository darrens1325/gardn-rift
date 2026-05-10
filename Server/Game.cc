#include <Server/Game.hh>

#include <Server/Client.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/TiledMap.hh>

#include <Shared/Binary.hh>
#include <Shared/Entity.hh>
#include <Shared/Map.hh>

#include <cstdlib>
#include <iostream>

static void _update_client(Simulation *sim, Client *client) {
    if (client == nullptr) return;
    if (!client->verified) return;
    if (sim == nullptr) return;
    if (!sim->ent_exists(client->camera)) return;
    std::set<EntityID> in_view;
    std::vector<EntityID> deletes;
    in_view.insert(client->camera);
    Entity &camera = sim->get_ent(client->camera);
    if (sim->ent_exists(camera.player)) 
        in_view.insert(camera.player);
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kClientUpdate);
    writer.write<EntityID>(client->camera);
    sim->spatial_hash.query(camera.camera_x, camera.camera_y, 960 / camera.fov + 50, 540 / camera.fov + 50, [&](Simulation *, Entity &ent){
        in_view.insert(ent.id);
    });

    for (EntityID const &i: client->in_view) {
        if (!in_view.contains(i)) {
            writer.write<EntityID>(i);
            deletes.push_back(i);
        }
    }

    for (EntityID const &i : deletes)
        client->in_view.erase(i);

    writer.write<EntityID>(NULL_ENTITY);
    //upcreates
    for (EntityID id: in_view) {
        DEBUG_ONLY(assert(sim->ent_exists(id));)
        Entity &ent = sim->get_ent(id);
        uint8_t create = !client->in_view.contains(id);
        writer.write<EntityID>(id);
        writer.write<uint8_t>(create | (ent.pending_delete << 1));
        ent.write(&writer, BIT_AT(create, 0));
        client->in_view.insert(id);
    }
    writer.write<EntityID>(NULL_ENTITY);
    //write arena stuff
    writer.write<uint8_t>(client->seen_arena);
    sim->arena_info.write(&writer, client->seen_arena);
    client->seen_arena = 1;
    client->send_packet(writer.packet, writer.at - writer.packet);
}

GameInstance::GameInstance() : simulation(), clients(), team_manager(&simulation) {}

void GameInstance::init() {
    // Load the Tiled map (collision rects + mob-spawn polygons). If this
    // fails the simulation falls back to the hardcoded MAP zones in
    // StaticData.hh — see Map::spawn_random_mob.
    char const *map_path = std::getenv("GARDN_MAP");
    TiledMap::load(map_path ? map_path : "Map/main/main.tmj");
    for (uint32_t i = 0; i < ENTITY_CAP / 2; ++i)
        Map::spawn_random_mob(&simulation);
    team_manager.add_team(ColorID::kBlue);
    team_manager.add_team(ColorID::kRed);
}

void GameInstance::tick() {
    // Stdin override (debug): a non-negative value posted by Native.cc's
    // reader thread jumps wave_tick directly so we can fast-forward into
    // a high-rarity phase without waiting out the round. Cleared after
    // applying.
    int64_t override_val = stdin_wave_tick_override.exchange(-1);
    if (override_val >= 0) {
        if (override_val > (int64_t)WAVE_TICKS_PER_ROUND)
            override_val = (int64_t)WAVE_TICKS_PER_ROUND;
        wave_tick = (uint32_t)override_val;
        std::cout << "[debug] wave_tick set to " << wave_tick << "\n";
    }
    // Advance the wave rarity *before* simulation.tick() so any mob spawned
    // this tick (in particular Map::spawn_random_mob from inside the sim)
    // sees the current tier. Linear ramp Common→Unique over the round.
    uint32_t wave_rarity = (uint32_t)((uint64_t)wave_tick * RarityID::kNumRarities / WAVE_TICKS_PER_ROUND);
    if (wave_rarity >= RarityID::kNumRarities) wave_rarity = RarityID::kNumRarities - 1;
    simulation.current_wave_rarity = wave_rarity;

    simulation.tick();
    for (Client *client : clients)
        _update_client(&simulation, client);
    simulation.post_tick();
    ++tick_count;
    ++wave_tick;

    // Count alive flowers across verified clients. We use this both to
    // arm the early-reset trigger ("had a flower, now don't") and to
    // remember that a round was *active* — without the latch we'd
    // re-fire end_round() the very next tick because the kRoundEnd
    // deletions leave us with zero flowers until the bots respawn.
    uint32_t alive_flowers = 0;
    for (Client *client : clients) {
        if (!client->verified) continue;
        if (!simulation.ent_exists(client->camera)) continue;
        Entity &camera = simulation.get_ent(client->camera);
        if (simulation.ent_alive(camera.player)) ++alive_flowers;
    }
    if (alive_flowers > 0) any_flower_this_round = 1;
    if (alive_flowers > max_flowers_this_round) max_flowers_this_round = alive_flowers;

    // Last-man-standing: if the round had ≥ 2 flowers in it at any
    // point and we're now down to ≤ 1, end the round. The remaining
    // flower (if any) is the winner. Solo sessions (peak == 1) don't
    // hit this branch — they only end on full wipeout or timeout.
    bool time_up = wave_tick >= WAVE_TICKS_PER_ROUND;
    bool wipeout = any_flower_this_round && alive_flowers == 0;
    bool last_standing = max_flowers_this_round >= 2 && alive_flowers <= 1;
    if (time_up || wipeout || last_standing) {
        printf("Round end: time_up=%d wipeout=%d last_standing=%d alive_flowers=%d max=%d\n",
               time_up, wipeout, last_standing, alive_flowers, max_flowers_this_round);
        end_round();
        wave_tick = 0;
        simulation.current_wave_rarity = 0;
        any_flower_this_round = 0;
        max_flowers_this_round = 0;
    }
}

void GameInstance::end_round() {
    // Pick the top-scoring living flower across all connected clients.
    std::string winner_name;
    uint32_t winner_score = 0;
    for (Client *client : clients) {
        if (!client->verified) continue;
        if (!simulation.ent_exists(client->camera)) continue;
        Entity &camera = simulation.get_ent(client->camera);
        if (!simulation.ent_alive(camera.player)) continue;
        Entity &player = simulation.get_ent(camera.player);
        if (player.score > winner_score) {
            winner_score = player.score;
            winner_name = player.name;
        }
    }
    // Broadcast the round-end packet *before* deleting flowers so clients
    // know whose score they saw last.
    Writer writer(Server::OUTGOING_PACKET);
    writer.write<uint8_t>(Clientbound::kRoundEnd);
    writer.write<std::string>(winner_name);
    writer.write<uint32_t>(winner_score);
    broadcast(Server::OUTGOING_PACKET, writer.at - writer.packet);

    // Kill every flower and wipe every camera's inventory. Two cases:
    //   1. Player alive — wipe the player's loadout *first* so Death.cc's
    //      flower handler sees only kNone slots (no drops at round end),
    //      reset score so the post-death respawn_level recomputes to 1,
    //      then request_delete. Death.cc's normal flower-death path then
    //      refills camera.inventory with basics for level 1, which is
    //      exactly what we want.
    //   2. Player already dead (mid-round death w/o respawn) — Death.cc
    //      won't run for this camera, so wipe + refill the camera
    //      inventory directly here.
    for (Client *client : clients) {
        if (!simulation.ent_exists(client->camera)) continue;
        Entity &camera = simulation.get_ent(client->camera);
        if (simulation.ent_alive(camera.player)) {
            Entity &player = simulation.get_ent(camera.player);
            for (uint32_t i = 0; i < player.loadout_count + MAX_SLOT_COUNT; ++i) {
                PetalTracker::remove_petal(&simulation, player.loadout_ids[i]);
                player.set_loadout_ids(i, PetalID::kNone);
            }
            player.set_score(0);
            simulation.request_delete(camera.player);
        } else {
            for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
                PetalTracker::remove_petal(&simulation, camera.inventory[i]);
                camera.set_inventory(i, PetalID::kNone);
            }
            camera.set_respawn_level(1);
            for (uint32_t i = 0; i < loadout_slots_at_level(camera.respawn_level); ++i) {
                camera.set_inventory(i, PetalID::kBasic);
                PetalTracker::add_petal(&simulation, PetalID::kBasic);
            }
        }
    }
}

void GameInstance::broadcast(uint8_t const *packet, size_t len) {
    for (Client *client : clients) {
        if (!client->verified) continue;
        client->send_packet(packet, len);
    }
}

void GameInstance::add_client(Client *client) {
    DEBUG_ONLY(assert(client->game != this);)
    if (client->game != nullptr)
        client->game->remove_client(client);
    client->game = this;
    clients.insert(client);
    Entity &ent = simulation.alloc_ent();
    ent.add_component(kCamera);
    ent.add_component(kRelations);
    #ifdef GAMEMODE_TDM
    EntityID team = team_manager.get_random_team();
    ent.set_team(team);
    ent.set_color(simulation.get_ent(team).color);
    #else
    ent.set_team(ent.id);
    ent.set_color(ColorID::kYellow); 
    #endif
    
    ent.set_fov(BASE_FOV);
    ent.set_respawn_level(1);
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.respawn_level); ++i)
        ent.set_inventory(i, PetalID::kBasic);
    if (frand() < 0.0001 && PetalTracker::get_count(&simulation, PetalID::kUniqueBasic) == 0)
        ent.set_inventory(0, PetalID::kUniqueBasic);
    for (uint32_t i = 0; i < loadout_slots_at_level(ent.respawn_level); ++i)
        PetalTracker::add_petal(&simulation, ent.inventory[i]);
    client->camera = ent.id;
    client->seen_arena = 0;
}

void GameInstance::remove_client(Client *client) {
    DEBUG_ONLY(assert(client->game == this);)
    clients.erase(client);
    if (simulation.ent_exists(client->camera)) {
        Entity &c = simulation.get_ent(client->camera);
        if (simulation.ent_exists(c.player))
            simulation.request_delete(c.player);
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            PetalTracker::remove_petal(&simulation, c.inventory[i]);
        simulation.request_delete(client->camera);
    }
    client->game = nullptr;
}