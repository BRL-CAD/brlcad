#include "common.cl"


struct superell_specific {
    double superell_V[3];		/* Vector to center of superellipsoid */
    double superell_invmsAu;		/* 1.0 / |Au|^2 */
    double superell_invmsBu;		/* 1.0 / |Bu|^2 */
    double superell_invmsCu;		/* 1.0 / |Cu|^2 */
    double superell_SoR[16];		/* matrix for local coordinate system, Scale(Rotate(V))*/
    double superell_invRSSR[16];	/* invR(Scale(Scale(Rot(V)))) */
    double superell_e;			/* east-west curvature power */
};

int superell_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct superell_specific *superell)
{
    double3 translated;			// translated shot vector
    double3 newShotPoint;		// P'
    double3 newShotDir;			// D'
    double3 normalizedShotPoint;	// P' with normalized dist from superell
    double equation[3];			// equation of superell to be solved
    bn_complex_t complexRoot[4];	// roots returned from poly solver
    double realRoot[4];			// real ray distance values
    short i, j;

    /* translate ray point */
    translated = r_pt - vload3(0, superell->superell_V);

    /* scale and rotate point to get P' */
    newShotPoint = MAT4X3VEC(superell->superell_SoR, translated);

    /* translate ray direction vector */
    newShotDir = MAT4X3VEC(superell->superell_SoR, r_dir);
    newShotDir = normalize(newShotDir);

    /* normalize distance from the superell.  substitutes a corrected ray
     * point, which contains a translation along the ray direction to the
     * closest approach to vertex of the superell.  Translating the ray
     * along the direction of the ray to the closest point near the
     * primitive's center vertex.  New ray origin is hence, normalized.
     */
    normalizedShotPoint = newShotDir * dot(newShotPoint, newShotDir);
    normalizedShotPoint = newShotPoint - normalizedShotPoint;

    /* Now generate the polynomial equation for passing to the root finder */
    /* (x^2 / A) + (y^2 / B) + (z^2 / C) - 1 */
    equation[0] = newShotPoint.x * newShotPoint.x * superell->superell_invmsAu + newShotPoint.y * newShotPoint.y * superell->superell_invmsBu + newShotPoint.z * newShotPoint.z * superell->superell_invmsCu - 1;
    /* (2xX / A) + (2yY / B) + (2zZ / C) */
    equation[1] = 2.0 * newShotDir.x * newShotPoint.x * superell->superell_invmsAu + 2.0 * newShotDir.y * newShotPoint.y * superell->superell_invmsBu + 2.0 * newShotDir.z * newShotPoint.z * superell->superell_invmsCu;
    /* (X^2 / A) + (Y^2 / B) + (Z^2 / C) */
    equation[2] = newShotDir.x * newShotDir.x * superell->superell_invmsAu + newShotDir.y * newShotDir.y * superell->superell_invmsBu + newShotDir.z * newShotDir.z * superell->superell_invmsCu;

    if ((i = rt_poly_roots(equation, 2, complexRoot)) != 2) {
	return 0; /* MISS */
    }

    /* XXX BEGIN CUT */
    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero
     * or sufficiently close, then use the real part as one value
     * of 't' for the intersections
     */
    for (j=0, i=0; j < 2; j++) {
	 if (NEAR_ZERO(complexRoot[j].im, 0.001))
	     realRoot[i++] = complexRoot[j].re;
    }

    /* Here, 'i' is number of points found */
    switch (i) {
	default:
	    return 0;	/* No hit */

	case 2:
	    {
		/* Sort most distant to least distant. */
		double u;
		if ((u=realRoot[0]) < realRoot[1]) {
			/* bubble larger towards [0] */
			realRoot[0] = realRoot[1];
			realRoot[1] = u;
		}
	    }
	    break;
	case 4:
	    {
	        short n;
		short lim;

		/* Inline rt_pt_sort().  Sorts realRoot[] into descending order. */
		for (lim = i-1; lim > 0; lim--) {
		    for (n = 0; n < lim; n++) {
			double u;
			if ((u=realRoot[n]) < realRoot[n+1]) {
			     /* bubble larger towards [0] */
			     realRoot[n] = realRoot[n+1];
			     realRoot[n+1] = u;
			}
		    }
		}
	    }
	    break;
    }

    /* Now, t[0] > t[npts-1] */
    /* realRoot[1] is entry point, and realRoot[0] is farthest exit point */
    struct hit hits[2];
    hits[0].hit_dist = realRoot[1];
    hits[1].hit_dist = realRoot[0];
    /* hits[0].hit_surfno = 0; */
    /* hits[1].hit_surfno = 0; */
    /* Set aside vector for rt_superell_norm() later */
    /* hits[0].hit_vpriv = newShotPoint + realRoot[1] * newShotDir; */
    /* hits[1].hit_vpriv = newShotPoint + realRoot[0] * newShotDir; */
    do_segp(res, idx, &hits[0], &hits[1]);

    if (i == 2)
	return 2;	/* HIT */
    
    /* 4 points */
    /* realRoot[3] is entry point, and realRoot[2] is exit point */
    hits[0].hit_dist = realRoot[3] * superell->superell_e;
    hits[1].hit_dist = realRoot[2] * superell->superell_e;
    hits[0].hit_surfno = 1;
    hits[1].hit_surfno = 1;
    hits[0].hit_vpriv = newShotPoint + realRoot[3] * newShotDir;
    hits[1].hit_vpriv = newShotPoint + realRoot[2] * newShotDir;
    do_segp(res, idx, &hits[0], &hits[1]);
    return 4;		/* HIT */
    /* XXX END CUT */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void superell_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct superell_specific *superell)
{
    double3 xlated;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    xlated = hitp->hit_point - vload3(0, superell->superell_V);
    hitp->hit_normal = MAT4X3VEC(superell->superell_invRSSR, xlated);
    hitp->hit_normal = normalize(hitp->hit_normal);
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
