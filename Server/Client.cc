#include <Server/Client.hh>

#include <Server/Game.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/Spawn.hh>
#include <Server/TiledMap.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <iostream>

static uint32_t const RARITY_TO_XP[RarityID::kNumRarities] = { 2, 10, 50, 200, 1000, 5000, 0 };

// Cap on the spawn-arg map path. Real map names live under Map/<dir>/<file>.tmj
// and are well under this; the limit just bounds protocol cost and rejects
// pathological inputs early.
static uint32_t const MAX_SPAWN_MAP_PATH_LENGTH = 128;

// Whitelist for client-supplied map paths. Must look like a Tiled map
// inside the server's Map/ directory and contain no parent-dir refs —
// otherwise a connecting client could ask the server to fopen any file
// on disk via TiledMap::load.
static bool is_safe_user_map_path(std::string const &path) {
    if (path.empty()) return false;
    if (path.size() < 5) return false;
    if (path.rfind("Map/", 0) != 0) return false;
    if (path.find("..") != std::string::npos) return false;
    if (path.size() < 4 || path.compare(path.size() - 4, 4, ".tmj") != 0) return false;
    for (char c : path) {
        if (c == '\0') return false;
        // Reject anything that isn't a plain ASCII path character. Avoids
        // smuggled control bytes / nullbytes through to fopen.
        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7f) return false;
    }
    return true;
}

Client::Client() : game(nullptr) {}

void Client::init() {
    DEBUG_ONLY(assert(game == nullptr);)
    Server::game.add_client(this);    
}

void Client::remove() {
    if (game == nullptr) return;
    game->remove_client(this);
}

void Client::disconnect() {
    if (ws == nullptr) return;
    remove();
    ws->end();
}

uint8_t Client::alive() {
    if (game == nullptr) return false;
    Simulation *simulation = &game->simulation;
    return simulation->ent_exists(camera) 
    && simulation->ent_exists(simulation->get_ent(camera).player);
}

#define VALIDATE(expr) if (!expr) { client->disconnect(); return; }

void Client::on_message(WebSocket *ws, std::string_view message, uint64_t code) {
    if (ws == nullptr) return;
    uint8_t const *data = reinterpret_cast<uint8_t const *>(message.data());
    Reader reader(data);
    Validator validator(data, data + message.size());
    Client *client = ws->getUserData();
    if (client == nullptr) {
        ws->end();
        return;
    }
    if (!client->verified) {
        VALIDATE(validator.validate_uint8());
        if (reader.read<uint8_t>() != Serverbound::kVerify) {
            //disconnect
            client->disconnect();
            return;
        }
        VALIDATE(validator.validate_uint64());
        if (reader.read<uint64_t>() != VERSION_HASH) {
            Writer writer(Server::OUTGOING_PACKET);
            writer.write<uint8_t>(Clientbound::kOutdated);
            client->send_packet(writer.packet, writer.at - writer.packet);
            client->disconnect();
            return;
        }
        client->verified = 1;
        client->init();
        return;
    }
    if (client->game == nullptr) {
        client->disconnect();
        return;
    }
    VALIDATE(validator.validate_uint8());
    switch (reader.read<uint8_t>()) {
        case Serverbound::kVerify:
            client->disconnect();
            return;
        case Serverbound::kClientInput: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.player);
            VALIDATE(validator.validate_float());
            VALIDATE(validator.validate_float());
            float x = reader.read<float>();
            float y = reader.read<float>();
            if (x == 0 && y == 0) player.acceleration.set(0,0);
            else {
                if (std::abs(x) > 5e3 || std::abs(y) > 5e3) break;
                Vector accel(x,y);
                float m = accel.magnitude();
                if (m > 200) accel.set_magnitude(PLAYER_ACCELERATION);
                else accel.set_magnitude(m / 200 * PLAYER_ACCELERATION);
                player.acceleration = accel;
            }
            VALIDATE(validator.validate_uint8());
            player.input = reader.read<uint8_t>();
            //store player's acceleration and input in camera (do not reset ever)
            break;
        }
        case Serverbound::kClientSpawn: {
            if (client->alive()) break;
            // Read + validate name and map_path *before* allocating the
            // player, so a malformed packet doesn't leave a stray flower
            // in the sim after the VALIDATE() macro short-returns.
            VALIDATE(validator.validate_string(MAX_NAME_LENGTH));
            std::string name;
            reader.read<std::string>(name);
            VALIDATE(UTF8Parser::is_valid_utf8(name));
            name = UTF8Parser::trunc_string(name, MAX_NAME_LENGTH);
            VALIDATE(validator.validate_string(MAX_SPAWN_MAP_PATH_LENGTH));
            std::string requested_map;
            reader.read<std::string>(requested_map);

            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            // Empty = "keep current camera map" (which is the default for
            // a fresh camera, see Server/Game.cc::add_client). Non-empty
            // is honored only if it passes the whitelist *and* loads;
            // anything else silently falls back rather than disconnecting
            // — the client just lands on the current map.
            if (!requested_map.empty()
                    && is_safe_user_map_path(requested_map)
                    && TiledMap::ensure_loaded(requested_map))
                camera.map_path = requested_map;
            Entity &player = alloc_player(simulation, camera.team);
            player_spawn(simulation, camera, player);
            player.set_name(name);
            break;
        }
        case Serverbound::kPetalDelete: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.player);
            VALIDATE(validator.validate_uint8());
            uint8_t pos = reader.read<uint8_t>();
            if (pos >= MAX_SLOT_COUNT + player.loadout_count) break;
            PetalID::T old_id = player.loadout_ids[pos];
            if (old_id != PetalID::kNone && old_id != PetalID::kBasic) {
                uint8_t rarity = PETAL_DATA[old_id].rarity;
                player.set_score(player.score + RARITY_TO_XP[rarity]);
                //need to delete if over cap
                if (player.deleted_petals.size() == player.deleted_petals.capacity())
                    //removes old trashed old petal
                    PetalTracker::remove_petal(simulation, player.deleted_petals[0]);
                player.deleted_petals.push_back(old_id);
            }
            player.set_loadout_ids(pos, PetalID::kNone);
            break;
        }
        case Serverbound::kPetalSwap: {
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.player);
            VALIDATE(validator.validate_uint8());
            uint8_t pos1 = reader.read<uint8_t>();
            if (pos1 >= MAX_SLOT_COUNT + player.loadout_count) break;
            VALIDATE(validator.validate_uint8());
            uint8_t pos2 = reader.read<uint8_t>();
            if (pos2 >= MAX_SLOT_COUNT + player.loadout_count) break;
            PetalID::T tmp = player.loadout_ids[pos1];
            player.set_loadout_ids(pos1, player.loadout_ids[pos2]);
            player.set_loadout_ids(pos2, tmp);
            break;
        }
        case Serverbound::kStep: {
            // Sync-mode pacing. The handler is a no-op outside sync
            // mode (client_requested_step early-outs on !sync_mode).
            client->game->client_requested_step(client);
            break;
        }
        case Serverbound::kClientChat: {
            // Sender identity comes from the player attached to the camera —
            // dropping chat from spectators avoids unauthenticated/anonymous
            // spam.
            if (!client->alive()) break;
            Simulation *simulation = &client->game->simulation;
            Entity &camera = simulation->get_ent(client->camera);
            Entity &player = simulation->get_ent(camera.player);
            VALIDATE(validator.validate_string(MAX_CHAT_LENGTH));
            std::string text;
            reader.read<std::string>(text);
            VALIDATE(UTF8Parser::is_valid_utf8(text));
            text = UTF8Parser::trunc_string(text, MAX_CHAT_LENGTH);
            if (text.empty()) break;
            // Per-client rate limit. tick_count is monotonic game-time on the
            // GameInstance, so the cooldown stays in game-time at any TPS.
            uint64_t now = client->game->tick_count;
            if (client->last_chat_tick != 0
                    && now - client->last_chat_tick < CHAT_COOLDOWN_TICKS)
                break;
            client->last_chat_tick = now;
            // Broadcast: { kChat, sender_name, sender_color, text }
            Writer writer(Server::OUTGOING_PACKET);
            writer.write<uint8_t>(Clientbound::kChat);
            writer.write<std::string>(player.name);
            writer.write<uint8_t>(player.color);
            writer.write<std::string>(text);
            client->game->broadcast(writer.packet, writer.at - writer.packet);
            break;
        }
    }
}

void Client::on_disconnect(WebSocket *ws, int code, std::string_view message) {
    std::cout << "client disconnection\n";
    Client *client = ws->getUserData();
    if (client == nullptr) return;
    client->remove();
    //Server::clients.erase(client);
    //delete player in systems
}