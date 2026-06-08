#include "common.cl"


struct superell_specific {
    double superell_V[3];		/* Vector to center of superellipsoid */
    double superell_invmsAu;		/* 1.0 / |Au|^2 */
    double superell_invmsBu;		/* 1.0 / |Bu|^2 */
    double superell_invmsCu;		/* 1.0 / |Cu|^2 */
    double superell_SoR[16];		/* matrix for local coordinate system, Scale(Rotate(V))*/
    double superell_invRSSR[16];	/* invR(Scale(Scale(Rot(V)))) */
    double superell_invR[16];           /* transposed rotation matrix */
    double superell_e;                  /* east-west curvature power */
    double superell_n;
    double superell_inve;
    double superell_invn;
};

double superell_eval(global const struct superell_specific *superell, const double3 P, const double3 D, double t)
{
    double x = P.x + t * D.x;
    double y = P.y + t * D.y;
    double z = P.z + t * D.z;

    double ax = sqrt(x * x + 1e-12);
    double ay = sqrt(y * y + 1e-12);
    double az = sqrt(z * z + 1e-12);

    double term_xy = pow(ax, superell->superell_inve) + pow(ay, superell->superell_inve);
    return pow(term_xy, superell->superell_e / superell->superell_n) + pow(az, superell->superell_invn) - 1.0;
}

double3 superell_grad(global const struct superell_specific *superell, const double3 W)
{
    double ax = sqrt(W.x * W.x + 1e-12);
    double ay = sqrt(W.y * W.y + 1e-12);
    double az = sqrt(W.z * W.z + 1e-12);

    double inve = superell->superell_inve;
    double invn = superell->superell_invn;
    double e_over_n = superell->superell_e / superell->superell_n;

    double term_xy = pow(ax, inve) + pow(ay, inve);
    if (term_xy < 1e-12) term_xy = 1e-12;

    double df_dx = invn * pow(term_xy, e_over_n - 1.0) * pow(ax, inve - 1.0) * (W.x / ax);
    double df_dy = invn * pow(term_xy, e_over_n - 1.0) * pow(ay, inve - 1.0) * (W.y / ay);
    double df_dz = invn * pow(az, invn - 1.0) * (W.z / az);

    return (double3)(df_dx, df_dy, df_dz);
}

double superell_refine_root(global const struct superell_specific *superell, const double3 P, const double3 D, double t0, double t1, double f0)
{
    double t = 0.5 * (t0 + t1);
    int iter;
    for (iter = 0; iter < 20; iter++) {
        double f = superell_eval(superell, P, D, t);
        if (fabs(f) < 1e-7) break;
        
        if (f * f0 < 0) {
            t1 = t;
        } else {
            t0 = t;
            f0 = f;
        }
        
        double3 W = P + t * D;
        double3 grad = superell_grad(superell, W);
        double df_dt = dot(grad, D);
        
        double t_next = t - f / df_dt;
        if (t_next > t0 && t_next < t1) {
            t = t_next;
        } else {
            t = 0.5 * (t0 + t1);
        }
    }
    return t;
}

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

    double dp, dd, pp, disc;
    double t_near, t_far;
    int num_roots = 0;
    double equation[5];
    int poly_success = 0;

    /* translate ray point */
    translated = r_pt - vload3(0, superell->superell_V);

    /* scale and rotate point to get P' */
    newShotPoint = MAT4X3VEC(superell->superell_SoR, translated);

    /* translate ray direction vector (unnormalized in local space) */
    newShotDir = MAT4X3VEC(superell->superell_SoR, r_dir);

    dp = dot(newShotDir, newShotPoint);
    dd = dot(newShotDir, newShotDir);
    pp = dot(newShotPoint, newShotPoint);

    disc = dp * dp - dd * (pp - 3.0);
    if (disc < 0) return 0;
    
    disc = sqrt(disc);
    t_near = (-dp - disc) / dd;
    t_far  = (-dp + disc) / dd;

    if (t_far < 0.0) return 0;
    if (t_near < 0.0) t_near = 0.0;

    if (fabs(superell->superell_n - 1.0) < 1e-5 && fabs(superell->superell_e - 1.0) < 1e-5) {
        equation[2] = pp - 1.0;
        equation[1] = 2.0 * dp;
        equation[0] = dd;
        if (rt_poly_roots(equation, 2, complexRoot) == 2) poly_success = 1;
    } else if (fabs(superell->superell_n - 0.5) < 1e-5 && fabs(superell->superell_e - 0.5) < 1e-5) {
        double3 P = newShotPoint; double3 D = newShotDir;
        equation[4] = P.x*P.x*P.x*P.x + P.y*P.y*P.y*P.y + P.z*P.z*P.z*P.z - 1.0;
        equation[3] = 4.0 * (P.x*P.x*P.x*D.x + P.y*P.y*P.y*D.y + P.z*P.z*P.z*D.z);
        equation[2] = 6.0 * (P.x*P.x*D.x*D.x + P.y*P.y*D.y*D.y + P.z*P.z*D.z*D.z);
        equation[1] = 4.0 * (P.x*D.x*D.x*D.x + P.y*D.y*D.y*D.y + P.z*D.z*D.z*D.z);
        equation[0] = D.x*D.x*D.x*D.x + D.y*D.y*D.y*D.y + D.z*D.z*D.z*D.z;
        if (rt_poly_roots(equation, 4, complexRoot) == 4) poly_success = 1;
    } else if (fabs(superell->superell_n - 0.5) < 1e-5 && fabs(superell->superell_e - 1.0) < 1e-5) {
        double3 P = newShotPoint; double3 D = newShotDir;
        double Pxy = P.x*P.x + P.y*P.y;
        double Dxy = D.x*D.x + D.y*D.y;
        double PDxy = P.x*D.x + P.y*D.y;
        equation[4] = Pxy*Pxy + P.z*P.z*P.z*P.z - 1.0;
        equation[3] = 4.0 * Pxy * PDxy + 4.0 * P.z*P.z*P.z*D.z;
        equation[2] = 2.0 * Pxy * Dxy + 4.0 * PDxy * PDxy + 6.0 * P.z*P.z*D.z*D.z;
        equation[1] = 4.0 * PDxy * Dxy + 4.0 * P.z*D.z*D.z*D.z;
        equation[0] = Dxy*Dxy + D.z*D.z*D.z*D.z;
        if (rt_poly_roots(equation, 4, complexRoot) == 4) poly_success = 1;
    }

    if (poly_success) {
        int dgr = 2;
        if (fabs(superell->superell_n - 0.5) < 1e-5) dgr = 4;
        for (i = 0; i < dgr; i++) {
            if (fabs(complexRoot[i].im) < 0.001) {
                realRoot[num_roots++] = complexRoot[i].re;
            }
        }
    } else {
        int max_steps = 100;
        double step = (t_far - t_near) / max_steps;
        double t = t_near;
        double f_prev = superell_eval(superell, newShotPoint, newShotDir, t);
        
        for (i = 0; i < max_steps; i++) {
            double t_next = t + step;
            double f_next = superell_eval(superell, newShotPoint, newShotDir, t_next);
            
            if (f_prev * f_next <= 0.0) {
                double root = superell_refine_root(superell, newShotPoint, newShotDir, t, t_next, f_prev);
                if (num_roots < 4) {
                    realRoot[num_roots++] = root;
                }
            }
            t = t_next;
            f_prev = f_next;
        }
    }

    if (num_roots < 2) return 0;

    /* Sort roots */
    for (i = 0; i < num_roots - 1; i++) {
        for (int k = 0; k < num_roots - i - 1; k++) {
            if (realRoot[k] > realRoot[k+1]) {
                double tmp = realRoot[k];
                realRoot[k] = realRoot[k+1];
                realRoot[k+1] = tmp;
            }
        }
    }

    /* Remove duplicates */
    int num_unique = 1;
    for (i = 1; i < num_roots; i++) {
        if (fabs(realRoot[i] - realRoot[num_unique-1]) > 1e-4) {
            realRoot[num_unique++] = realRoot[i];
        }
    }
    num_roots = num_unique;

    struct hit hits[2];
    for (i = 0; i + 1 < num_roots; i += 2) {
        hits[0].hit_dist = realRoot[i];
        hits[1].hit_dist = realRoot[i+1];
        hits[0].hit_surfno = 0;
        hits[1].hit_surfno = 0;
        do_segp(res, idx, &hits[0], &hits[1]);
    }

    return num_roots;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void superell_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct superell_specific *superell)
{
    double3 xlated, P, grad;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    xlated = hitp->hit_point - vload3(0, superell->superell_V);
    
    P = MAT4X3VEC(superell->superell_SoR, xlated);
    grad = superell_grad(superell, P);
    hitp->hit_normal = MAT4X3VEC(superell->superell_invR, grad);
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
