#include "systems/builtin.hpp"

#include "runtime.hpp"

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
    double delta = (v.kind == CLAY_VAR_I64) ? (double)v.i
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
            WorldTransform2D wt{
                locals.dense[i].x, locals.dense[i].y, locals.dense[i].rotation,
                locals.dense[i].scale};
            if (!prev || prev->x != wt.x || prev->y != wt.y ||
                prev->rotation != wt.rotation || prev->scale != wt.scale) {
                world_ts.set(e, wt);
                changed = true;
            }
        }

        /* Pass 2: resolve children whose parent already has a world transform. */
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

} // namespace clay