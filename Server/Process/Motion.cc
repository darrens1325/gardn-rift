#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

void tick_entity_motion(Simulation *sim, Entity &ent) {
    // Physics here is intentionally per-tick rather than dt-scaled. The whole
    // point of bumping TPS (e.g. for bot training) is to advance more game-
    // ticks per wall-clock second; per-tick integration preserves identical
    // tick-by-tick behavior at any TPS, which is what we want. The visible
    // consequence is that *wall-clock* speed scales linearly with TPS — at
    // TPS=400 mobs and players cover 20× more distance per real second than
    // at TPS=20. Terminal velocity (a/f distance/tick) is TPS-invariant; only
    // the time it takes to reach it (in real seconds) shrinks. Don't add a
    // `* dt` here without auditing every constant in StaticData.cc.
    if (ent.pending_delete) return;
    if (ent.slow_ticks > 0) {
        ent.speed_ratio *= 0.5;
        --ent.slow_ticks;
    }
    ent.velocity *= (1 - ent.friction);
    ent.acceleration *= ent.speed_ratio;
    ent.velocity += ent.acceleration;
    ent.set_x(ent.x + ent.velocity.x + ent.collision_velocity.x);
    ent.set_y(ent.y + ent.velocity.y + ent.collision_velocity.y);
    ent.collision_velocity *= 0.5;
    ent.velocity += ent.collision_velocity;
    if (!ent.has_component(kPetal) && !ent.has_component(kWeb)) {
        ent.set_x(fclamp(ent.x, ent.radius, ARENA_WIDTH - ent.radius));
        ent.set_y(fclamp(ent.y, ent.radius, ARENA_HEIGHT - ent.radius));
    }
    if (ent.has_component(kFlower)) {
        if (ent.acceleration.x != 0 || ent.acceleration.y != 0)
            ent.set_eye_angle(ent.acceleration.angle());
    }
    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}