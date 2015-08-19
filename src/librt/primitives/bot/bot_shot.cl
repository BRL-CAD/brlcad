#include "common.cl"


struct bot_specific {
    ulong offsets[3];    // To: BVH, Triangles, Normals.
    uint ntri;
};

struct tri_specific {
    double v0[3];
    double v1[3];
    double v2[3];
    int surfno;
};

int bot_shot(global struct hit *res, const double3 r_pt, const double3 r_dir, const uint idx, global const uchar *args)
{
    global const struct bot_specific *bot = args;
    const uint ntri = bot->ntri;

    args += sizeof(struct bot_specific);
    global const struct tri_specific *tri = args;

    struct hit hits[256];
    uint hit_count;

    hit_count = 0;
    for (uint i=0; i<ntri; i++) {
        const double3 V0 = vload3(0, tri[i].v0);
        const double3 V1 = vload3(0, tri[i].v1);
        const double3 V2 = vload3(0, tri[i].v2);

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
            hits[hit_count].hit_surfno = i;     // HACK
            hits[hit_count].hit_vpriv = mix;
            hit_count++;
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

    global const struct bot_specific *bot = args;
    const uint ntri = bot->ntri;

    args += sizeof(struct bot_specific);
    global const struct tri_specific *tri = args+sizeof(struct tri_specific)*h;
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
