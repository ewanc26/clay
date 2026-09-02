#include "systems/collision.hpp"

#include <cmath>

namespace clay {

Contact2D circle_circle(cl_v2 a_pos, float a_r, cl_v2 b_pos, float b_r) {
    cl_v2 delta = cl_v2_sub(b_pos, a_pos);
    float dist_sq = cl_v2_dot(delta, delta);
    float radii = a_r + b_r;
    if (dist_sq >= radii * radii) return {};

    Contact2D c;
    float dist = std::sqrt(dist_sq);
    c.overlap = true;
    if (dist > 0.0f) {
        c.normal = cl_v2_normalize(delta);
    } else {
        c.normal = cl_v2_make(1.0f, 0.0f);
    }
    c.depth = radii - dist;
    c.point = cl_v2_make(a_pos.x + c.normal.x * (a_r - c.depth / 2.0f),
                         a_pos.y + c.normal.y * (a_r - c.depth / 2.0f));
    return c;
}

Contact2D aabb_aabb(cl_v2 a_pos, float a_hw, float a_he,
                    cl_v2 b_pos, float b_hw, float b_he) {
    float dx = b_pos.x - a_pos.x;
    float dy = b_pos.y - a_pos.y;
    float px = (a_hw + b_hw) - std::fabs(dx);
    float py = (a_he + b_he) - std::fabs(dy);
    if (px <= 0.0f || py <= 0.0f) return {};

    Contact2D c;
    c.overlap = true;
    /* Separating axis: the direction of least penetration. */
    if (px < py) {
        c.normal = cl_v2_make(dx < 0.0f ? -1.0f : 1.0f, 0.0f);
        c.depth = px;
    } else {
        c.normal = cl_v2_make(0.0f, dy < 0.0f ? -1.0f : 1.0f);
        c.depth = py;
    }
    /* Contact point at the midpoint, offset along the normal by the
     * smaller half-extent to land inside the overlap region. */
    c.point = cl_v2_make((a_pos.x + b_pos.x) / 2.0f,
                         (a_pos.y + b_pos.y) / 2.0f);
    return c;
}

Contact2D aabb_circle(cl_v2 box_pos, float box_hw, float box_he,
                      cl_v2 circle_pos, float circle_r) {
    float dx = circle_pos.x - box_pos.x;
    float dy = circle_pos.y - box_pos.y;
    /* Clamp circle center to the box's nearest edge. */
    float closest_x = std::fmaxf(box_pos.x - box_hw,
                          std::fminf(circle_pos.x, box_pos.x + box_hw));
    float closest_y = std::fmaxf(box_pos.y - box_he,
                          std::fminf(circle_pos.y, box_pos.y + box_he));
    float diff_x = circle_pos.x - closest_x;
    float diff_y = circle_pos.y - closest_y;
    float dist_sq = diff_x * diff_x + diff_y * diff_y;
    if (dist_sq >= circle_r * circle_r) return {};

    Contact2D c;
    c.overlap = true;
    float dist = std::sqrt(dist_sq);
    if (dist > 0.0f) {
        c.normal = cl_v2_normalize(cl_v2_make(diff_x, diff_y));
    } else {
        /* Circle center is inside the box; push out along the shortest
         * axis. */
        float pen_x = box_hw - std::fabs(dx);
        float pen_y = box_he - std::fabs(dy);
        if (pen_x < pen_y) {
            c.normal = cl_v2_make(dx < 0.0f ? -1.0f : 1.0f, 0.0f);
        } else {
            c.normal = cl_v2_make(0.0f, dy < 0.0f ? -1.0f : 1.0f);
        }
        dist = std::fminf(pen_x, pen_y);
    }
    c.depth = circle_r - dist;
    c.point = cl_v2_make(closest_x, closest_y);
    return c;
}

} // namespace clay
