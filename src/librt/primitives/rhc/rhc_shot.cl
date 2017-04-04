#include "common.cl"

/* hit_surfno is set to one of these */
#define RHC_NORM_BODY (1)		/* compute normal */
#define RHC_NORM_TOP (2)		/* copy rhc_N */
#define RHC_NORM_FRT (3)		/* copy reverse rhc_N */
#define RHC_NORM_BACK (4)

struct rhc_specific {
    double rhc_V[3];		/* vector to rhc origin */
    double rhc_Bunit[3];	/* unit B vector */
    double rhc_Hunit[3];	/* unit H vector */
    double rhc_Runit[3];	/* unit vector, B x H */
    double rhc_SoR[16];	/* Scale(Rot(vect)) */
    double rhc_invRoS[16];	/* invRot(Scale(vect)) */
    double rhc_cprime;	/* c / |B| */
};

int rhc_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct rhc_specific *rhc)
{
    double3 dprime;		/* D' */
    double3 pprime;		/* P' */
    double k1, k2;		/* distance constants of solution */
    double x;
    double3 xlated;		/* translated vector */
    struct hit hits[3];	/* 2 potential hit points */
    struct hit *hitp;	/* pointer to hit point */

    hitp = &hits[0];

    /* out, Mat, vect */
    dprime = MAT4X3VEC(rhc->rhc_SoR, r_dir);
    xlated = r_pt - vload3(0, rhc->rhc_V);
    pprime = MAT4X3VEC(rhc->rhc_SoR, xlated);

    x = rhc->rhc_cprime;

    if (!(ZERO(dprime.y) && ZERO(dprime.z))) {
        /* Find roots of the equation, using formula for quadratic */
        double a, b, c;	/* coeffs of polynomial */
        double disc;    /* disc of radical */

        a = dprime.z * dprime.z - dprime.y * dprime.y * (1 + 2 * x);
        b = 2 * ((pprime.z + x + 1) * dprime.z
            - (2 * x + 1) * dprime.y * pprime.y);
        c = (pprime.z + x + 1) * (pprime.z + x + 1)
            - (2 * x + 1) * pprime.y * pprime.y - x * x;

        if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
            disc = b * b - 4 * a * c;

            if (disc > 0) {
                disc = sqrt(disc);
                
                k1 = (-b + disc) / (2.0 * a);
                k2 = (-b - disc) / (2.0 * a);
                
                /*
                 * k1 and k2 are potential solutions to intersection with side.
                 * See if they fall in range.
                 */
                hitp->hit_vpriv = pprime + k1 * dprime;  /* hit' */
                
                if (hitp->hit_vpriv.x >= -1.0
                    && hitp->hit_vpriv.x <= 0.0
                    && hitp->hit_vpriv.z >= -1.0
                    && hitp->hit_vpriv.z <= 0.0) {
                    hitp->hit_dist = k1;
                    hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
                    hitp++;
                }
                
                hitp->hit_vpriv = pprime + k2 * dprime;  /* hit' */
                
                if (hitp->hit_vpriv.x >= -1.0
                    && hitp->hit_vpriv.x <= 0.0
                    && hitp->hit_vpriv.z >= -1.0
                    && hitp->hit_vpriv.z <= 0.0) {
                    hitp->hit_dist = k2;
                    hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
                    hitp++;
                }
            }
        } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
            k1 = -c / b;
            hitp->hit_vpriv = pprime + k1 * dprime;
            
            if (hitp->hit_vpriv.x >= -1.0
                && hitp->hit_vpriv.x <= 0.0
                && hitp->hit_vpriv.z >= -1.0
                && hitp->hit_vpriv.z <= 0.0) {
                hitp->hit_dist = k1;
                hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
                hitp++;
            }
        }
    }
        
    /*
     * Check for hitting the top and end plates.
     */
    
    /* check front and back plates */
    if (hitp < &hits[2]  &&  !ZERO(dprime.x)) {
        /* 0 or 1 hits so far, this is worthwhile */
        k1 = -pprime.x / dprime.x;    /* front plate */
        k2 = (-1.0 - pprime.x) / dprime.x;	/* back plate */
        
        hitp->hit_vpriv = pprime + k1 * dprime; /* hit' */
        
        if ((hitp->hit_vpriv.z + x + 1.0)
            * (hitp->hit_vpriv.z + x + 1.0)
            - hitp->hit_vpriv.y * hitp->hit_vpriv.y
            * (1.0 + 2 * x) >= x * x
            && hitp->hit_vpriv.z >= -1.0
            && hitp->hit_vpriv.z <= 0.0) {
            hitp->hit_dist = k1;
            hitp->hit_surfno = RHC_NORM_FRT;	/* -H */
            hitp++;
        }
        
        hitp->hit_vpriv = pprime + k2 * dprime;	/* hit' */
        
        if ((hitp->hit_vpriv.z + x + 1.0)
            * (hitp->hit_vpriv.z + x + 1.0)
            - hitp->hit_vpriv.y * hitp->hit_vpriv.y
            * (1.0 + 2 * x) >= x * x
            && hitp->hit_vpriv.z >= -1.0
            && hitp->hit_vpriv.z <= 0.0) {
            hitp->hit_dist = k2;
            hitp->hit_surfno = RHC_NORM_BACK;	/* +H */
            hitp++;
        }
    }
    
    /* check top plate */
    if (hitp == &hits[1]  &&  !ZERO(dprime.z)) {
        /* 0 or 1 hits so far, this is worthwhile */
        k1 = -pprime.z / dprime.z;		/* top plate */
        
        hitp->hit_vpriv = pprime + k1 * dprime; /* hit' */
        
        if (hitp->hit_vpriv.x >= -1.0 &&  hitp->hit_vpriv.x <= 0.0
            && hitp->hit_vpriv.y >= -1.0
            && hitp->hit_vpriv.y <= 1.0) {
            hitp->hit_dist = k1;
            hitp->hit_surfno = RHC_NORM_TOP;	/* -B */
            hitp++;
        }
    }

    if (hitp != &hits[2]) {
        return 0;    /* MISS */
    }
        
    if (hits[0].hit_dist < hits[1].hit_dist) {
        /* entry is [0], exit is [1] */
        do_segp(res, idx, &hits[0], &hits[1]);
    } else {
        /* entry is [1], exit is [0] */
        do_segp(res, idx, &hits[1], &hits[0]);
    }
        
    return 2;   /* HIT */
}

void rhc_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct rhc_specific *rhc)
{
    double c;
    double3 can_normal; /* normal to canonical rhc */
    
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    switch (hitp->hit_surfno) {
        case RHC_NORM_BODY:
            c = rhc->rhc_cprime;
            can_normal = (double3){0.0f, hitp->hit_vpriv.y * (1.0 + 2.0 * c), -hitp->hit_vpriv.z - c - 1.0};
            hitp->hit_normal = MAT4X3VEC(rhc->rhc_invRoS, can_normal);
            hitp->hit_normal = normalize(hitp->hit_normal);
            break;
        case RHC_NORM_TOP:
            hitp->hit_normal = -vload3(0, rhc->rhc_Bunit);
            break;
        case RHC_NORM_FRT:
            hitp->hit_normal = -vload3(0, rhc->rhc_Hunit);
            break;
        case RHC_NORM_BACK:
            hitp->hit_normal = vload3(0, rhc->rhc_Hunit);
            break;
        default:
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
