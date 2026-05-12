#pragma once

#include <Shared/EntityDef.hh>
#include <Shared/Helpers.hh>

#include <cstdint>
#include <string>

enum Clientbound {
    kDisconnect,
    kClientUpdate,
    kOutdated,
    // Broadcast: { u8: kChat, string: sender_name, u8: sender_color, string: text }
    kChat,
    // Broadcast at round end (every WAVE_TICKS_PER_ROUND game-ticks):
    //   { u8: kRoundEnd, string: winner_name, u32: winner_score }
    // Server has already cleared every flower and inventory by the time it
    // sends this; clients should treat it as "you may respawn now."
    kRoundEnd,
};

enum Serverbound {
    kVerify,
    kClientInput,
    kClientSpawn,
    kPetalSwap,
    kPetalDelete,
    // { u8: kClientChat, string: text } — server stamps the sender from the
    // camera's active player and rebroadcasts as Clientbound::kChat. Rejected
    // if the player isn't alive or the rate-limit hasn't elapsed.
    kClientChat,
    // { u8: kStep } — used in sync mode only. Each verified client raises
    // its "ready for next tick" flag. When every verified client has
    // raised the flag, GameInstance ticks once and clears them all. In
    // wall-clock mode (the default — server runs its own us_timer_set
    // driver in Native.cc) this opcode is a no-op. The mode is enabled
    // by the env var GARDN_SYNC=1 at server startup; bots opt in with
    // run.py's `--sync` flag.
    kStep,
};

class Writer {
public:
    uint8_t *packet;
    uint8_t *at;
    Writer(uint8_t *);
    
    template<typename T>
    void write(T const &);
};

class Reader {
public:
    uint8_t const *packet;
    uint8_t const *at;
    Reader(uint8_t const *);

    template<typename T>
    T read();

    template<typename T>
    void read(T &);
};

class Validator {
public:
    uint8_t const *at;
    uint8_t const *end;
    Validator(uint8_t const *, uint8_t const *);

    uint8_t validate_uint8();
    uint8_t validate_uint32();
    uint8_t validate_uint64();
    uint8_t validate_float();
    uint8_t validate_string(uint32_t);
};
