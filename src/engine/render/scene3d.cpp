#include "render/scene3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace clay {

namespace {

/* Read an optional [x,y,z] array into a cl_v3; returns identity on absence. */
cl_v3 vec3_from(cl_json_node *n, cl_v3 dflt) {
    if (!n || n->kind != CLAY_J_ARR || n->arr.n < 3) return dflt;
    cl_json_node *x = n->arr.items[0];
    cl_json_node *y = n->arr.items[1];
    cl_json_node *z = n->arr.items[2];
    auto num = [](cl_json_node *v, float d) {
        if (!v) return d;
        if (v->kind == CLAY_J_F64) return (float)v->f;
        if (v->kind == CLAY_J_I64) return (float)v->i;
        return d;
    };
    return cl_v3_make(num(x, dflt.x), num(y, dflt.y), num(z, dflt.z));
}

Rgba color_from(cl_json_node *n, Rgba dflt) {
    if (!n) return dflt;
    cl_v3 v = vec3_from(n, cl_v3_make(dflt.r, dflt.g, dflt.b));
    Rgba c;
    c.r = (uint8_t)std::clamp(v.x, 0.0f, 255.0f);
    c.g = (uint8_t)std::clamp(v.y, 0.0f, 255.0f);
    c.b = (uint8_t)std::clamp(v.z, 0.0f, 255.0f);
    c.a = dflt.a;
    if (n->arr.n >= 4 && n->arr.items[3]->kind == CLAY_J_I64)
        c.a = (uint8_t)n->arr.items[3]->i;
    return c;
}

float number(cl_json_node *n, float dflt) {
    if (!n) return dflt;
    if (n->kind == CLAY_J_F64) return (float)n->f;
    if (n->kind == CLAY_J_I64) return (float)n->i;
    return dflt;
}

bool string_equals(cl_json_node *n, const char *want) {
    if (!n || n->kind != CLAY_J_STR) return false;
    cl_str s = n->s;
    size_t len = 0;
    while (want[len]) len++;
    return s.len == len && std::memcmp(s.data, want, len) == 0;
}

std::string node_str(cl_json_node *n) {
    if (!n || n->kind != CLAY_J_STR) return {};
    return std::string(n->s.data, n->s.len);
}

/* Build a model matrix from an optional `transform` object. */
cl_m4 transform_from(cl_json_node *t) {
    cl_m4 m = cl_m4_identity();
    if (!t || t->kind != CLAY_J_OBJ) return m;

    cl_v3 pos = vec3_from(cl_json_get_cstr(t, "pos"), cl_v3_make(0, 0, 0));
    cl_v3 euler = vec3_from(cl_json_get_cstr(t, "euler"), cl_v3_make(0, 0, 0));

    /* uniform scale if scalar, per-axis if [x,y,z]. */
    cl_m4 s = cl_m4_identity();
    cl_json_node *scale = cl_json_get_cstr(t, "scale");
    if (scale && scale->kind == CLAY_J_ARR && scale->arr.n >= 3) {
        s = cl_m4_scale(number(scale->arr.items[0], 1),
                        number(scale->arr.items[1], 1),
                        number(scale->arr.items[2], 1));
    }

    cl_m4 r = cl_m4_identity();
    r = cl_m4_mul(r, cl_m4_rotate_y(euler.y));
    r = cl_m4_mul(r, cl_m4_rotate_x(euler.x));
    r = cl_m4_mul(r, cl_m4_rotate_z(euler.z));

    cl_m4 tr = cl_m4_translate(pos.x, pos.y, pos.z);
    return cl_m4_mul(cl_m4_mul(cl_m4_mul(tr, s), r), cl_m4_identity());
}

} // namespace

bool ClayScene::load(cl_str text) {
    meshes_.clear();
    instances_.clear();
    index_.clear();
    light_ = {};
    point_lights_.clear();
    camera_ = {};
    settings_ = {};

    cl_arena a;
    std::vector<unsigned char> storage(1 << 16);
    cl_arena_init(&a, storage.data(), storage.size());

    cl_json_node root;
    cl_err err = cl_json_parse(&root, &a, text);
    if (err != CLAY_OK) return false;

    cl_json_node *version = cl_json_get_cstr(&root, "version");
    if (version && version->kind == CLAY_J_I64 && version->i != 1) return false;

    /* --- settings --- */
    cl_json_node *settings = cl_json_get_cstr(&root, "settings");
    if (settings && settings->kind == CLAY_J_OBJ) {
        cl_json_node *seed = cl_json_get_cstr(settings, "seed");
        if (seed && seed->kind == CLAY_J_I64)
            settings_.seed = (uint64_t)seed->i;
        cl_json_node *fps = cl_json_get_cstr(settings, "fps");
        if (fps && fps->kind == CLAY_J_I64)
            settings_.fps = (int)fps->i;
        cl_json_node *res = cl_json_get_cstr(settings, "resolution");
        if (res && res->kind == CLAY_J_ARR && res->arr.n >= 2) {
            if (res->arr.items[0] && res->arr.items[0]->kind == CLAY_J_I64)
                settings_.resolution[0] = (int)res->arr.items[0]->i;
            if (res->arr.items[1] && res->arr.items[1]->kind == CLAY_J_I64)
                settings_.resolution[1] = (int)res->arr.items[1]->i;
        }
    }

    /* --- meshes --- */
    cl_json_node *meshes = cl_json_get_cstr(&root, "meshes");
    if (meshes && meshes->kind == CLAY_J_ARR) {
        for (size_t i = 0; i < meshes->arr.n; i++) {
            cl_json_node *m = meshes->arr.items[i];
            if (!m || m->kind != CLAY_J_OBJ) continue;
            ClayMesh cm;
            cm.name = node_str(cl_json_get_cstr(m, "name"));
            cm.uid = node_str(cl_json_get_cstr(m, "uid"));
            cm.color =
                color_from(cl_json_get_cstr(m, "color"), {200, 200, 200, 255});

            cl_json_node *prim = cl_json_get_cstr(m, "primitive");
            if (prim) {
                float lit = number(cl_json_get_cstr(m, "radius"), 0.5f);
                if (string_equals(prim, "cube"))
                    build_cube(cm.mesh, 0.5f);
                else if (string_equals(prim, "sphere"))
                    build_sphere(cm.mesh, lit, 16, 12);
                else
                    build_plane(cm.mesh, 2.0f, 2.0f, 4, 4);
            } else {
                /* Inline positions + indices. */
                cl_json_node *pos = cl_json_get_cstr(m, "positions");
                cl_json_node *idx = cl_json_get_cstr(m, "indices");
                if (pos && pos->kind == CLAY_J_ARR && idx &&
                    idx->kind == CLAY_J_ARR) {
                    std::vector<cl_v3> verts;
                    verts.reserve(pos->arr.n);
                    for (size_t k = 0; k < pos->arr.n; k++)
                        verts.push_back(
                            vec3_from(pos->arr.items[k], {0, 0, 0}));
                    for (size_t k = 0; k + 2 < idx->arr.n; k += 3) {
                        auto at = [&](size_t j) -> unsigned {
                            cl_json_node *e = idx->arr.items[k + j];
                            return e && e->kind == CLAY_J_I64 ? (unsigned)e->i
                                                              : 0u;
                        };
                        unsigned ia = at(0), ib = at(1), ic = at(2);
                        if (ia < verts.size() && ib < verts.size() &&
                            ic < verts.size())
                            cm.mesh.add_triangle(verts[ia], verts[ib],
                                                 verts[ic]);
                    }
                }
            }
            if (cm.name.empty() || cm.mesh.empty()) continue;
            index_[cm.name] = meshes_.size();
            meshes_.push_back(std::move(cm));
        }
    }

    /* --- scene --- */
    cl_json_node *scene = cl_json_get_cstr(&root, "scene");
    if (scene && scene->kind == CLAY_J_ARR) {
        for (size_t i = 0; i < scene->arr.n; i++) {
            cl_json_node *e = scene->arr.items[i];
            if (!e || e->kind != CLAY_J_OBJ) continue;
            cl_json_node *comp = cl_json_get_cstr(e, "component");
            if (string_equals(comp, "directional_light")) {
                light_.dir = vec3_from(cl_json_get_cstr(e, "dir"),
                                       cl_v3_make(0.3f, 0.5f, 0.8f));
                light_.intensity =
                    number(cl_json_get_cstr(e, "intensity"), 1.0f);
                continue;
            }
            if (string_equals(comp, "camera")) {
                camera_.eye = vec3_from(cl_json_get_cstr(e, "eye"),
                                        cl_v3_make(0, 0, 6));
                camera_.target = vec3_from(cl_json_get_cstr(e, "target"),
                                           cl_v3_make(0, 0, 0));
                camera_.up = vec3_from(cl_json_get_cstr(e, "up"),
                                       cl_v3_make(0, 1, 0));
                camera_.fov_y_rad =
                    number(cl_json_get_cstr(e, "fov"), 0.9f);
                camera_.znear = number(cl_json_get_cstr(e, "znear"), 0.1f);
                camera_.zfar = number(cl_json_get_cstr(e, "zfar"), 100.0f);
                continue;
            }
            if (string_equals(comp, "point_light")) {
                ClayPointLight pl;
                pl.pos = vec3_from(cl_json_get_cstr(e, "pos"),
                                   cl_v3_make(0, 0, 0));
                pl.intensity = number(cl_json_get_cstr(e, "intensity"), 1.0f);
                pl.attenuation =
                    number(cl_json_get_cstr(e, "attenuation"), 0.0f);
                point_lights_.push_back(pl);
                continue;
            }
            if (!string_equals(comp, "mesh_instance")) continue;

            ClayInstance inst;
            inst.mesh_name = node_str(cl_json_get_cstr(e, "mesh"));
            inst.mesh_uid = node_str(cl_json_get_cstr(e, "mesh_uid"));
            inst.model = transform_from(cl_json_get_cstr(e, "transform"));
            inst.color =
                color_from(cl_json_get_cstr(e, "color"), {200, 200, 200, 255});
            instances_.push_back(std::move(inst));
        }
    }
    return true;
}

cl_m4 ClayScene::view_matrix() const {
    return cl_m4_look_at(camera_.eye, camera_.target, camera_.up);
}

cl_m4 ClayScene::proj_matrix(float aspect) const {
    return cl_m4_perspective(camera_.fov_y_rad, aspect, camera_.znear,
                             camera_.zfar);
}

void ClayScene::render(IRenderer &r, cl_m4 view, cl_m4 proj) {
    /* Model matrix per instance composes translate*scale*rotate; the mesh
     * library supplies geometry + base color. */
    for (const ClayInstance &inst : instances_) {
        /* Resolve the mesh by name, falling back to uid (dual reference). */
        size_t idx = meshes_.size();
        auto it = index_.find(inst.mesh_name);
        if (it != index_.end()) {
            idx = it->second;
        } else if (!inst.mesh_uid.empty()) {
            for (size_t k = 0; k < meshes_.size(); k++) {
                if (meshes_[k].uid == inst.mesh_uid) {
                    idx = k;
                    break;
                }
            }
        }
        if (idx >= meshes_.size()) continue;
        const ClayMesh &cm = meshes_[idx];

        cl_v3 pl_pos = {0.0f, 0.0f, 0.0f};
        float pl_int = 0.0f;
        float pl_att = 0.0f;
        if (!point_lights_.empty()) {
            const ClayPointLight &pl = point_lights_[0];
            pl_pos = pl.pos;
            pl_int = pl.intensity;
            pl_att = pl.attenuation;
        }

        r.draw_mesh(cm.mesh, inst.model, view, proj, inst.color, light_.dir,
                    light_.intensity, pl_pos, pl_int, pl_att, 0.35f);
    }
}

} // namespace clay