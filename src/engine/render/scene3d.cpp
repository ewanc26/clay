#include "render/scene3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace clay {

namespace {

bool string_equals(cl_json_node *n, const char *want) {
    if (!n || n->kind != CLAY_J_STR) return false;
    size_t len = std::strlen(want);
    return n->s.len == len && std::memcmp(n->s.data, want, len) == 0;
}

bool read_string(cl_json_node *n, std::string &out) {
    if (!n || n->kind != CLAY_J_STR) return false;
    out.assign(n->s.data, n->s.len);
    return !out.empty();
}

bool read_number(cl_json_node *n, float &out) {
    if (!n) return false;
    double value;
    if (n->kind == CLAY_J_F64)
        value = n->f;
    else if (n->kind == CLAY_J_I64)
        value = (double)n->i;
    else
        return false;
    if (!std::isfinite(value) || value < -(double)std::numeric_limits<float>::max() ||
        value > (double)std::numeric_limits<float>::max())
        return false;
    out = (float)value;
    return true;
}

bool read_positive_int(cl_json_node *n, unsigned &out) {
    if (!n || n->kind != CLAY_J_I64 || n->i <= 0 ||
        (uint64_t)n->i > (uint64_t)std::numeric_limits<unsigned>::max())
        return false;
    out = (unsigned)n->i;
    return true;
}

bool read_vec3(cl_json_node *n, cl_v3 &out) {
    if (!n || n->kind != CLAY_J_ARR || n->arr.n != 3) return false;
    float x, y, z;
    if (!read_number(n->arr.items[0], x) || !read_number(n->arr.items[1], y) ||
        !read_number(n->arr.items[2], z))
        return false;
    out = cl_v3_make(x, y, z);
    return true;
}

bool read_color(cl_json_node *n, Rgba &out) {
    if (!n || n->kind != CLAY_J_ARR ||
        (n->arr.n != 3 && n->arr.n != 4))
        return false;

    uint8_t channels[4] = {0, 0, 0, 255};
    for (size_t i = 0; i < n->arr.n; i++) {
        cl_json_node *v = n->arr.items[i];
        if (!v || v->kind != CLAY_J_I64 || v->i < 0 || v->i > 255)
            return false;
        channels[i] = (uint8_t)v->i;
    }
    out = {channels[0], channels[1], channels[2], channels[3]};
    return true;
}

bool transform_from(cl_json_node *t, cl_m4 &out) {
    cl_v3 pos = cl_v3_make(0, 0, 0);
    cl_v3 euler = cl_v3_make(0, 0, 0);
    cl_v3 scale = cl_v3_make(1, 1, 1);

    if (t) {
        if (t->kind != CLAY_J_OBJ) return false;

        if (cl_json_node *p = cl_json_get_cstr(t, "pos")) {
            if (!read_vec3(p, pos)) return false;
        }
        if (cl_json_node *e = cl_json_get_cstr(t, "euler")) {
            if (!read_vec3(e, euler)) return false;
        }
        if (cl_json_node *s = cl_json_get_cstr(t, "scale")) {
            if (s->kind == CLAY_J_ARR) {
                if (!read_vec3(s, scale)) return false;
            } else {
                float uniform;
                if (!read_number(s, uniform)) return false;
                scale = cl_v3_make(uniform, uniform, uniform);
            }
        }
    }

    cl_m4 sm = cl_m4_scale(scale.x, scale.y, scale.z);
    cl_m4 rm = cl_m4_identity();
    rm = cl_m4_mul(rm, cl_m4_rotate_y(euler.y));
    rm = cl_m4_mul(rm, cl_m4_rotate_x(euler.x));
    rm = cl_m4_mul(rm, cl_m4_rotate_z(euler.z));
    cl_m4 tm = cl_m4_translate(pos.x, pos.y, pos.z);

    /* Row-vector convention: v' = v * S * R * T. Translation therefore stays
     * in world space instead of being scaled/rotated with the object. */
    out = cl_m4_mul(cl_m4_mul(sm, rm), tm);
    return true;
}

bool parse_render_settings(cl_json_node *settings, ClaySettings &out) {
    cl_json_node *seed = cl_json_get_cstr(settings, "seed");
    if (seed) {
        if (seed->kind == CLAY_J_I64) {
            if (seed->i < 0) return false;
            out.seed = (uint64_t)seed->i;
            out.has_seed = true;
        } else if (seed->kind == CLAY_J_NIL ||
                   (seed->kind == CLAY_J_BOOL && !seed->b)) {
            out.has_seed = false;
        } else {
            return false;
        }
    }

    if (cl_json_node *fps = cl_json_get_cstr(settings, "fps")) {
        if (fps->kind != CLAY_J_I64 || fps->i <= 0 || fps->i > 1000)
            return false;
        out.fps = (int)fps->i;
    }

    /* Compatibility with the first demo file written before the v1 contract
     * was tightened. `render.width/height` is canonical; `resolution` remains
     * an accepted alias for existing files. */
    if (cl_json_node *res = cl_json_get_cstr(settings, "resolution")) {
        if (res->kind != CLAY_J_ARR || res->arr.n != 2) return false;
        for (int i = 0; i < 2; i++) {
            cl_json_node *v = res->arr.items[i];
            if (!v || v->kind != CLAY_J_I64 || v->i <= 0 || v->i > 8192)
                return false;
            out.resolution[i] = (int)v->i;
        }
    }

    if (cl_json_node *render = cl_json_get_cstr(settings, "render")) {
        if (render->kind != CLAY_J_OBJ) return false;
        if (cl_json_node *width = cl_json_get_cstr(render, "width")) {
            if (width->kind != CLAY_J_I64 || width->i <= 0 || width->i > 8192)
                return false;
            out.resolution[0] = (int)width->i;
        }
        if (cl_json_node *height = cl_json_get_cstr(render, "height")) {
            if (height->kind != CLAY_J_I64 || height->i <= 0 || height->i > 8192)
                return false;
            out.resolution[1] = (int)height->i;
        }
        if (cl_json_node *clear = cl_json_get_cstr(render, "clear")) {
            if (!read_color(clear, out.clear)) return false;
        }
    }
    return true;
}

} // namespace

bool ClayScene::load(cl_str text) {
    meshes_.clear();
    instances_.clear();
    index_.clear();
    uid_index_.clear();
    light_ = {};
    point_lights_.clear();
    camera_ = {};
    settings_ = {};

    if (!text.data || text.len == 0) return false;
    if (text.len > (std::numeric_limits<size_t>::max() - (1u << 16)) / 8u)
        return false;
    const size_t arena_size =
        std::max<size_t>(1u << 16, text.len * 8u + (1u << 12));
    std::vector<unsigned char> storage(arena_size);
    cl_arena a;
    cl_arena_init(&a, storage.data(), storage.size());

    cl_json_node root;
    if (cl_json_parse(&root, &a, text) != CLAY_OK || root.kind != CLAY_J_OBJ)
        return false;

    cl_json_node *version = cl_json_get_cstr(&root, "version");
    if (!version || version->kind != CLAY_J_I64 || version->i != 1)
        return false;

    if (cl_json_node *settings = cl_json_get_cstr(&root, "settings")) {
        if (settings->kind != CLAY_J_OBJ ||
            !parse_render_settings(settings, settings_))
            return false;
    }

    if (cl_json_node *meshes = cl_json_get_cstr(&root, "meshes")) {
        if (meshes->kind != CLAY_J_ARR) return false;

        for (size_t i = 0; i < meshes->arr.n; i++) {
            cl_json_node *m = meshes->arr.items[i];
            if (!m || m->kind != CLAY_J_OBJ) return false;

            ClayMesh cm;
            if (!read_string(cl_json_get_cstr(m, "name"), cm.name)) return false;
            if (index_.find(cm.name) != index_.end()) return false;

            if (cl_json_node *uid = cl_json_get_cstr(m, "uid")) {
                if (!read_string(uid, cm.uid) ||
                    uid_index_.find(cm.uid) != uid_index_.end())
                    return false;
            }
            if (cl_json_node *color = cl_json_get_cstr(m, "color")) {
                if (!read_color(color, cm.color)) return false;
            }

            cl_json_node *prim = cl_json_get_cstr(m, "primitive");
            if (prim) {
                if (prim->kind != CLAY_J_STR) return false;
                if (string_equals(prim, "cube")) {
                    float half_extent = 0.5f;
                    if (cl_json_node *he = cl_json_get_cstr(m, "half_extent")) {
                        if (!read_number(he, half_extent) || half_extent <= 0.0f)
                            return false;
                    }
                    build_cube(cm.mesh, half_extent);
                } else if (string_equals(prim, "sphere")) {
                    float radius = 0.5f;
                    unsigned rings = 16, slices = 12;
                    if (cl_json_node *r = cl_json_get_cstr(m, "radius")) {
                        if (!read_number(r, radius) || radius <= 0.0f)
                            return false;
                    }
                    if (cl_json_node *r = cl_json_get_cstr(m, "rings")) {
                        if (!read_positive_int(r, rings) || rings < 2) return false;
                    }
                    if (cl_json_node *s = cl_json_get_cstr(m, "slices")) {
                        if (!read_positive_int(s, slices) || slices < 3)
                            return false;
                    }
                    build_sphere(cm.mesh, radius, rings, slices);
                } else if (string_equals(prim, "plane")) {
                    float width = 2.0f, height = 2.0f;
                    unsigned nx = 4, ny = 4;
                    if (cl_json_node *w = cl_json_get_cstr(m, "width")) {
                        if (!read_number(w, width) || width <= 0.0f) return false;
                    }
                    if (cl_json_node *h = cl_json_get_cstr(m, "height")) {
                        if (!read_number(h, height) || height <= 0.0f)
                            return false;
                    }
                    if (cl_json_node *x = cl_json_get_cstr(m, "nx")) {
                        if (!read_positive_int(x, nx)) return false;
                    }
                    if (cl_json_node *y = cl_json_get_cstr(m, "ny")) {
                        if (!read_positive_int(y, ny)) return false;
                    }
                    build_plane(cm.mesh, width, height, nx, ny);
                } else {
                    return false;
                }
            } else {
                cl_json_node *pos = cl_json_get_cstr(m, "positions");
                cl_json_node *idx = cl_json_get_cstr(m, "indices");
                if (!pos || pos->kind != CLAY_J_ARR || !idx ||
                    idx->kind != CLAY_J_ARR || pos->arr.n == 0 ||
                    idx->arr.n == 0 || idx->arr.n % 3 != 0)
                    return false;

                cm.mesh.positions.reserve(pos->arr.n);
                for (size_t k = 0; k < pos->arr.n; k++) {
                    cl_v3 v;
                    if (!read_vec3(pos->arr.items[k], v)) return false;
                    cm.mesh.positions.push_back(v);
                }
                cm.mesh.indices.reserve(idx->arr.n);
                for (size_t k = 0; k < idx->arr.n; k++) {
                    cl_json_node *entry = idx->arr.items[k];
                    if (!entry || entry->kind != CLAY_J_I64 || entry->i < 0 ||
                        (uint64_t)entry->i >= cm.mesh.positions.size() ||
                        (uint64_t)entry->i >
                            (uint64_t)std::numeric_limits<unsigned>::max())
                        return false;
                    cm.mesh.indices.push_back((unsigned)entry->i);
                }
            }

            if (cm.mesh.empty()) return false;
            size_t mesh_index = meshes_.size();
            index_.emplace(cm.name, mesh_index);
            if (!cm.uid.empty()) uid_index_.emplace(cm.uid, mesh_index);
            meshes_.push_back(std::move(cm));
        }
    }

    if (cl_json_node *scene = cl_json_get_cstr(&root, "scene")) {
        if (scene->kind != CLAY_J_ARR) return false;
        bool saw_directional = false;
        bool saw_camera = false;

        for (size_t i = 0; i < scene->arr.n; i++) {
            cl_json_node *e = scene->arr.items[i];
            if (!e || e->kind != CLAY_J_OBJ) return false;
            cl_json_node *comp = cl_json_get_cstr(e, "component");
            if (!comp || comp->kind != CLAY_J_STR) return false;

            if (string_equals(comp, "directional_light")) {
                if (saw_directional) return false;
                saw_directional = true;
                if (cl_json_node *dir = cl_json_get_cstr(e, "dir")) {
                    if (!read_vec3(dir, light_.dir) ||
                        cl_v3_length(light_.dir) <= 0.0f)
                        return false;
                    light_.dir = cl_v3_normalize(light_.dir);
                }
                if (cl_json_node *intensity = cl_json_get_cstr(e, "intensity")) {
                    if (!read_number(intensity, light_.intensity) ||
                        light_.intensity < 0.0f)
                        return false;
                }
                continue;
            }

            if (string_equals(comp, "camera")) {
                if (saw_camera) return false;
                saw_camera = true;
                if (cl_json_node *eye = cl_json_get_cstr(e, "eye")) {
                    if (!read_vec3(eye, camera_.eye)) return false;
                }
                if (cl_json_node *target = cl_json_get_cstr(e, "target")) {
                    if (!read_vec3(target, camera_.target)) return false;
                }
                if (cl_json_node *up = cl_json_get_cstr(e, "up")) {
                    if (!read_vec3(up, camera_.up) || cl_v3_length(camera_.up) <= 0.0f)
                        return false;
                }
                if (cl_json_node *fov = cl_json_get_cstr(e, "fov")) {
                    if (!read_number(fov, camera_.fov_y_rad) ||
                        camera_.fov_y_rad <= 0.0f ||
                        camera_.fov_y_rad >= 3.14159265f)
                        return false;
                }
                if (cl_json_node *znear = cl_json_get_cstr(e, "znear")) {
                    if (!read_number(znear, camera_.znear) || camera_.znear <= 0.0f)
                        return false;
                }
                if (cl_json_node *zfar = cl_json_get_cstr(e, "zfar")) {
                    if (!read_number(zfar, camera_.zfar)) return false;
                }
                if (camera_.zfar <= camera_.znear) return false;
                continue;
            }

            if (string_equals(comp, "point_light")) {
                if (!point_lights_.empty()) return false;
                ClayPointLight pl;
                if (cl_json_node *pos = cl_json_get_cstr(e, "pos")) {
                    if (!read_vec3(pos, pl.pos)) return false;
                }
                if (cl_json_node *intensity = cl_json_get_cstr(e, "intensity")) {
                    if (!read_number(intensity, pl.intensity) || pl.intensity < 0.0f)
                        return false;
                } else {
                    pl.intensity = 1.0f;
                }
                if (cl_json_node *attenuation =
                        cl_json_get_cstr(e, "attenuation")) {
                    if (!read_number(attenuation, pl.attenuation) ||
                        pl.attenuation < 0.0f)
                        return false;
                }
                point_lights_.push_back(pl);
                continue;
            }

            if (string_equals(comp, "mesh_instance")) {
                ClayInstance inst;
                if (cl_json_node *mesh = cl_json_get_cstr(e, "mesh")) {
                    if (!read_string(mesh, inst.mesh_name)) return false;
                }
                if (cl_json_node *uid = cl_json_get_cstr(e, "mesh_uid")) {
                    if (!read_string(uid, inst.mesh_uid)) return false;
                }
                if (inst.mesh_name.empty() && inst.mesh_uid.empty()) return false;
                if (!transform_from(cl_json_get_cstr(e, "transform"), inst.model))
                    return false;
                if (cl_json_node *color = cl_json_get_cstr(e, "color")) {
                    if (!read_color(color, inst.color)) return false;
                    inst.has_color = true;
                }
                instances_.push_back(std::move(inst));
                continue;
            }

            /* Unknown component names are forwards-compatible and ignored. */
        }
    }

    for (const ClayInstance &inst : instances_)
        if (resolve_mesh(inst) >= meshes_.size()) return false;

    return true;
}

size_t ClayScene::resolve_mesh(const ClayInstance &inst) const {
    if (!inst.mesh_uid.empty()) {
        auto uid = uid_index_.find(inst.mesh_uid);
        if (uid != uid_index_.end()) return uid->second;
    }
    if (!inst.mesh_name.empty()) {
        auto name = index_.find(inst.mesh_name);
        if (name != index_.end()) return name->second;
    }
    return meshes_.size();
}

cl_m4 ClayScene::view_matrix() const {
    return cl_m4_look_at(camera_.eye, camera_.target, camera_.up);
}

cl_m4 ClayScene::proj_matrix(float aspect) const {
    return cl_m4_perspective(camera_.fov_y_rad, aspect, camera_.znear,
                             camera_.zfar);
}

void ClayScene::render(IRenderer &r, cl_m4 view, cl_m4 proj) {
    for (const ClayInstance &inst : instances_) {
        size_t idx = resolve_mesh(inst);
        if (idx >= meshes_.size()) continue; /* load() already validates this */
        const ClayMesh &cm = meshes_[idx];

        cl_v3 pl_pos = {0.0f, 0.0f, 0.0f};
        float pl_int = 0.0f;
        float pl_att = 0.0f;
        if (!point_lights_.empty()) {
            const ClayPointLight &pl = point_lights_.front();
            pl_pos = pl.pos;
            pl_int = pl.intensity;
            pl_att = pl.attenuation;
        }

        Rgba color = inst.has_color ? inst.color : cm.color;
        r.draw_mesh(cm.mesh, inst.model, view, proj, color, light_.dir,
                    light_.intensity, pl_pos, pl_int, pl_att, 0.35f);
    }
}

} // namespace clay
