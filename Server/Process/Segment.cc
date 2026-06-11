#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

void tick_segment_behavior(Simulation *sim, Entity &ent) {
    if (!ent.is_tail) return;
    if (sim->ent_alive(ent.seg_head)) {
        Entity &par = sim->get_ent(ent.seg_head);
        Vector diff(ent.x - par.x, ent.y - par.y);
        diff.set_magnitude(ent.radius + par.radius + 0.01);
        ent.set_x(par.x + diff.x);
        ent.set_y(par.y + diff.y);
        ent.set_angle(diff.angle() + M_PI);
        if (sim->ent_alive(par.target))
            ent.target = par.target;
    } else if (ent.mob_id == MobID::kLeech) {
        // The leech is one creature: damage to any segment is redirected to the
        // head (see inflict_damage), so a body only dies once the head is gone.
        // When that happens the body deletes itself, cascading down the chain.
        // It drops nothing — the head already rolled the leech's single drop.
        BIT_SET(ent.flags, EntityFlags::kNoDrops)
        sim->request_delete(ent.id);
    }
}