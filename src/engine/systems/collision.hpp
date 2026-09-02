#ifndef CLAY_ENGINE_SYSTEMS_COLLISION_HPP
#define CLAY_ENGINE_SYSTEMS_COLLISION_HPP

#include "ecs/components.hpp"

#include <clay/clay.h>

namespace clay {

/* Result of a 2D intersection test: overlap flag plus the contact point
 * (midpoint of the intersection region) and the separating normal for
 * resolution. All fields are zeroed when there is no contact. */
struct Contact2D {
    bool overlap = false;
    cl_v2 point{0.0f, 0.0f}; /* contact point in world space */
    cl_v2 normal{0.0f, 0.0f}; /* from A to B, normalized */
    float depth = 0.0f;       /* penetration depth */
};

/* Circle vs circle. */
Contact2D circle_circle(cl_v2 a_pos, float a_r,
                        cl_v2 b_pos, float b_r);

/* AABB (centered on pos, half-extents hw/he) vs AABB. */
Contact2D aabb_aabb(cl_v2 a_pos, float a_hw, float a_he,
                    cl_v2 b_pos, float b_hw, float b_he);

/* AABB vs circle. */
Contact2D aabb_circle(cl_v2 a_pos, float a_hw, float a_he,
                      cl_v2 b_pos, float b_r);

} // namespace clay

#endif /* CLAY_ENGINE_SYSTEMS_COLLISION_HPP */
