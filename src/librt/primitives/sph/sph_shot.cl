#include "common.cl"


struct sph_shot_specific {
    double sph_V[3];
    double sph_radsq;
};

int sph_shot(global struct hit *res, const double3 r_pt, const double3 r_dir, global const struct sph_shot_specific *sph)
{
    const double3 V = vload3(0, &sph->sph_V[0]);
    const double radsq = sph->sph_radsq;

    double3 ov;        // ray origin to center (V - P)
    double magsq_ov;   // length squared of ov
    double b;          // second term of quadratic eqn
    double root;       // root of radical

    ov = V - r_pt;
    b = dot(r_dir, ov);
    magsq_ov = dot(ov, ov);

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

    // we know root is positive, so we know the smaller t
    res[0].hit_dist = b - root;
    res[1].hit_dist = b + root;
    res[0].hit_surfno = 0;
    res[1].hit_surfno = 0;
    return 2;       // HIT
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
