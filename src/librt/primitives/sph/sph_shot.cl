#include "common.cl"


struct sph_specific {
    double sph_V[3];     /* Vector to center of sphere */
    double sph_radsq;    /* Radius squared */
    double sph_invrad;   /* Inverse radius (for normal) */
};

int sph_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct sph_specific *sph)
{
    double3 ov;        // ray origin to center (V - P)
    double magsq_ov;   // length squared of ov
    double b;          // second term of quadratic eqn
    double root;       // root of radical

    ov = vload3(0, sph->sph_V) - r_pt;
    b = dot(r_dir, ov);
    magsq_ov = dot(ov, ov);

    const double radsq = sph->sph_radsq;
    if (magsq_ov >= radsq) {
	// ray origin is outside of sphere
	if (b < 0) {
	    // ray direction is away from sphere
	    return 0;       // No hit
	}
	root = b*b - magsq_ov + radsq;
	if (root <= 0) {
	    // no real roots
	    return 0;       // No hit
	}
    } else {
	root = b*b - magsq_ov + radsq;
    }
    root = sqrt(root);

    struct hit hits[2];

    // we know root is positive, so we know the smaller t
    hits[0].hit_dist = b - root;
    hits[0].hit_surfno = 0;
    hits[1].hit_dist = b + root;
    hits[1].hit_surfno = 0;

    do_segp(res, idx, &hits[0], &hits[1]);
    return 2;       // HIT
}


void sph_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct sph_specific *sph)
{
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    hitp->hit_normal = hitp->hit_point - vload3(0, sph->sph_V);
    hitp->hit_normal = hitp->hit_normal * sph->sph_invrad;
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
