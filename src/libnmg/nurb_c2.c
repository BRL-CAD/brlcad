/*                       N U R B _ C 2 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nurb */
/** @{ */
/** @file primitives/bspline/nurb_c2.c
 *
 * Given parametric u, v values, return the curvature of the surface.
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bn/mat.h"
#include "nmg.h"


void
nmg_nurb_curvature(struct nmg_curvature *cvp, const struct face_g_snurb *srf, fastf_t u, fastf_t v)
{
    struct face_g_snurb * us, *vs, * uus, * vvs, *uvs;
    fastf_t ue[4], ve[4], uue[4], vve[4], uve[4], se[4];
    fastf_t E, F, G;                /* First Fundamental Form */
    fastf_t L, M, N;                /* Second Fundamental form */
    fastf_t denom;
    fastf_t wein[4];                /*Weingarten matrix */
    fastf_t evec[3];
    fastf_t mean, gauss, discrim;
    vect_t norm;
    int i;

    us = nmg_nurb_s_diff(srf, RT_NURB_SPLIT_ROW);
    vs = nmg_nurb_s_diff(srf, RT_NURB_SPLIT_COL);
    uus = nmg_nurb_s_diff(us, RT_NURB_SPLIT_ROW);
    vvs = nmg_nurb_s_diff(vs, RT_NURB_SPLIT_COL);
    uvs = nmg_nurb_s_diff(vs, RT_NURB_SPLIT_ROW);

    nmg_nurb_s_eval(srf, u, v, se);
    nmg_nurb_s_eval(us, u, v, ue);
    nmg_nurb_s_eval(vs, u, v, ve);
    nmg_nurb_s_eval(uus, u, v, uue);
    nmg_nurb_s_eval(vvs, u, v, vve);
    nmg_nurb_s_eval(uvs, u, v, uve);

    nmg_nurb_free_snurb(us);
    nmg_nurb_free_snurb(vs);
    nmg_nurb_free_snurb(uus);
    nmg_nurb_free_snurb(vvs);
    nmg_nurb_free_snurb(uvs);

    if (RT_NURB_IS_PT_RATIONAL(srf->pt_type)) {
	for (i = 0; i < 3; i++) {
	    ue[i] = (1.0 / se[3] * ue[i]) -
		(ue[3]/se[3]) * se[0]/se[3];
	    ve[i] = (1.0 / se[3] * ve[i]) -
		(ve[3]/se[3]) * se[0]/se[3];
	}
	VCROSS(norm, ue, ve);
	VUNITIZE(norm);
	E = VDOT(ue, ue);
	F = VDOT(ue, ve);
	G = VDOT(ve, ve);

	for (i = 0; i < 3; i++) {
	    uue[i] = (1.0 / se[3] * uue[i]) -
		2 * (uue[3]/se[3]) * uue[i] -
		uue[3]/se[3] * (se[i]/se[3]);

	    vve[i] = (1.0 / se[3] * vve[i]) -
		2 * (vve[3]/se[3]) * vve[i] -
		vve[3]/se[3] * (se[i]/se[3]);

	    uve[i] = 1.0 / se[3] * uve[i] +
		(-1.0 / (se[3] * se[3])) *
		(ve[3] * ue[i] + ue[3] * ve[i] +
		 uve[3] * se[i]) +
		(-2.0 / (se[3] * se[3] * se[3])) *
		(ve[3] * ue[3] * se[i]);
	}

	L = VDOT(norm, uue);
	M = VDOT(norm, uve);
	N = VDOT(norm, vve);

    } else {
	VCROSS(norm, ue, ve);
	VUNITIZE(norm);
	E = VDOT(ue, ue);
	F = VDOT(ue, ve);
	G = VDOT(ve, ve);

	L = VDOT(norm, uue);
	M = VDOT(norm, uve);
	N = VDOT(norm, vve);
    }

    if (srf->order[0] <= 2 && srf->order[1] <= 2) {
	cvp->crv_c1 = cvp->crv_c2 = 0;
	bn_vec_ortho(cvp->crv_pdir, norm);
	return;
    }

    denom = ((E*G) - (F*F));
    gauss = (L * N - M *M)/denom;
    mean = (G * L + E * N - 2 * F * M) / (2 * denom);
    discrim = sqrt(mean * mean - gauss);

    cvp->crv_c1 = mean - discrim;
    cvp->crv_c2 = mean + discrim;

    if (fabs(E*G - F*F) < 0.0001) {
	/* XXX */
	bu_log("nmg_nurb_curvature: first fundamental form is singular E = %g F= %g G = %g\n",
	       E, F, G);
	bn_vec_ortho(cvp->crv_pdir, norm);	/* sanity */
	return;
    }

    wein[0] = ((G * L) - (F * M))/ (denom);
    wein[1] = ((G * M) - (F * N))/ (denom);
    wein[2] = ((E * M) - (F * L))/ (denom);
    wein[3] = ((E * N) - (F * M))/ (denom);

    if (fabs(wein[1]) < 0.0001 && fabs(wein[3] - cvp->crv_c1) < 0.0001) {
	evec[0] = 0.0; evec[1] = 1.0;
    } else {
	evec[0] = 1.0;
	if (fabs(wein[1]) > fabs(wein[3] - cvp->crv_c1)) {
	    evec[1] = (cvp->crv_c1 - wein[0]) / wein[1];
	} else {
	    evec[1] = wein[2] / (cvp->crv_c1 - wein[3]);
	}
    }

    cvp->crv_pdir[0] = evec[0] * ue[0] + evec[1] * ve[0];
    cvp->crv_pdir[1] = evec[0] * ue[1] + evec[1] * ve[1];
    cvp->crv_pdir[2] = evec[0] * ue[2] + evec[1] * ve[2];
    VUNITIZE(cvp->crv_pdir);
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
