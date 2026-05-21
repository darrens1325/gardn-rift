#include <Shared/Config.hh>

// Bumped because the wave-system rolled out new wire features: kRoundEnd
// broadcasts, plus 124 new petal IDs in the enum. Old clients would
// misinterpret either. Bumped again when kClientSpawn gained a trailing
// `map_path` string for picking a non-default spawn map — old clients
// would short-send the packet and trigger validator-disconnect.
extern const uint64_t VERSION_HASH = 4728567265382328ll;
extern const uint32_t SERVER_PORT = 9001;
extern const uint32_t MAX_NAME_LENGTH = 16;
extern const uint32_t MAX_CHAT_LENGTH = 80;
// SIM_RATE/2 = 0.5 game-second between messages from one client. Keep in
// game-time so it scales with TPS.
extern const uint32_t CHAT_COOLDOWN_TICKS = 10;

//your ws host url may not follow this format, change it to fit your needs
extern std::string const WS_URL = "ws://localhost:"+std::to_string(SERVER_PORT);