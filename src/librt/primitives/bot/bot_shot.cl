#include "common.cl"


/* orientations for BOT */
#define RT_BOT_UNORIENTED 1	/**< @brief unoriented triangles */
#define RT_BOT_CCW 2 		/**< @brief oriented counter-clockwise */
#define RT_BOT_CW 3		/**< @brief oriented clockwise */

/* flags for bot_flags */
#define RT_BOT_HAS_SURFACE_NORMALS 0x1 /**< @brief This primitive may have surface normals at each face vertex */
#define RT_BOT_USE_NORMALS 0x2         /**< @brief Use the surface normals if they exist */

struct bot_specific {
    ulong offsets[5];    // To: BVH, Triangles, Normals.
    uint ntri;
    uchar orientation;
    uchar flags;
    uchar pad[2];
};

struct tri_specific {
    double v0[3];
    double v1[3];
    double v2[3];
    int surfno;
    uchar pad[4];
};

int bot_shot(RESULT_TYPE *res, const double3 r_pt, double3 r_dir, const uint idx, global const uchar *args)
{
    global const struct bot_specific *bot =
        (global const struct bot_specific *)(args);
    global struct linear_bvh_node *nodes =
        (global struct linear_bvh_node *)(args+bot->offsets[1]);
    global const struct tri_specific *tri =
        (global const struct tri_specific *)(args+bot->offsets[2]);

    const uint ntri = bot->ntri;

    struct hit hits[256];
    uint hit_count;
    hit_count = 0;

    const double3 r_idir = select(INFINITY, DOUBLE_C(1.0)/r_dir, isgreaterequal(fabs(r_dir), SQRT_SMALL_FASTF));
    r_dir = select(0, r_dir, isgreaterequal(fabs(r_dir), SQRT_SMALL_FASTF));

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
                    const uint id = node->u.primitives_offset + i;
                    const double3 V0 = vload3(0, tri[id].v0);
                    const double3 V1 = vload3(0, tri[id].v1);
                    const double3 V2 = vload3(0, tri[id].v2);

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
                        hits[hit_count].hit_surfno = id;     // HACK
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
        do_segp(res, idx, &hits[i], &hits[i]);
    }
    return hit_count;
}


void bot_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const uchar *args)
{
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;

    const int h = hitp->hit_surfno;

    global const struct bot_specific *bot =
        (global const struct bot_specific *)(args);

    double3 normal;
    if (bot->offsets[3] == bot->offsets[4]) {
	global const struct tri_specific *tri =
		(global const struct tri_specific*)(args+bot->offsets[2]);
	const double3 v0 = vload3(0, tri[h].v0);
	const double3 v1 = vload3(0, tri[h].v1);
	const double3 v2 = vload3(0, tri[h].v2);
	normal = normalize(cross(v1-v0, v2-v0));
    } else {
	global const double *normals = (global const double*)(args+bot->offsets[3]);
	const size_t base = h*9;
	double3 n0 = vload3(0, normals+base);
	double3 n1 = vload3(1, normals+base);
	double3 n2 = vload3(2, normals+base);
	const double3 mix = clamp(hitp->hit_vpriv, DOUBLE_C(0.0), DOUBLE_C(1.0));
	normal = normalize(n0*mix.x + n1*mix.y + n2*mix.z);
    }

    if (bot->orientation == RT_BOT_UNORIENTED) {
	normal = normal*sign(-dot(normal, r_dir));
    }

    hitp->hit_normal = normal;
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

