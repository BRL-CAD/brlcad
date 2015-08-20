#include "common.cl"


struct bot_specific {
    ulong offsets[3];    // To: BVH, Triangles, Normals.
    uint ntri;
    uchar pad[4];
};

struct tri_specific {
    double v0[3];
    double v1[3];
    double v2[3];
    int surfno;
    uchar pad[4];
};

int bot_shot(global struct hit *res, const double3 r_pt, double3 r_dir, const uint idx, global const uchar *args)
{
    global const struct bot_specific *bot =
        (global const struct bot_specific *)(args);
    global struct linear_bvh_node *nodes =
        (global struct linear_bvh_node *)(args+bot->offsets[0]);
    global const struct tri_specific *tri =
        (global const struct tri_specific *)(args+bot->offsets[1]);

    const uint ntri = bot->ntri;

    if (ntri <= 0) {
        return 0;   // No hit
    }

    struct hit hits[256];
    uint hit_count;
    hit_count = 0;

    const long3 oblique = isgreaterequal(fabs(r_dir), SQRT_SMALL_FASTF);
    const double3 r_idir = select(INFINITY, 1.0/r_dir, oblique);
    r_dir = select(0.0, r_dir, oblique);

    uchar dir_is_neg[3];
    int to_visit_offset = 0, current_node_index = 0;
    int nodes_to_visit[64];

    vstore3(convert_uchar3(r_idir < 0), 0, dir_is_neg);

    /* Follow ray through BVH nodes to find primitive intersections */
    for (;;) {
        const global struct linear_bvh_node *node = &nodes[current_node_index];
        const global struct bvh_bounds *bounds = &node->bounds;

        /* Check ray against BVH node */
        if (rt_in_rpp(r_pt, r_idir, bounds->p_min, bounds->p_max)) {
            if (node->n_primitives > 0) {
                /* Intersect ray with primitives in leaf BVH node */
                for (uint i=0; i<node->n_primitives; i++) {
                    const uint idx = node->u.primitives_offset + i;
                    const double3 V0 = vload3(0, tri[idx].v0);
                    const double3 V1 = vload3(0, tri[idx].v1);
                    const double3 V2 = vload3(0, tri[idx].v2);

                    // Find vectors for two edges sharing V0.
                    const double3 e1 = V1-V0;
                    const double3 e2 = V2-V0;

                    double3 T, P, Q;
                    double3 mix;
                    double t;

                    // Begin calculating determinant - also used to calculate U parameter.
                    P = cross(r_dir, e2);

                    // If determinant is near zero, ray lies in plane of triangle.
                    const double det = dot(e1, P);

                    // Backface culling.

                    if (ZERO(det)) {
                        continue;   // No hit
                    }

                    const double idet = 1.0/det;

                    // Calculate distance from vert0 to ray origin.
                    T = r_pt - V0;

                    // Calculate u parameter (beta) and test bounds.
                    mix.y = dot(T, P) * idet;

                    // The intersection lies outside of the triangle.
                    if (mix.y < 0.0 || mix.y > 1.0) {
                        continue;   // No hit
                    }

                    // Prepare to test v (gamma) parameter.
                    Q = cross(T, e1);

                    // Calculate v parameter and test bounds.
                    mix.z = dot(r_dir, Q) * idet;

                    mix.x = 1.0 - mix.y - mix.z;

                    // The intersection lies outside of the triangle.
                    if (mix.z < 0.0 || mix.x < 0.0) {
                        continue;   // No hit
                    }

                    // Calculate t, ray intersects triangle.
                    t = dot(e2, Q) * idet;

                    if (isnan(t)) {
                        continue;
                    }

                    // Triangle Intersected, append it in the list.
                    if (hit_count < 0xFF) {
                        hits[hit_count].hit_dist = t;
                        hits[hit_count].hit_surfno = idx;     // HACK
                        hits[hit_count].hit_vpriv = mix;
                        hit_count++;
                    }
                }
                if (to_visit_offset == 0) break;
                current_node_index = nodes_to_visit[--to_visit_offset];
            } else {
                /* Put far BVH node on nodes_to_visit stack, advance to near
                 * node
                 */
                if (dir_is_neg[node->axis]) {
                    nodes_to_visit[to_visit_offset++] = current_node_index + 1;
                    current_node_index = node->u.second_child_offset;
                } else {
                    nodes_to_visit[to_visit_offset++] = node->u.second_child_offset;
                    current_node_index = current_node_index + 1;
                }
            }
        } else {
            if (to_visit_offset == 0) break;
            current_node_index = nodes_to_visit[--to_visit_offset];
        }
    }

    // If we hit something, then sort the hit triangles on demand.
    for (uint i=0; i < hit_count; i++) {
        // Sort the list so that HitList is in order wrt [i].
        for (uint n = i; n < hit_count; n++) {
            if (hits[n].hit_dist < hits[i].hit_dist) {
                struct hit tmp;
                // Swap.
                tmp = hits[i];
                hits[i] = hits[n];
                hits[n] = tmp;
            }
        }
    }

    for (uint i=0; i < hit_count; i++) {
        do_hitp(res, i, idx, &hits[i]);
    }
    return hit_count;
}


void bot_norm(global struct hit *hitp, const double3 r_pt, const double3 r_dir, global const uchar *args)
{
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;

    const int h = hitp->hit_surfno;

    global const struct bot_specific *bot =
        (global const struct bot_specific *)(args);
    const uint ntri = bot->ntri;

    global const struct tri_specific *tri =
        (global const struct tri_specific *)(args+bot->offsets[1]+sizeof(struct tri_specific)*h);
    const double3 V0 = vload3(0, tri->v0);
    const double3 V1 = vload3(0, tri->v1);
    const double3 V2 = vload3(0, tri->v2);
    hitp->hit_normal = normalize(cross(V1-V0, V2-V0));
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
