#include "common.cl"


struct ell_shot_specific {
    double ell_SoR[16];
    double ell_V[3];
};

int ell_shot(global struct hit *res, const double3 r_pt, const double3 r_dir, global const struct ell_shot_specific *ell)
{
    const double16 SoR = vload16(0, &ell->ell_SoR[0]);
    const double3 V = vload3(0, &ell->ell_V[0]);

    double3 dprime;	// D'
    double3 pprime;	// P'
    double3 xlated;	// translated vector
    double dp, dd;	// D' dot P', D' dot D'
    double k1, k2;	// distance constants of solution
    double root;	// root of radical

    /* out, Mat, vect */
    const double f = 1.0/SoR.sf;
    dprime.x = dot(SoR.s012, r_dir) * f;
    dprime.y = dot(SoR.s456, r_dir) * f;
    dprime.z = dot(SoR.s89a, r_dir) * f;
    xlated = r_pt - V;
    pprime.x = dot(SoR.s012, xlated) * f;
    pprime.y = dot(SoR.s456, xlated) * f;
    pprime.z = dot(SoR.s89a, xlated) * f;

    dp = dot(dprime, pprime);
    dd = dot(dprime, dprime);

    if ((root = dp*dp - dd * (dot(pprime, pprime)-1.0)) < 0) {
	return 0;	// No hit
    } else {
	root = sqrt(root);
	if ((k1=(-dp+root)/dd) <= (k2=(-dp-root)/dd)) {
	    /* k1 is entry, k2 is exit */
            res[0].hit_dist = k1;
            res[1].hit_dist = k2;
	} else {
	    /* k2 is entry, k1 is exit */
            res[0].hit_dist = k2;
            res[1].hit_dist = k1;
	}
        res[0].hit_surfno = 0;
        res[1].hit_surfno = 0;
	return 2;	// HIT
    }
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

