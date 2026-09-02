#include "render/render_system.hpp"

#include "render/scene3d.hpp"
#include "runtime.hpp"

#include <cmath>

namespace clay {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

Rgba u8c(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return Rgba{(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
}
Rgba f32c(float r, float g, float b, float a) {
    return u8c((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f),
               (uint32_t)(b * 255.0f), (uint32_t)(a * 255.0f));
}

void draw_ring(IRenderer &r, float cx, float cy, float radius, Rgba c) {
    const int segments = 48;
    float prev_x = cx + radius;
    float prev_y = cy;
    for (int i = 1; i <= segments; i++) {
        float a = (float)i / (float)segments * (float)kTwoPi;
        float x = cx + std::cos(a) * radius;
        float y = cy + std::sin(a) * radius;
        r.draw_line(prev_x, prev_y, x, y, c);
        prev_x = x;
        prev_y = y;
    }
}

} // namespace

void GardenRenderSystem::render(Runtime &rt, IRenderer &renderer) {
    Rgba soil = u8c(38, 32, 27, 255);
    renderer.begin_frame(soil);

    const int w = rt.width();
    const int h = rt.height();

    ComponentStorage<Transform2D> &ts = rt.world().storage<Transform2D>();
    ComponentStorage<Color> &cs = rt.world().storage<Color>();
    ComponentStorage<RippleRing> &rs = rt.world().storage<RippleRing>();

    renderer.fill_rect(0.0f, h - 46.0f, w, 46.0f, u8c(62, 52, 38, 255));

    for (size_t i = 0; i < ts.count(); i++) {
        Entity e = ts.owner[i];
        Transform2D &t = ts.dense[i];
        Color *c = cs.find(e);
        Rgba base = f32c(c ? c->r : 0.63f, c ? c->g : 0.63f,
                         c ? c->b : 0.63f, 1.0f);
        Kind *k = rt.world().storage<Kind>().find(e);
        Species species = k ? k->species : Species::Unknown;

        switch (species) {
        case Species::Animal: {
            renderer.fill_circle(t.x, t.y, 7.0f, base);
            Rgba outline = u8c((uint32_t)(base.r / 2), (uint32_t)(base.g / 2),
                               (uint32_t)(base.b / 2), 255);
            renderer.draw_line(t.x - 8, t.y, t.x + 8, t.y, outline);
            Velocity *v = rt.world().storage<Velocity>().find(e);
            if (v) {
                float len = std::sqrt(v->x * v->x + v->y * v->y);
                if (len > 1.0f) {
                    float ux = v->x / len * 9.0f;
                    float uy = v->y / len * 9.0f;
                    renderer.draw_line(t.x, t.y, t.x + ux, t.y + uy, outline);
                }
            }
            break;
        }
        case Species::Sculpture: {
            float s = 14.0f;
            renderer.fill_triangle(t.x, t.y - s, t.x - s, t.y + s, t.x + s,
                                   t.y + s, base);
            Rgba shade = u8c((uint32_t)(base.r / 2), (uint32_t)(base.g / 2),
                             (uint32_t)(base.b / 3), 255);
            renderer.fill_triangle(t.x, t.y - s, t.x - s, t.y + s, t.x,
                                   t.y + s, shade);
            renderer.fill_circle(t.x, t.y - s, 3.0f, u8c(255, 250, 235, 255));
            break;
        }
        case Species::Ripple: {
            RippleRing *ring = rs.find(e);
            float radius = ring ? ring->radius : 20.0f;
            float alpha = c ? c->a * 255.0f : 255.0f;
            Rgba dim = u8c(base.r, base.g, base.b, (uint32_t)alpha);
            draw_ring(renderer, t.x, t.y, radius, dim);
            break;
        }
        case Species::Pebble:
            renderer.fill_circle(t.x, t.y, 2.2f, base);
            break;
        default:
            break;
        }
    }

    renderer.draw_line((float)rt.cursor_x() - 8, (float)rt.cursor_y(),
                       (float)rt.cursor_x() + 8, (float)rt.cursor_y(),
                       u8c(255, 230, 190, 255));
    renderer.draw_line((float)rt.cursor_x(), (float)rt.cursor_y() - 8,
                       (float)rt.cursor_x(), (float)rt.cursor_y() + 8,
                       u8c(255, 230, 190, 255));

    if (rt.flash_remaining() > 0.0) {
        float t = (float)(rt.flash_remaining() /
                          (rt.flash_duration() > 0.0 ? rt.flash_duration()
                                                     : 1.0));
        const Color &fc = rt.flash_color();
        Rgba over = f32c(fc.r, fc.g, fc.b, 0.0f + t);
        renderer.fill_rect(0, 0, w, h, over);
    }

    renderer.end_frame();
}

/* ------------------------------------------------------ 3D scene frame */

Scene3DRenderSystem::Scene3DRenderSystem(ClayScene &scene, float fov_y_rad)
    : scene_(scene) {
    (void)fov_y_rad; /* fov now comes from the scene camera; kept for API
                      * compatibility with existing callers. */
}

void Scene3DRenderSystem::render(Runtime &rt, IRenderer &renderer) {
    renderer.begin_frame(u8c(24, 26, 34, 255));

    const int w = rt.width();
    const int h = rt.height();
    if (w <= 0 || h <= 0) {
        renderer.end_frame();
        return;
    }

    /* Use the scene's camera (eye/target/up + fov) if the .clay file
     * declares one; otherwise the ClayCamera defaults apply (z=6, origin). */
    cl_m4 view = scene_.view_matrix();
    cl_m4 proj = scene_.proj_matrix((float)w / (float)h);
    scene_.render(renderer, view, proj);

    renderer.end_frame();
}

} // namespace clay