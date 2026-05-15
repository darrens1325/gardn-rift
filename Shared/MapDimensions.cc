#include <Shared/MapDimensions.hh>

// Runtime arena dimensions. Initialized to the configure-time values derived
// from Map/main/main.tmj; TiledMap::load() overwrites these on the server
// side from whatever .tmj it actually loads at startup. In the bundle build,
// Bundle/Bridge.cc copies the server's values into gardn::client:: after
// server init so both sides agree.

uint32_t ARENA_WIDTH  = INITIAL_ARENA_WIDTH;
uint32_t ARENA_HEIGHT = INITIAL_ARENA_HEIGHT;
