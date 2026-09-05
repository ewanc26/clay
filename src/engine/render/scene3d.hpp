#ifndef CLAY_ENGINE_RENDER_SCENE3D_HPP
#define CLAY_ENGINE_RENDER_SCENE3D_HPP

#include "clay/clay.h"
#include "render/renderer.hpp"
#include "render/raster3d.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace clay {

/* A named triangulated mesh, generated from a .clay `meshes` entry or a
 * builtin `primitive`. */
struct ClayMesh {
    std::string name;
    std::string uid; /* stable cross-file id; empty when single-file */
    Mesh3D mesh;
    Rgba color{200, 200, 200, 255}; /* flat shading base */
};

/* A camera-facing placement of a mesh, from a .clay `scene` entry. */
struct ClayInstance {
    std::string mesh_name;
    std::string mesh_uid; /* uid is authoritative when both are present */
    cl_m4 model{};
    Rgba color{200, 200, 200, 255};
    bool has_color = false; /* otherwise inherit the mesh's base color */
};

/* One directional light, from the scene. */
struct ClayLight {
    cl_v3 dir{0.3f, 0.5f, 0.8f};
    float intensity = 1.0f;
};

/* One point light, from the scene. Version 1 allows at most one. */
struct ClayPointLight {
    cl_v3 pos{0.0f, 0.0f, 0.0f};
    float intensity = 0.0f;
    float attenuation = 0.0f;
};

/* Camera from the scene. Defaults look down -Z from z=6. */
struct ClayCamera {
    cl_v3 eye{0.0f, 0.0f, 6.0f};
    cl_v3 target{0.0f, 0.0f, 0.0f};
    cl_v3 up{0.0f, 1.0f, 0.0f};
    float fov_y_rad = 0.9f;
    float znear = 0.1f;
    float zfar = 100.0f;
};

/* Settings from the .clay top-level block. */
struct ClaySettings {
    uint64_t seed = 0;
    bool has_seed = false;
    int fps = 60;
    int resolution[2] = {640, 480};
    bool has_resolution = false;
    Rgba clear{24, 26, 34, 255};
};

/* A 3D scene loaded from one .clay document: a named mesh library, a list of
 * instanced meshes, lights and camera. JSON syntax is parsed exclusively by
 * the public C ABI; schema validation and C++ ownership live here. */
class ClayScene {
  public:
    /* Parse and validate a complete version-1 .clay document. Returns false
     * for malformed JSON, unsupported/missing versions, malformed recognised
     * fields, duplicate stable IDs, or unresolved mesh references. */
    bool load(cl_str text);

    size_t mesh_count() const {
        return meshes_.size();
    }
    size_t instance_count() const {
        return instances_.size();
    }
    const std::vector<ClayMesh> &meshes() const {
        return meshes_;
    }
    const std::vector<ClayInstance> &instances() const {
        return instances_;
    }
    const ClayLight &light() const {
        return light_;
    }
    const std::vector<ClayPointLight> &point_lights() const {
        return point_lights_;
    }
    const ClayCamera &camera() const {
        return camera_;
    }
    const ClaySettings &settings() const {
        return settings_;
    }

    cl_m4 view_matrix() const;
    cl_m4 proj_matrix(float aspect) const;
    void render(IRenderer &r, cl_m4 view, cl_m4 proj);

  private:
    size_t resolve_mesh(const ClayInstance &inst) const;

    std::unordered_map<std::string, size_t> index_;     /* name -> meshes_ */
    std::unordered_map<std::string, size_t> uid_index_; /* uid -> meshes_ */
    std::vector<ClayMesh> meshes_;
    std::vector<ClayInstance> instances_;
    ClayLight light_;
    std::vector<ClayPointLight> point_lights_;
    ClayCamera camera_;
    ClaySettings settings_;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_SCENE3D_HPP */
