#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

static void _focus_lose_clause(Entity &ent, Vector const &v) {
    if (v.magnitude() > 1.5 * MOB_DATA[ent.mob_id].attributes.aggro_radius) ent.target = NULL_ENTITY;
}

static void default_tick_idle(Simulation *sim, Entity &ent) {
    if (ent.ai_tick >= 1 * SIM_RATE) {
        ent.ai_tick = 0;
        ent.set_angle(frand() * 2 * M_PI);
        ent.ai_state = AIState::kIdleMoving;
    }
}

static void default_tick_idle_moving(Simulation *sim, Entity &ent) {
    if (ent.ai_tick > 2.5 * SIM_RATE) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    }
    if (ent.ai_tick < 0.5 * SIM_RATE) return;
    float r = (ent.ai_tick - 0.5 * SIM_RATE) / (2 * SIM_RATE);
    ent.acceleration
        .unit_normal(ent.angle)
        .set_magnitude(2 * PLAYER_ACCELERATION * (r - r * r));
}

static void default_tick_returning(Simulation *sim, Entity &ent, float speed = 1.0) {
    if (!sim->ent_alive(ent.parent)) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    }
    Entity &parent = sim->get_ent(ent.parent);
    Vector delta(parent.x - ent.x, parent.y - ent.y);
    if (delta.magnitude() > 300) {
        ent.ai_tick = 0;
    } else if (ent.ai_tick > 2 * SIM_RATE || delta.magnitude() < 100) {
        ent.ai_tick = 0;
        ent.ai_state = AIState::kIdle;
        return;
    } 
    delta.set_magnitude(PLAYER_ACCELERATION * speed);
    ent.acceleration = delta;
    ent.set_angle(delta.angle());
}


static void tick_default_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            default_tick_idle(sim, ent);
            break;
        }
        case AIState::kIdleMoving: {
            default_tick_idle_moving(sim, ent);
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
}

static void tick_default_neutral(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        v.set_magnitude(PLAYER_ACCELERATION * 0.975);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.target = NULL_ENTITY;
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        tick_default_passive(sim, ent);
    }
}

static void tick_default_aggro(Simulation *sim, Entity &ent, float speed) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * speed);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        //if (ent.ai_state != AIState::kReturning) 
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.radius);
        tick_default_passive(sim, ent);
    }
}

static void tick_bee_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            if (ent.ai_tick >= 5 * SIM_RATE) {
                ent.ai_tick = 0;
                ent.set_angle(frand() * 2 * M_PI);
                ent.ai_state = AIState::kIdle;
            }
            ent.set_angle(ent.angle + 1.5 * sinf(((float) ent.lifetime) / (SIM_RATE / 2)) / SIM_RATE);
            Vector v(cosf(ent.angle), sinf(ent.angle));
            v *= 1.5;
            if (ent.lifetime % (SIM_RATE * 3 / 2) < SIM_RATE / 2)
                v *= 0.5;
            ent.acceleration = v;
            break;
        }
        case AIState::kIdleMoving: {
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
}

static void tick_hornet_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        float dist = v.magnitude();
        if (dist > 300) {
            v.set_magnitude(PLAYER_ACCELERATION * 0.975);
            ent.acceleration = v;
        } else {
            ent.acceleration.set(0,0);
        }
        ent.set_angle(v.angle());
        if (ent.ai_tick >= 1.5 * SIM_RATE && dist < 800) {
            ent.ai_tick = 0;
            // Scale the missile by the hornet's rolled rarity so a Mythic
            // Hornet fires a missile that's MOB_RADIUS_MULT[Mythic]× as
            // wide and 1.5^Δ× as damaging as a Common one. Without this
            // the projectile stays Common-sized regardless of how
            // intimidating the parent mob looks.
            float r_mult = mob_radius_mult(ent.mob_rarity);
            float d_mult = mob_dmg_mult(ent.mob_rarity, MOB_DATA[ent.mob_id].rarity);
            Entity &missile = alloc_petal(sim, PetalID::kMissile, ent);
            missile.set_radius(missile.radius * r_mult);
            missile.damage = 10 * d_mult;
            missile.health = missile.max_health = 10 * d_mult;
            entity_set_despawn_tick(missile, 3 * SIM_RATE);
            missile.set_angle(ent.angle);
            missile.acceleration.unit_normal(ent.angle).set_magnitude(40 * PLAYER_ACCELERATION);
            Vector kb;
            kb.unit_normal(ent.angle - M_PI).set_magnitude(2.5 * PLAYER_ACCELERATION);
            ent.velocity += kb;
        }
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
            ent.target = NULL_ENTITY;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius);
        tick_bee_passive(sim, ent);;
    }
}

static void tick_wasp_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        float dist = v.magnitude();
        if (dist > 300) {
            v.set_magnitude(PLAYER_ACCELERATION * 0.975);
            ent.acceleration = v;
        } else {
            ent.acceleration.set(0,0);
        }
        ent.set_angle(v.angle());
        if (ent.ai_tick >= 0.75 * SIM_RATE && dist < 800) {
            ent.ai_tick = 0;
            // Scale the missile by the hornet's rolled rarity so a Mythic
            // Hornet fires a missile that's MOB_RADIUS_MULT[Mythic]× as
            // wide and 1.5^Δ× as damaging as a Common one. Without this
            // the projectile stays Common-sized regardless of how
            // intimidating the parent mob looks.
            float r_mult = mob_radius_mult(ent.mob_rarity);
            float d_mult = mob_dmg_mult(ent.mob_rarity, MOB_DATA[ent.mob_id].rarity);
            Entity &missile = alloc_petal(sim, PetalID::kMissile, ent);
            missile.set_radius(missile.radius * r_mult);
            missile.damage = 10 * d_mult;
            missile.health = missile.max_health = 10 * d_mult;
            entity_set_despawn_tick(missile, 3 * SIM_RATE);
            missile.set_angle(ent.angle);
            missile.acceleration.unit_normal(ent.angle).set_magnitude(40 * PLAYER_ACCELERATION);
            Vector kb;
            kb.unit_normal(ent.angle - M_PI).set_magnitude(2.5 * PLAYER_ACCELERATION);
            ent.velocity += kb;
        }
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
            ent.target = NULL_ENTITY;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius);
        tick_bee_passive(sim, ent);;
    }
}

static void tick_mantis_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        float dist = v.magnitude();
        if (dist > 300) {
            v.set_magnitude(PLAYER_ACCELERATION * 0.975);
            ent.acceleration = v;
        } else {
            ent.acceleration.set(0,0);
        }
        ent.set_angle(v.angle());
        if (ent.ai_tick >= 0.75 * SIM_RATE && dist < 800) {
            ent.ai_tick = 0;
            // Scale the missile by the hornet's rolled rarity so a Mythic
            // Hornet fires a missile that's MOB_RADIUS_MULT[Mythic]× as
            // wide and 1.5^Δ× as damaging as a Common one. Without this
            // the projectile stays Common-sized regardless of how
            // intimidating the parent mob looks.
            float r_mult = mob_radius_mult(ent.mob_rarity);
            float d_mult = mob_dmg_mult(ent.mob_rarity, MOB_DATA[ent.mob_id].rarity);
            Entity &missile = alloc_petal(sim, PetalID::kPeas, ent);
            missile.set_radius(missile.radius * r_mult);
            missile.damage = 10 * d_mult;
            missile.health = missile.max_health = 10 * d_mult;
            entity_set_despawn_tick(missile, 3 * SIM_RATE);
            missile.set_angle(ent.angle);
            // tick_petal_behavior runs before motion and zeros out the
            // acceleration on a despawning kPeas (the default switch arm),
            // so setting acceleration here is a no-op. Set velocity
            // directly and drop friction so the missile actually flies
            // for the full 3-second despawn instead of stalling on launch.
            missile.friction = 0;
            missile.velocity.unit_normal(ent.angle).set_magnitude(8 * PLAYER_ACCELERATION);
            Vector kb;
            kb.unit_normal(ent.angle - M_PI).set_magnitude(2.5 * PLAYER_ACCELERATION);
            ent.velocity += kb;
        }
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
            ent.target = NULL_ENTITY;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius);
        tick_bee_passive(sim, ent);;
    }
}

static void tick_centipede_passive(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            ent.set_angle(ent.angle + 0.25 / SIM_RATE);
            if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdleMoving;
            break;
        }
        case AIState::kIdleMoving: {
            ent.set_angle(ent.angle - 0.25 / SIM_RATE);
            if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdle;
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent);
            break;
        }
    }
    ent.acceleration.unit_normal(ent.angle).set_magnitude(PLAYER_ACCELERATION / 10);
}

static void tick_centipede_neutral(Simulation *sim, Entity &ent, float speed) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        v.set_magnitude(PLAYER_ACCELERATION * speed);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        //ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.radius);
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_angle(ent.angle + 0.25 / SIM_RATE);
                if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdleMoving;
                ent.acceleration.unit_normal(ent.angle).set_magnitude(PLAYER_ACCELERATION * speed);
                break;
            }
            case AIState::kIdleMoving: {
                ent.set_angle(ent.angle - 0.25 / SIM_RATE);
                if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.angle).set_magnitude(PLAYER_ACCELERATION * speed);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

static void tick_centipede_aggro(Simulation *sim, Entity &ent) {
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * 0.95);
        ent.acceleration = v;
        ent.set_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.radius);
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_angle(ent.angle + 0.25 / SIM_RATE);
                if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdleMoving;
                ent.acceleration.unit_normal(ent.angle).set_magnitude(PLAYER_ACCELERATION / 10);
                break;
            }
            case AIState::kIdleMoving: {
                ent.set_angle(ent.angle - 0.25 / SIM_RATE);
                if (frand() < 1 / (5.0 * SIM_RATE)) ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.angle).set_magnitude(PLAYER_ACCELERATION / 10);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

static void tick_sandstorm(Simulation *sim, Entity &ent) {
    switch(ent.ai_state) {
        case AIState::kIdle: {
            // 1 event per game-second on average. The original `frand() >
            // 1.0f / SIM_RATE` was inverted — per-second rate scaled with SIM_RATE, so
            // at SIM_RATE=400 the sandstorm transitioned almost every tick
            // (~400×/sec) instead of ~1×/sec.
            if (frand() < 1.0f / SIM_RATE) {
                ent.ai_tick = 0;
                ent.heading_angle = frand() * 2 * M_PI;
                ent.ai_state = AIState::kIdleMoving;
            }
            Vector rand = Vector::rand(PLAYER_ACCELERATION * 0.5);
            ent.acceleration.set(rand.x, rand.y);
            break;
        }
        case AIState::kIdleMoving: {
            if (ent.ai_tick >= 2.5 * SIM_RATE) {
                ent.ai_tick = 0;
                ent.ai_state = AIState::kIdle;
            }
            // 2.5 heading perturbations per game-second. Same inversion bug
            // as above; with `>` the sandstorm's heading wobbled on every
            // tick at high SIM_RATE, producing visible "spasms".
            if (frand() < 2.5f / SIM_RATE)
                ent.heading_angle += frand() * M_PI - M_PI / 2;
            Vector head;
            head.unit_normal(ent.heading_angle);
            head.set_magnitude(PLAYER_ACCELERATION);
            Vector rand;
            rand.unit_normal(ent.heading_angle + frand() * M_PI - M_PI / 2);
            rand.set_magnitude(PLAYER_ACCELERATION * 0.5);
            head += rand;
            ent.acceleration.set(head.x, head.y);
            break;
        }
        case AIState::kReturning: {
            default_tick_returning(sim, ent, 1.5);
            break;
        }
        default:
            ent.ai_state = AIState::kIdle;
            break;
    }
    if (sim->ent_alive(ent.parent)) {
        Entity &parent = sim->get_ent(ent.parent);
        ent.acceleration = (ent.acceleration + parent.acceleration) * 0.75;
    }
}

static void tick_digger(Simulation *sim, Entity &ent) {
    ent.input = 0;
    if (sim->ent_alive(ent.target)) {
        Entity &target = sim->get_ent(ent.target);
        Vector v(target.x - ent.x, target.y - ent.y);
        _focus_lose_clause(ent, v);
        v.set_magnitude(PLAYER_ACCELERATION * 0.95);
        if (ent.health / ent.max_health > 0.1) {
            BIT_SET(ent.input, InputFlags::kAttacking);
        } else {
            BIT_SET(ent.input, InputFlags::kDefending);
            v *= -1;
        }
        ent.acceleration = v;
        ent.set_eye_angle(v.angle());
        return;
    } else {
        if (!(ent.target == NULL_ENTITY)) {
            ent.ai_state = AIState::kIdle;
            ent.ai_tick = 0;
        }
        ent.target = find_nearest_enemy(sim, ent, ent.detection_radius + ent.radius);
        switch(ent.ai_state) {
            case AIState::kIdle: {
                ent.set_eye_angle(frand() * M_PI * 2);
                ent.ai_state = AIState::kIdleMoving;
                ent.ai_tick = 0;
                break;
            }
            case AIState::kIdleMoving: {
                if (ent.ai_tick > 5 * SIM_RATE)
                    ent.ai_state = AIState::kIdle;
                ent.acceleration.unit_normal(ent.eye_angle).set_magnitude(PLAYER_ACCELERATION);
                break;
            }
            case AIState::kReturning: {
                default_tick_returning(sim, ent);
                break;
            }
        }
    }
}

void tick_ai_behavior(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    if (sim->ent_alive(ent.seg_head)) return;
    ent.acceleration.set(0,0);
    if (!(ent.parent == NULL_ENTITY)) {
        if (!sim->ent_alive(ent.parent)) {
            if (BIT_AT(ent.flags, EntityFlags::kDieOnParentDeath))
                sim->request_delete(ent.id);
            ent.set_parent(NULL_ENTITY);
        } else {
            Entity &parent = sim->get_ent(ent.parent);
            Vector delta(parent.x - ent.x, parent.y - ent.y);
            if (delta.magnitude() > SUMMON_RETREAT_RADIUS) {
                ent.target = NULL_ENTITY;
                ent.ai_state = AIState::kReturning;
            }
        }
    }
    if (BIT_AT(ent.flags, EntityFlags::kIsCulled)) {
        ent.target = NULL_ENTITY;
        ent.ai_tick = 0;
        return;
    }
    switch(ent.mob_id) {
        case MobID::kBabyAnt:            
        case MobID::kLadybug:
        case MobID::kMassiveLadybug:
            tick_default_passive(sim, ent);
            break;
        case MobID::kBee:
            tick_bee_passive(sim, ent);
            break;
        case MobID::kCentipede:
            tick_centipede_passive(sim, ent);
            break;
        case MobID::kEvilCentipede:
            tick_centipede_aggro(sim, ent);
            break;
        case MobID::kDesertCentipede:
            tick_centipede_neutral(sim, ent, 1.33);
            break;
        case MobID::kWorkerAnt:
        case MobID::kDarkLadybug:
        case MobID::kShinyLadybug:
            tick_default_neutral(sim, ent);
            break;
        case MobID::kSoldierAnt:
        case MobID::kBeetle:
        case MobID::kMassiveBeetle:
            tick_default_aggro(sim, ent, 0.95);
            break;
        case MobID::kScorpion:
            tick_default_aggro(sim, ent, 1.20);
            break;
        case MobID::kSpider:
            // Web radius scales with the spider's rolled rarity (Mythic
            // spider drops MOB_RADIUS_MULT[Mythic]× the radius web).
            if (ent.lifetime % (SIM_RATE) == 0)
                alloc_web(sim, 25 * mob_radius_mult(ent.mob_rarity), ent);
            tick_default_aggro(sim, ent, 1.20);
            break;
        case MobID::kQueenAnt:
            if (ent.lifetime % (2 * SIM_RATE) == 0) {
                Vector behind;
                behind.unit_normal(ent.angle + M_PI);
                behind *= ent.radius;
                // Spawned soldier inherits the queen's rolled rarity —
                // a Mythic Queen pops out Mythic-sized soldiers, so
                // their bodies match the queen they came from.
                Entity &spawned = alloc_mob(sim, MobID::kSoldierAnt, ent.x + behind.x, ent.y + behind.y, ent.team, (int)ent.mob_rarity);
                entity_set_despawn_tick(spawned, 10 * SIM_RATE);
                spawned.set_parent(ent.parent);
            }
            tick_default_aggro(sim, ent, 0.95);
            break;
        case MobID::kWasp:
            tick_wasp_aggro(sim, ent);
            break;
        case MobID::kHornet:
            tick_hornet_aggro(sim, ent);
            break;
        case MobID::kBush:
        case MobID::kBoulder:
        case MobID::kRock:
        case MobID::kCactus:
        case MobID::kSquare:
            break;
        case MobID::kSandstorm:
            tick_sandstorm(sim, ent);
            break;
        case MobID::kDigger:
            tick_digger(sim, ent);
        case MobID::kLeafbug:
            tick_default_neutral(sim, ent);
            break;
        case MobID::kMantis:
            tick_mantis_aggro(sim, ent);
            break;
        default:
            break;
    }
    ++ent.ai_tick;
}