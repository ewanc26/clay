#include "systems/builtin.hpp"

#include "runtime.hpp"
#include "systems/collision.hpp"

#include <cmath>
#include <cstring>
#include <vector>

namespace clay {

namespace {

constexpr float kMargin = 24.0f;

void wrap(float &v, float limit) {
    if (v < -kMargin) v += (limit + 2.0f * kMargin);
    if (v > limit + kMargin) v -= (limit + 2.0f * kMargin);
}

bool is_wanderer(Species s) {
    return s == Species::Animal || s == Species::Pebble;
}

/* Compose a local Transform2D with a parent WorldTransform2D into a child
 * WorldTransform2D: world = parent * local. */
WorldTransform2D compose_transform(const Transform2D &child,
                                   const WorldTransform2D &parent) {
    float cr = std::cos(parent.rotation);
    float sr = std::sin(parent.rotation);
    float ps = parent.scale;
    WorldTransform2D wt;
    wt.x = parent.x + child.x * ps * cr - child.y * ps * sr;
    wt.y = parent.y + child.x * ps * sr + child.y * ps * cr;
    wt.rotation = parent.rotation + child.rotation;
    wt.scale = parent.scale * child.scale;
    return wt;
}

/* Return the mass of an entity's PhysicsBody2D, or 0 if it has none. */
float body_mass(World &world, Entity e) {
    PhysicsBody2D *b = world.storage<PhysicsBody2D>().find(e);
    return b ? b->mass : 0.0f;
}

} // namespace

void MovementSystem::update(Runtime &rt, double dt) {
    ComponentStorage<Transform2D> &ts = rt.world().storage<Transform2D>();
    ComponentStorage<Velocity> &vs = rt.world().storage<Velocity>();
    ComponentStorage<RippleRing> &rs = rt.world().storage<RippleRing>();

    for (size_t i = 0; i < ts.count(); i++) {
        Entity e = ts.owner[i];
        Kind *k = rt.world().storage<Kind>().find(e);
        if (k && is_wanderer(k->species)) {
            Velocity *v = vs.find(e);
            if (!v) continue;
            ts.dense[i].x += v->x * (float)dt;
            ts.dense[i].y += v->y * (float)dt;
            wrap(ts.dense[i].x, (float)rt.width());
            wrap(ts.dense[i].y, (float)rt.height());
        }
        RippleRing *ring = rs.find(e);
        if (ring) {
            ring->radius += ring->speed * (float)dt;
        }
    }
    note_reaction(rt);
}

void LifespanSystem::update(Runtime &rt, double dt) {
    ComponentStorage<LifeSpan> &ls = rt.world().storage<LifeSpan>();
    std::vector<std::pair<Entity, float>> expiring;
    for (size_t i = 0; i < ls.count(); i++) {
        Kind *k = rt.world().storage<Kind>().find(ls.owner[i]);
        if (k && (k->species == Species::Sculpture)) continue;
        ls.dense[i].remaining -= (float)dt;
        if (ls.dense[i].remaining <= 0.0f) {
            expiring.push_back({ls.owner[i], ls.dense[i].remaining});
        }
    }
    if (expiring.empty()) return;
    for (const auto &pair : expiring) rt.destroy_entity(pair.first);
    note_reaction(rt);
}

void CursorMagnetSystem::update(Runtime &rt, double dt) {
    ComponentStorage<MagnetStrength> &ms = rt.world().storage<MagnetStrength>();
    ComponentStorage<Transform2D> &ts = rt.world().storage<Transform2D>();
    ComponentStorage<Velocity> &vs = rt.world().storage<Velocity>();

    float cx = (float)rt.cursor_x();
    float cy = (float)rt.cursor_y();

    for (size_t i = 0; i < ms.count(); i++) {
        Transform2D *t = ts.find(ms.owner[i]);
        Velocity *v = vs.find(ms.owner[i]);
        if (!t || !v) continue;
        float dx = cx - t->x;
        float dy = cy - t->y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 1.0f) continue;
        float strength = ms.dense[i].to_cursor;
        v->x += dx / dist * strength * (float)dt;
        v->y += dy / dist * strength * (float)dt;

        float speed = std::sqrt(v->x * v->x + v->y * v->y);
        constexpr float kMaxSpeed = 240.0f;
        if (speed > kMaxSpeed) {
            v->x = v->x / speed * kMaxSpeed;
            v->y = v->y / speed * kMaxSpeed;
        }
    }
    note_reaction(rt);
}

void HueShiftSystem::update(Runtime &rt, double dt) {
    (void)dt;
    ComponentStorage<Color> &cs = rt.world().storage<Color>();
    ComponentStorage<Velocity> &vs = rt.world().storage<Velocity>();

    for (size_t i = 0; i < cs.count(); i++) {
        Entity e = cs.owner[i];
        Velocity *v = vs.find(e);
        if (!v) continue;
        float speed = std::sqrt(v->x * v->x + v->y * v->y);
        float shift = speed / 240.0f * 0.012f;
        Color &c = cs.dense[i];
        float amount = shift * std::sin((float)rt.sim_time() * 0.7f);
        c.r += amount;
        c.g -= amount * 0.5f;
        c.b += amount * 0.5f;
        if (c.r < 0.0f) c.r = 0.0f;
        if (c.r > 1.0f) c.r = 1.0f;
        if (c.g < 0.0f) c.g = 0.0f;
        if (c.b < 0.0f) c.b = 0.0f;
    }
}

void RippleSystem::on_event(Runtime &rt, const Event &ev) {
    if (channel_name(ev.channel) != "input.wheel") return;
    cl_variant v = ev.value;
    double delta = (v.kind == CLAY_VAR_I64)
                       ? (double)v.i
                       : (v.kind == CLAY_VAR_F64 ? v.f : 0.0);
    if (delta != 0.0) {
        saw_wheel_ = true;
        last_wheel_ = delta;
    }
    note_reaction(rt);
}

void RippleSystem::update(Runtime &rt, double dt) {
    ComponentStorage<RippleRing> &rs = rt.world().storage<RippleRing>();
    ComponentStorage<Color> &cs = rt.world().storage<Color>();

    if (saw_wheel_) {
        for (size_t i = 0; i < rs.count(); i++) {
            Color *c = cs.find(rs.owner[i]);
            if (c) {
                c->a += (float)last_wheel_ * 0.05f;
                if (c->a > 1.0f) c->a = 1.0f;
                if (c->a < 0.1f) c->a = 0.1f;
            }
        }
        saw_wheel_ = false;
        note_reaction(rt);
    }
    (void)dt;
}

void SceneGraphSystem::update(Runtime &rt, double dt) {
    (void)dt;
    World &world = rt.world();
    ComponentStorage<Transform2D> &locals = world.storage<Transform2D>();
    ComponentStorage<Parent> &parents = world.storage<Parent>();
    ComponentStorage<WorldTransform2D> &world_ts =
        world.storage<WorldTransform2D>();

    /* Multi-pass resolution: each pass resolves one level of hierarchy depth.
     * Deterministic because ComponentStorage iterates in insertion order. */
    constexpr int kMaxPasses = 16;
    for (int pass = 0; pass < kMaxPasses; pass++) {
        bool changed = false;

        /* Pass 1: copy local -> world for entities without a Parent. */
        for (size_t i = 0; i < locals.count(); i++) {
            Entity e = locals.owner[i];
            if (parents.find(e)) continue; /* has parent, defer */
            WorldTransform2D *prev = world_ts.find(e);
            WorldTransform2D wt{locals.dense[i].x, locals.dense[i].y,
                                locals.dense[i].rotation,
                                locals.dense[i].scale};
            if (!prev || prev->x != wt.x || prev->y != wt.y ||
                prev->rotation != wt.rotation || prev->scale != wt.scale) {
                world_ts.set(e, wt);
                changed = true;
            }
        }

        /* Pass 2: resolve children whose parent already has a world transform.
         */
        for (size_t i = 0; i < parents.count(); i++) {
            Entity child = parents.owner[i];
            Entity parent = parents.dense[i].parent;

            Transform2D *local = locals.find(child);
            WorldTransform2D *parent_wt = world_ts.find(parent);
            if (!local || !parent_wt) continue;

            WorldTransform2D wt = compose_transform(*local, *parent_wt);
            WorldTransform2D *prev = world_ts.find(child);
            if (!prev || prev->x != wt.x || prev->y != wt.y ||
                prev->rotation != wt.rotation || prev->scale != wt.scale) {
                world_ts.set(child, wt);
                changed = true;
            }
        }

        if (!changed) break;
    }
}

void PhysicsSystem::update(Runtime &rt, double dt) {
    (void)dt;
    World &world = rt.world();

    ComponentStorage<WorldTransform2D> &wts = world.storage<WorldTransform2D>();
    ComponentStorage<BoxCollider2D> &boxes = world.storage<BoxCollider2D>();
    ComponentStorage<CircleCollider2D> &circles =
        world.storage<CircleCollider2D>();

    auto resolve_pos = [&](Entity e) -> cl_v2 {
        WorldTransform2D *wt = wts.find(e);
        if (wt) return cl_v2_make(wt->x, wt->y);
        Transform2D *t = world.storage<Transform2D>().find(e);
        return t ? cl_v2_make(t->x, t->y) : cl_v2_make(0, 0);
    };

    /* Helper: publish a collision event and correct positions. */
    auto handle_contact = [&](Entity a, Entity b, const Contact2D &c) {
        rt.hub().publish(channel(CLAY_CH_COLLISION),
                         cl_variant_str(cl_str_c("collision")));
        float ma = body_mass(world, a);
        float mb = body_mass(world, b);
        float total = ma + mb;
        if (total <= 0.0f) return;
        float ka = mb / total; /* lighter body moves more */
        float kb = ma / total;
        Transform2D *ta = world.storage<Transform2D>().find(a);
        Transform2D *tb = world.storage<Transform2D>().find(b);
        if (ta) {
            ta->x -= c.normal.x * c.depth * ka;
            ta->y -= c.normal.y * c.depth * ka;
        }
        if (tb) {
            tb->x += c.normal.x * c.depth * kb;
            tb->y += c.normal.y * c.depth * kb;
        }
    };

    /* Box vs Box. */
    for (size_t i = 0; i < boxes.count(); i++) {
        Entity a = boxes.owner[i];
        for (size_t j = i + 1; j < boxes.count(); j++) {
            Entity b = boxes.owner[j];
            Contact2D c = aabb_aabb(resolve_pos(a), boxes.dense[i].width / 2.0f,
                                    boxes.dense[i].height / 2.0f,
                                    resolve_pos(b), boxes.dense[j].width / 2.0f,
                                    boxes.dense[j].height / 2.0f);
            if (c.overlap) handle_contact(a, b, c);
        }
    }

    /* Box vs Circle. */
    for (size_t i = 0; i < boxes.count(); i++) {
        Entity a = boxes.owner[i];
        for (size_t j = 0; j < circles.count(); j++) {
            Entity b = circles.owner[j];
            Contact2D c =
                aabb_circle(resolve_pos(a), boxes.dense[i].width / 2.0f,
                            boxes.dense[i].height / 2.0f, resolve_pos(b),
                            circles.dense[j].radius);
            if (c.overlap) handle_contact(a, b, c);
        }
    }

    /* Circle vs Circle. */
    for (size_t i = 0; i < circles.count(); i++) {
        Entity a = circles.owner[i];
        for (size_t j = i + 1; j < circles.count(); j++) {
            Entity b = circles.owner[j];
            Contact2D c =
                circle_circle(resolve_pos(a), circles.dense[i].radius,
                              resolve_pos(b), circles.dense[j].radius);
            if (c.overlap) handle_contact(a, b, c);
        }
    }
}

void AnimationSystem::update(Runtime &rt, double dt) {
    World &world = rt.world();
    ComponentStorage<Tween> &tws = world.storage<Tween>();

    for (size_t i = 0; i < tws.count(); i++) {
        Tween &tw = tws.dense[i];
        if (!tw.active) continue;

        tw.elapsed += (float)dt;
        float t = tw.duration > 0.0f ? (tw.elapsed / tw.duration) : 1.0f;
        float eased = cl_ease_apply(tw.ease, t);
        tw.value = tw.from + (tw.to - tw.from) * eased;

        if (tw.elapsed >= tw.duration) {
            if (tw.loop) {
                tw.elapsed -= tw.duration;
                if (tw.elapsed < 0.0f) tw.elapsed = 0.0f;
                t = tw.elapsed / tw.duration;
                eased = cl_ease_apply(tw.ease, t);
                tw.value = tw.from + (tw.to - tw.from) * eased;
            } else {
                tw.active = false;
                tw.value = tw.to;
                rt.hub().publish(
                    channel(CLAY_CH_WORLD),
                    cl_variant_str(cl_str_c("animation.complete")));
            }
        }
    }
}

void AudioSourceSystem::update(Runtime &rt, double) {
    World &world = rt.world();
    ComponentStorage<AudioSource2D> &sources = world.storage<AudioSource2D>();
    ComponentStorage<WorldTransform2D> &world_transforms =
        world.storage<WorldTransform2D>();
    ComponentStorage<Transform2D> &transforms = world.storage<Transform2D>();
    for (size_t i = 0; i < sources.dense.size(); ++i) {
        const Entity entity = sources.owner[i];
        AudioSource2D &source = sources.dense[i];
        if (source.voice == 0 || !source.enabled) continue;
        const WorldTransform2D *world_transform = world_transforms.find(entity);
        const Transform2D *transform = transforms.find(entity);
        const float x = world_transform ? world_transform->x
                                        : (transform ? transform->x : 0.0F);
        const float y = world_transform ? world_transform->y
                                        : (transform ? transform->y : 0.0F);
        rt.audio().set_voice_position(source.voice, x, y, source.max_distance);
    }
}

} // namespace clay
