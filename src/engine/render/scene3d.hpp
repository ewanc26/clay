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
 * builtin `primitive`. The generator substitutes memory for tedium: a scene
 * never spells out a tessellation by hand. */
struct ClayMesh {
    std::string name;
    std::string uid; /* stable cross-file id; empty when single-file */
    Mesh3D mesh;
    Rgba color; /* flat shading base */
};

/* A camera-facing placement of a mesh, from a .clay `scene` entry. */
struct ClayInstance {
    std::string mesh_name; /* resolves against MeshLibrary by name then uid */
    std::string mesh_uid;
    cl_m4 model{};
    Rgba color;
};

/* One directional light, from the scene. */
struct ClayLight {
    cl_v3 dir{0.3f, 0.5f, 0.8f};
    float intensity = 1.0f;
};

/* One point light, from the scene. */
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
    int fps = 60;
    int resolution[2] = {640, 480};
};

/* A 3D scene loaded from one .clay document: a named mesh library, a list of
 * instanced meshes, and a light. Loading goes through the single C ABI
 * cl_json_parse; nothing here peeks at the 2D garden ECS. */
class ClayScene {
  public:
    /* Parse `text` (a complete .clay document). Returns false on unsupported
     * version, malformed JSON, or an unknown but *referenced* mesh name. */
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

    /* Build the view matrix from the scene camera (or default). */
    cl_m4 view_matrix() const;

    /* Build the projection matrix from the scene camera + aspect. */
    cl_m4 proj_matrix(float aspect) const;

    /* Draw every instance into `r` using camera `view`/`proj`. */
    void render(IRenderer &r, cl_m4 view, cl_m4 proj);

  private:
    std::unordered_map<std::string, size_t> index_; /* name -> meshes_ */
    std::vector<ClayMesh> meshes_;
    std::vector<ClayInstance> instances_;
    ClayLight light_;
    std::vector<ClayPointLight> point_lights_;
    ClayCamera camera_;
    ClaySettings settings_;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_SCENE3D_HPP */