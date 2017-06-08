#include "common.cl"

/* hit_surfno is set to one of these */
#define RPC_NORM_BODY (1)		/* compute normal */
#define RPC_NORM_TOP (2)		/* copy tgc_N */
#define RPC_NORM_FRT (3)		/* copy reverse tgc_N */
#define RPC_NORM_BACK (4)

struct rpc_specific {
    double rpc_V[3];		/* vector to rpc origin */
    double rpc_Bunit[3];	/* unit B vector */
    double rpc_Hunit[3];	/* unit H vector */
    double rpc_Runit[3];	/* unit vector, B x H */
    double rpc_SoR[16];	/* Scale(Rot(vect)) */
    double rpc_invRoS[16];	/* invRot(Scale(vect)) */
};

int rpc_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct rpc_specific *rpc)
{
    double3 dprime;		/* D' */
    double3 pprime;		/* P' */
    double k1, k2;		/* distance constants of solution */
    double3 xlated;		/* translated vector */
    struct hit hits[3];	/* 2 potential hit points */
    struct hit *hitp;	/* pointer to hit point */
    
    hitp = &hits[0];

    dprime = MAT4X3VEC(rpc->rpc_SoR, r_dir);
    xlated = r_pt - vload3(0, rpc->rpc_V);
    pprime = MAT4X3VEC(rpc->rpc_SoR, xlated);
    
    /* Find roots of the equation, using formula for quadratic */
    if (!NEAR_ZERO(dprime.y, RT_PCOEF_TOL)) {
        double a, b, c;	/* coeffs of polynomial */
        double disc;	/* disc of radical */
        
        a = dprime.y * dprime.y;
        b = 2.0 * dprime.y * pprime.y - dprime.z;
        c = pprime.y * pprime.y - pprime.z - 1.0;
        disc = b * b - 4.0 * a * c;
        if (disc > 0.0) {
            disc = sqrt(disc);
        
            k1 = (-b + disc) / (2.0 * a);
            k2 = (-b - disc) / (2.0 * a);
        
            /*
             * k1 and k2 are potential solutions to intersection with
             * side.  See if they fall in range.
             */
            hitp->hit_vpriv = pprime + k1 * dprime;	/* hit' */
            if (hitp->hit_vpriv.x >= -1.0 && hitp->hit_vpriv.x <= 0.0
                && hitp->hit_vpriv.z <= 0.0) {
                hitp->hit_dist = k1;
                hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
                hitp++;
            }
        
            hitp->hit_vpriv = pprime + k2 * dprime;	/* hit' */
            if (hitp->hit_vpriv.x >= -1.0 && hitp->hit_vpriv.x <= 0.0
                && hitp->hit_vpriv.z <= 0.0) {
                hitp->hit_dist = k2;
                hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
                hitp++;
            }
        }
    } else if (!NEAR_ZERO(dprime.z, RT_PCOEF_TOL)) {
        k1 = (pprime.y * pprime.y - pprime.z - 1.0) / dprime.z;
        hitp->hit_vpriv = pprime + k1 * dprime;	/* hit' */
        if (hitp->hit_vpriv.x >= -1.0 && hitp->hit_vpriv.x <= 0.0
            && hitp->hit_vpriv.z <= 0.0) {
            hitp->hit_dist = k1;
            hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
            hitp++;
        }
    }
    
    /*
     * Check for hitting the end plates.
     */
    
    /* check front and back plates */
    if (hitp < &hits[2]  &&  !NEAR_ZERO(dprime.x, RT_PCOEF_TOL)) {
        /* 0 or 1 hits so far, this is worthwhile */
        k1 = -pprime.x / dprime.x;		/* front plate */
        k2 = (-1.0 - pprime.x) / dprime.x;	/* back plate */
        
        hitp->hit_vpriv = pprime + k1 * dprime;	/* hit' */
        if (hitp->hit_vpriv.y * hitp->hit_vpriv.y
            - hitp->hit_vpriv.z <= 1.0
            && hitp->hit_vpriv.z <= 0.0) {
            hitp->hit_dist = k1;
            hitp->hit_surfno = RPC_NORM_FRT;	/* -H */
            hitp++;
        }
        
        hitp->hit_vpriv = pprime + k2 * dprime;	/* hit' */
        if (hitp->hit_vpriv.y * hitp->hit_vpriv.y
            - hitp->hit_vpriv.z <= 1.0
            && hitp->hit_vpriv.z <= 0.0) {
            hitp->hit_dist = k2;
            hitp->hit_surfno = RPC_NORM_BACK;	/* +H */
            hitp++;
        }
    }
    
    /* check top plate */
    if (hitp == &hits[1]  &&  !NEAR_ZERO(dprime.z, RT_PCOEF_TOL)) {
        /* 1 hit so far, this is worthwhile */
        k1 = -pprime.z / dprime.z;		/* top plate */
        
        hitp->hit_vpriv = pprime + k1 * dprime;	/* hit' */
        if (hitp->hit_vpriv.x >= -1.0 &&  hitp->hit_vpriv.x <= 0.0
            && hitp->hit_vpriv.y >= -1.0
            && hitp->hit_vpriv.y <= 1.0) {
            hitp->hit_dist = k1;
            hitp->hit_surfno = RPC_NORM_TOP;	/* -B */
            hitp++;
        }
    }
    
    if (hitp != &hits[2]) 
        return 0;	/* MISS */
    
    if (hits[0].hit_dist < hits[1].hit_dist) {
        /* entry is [0], exit is [1] */
        do_segp(res, idx, &hits[0], &hits[1]);
    } else {
        /* entry is [1], exit is [0] */
        do_segp(res, idx, &hits[1], &hits[0]);
    }

    return 2;   /* HIT */
}

void rpc_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct rpc_specific *rpc)
{
    double3 can_normal; /* normal to canonical rpc */
    
    hitp->hit_point = r_pt * r_dir + hitp->hit_dist;
    switch (hitp->hit_surfno) {
        case RPC_NORM_BODY:
            can_normal = (double3){0.0, hitp->hit_vpriv.y, -0.5};
            hitp->hit_normal = MAT4X3VEC(rpc->rpc_invRoS, can_normal);
            hitp->hit_normal = normalize(hitp->hit_normal);
            break;
        case RPC_NORM_TOP:
            hitp->hit_normal = -vload3(0, rpc->rpc_Bunit);
            break;
        case RPC_NORM_FRT:
            hitp->hit_normal = -vload3(0, rpc->rpc_Hunit);
            break;
        case RPC_NORM_BACK:
            hitp->hit_normal = vload3(0, rpc->rpc_Hunit);
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
