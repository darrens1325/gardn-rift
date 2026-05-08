#include <Shared/Config.hh>

// Bumped because chat is a new wire feature: clients built before this won't
// understand C_CHAT broadcasts and the server won't route their inputs.
extern const uint64_t VERSION_HASH = 4728567265382324ll;
extern const uint32_t SERVER_PORT = 9001;
extern const uint32_t MAX_NAME_LENGTH = 16;
extern const uint32_t MAX_CHAT_LENGTH = 80;
// SIM_RATE/2 = 0.5 game-second between messages from one client. Keep in
// game-time so it scales with TPS.
extern const uint32_t CHAT_COOLDOWN_TICKS = 10;

//your ws host url may not follow this format, change it to fit your needs
extern std::string const WS_URL = "ws://localhost:"+std::to_string(SERVER_PORT);