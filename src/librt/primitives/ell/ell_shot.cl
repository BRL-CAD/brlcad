#include "common.cl"


struct ell_specific {
    double ell_V[3];         /* Vector to center of ellipsoid */
    double ell_SoR[16];      /* Scale(Rot(vect)) */
    double ell_invRSSR[16];  /* invRot(Scale(Scale(Rot(vect)))) */
};

int ell_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct ell_specific *ell)
{
    double3 dprime;	// D'
    double3 pprime;	// P'
    double3 xlated;	// translated vector
    double dp, dd;	// D' dot P', D' dot D'
    double k1, k2;	// distance constants of solution
    double root;	// root of radical

    dprime = MAT4X3VEC(ell->ell_SoR, r_dir);
    xlated = r_pt - vload3(0, ell->ell_V);
    pprime = MAT4X3VEC(ell->ell_SoR, xlated);

    dp = dot(dprime, pprime);
    dd = dot(dprime, dprime);

    if ((root = dp*dp - dd * (dot(pprime, pprime)-1.0)) < 0) {
	return 0;	// No hit
    } else {
	struct hit hits[2];

	root = sqrt(root);
	if ((k1=(-dp+root)/dd) <= (k2=(-dp-root)/dd)) {
	    /* k1 is entry, k2 is exit */
            hits[0].hit_dist = k1;
            hits[0].hit_surfno = 0;
            hits[1].hit_dist = k2;
            hits[1].hit_surfno = 0;
	} else {
	    /* k2 is entry, k1 is exit */
            hits[0].hit_dist = k2;
            hits[0].hit_surfno = 0;
            hits[1].hit_dist = k1;
            hits[1].hit_surfno = 0;
	}

	do_segp(res, idx, &hits[0], &hits[1]);
	return 2;	// HIT
    }
}


void ell_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct ell_specific *ell)
{
    double3 xlated;
    double scale;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    xlated = hitp->hit_point - vload3(0, ell->ell_V);
    hitp->hit_normal = MAT4X3VEC(ell->ell_invRSSR, xlated);
    scale = 1.0 / length(hitp->hit_normal);
    hitp->hit_normal = hitp->hit_normal * scale;

    /* tuck away this scale for the curvature routine */
    //hitp->hit_vpriv.x = scale;
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

