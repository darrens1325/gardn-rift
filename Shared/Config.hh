#pragma once

#include <cstdint>
#include <string>

extern std::string const WS_URL;
extern uint64_t const VERSION_HASH;
extern uint32_t const SERVER_PORT;
extern uint32_t const MAX_NAME_LENGTH;
extern uint32_t const MAX_CHAT_LENGTH;
// Minimum game-ticks between successive chat messages from a single client.
extern uint32_t const CHAT_COOLDOWN_TICKS;