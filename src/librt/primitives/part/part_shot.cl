#include "common.cl"


/* hit_surfno flags for which end was hit */
#define RT_PARTICLE_SURF_VSPHERE 1
#define RT_PARTICLE_SURF_BODY 2
#define RT_PARTICLE_SURF_HSPHERE 3

#define RT_PARTICLE_TYPE_SPHERE 1
#define RT_PARTICLE_TYPE_CYLINDER 2
#define RT_PARTICLE_TYPE_CONE 3


struct part_specific {
    double part_V[3];
    double part_H[3];
    double part_vrad;
    double part_hrad;
    int part_type;		/**< @brief sphere, cylinder, cone */
    double part_SoR[16];	/* Scale(Rot(vect)) */
    double part_invRoS[16];	/* invRot(Scale(vect)) */
    double part_vrad_prime;
    double part_hrad_prime;
    /* For the "equivalent cone" */
    double part_v_hdist;	/* dist of base plate on unit cone */
    double part_h_hdist;
};


int part_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct part_specific *part)
{
    double3 dprime;		/* D' */
    double3 pprime;		/* P' */
    double3 xlated;		/* translated ray start point */
    double t1, t2;		/* distance constants of solution */
    double f;
    struct hit hits[4];		/* 4 potential hit points */
    struct hit *hitp;
    int check_v, check_h;

    hitp = &hits[0];

    if (part->part_type == RT_PARTICLE_TYPE_SPHERE) {
	double3 ov;		/* ray origin to center (V - P) */
	double vrad_sq;
	double magsq_ov;	/* length squared of ov */
	double b;		/* second term of quadratic eqn */
	double root;		/* root of radical */

	ov = vload3(0, part->part_V) - r_pt;
	b = dot(r_dir, ov);
	magsq_ov = dot(ov, ov);

	if (magsq_ov >= (vrad_sq = part->part_vrad * part->part_vrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		return 0;		/* No hit */
	    }
	    root = b*b - magsq_ov + vrad_sq;
	    if (root <= 0) {
		/* no real roots */
		return 0;		/* No hit */
	    }
	} else {
	    root = b*b - magsq_ov + vrad_sq;
	}
	root = sqrt(root);

	/* we know root is positive, so we know the smaller t */
	hits[0].hit_dist = b - root;
	hits[0].hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	hits[1].hit_dist = b + root;
	hits[1].hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	do_segp(res, idx, &hits[0], &hits[1]);
	return 2;			/* HIT */
    }

    /* Transform ray to coordinate system of unit cone at origin */
    dprime = MAT4X3VEC(part->part_SoR, r_dir);
    xlated = r_pt - vload3(0, part->part_V);
    pprime = MAT4X3VEC(part->part_SoR, xlated);

    if (ZERO(dprime.x) && ZERO(dprime.y)) {
	check_v = check_h = 1;
    } else {
	double a, b, c;
	double root;		/* root of radical */

	check_v = check_h = 0;
	/* Find roots of the equation, using formula for quadratic */
	/* Note that vrad' = 1 and hrad' = hrad/vrad */
	if (part->part_type == RT_PARTICLE_TYPE_CYLINDER) {
	    /* Cylinder case, hrad == vrad, m = 0 */

	    a = dprime.x*dprime.x + dprime.y*dprime.y;
	    b = dprime.x*pprime.x + dprime.y*pprime.y;
	    c = pprime.x*pprime.x + pprime.y*pprime.y - 1;
	} else {
	    /* Cone case */
	    double m, msq;

	    m = part->part_hrad_prime - part->part_vrad_prime;

	    /* This quadratic has had a factor of 2 divided out of "b"
	     * throughout.  More efficient, but the same answers.
	     */
	    a = dprime.x*dprime.x + dprime.y*dprime.y -
		(msq = m*m) * dprime.z*dprime.z;
	    b = dprime.x*pprime.x + dprime.y*pprime.y -
		msq * dprime.z*pprime.z -
		m * dprime.z;		/* * part->part_vrad_prime */
	    c = pprime.x*pprime.x + pprime.y*pprime.y -
		msq * pprime.z*pprime.z -
		2 * m * pprime.z - 1;
	    /* was: ... -2m * vrad' * Pz' - vrad'**2 */
	}

	if ((root = b*b - a * c) > 0) {
	    root = sqrt(root);
	    t1 = (root-b) / a;
	    t2 = -(root+b) / a;

	    /*
	     * t1 and t2 are potential solutions to intersection with side.
	     * Find hit' point, see if Z values fall in range.
	     */
	    if ((f = pprime.z + t1 * dprime.z) >= part->part_v_hdist) {
		check_h = 1;		/* may also hit off end */
		if (f <= part->part_h_hdist) {
		    /** VJOIN1(hitp->hit_vpriv, pprime, t1, dprime); **/
		    hitp->hit_vpriv.x = pprime.x + t1 * dprime.x;
		    hitp->hit_vpriv.y = pprime.y + t1 * dprime.y;
		    hitp->hit_vpriv.z = f;
		    hitp->hit_dist = t1;
		    hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
		    hitp++;
		}
	    } else {
		check_v = 1;
	    }

	    if ((f = pprime.z + t2 * dprime.z) >= part->part_v_hdist) {
		check_h = 1;		/* may also hit off end */
		if (f <= part->part_h_hdist) {
		    /** VJOIN1(hitp->hit_vpriv, pprime, t1, dprime); **/
		    hitp->hit_vpriv.x = pprime.x + t2 * dprime.x;
		    hitp->hit_vpriv.y = pprime.y + t2 * dprime.y;
		    hitp->hit_vpriv.z = f;
		    hitp->hit_dist = t2;
		    hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
		    hitp++;
		}
	    } else {
		check_v = 1;
	    }
	}
    }

    /*
     * Check for hitting the end hemispheres.
     */
    if (check_v) {
	double3 ov;		/* ray origin to center (V - P) */
	double rad_sq;
	double magsq_ov;	/* length squared of ov */
	double b;
	double root;		/* root of radical */
	bool no_roots = false;

	/*
	 * First, consider a hit on V hemisphere.
	 */
	ov = vload3(0, part->part_V) - r_pt;
	b = dot(r_dir, ov);
	magsq_ov = dot(ov, ov);
	if (magsq_ov >= (rad_sq = part->part_vrad * part->part_vrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		no_roots = true;
	    } else {
		root = b*b - magsq_ov + rad_sq;
		if (root <= 0) {
		    /* no real roots */
		    no_roots = true;
		}
	    }
	} else {
	    root = b*b - magsq_ov + rad_sq;
	}
	if (!no_roots) {
	    root = sqrt(root);
	    t1 = b - root;
	    /* see if hit'[Z] is below V end of cylinder */
	    if (pprime.z + t1 * dprime.z <= part->part_v_hdist) {
		hitp->hit_dist = t1;
		hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
		hitp++;
	    }
	    t2 = b + root;
	    if (pprime.z + t2 * dprime.z >= part->part_v_hdist) {
		hitp->hit_dist = t2;
		hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
		hitp++;
	    }
	}
    }

    if (check_h) {
	double3 ov;		/* ray origin to center (V - P) */
	double rad_sq;
	double magsq_ov;	/* length squared of ov */
	double b;		/* second term of quadratic eqn */
	double root;		/* root of radical */
	bool no_roots = false;

	/*
	 * Next, consider a hit on H hemisphere
	 */
	ov = vload3(0, part->part_V) + vload3(0, part->part_H);
	ov = ov - r_pt;
	b = dot(r_dir, ov);
	magsq_ov = dot(ov, ov);
	if (magsq_ov >= (rad_sq = part->part_hrad * part->part_hrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		no_roots = true;
	    } else {
		root = b*b - magsq_ov + rad_sq;
		if (root <= 0) {
		    /* no real roots */
		    no_roots = true;
		}
	    }
	} else {
	    root = b*b - magsq_ov + rad_sq;
	}
	if (!no_roots) {
	    root = sqrt(root);
	    t1 = b - root;
	    /* see if hit'[Z] is above H end of cylinder */
	    if (pprime.z + t1 * dprime.z >= part->part_h_hdist) {
		hitp->hit_dist = t1;
		hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
		hitp++;
	    }
	    t2 = b + root;
	    if (pprime.z + t2 * dprime.z >= part->part_h_hdist) {
		hitp->hit_dist = t2;
		hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
		hitp++;
	    }
	}
    }

    if (hitp == &hits[0])
	return 0;	/* MISS */
    if (hitp == &hits[1]) {
	/* Only one hit, make it a 0-thickness segment */
	hits[1] = hits[0];		/* struct copy */
	hitp++;
    } else if (hitp > &hits[2]) {
	/*
	 * More than two intersections found.
	 * This can happen when a ray grazes down along a tangent
	 * line; the intersection interval from the hemisphere
	 * may not quite join up with the interval from the cone.
	 * Since particles are conv	ex, all we need to do is to
	 * return the maximum extent of the ray.
	 * Do this by sorting the intersections,
	 * and using the minimum and maximum values.
	 */
	primitive_hitsort(hits, hitp - &hits[0]);

	/* [0] is minimum, make [1] be maximum (hitp is +1 off end) */
	hits[1] = hitp[-1];	/* struct copy */
    }

    if (hits[0].hit_dist < hits[1].hit_dist) {
	/* entry is [0], exit is [1] */
	do_segp(res, idx, &hits[0], &hits[1]);
    } else {
	/* entry is [1], exit is [0] */
	do_segp(res, idx, &hits[1], &hits[0]);
    }
    return 2;			/* HIT */
}


void part_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct part_specific *part)
{
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    switch (hitp->hit_surfno) {
	case RT_PARTICLE_SURF_VSPHERE:
	    hitp->hit_normal = hitp->hit_point - vload3(0, part->part_V);
	    hitp->hit_normal = normalize(hitp->hit_normal);
	    break;
	case RT_PARTICLE_SURF_HSPHERE:
	    hitp->hit_normal = hitp->hit_point - vload3(0, part->part_V) - vload3(0, part->part_H);
	    hitp->hit_normal = normalize(hitp->hit_normal);
	    break;
	case RT_PARTICLE_SURF_BODY:
	    /* compute it */
	    if (part->part_type == RT_PARTICLE_TYPE_CYLINDER) {
		/* The X' and Y' components of hit' are N' */
		double3 can_normal;
		can_normal = (double3){
		    hitp->hit_vpriv.x,
		    hitp->hit_vpriv.y,
		    0};
		hitp->hit_normal = MAT4X3VEC(part->part_invRoS, can_normal);
		hitp->hit_normal = normalize(hitp->hit_normal);
		//hitp->hit_vpriv.z = 0;
	    } else {
		/* The cone case */
		double s, m;
		double3 unorm;
		/* vpriv[Z] ranges from 0 to 1 (roughly) */
		/* Rescale X' and Y' into unit circle */
		m = part->part_hrad_prime - part->part_vrad_prime;
		s = 1/(part->part_vrad_prime + m * hitp->hit_vpriv.z);
		unorm.x = hitp->hit_vpriv.x * s;
		unorm.y = hitp->hit_vpriv.y * s;
		/* Z' is constant, from slope of cylinder wall*/
		unorm.z = -m / sqrt(m*m+1);
		hitp->hit_normal = MAT4X3VEC(part->part_invRoS, unorm);
		hitp->hit_normal = normalize(hitp->hit_normal);
	    }
	    break;
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
