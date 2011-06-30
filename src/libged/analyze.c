/*                          A N A L Y Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/analyze.c
 *
 * The analyze command.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"

#include "./ged_private.h"


/* Conversion factor for Gallons to cubic millimeters */
#define GALLONS_TO_MM3 3785411.784


/* ARB face printout array */
static const int prface[5][6] = {
    {123, 124, 234, 134, -111, -111},		/* ARB4 */
    {1234, 125, 235, 345, 145, -111},		/* ARB5 */
    {1234, 2365, 1564, 512, 634, -111},		/* ARB6 */
    {1234, 567, 145, 2376, 1265, 4375},		/* ARB7 */
    {1234, 5678, 1584, 2376, 1265, 4378},	/* ARB8 */
};


/* division of an arb8 into 6 arb4s */
static const int farb4[6][4] = {
    {0, 1, 2, 4},
    {4, 5, 6, 1},
    {1, 2, 6, 4},
    {0, 2, 3, 4},
    {4, 6, 7, 2},
    {2, 3, 7, 4},
};


/* edge definition array */
static const int nedge[5][24] = {
    {0, 1, 1, 2, 2, 0, 0, 3, 3, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* ARB4 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 4, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1},		/* ARB5 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 5, 3, 5, 4, 5, -1, -1, -1, -1, -1, -1},		/* ARB6 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 3, 4, 1, 5, 2, 6, 4, 5, 5, 6, 4, 6, -1, -1},			/* ARB7 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 4, 5, 1, 5, 5, 6, 6, 7, 4, 7, 3, 7, 2, 6},			/* ARB8 */
};


/*
 * F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
static void
findang(fastf_t *angles, fastf_t *unitv)
{
    fastf_t f;

    /* convert direction cosines into axis angles */
    if (unitv[X] <= -1.0) angles[X] = -90.0;
    else if (unitv[X] >= 1.0) angles[X] = 90.0;
    else angles[X] = acos(unitv[X]) * bn_radtodeg;

    if (unitv[Y] <= -1.0) angles[Y] = -90.0;
    else if (unitv[Y] >= 1.0) angles[Y] = 90.0;
    else angles[Y] = acos(unitv[Y]) * bn_radtodeg;

    if (unitv[Z] <= -1.0) angles[Z] = -90.0;
    else if (unitv[Z] >= 1.0) angles[Z] = 90.0;
    else angles[Z] = acos(unitv[Z]) * bn_radtodeg;

    /* fallback angle */
    if (unitv[Z] <= -1.0) unitv[Z] = -1.0;
    else if (unitv[Z] >= 1.0) unitv[Z] = 1.0;
    angles[4] = asin(unitv[Z]);

    /* rotation angle */
    /* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
     * with an epsilon of +/- 1.0e-17, so the tolerance below was
     * substituted for the original +/- 1.0e-20.
     */
    if ((f = cos(angles[4])) > 1.0e-16 || f < -1.0e-16) {
	f = unitv[X]/f;
	if (f <= -1.0)
	    angles[3] = 180;
	else if (f >= 1.0)
	    angles[3] = 0;
	else
	    angles[3] = bn_radtodeg * acos(f);
    }  else
	angles[3] = 0.0;
    if (unitv[Y] < 0)
	angles[3] = 360.0 - angles[3];

    angles[4] *= bn_radtodeg;
}


/* Analyzes an arb face
 */
static double
analyze_face(struct ged *gedp, int face, fastf_t *center_pt, const struct rt_arb_internal *arb, int type, const struct bn_tol *tol)


/* reference center point */


{
    int i, j, k;
    int a, b, c, d;		/* 4 points of face to look at */
    fastf_t angles[5];	/* direction cosines, rot, fb */
    fastf_t temp;
    fastf_t area[2], len[6];
    vect_t v_temp;
    plane_t plane;
    double face_area = 0;

    a = rt_arb_faces[type][face*4+0];
    b = rt_arb_faces[type][face*4+1];
    c = rt_arb_faces[type][face*4+2];
    d = rt_arb_faces[type][face*4+3];

    if (a == -1)
	return 0;

    /* find plane eqn for this face */
    if (bn_mk_plane_3pts(plane, arb->pt[a], arb->pt[b],
			 arb->pt[c], tol) < 0) {
	bu_vls_printf(gedp->ged_result_str, "| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
		      a+1, b+1, c+1, d+1);
	return 0;
    }

    /* the plane equations returned by planeqn above do not
     * necessarily point outward. Use the reference center
     * point for the arb and reverse direction for
     * any errant planes. This corrects the output rotation,
     * fallback angles so that they always give the outward
     * pointing normal vector.
     */
    if ((plane[W] - VDOT(center_pt, &plane[0])) < 0.0) {
	for (i=0; i<4; i++)
	    plane[i] *= -1.0;
    }

    /* plane[] contains normalized eqn of plane of this face
     * find the dir cos, rot, fb angles
     */
    findang(angles, &plane[0]);

    /* find the surface area of this face */
    for (i=0; i<3; i++) {
	j = rt_arb_faces[type][face*4+i];
	k = rt_arb_faces[type][face*4+i+1];
	VSUB2(v_temp, arb->pt[k], arb->pt[j]);
	len[i] = MAGNITUDE(v_temp);
    }
    len[4] = len[2];
    j = rt_arb_faces[type][face*4+0];
    for (i=2; i<4; i++) {
	k = rt_arb_faces[type][face*4+i];
	VSUB2(v_temp, arb->pt[k], arb->pt[j]);
	len[((i*2)-1)] = MAGNITUDE(v_temp);
    }
    len[2] = len[3];

    for (i=0; i<2; i++) {
	j = i*3;
	temp = .5 * (len[j] + len[j+1] + len[j+2]);
	area[i] = sqrt(temp * (temp - len[j]) * (temp - len[j+1]) * (temp - len[j+2]));
	face_area += area[i];
    }

    bu_vls_printf(gedp->ged_result_str, "| %4d |", prface[type][face]);
    bu_vls_printf(gedp->ged_result_str, " %6.8f %6.8f | %6.8f %6.8f %6.8f %11.8f |",
		  angles[3], angles[4],
		  plane[X], plane[Y], plane[Z],
		  plane[W]*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "   %13.8f  |\n",
		  (area[0]+area[1])*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    return face_area;
}


/* Analyzes arb edges - finds lengths */
static void
analyze_edge(struct ged *gedp, int edge, const struct rt_arb_internal *arb, int type)
{
    int a, b;
    static vect_t v_temp;

    a = nedge[type][edge*2];
    b = nedge[type][edge*2+1];

    if (b == -1) {
	/* fill out the line */
	if ((a = edge%4) == 0)
	    return;
	if (a == 1) {
	    bu_vls_printf(gedp->ged_result_str, "  |                |                |                |\n  ");
	    return;
	}
	if (a == 2) {
	    bu_vls_printf(gedp->ged_result_str, "  |                |                |\n  ");
	    return;
	}
	bu_vls_printf(gedp->ged_result_str, "  |                |\n  ");
	return;
    }

    VSUB2(v_temp, arb->pt[b], arb->pt[a]);
    bu_vls_printf(gedp->ged_result_str, "  |  %d%d %9.8f",
		  a+1, b+1, MAGNITUDE(v_temp)*gedp->ged_wdbp->dbip->dbi_base2local);

    if (++edge%4 == 0)
	bu_vls_printf(gedp->ged_result_str, "  |\n  ");
}


/* Finds volume of an arb4 defined by farb4[loc][] 	*/
static double
analyze_find_vol(int loc, struct rt_arb_internal *arb, struct bn_tol *tol)
{
    int a, b, c, d;
    fastf_t vol, height, len[3], temp, areabase;
    vect_t v_temp;
    plane_t plane;

    /* a, b, c = base of the arb4 */
    a = farb4[loc][0];
    b = farb4[loc][1];
    c = farb4[loc][2];

    /* d = "top" point of arb4 */
    d = farb4[loc][3];

    if (bn_mk_plane_3pts(plane, arb->pt[a], arb->pt[b],
			 arb->pt[c], tol) < 0)
	return 0.0;

    /* have a good arb4 - find its volume */
    height = fabs(plane[W] - VDOT(&plane[0], arb->pt[d]));
    VSUB2(v_temp, arb->pt[b], arb->pt[a]);
    len[0] = MAGNITUDE(v_temp);
    VSUB2(v_temp, arb->pt[c], arb->pt[a]);
    len[1] = MAGNITUDE(v_temp);
    VSUB2(v_temp, arb->pt[c], arb->pt[b]);
    len[2] = MAGNITUDE(v_temp);
    temp = 0.5 * (len[0] + len[1] + len[2]);
    areabase = sqrt(temp * (temp-len[0]) * (temp-len[1]) * (temp-len[2]));
    vol = areabase * height / 3.0;
    return vol;
}


/*
 * A R B _ A N A L
 */
static void
analyze_arb(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    int i;
    point_t center_pt;
    double tot_vol;
    double tot_area;
    int cgtype;		/* COMGEOM arb type: # of vertices */
    int type;

    /* find the specific arb type, in GIFT order. */
    if ((cgtype = rt_arb_std_type(ip, &gedp->ged_wdbp->wdb_tol)) == 0) {
	bu_vls_printf(gedp->ged_result_str, "analyze_arb: bad ARB\n");
	return;
    }

    tot_area = tot_vol = 0.0;

    type = cgtype - 4;

    /* analyze each face, use center point of arb for reference */
    bu_vls_printf(gedp->ged_result_str, "\n------------------------------------------------------------------------------\n");
    bu_vls_printf(gedp->ged_result_str, "| FACE |   ROT     FB  |        PLANE EQUATION            |   SURFACE AREA   |\n");
    bu_vls_printf(gedp->ged_result_str, "|------|---------------|----------------------------------|------------------|\n");
    rt_arb_centroid(center_pt, arb, cgtype);

    for (i=0; i<6; i++)
	tot_area += analyze_face(gedp, i, center_pt, arb, type, &gedp->ged_wdbp->wdb_tol);

    bu_vls_printf(gedp->ged_result_str, "------------------------------------------------------------------------------\n");

    /* analyze each edge */
    bu_vls_printf(gedp->ged_result_str, "    | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   |\n");
    bu_vls_printf(gedp->ged_result_str, "    |----------------|----------------|----------------|----------------|\n  ");

    /* set up the records for arb4's and arb6's */
    {
	struct rt_arb_internal earb;

	earb = *arb;		/* struct copy */
	if (cgtype == 4) {
	    VMOVE(earb.pt[3], earb.pt[4]);
	} else if (cgtype == 6) {
	    VMOVE(earb.pt[5], earb.pt[6]);
	}
	for (i=0; i<12; i++) {
	    analyze_edge(gedp, i, &earb, type);
	    if (nedge[type][i*2] == -1)
		break;
	}
    }
    bu_vls_printf(gedp->ged_result_str, "  ---------------------------------------------------------------------\n");

    /* find the volume - break arb8 into 6 arb4s */
    for (i=0; i<6; i++)
	tot_vol += analyze_find_vol(i, arb, &gedp->ged_wdbp->wdb_tol);

    bu_vls_printf(gedp->ged_result_str, "      | Volume = %18.8f    Surface Area = %15.8f |\n",
		  tot_vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  tot_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "      |          %18.8f gal                               |\n",
		  tot_vol/GALLONS_TO_MM3);
    bu_vls_printf(gedp->ged_result_str, "      -----------------------------------------------------------------\n");
}


/* analyze a torus */
static void
analyze_tor(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;
    fastf_t r1, r2, vol, sur_area;

    RT_TOR_CK_MAGIC(tor);

    r1 = tor->r_a;
    r2 = tor->r_h;

    vol = 2.0 * M_PI * M_PI * r1 * r2 * r2;
    sur_area = 4.0 * M_PI * M_PI * r1 * r2;

    bu_vls_printf(gedp->ged_result_str, "TOR Vol = %.8f (%.8f gal) Surface Area = %.8f\n",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3,
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);

    return;
}


#define PROLATE 1
#define OBLATE 2

/* analyze an ell */
static void
analyze_ell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_ell_internal *ell = (struct rt_ell_internal *)ip->idb_ptr;
    fastf_t ma, mb, mc;
#ifdef major		/* Some systems have these defined as macros!!! */
#undef major
#endif
#ifdef minor
#undef minor
#endif
    fastf_t ecc, major, minor;
    fastf_t vol, sur_area;
    int type;

    RT_ELL_CK_MAGIC(ell);

    ma = MAGNITUDE(ell->a);
    mb = MAGNITUDE(ell->b);
    mc = MAGNITUDE(ell->c);

    type = 0;

    vol = 4.0 * M_PI * ma * mb * mc / 3.0;
    bu_vls_printf(gedp->ged_result_str, "ELL Volume = %.8f (%.8f gal)",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3);

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		      sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
	return;
    }
    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major = mc;
	    minor = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major = ma;
	    minor = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major = mb;
		minor = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major = ma;
		minor = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major = ma;
		    minor = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major = mb;
		    minor = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }
    ecc = sqrt(major*major - minor*minor) / major;
    if (type == PROLATE) {
	sur_area = 2.0 * M_PI * minor * minor +
	    (2.0 * M_PI * (major*minor/ecc) * asin(ecc));
    } else if (type == OBLATE) {
	sur_area = 2.0 * M_PI * major * major +
	    (M_PI * (minor*minor/ecc) * log((1.0+ecc)/(1.0-ecc)));
    } else {
	sur_area = 0.0;
    }

    bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
}


/* analyze an superell */
static void
analyze_superell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    fastf_t ma, mb, mc;
#ifdef major		/* Some systems have these defined as macros!!! */
#undef major
#endif
#ifdef minor
#undef minor
#endif
    fastf_t ecc, major, minor;
    fastf_t vol, sur_area;
    int type;

    RT_SUPERELL_CK_MAGIC(superell);

    ma = MAGNITUDE(superell->a);
    mb = MAGNITUDE(superell->b);
    mc = MAGNITUDE(superell->c);

    type = 0;

    vol = 4.0 * M_PI * ma * mb * mc / 3.0;
    bu_vls_printf(gedp->ged_result_str, "SUPERELL Volume = %.8f (%.8f gal)",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3);

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		      sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
	return;
    }
    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major = mc;
	    minor = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major = ma;
	    minor = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major = mb;
		minor = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major = ma;
		minor = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major = ma;
		    minor = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major = mb;
		    minor = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }
    ecc = sqrt(major*major - minor*minor) / major;
    if (type == PROLATE) {
	sur_area = 2.0 * M_PI * minor * minor +
	    (2.0 * M_PI * (major*minor/ecc) * asin(ecc));
    } else if (type == OBLATE) {
	sur_area = 2.0 * M_PI * major * major +
	    (M_PI * (minor*minor/ecc) * log((1.0+ecc)/(1.0-ecc)));
    } else {
	sur_area = 0.0;
    }

    bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
}


#define GED_ANAL_RCC 1
#define GED_ANAL_TRC 2
#define GED_ANAL_REC 3

/* analyze tgc */
static void
analyze_tgc(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    fastf_t maxb, ma, mb, mc, md, mh;
    fastf_t area_base, area_top, area_side, vol;
    vect_t axb;
    int cgtype = 0;

    RT_TGC_CK_MAGIC(tgc);

    VCROSS(axb, tgc->a, tgc->b);
    maxb = MAGNITUDE(axb);
    ma = MAGNITUDE(tgc->a);
    mb = MAGNITUDE(tgc->b);
    mc = MAGNITUDE(tgc->c);
    md = MAGNITUDE(tgc->d);
    mh = MAGNITUDE(tgc->h);

    /* check for right cylinder */
    if (fabs(fabs(VDOT(tgc->h, axb)) - (mh*maxb)) < .00001) {
	/* have a right cylinder */
	if (fabs(ma-mb) < .00001) {
	    /* have a circular base */
	    if (fabs(mc-md) < .00001) {
		/* have a circular top */
		if (fabs(ma-mc) < .00001)
		    cgtype = GED_ANAL_RCC;
		else
		    cgtype = GED_ANAL_TRC;
	    }
	} else {
	    /* have an elliptical base */
	    if (fabs(ma-mc) < .00001 && fabs(mb-md) < .00001)
		cgtype = GED_ANAL_REC;
	}
    }

    switch (cgtype) {

	case GED_ANAL_RCC:
	    area_base = M_PI * ma * ma;
	    area_top = area_base;
	    area_side = 2.0 * M_PI * ma * mh;
	    vol = M_PI * ma * ma * mh;
	    bu_vls_printf(gedp->ged_result_str, "RCC ");
	    break;

	case GED_ANAL_TRC:
	    area_base = M_PI * ma * ma;
	    area_top = M_PI * mc * mc;
	    area_side = M_PI * (ma+mc) * sqrt((ma-mc)*(ma-mc)+(mh*mh));
	    vol = M_PI * mh * (ma*ma + mc*mc + ma*mc) / 3.0;
	    bu_vls_printf(gedp->ged_result_str, "TRC ");
	    break;

	case GED_ANAL_REC:
	    area_base = M_PI * ma * mb;
	    area_top = M_PI * mc * md;
	    /* approximate */
	    area_side = 2.0 * M_PI * mh * sqrt(0.5 * (ma*ma + mb*mb));
	    vol = M_PI * ma * mb * mh;
	    bu_vls_printf(gedp->ged_result_str, "REC ");
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "TGC Cannot find areas and volume\n");
	    return;
    }

    /* print the results */
    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  base(AxB)=%.8f  top(CxD)=%.8f  side=%.8f\n",
		  area_base*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_top*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_side*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (area_base+area_top+area_side)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local, vol/GALLONS_TO_MM3);
    /* Print units? */
    return;

}


/* analyze ars */
static void
analyze_ars(struct ged *gedp, const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    bu_vls_printf(gedp->ged_result_str, "ARS analyze not implemented\n");
}


/* XXX analyze spline needed
 * static void
 * analyze_spline(gedp->ged_result_str, ip)
 * struct bu_vls *vp;
 * const struct rt_db_internal *ip;
 * {
 * bu_vls_printf(gedp->ged_result_str, "SPLINE analyze not implemented\n");
 * }
 */

/* analyze particle */
static void
analyze_part(struct ged *gedp, const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    bu_vls_printf(gedp->ged_result_str, "PARTICLE analyze not implemented\n");
}


#define arcsinh(x) (log((x) + sqrt((x)*(x) + 1.)))

/* analyze rpc */
static void
analyze_rpc(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area_parab, area_body, b, h, r, vol_parab;
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;

    RT_RPC_CK_MAGIC(rpc);

    b = MAGNITUDE(rpc->rpc_B);
    h = MAGNITUDE(rpc->rpc_H);
    r = rpc->rpc_r;

    /* area of one parabolic side */
    area_parab = 4./3 * b*r;

    /* volume of rpc */
    vol_parab = area_parab*h;

    /* surface area of parabolic body */
    area_body = .5*sqrt(r*r + 4.*b*b) + .25*r*r/b*arcsinh(2.*b/r);
    area_body *= 2.;

    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  front(BxR)=%.8f  top(RxH)=%.8f  body=%.8f\n",
		  area_parab*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  2*r*h*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_body*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (2*area_parab+2*r*h+area_body)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_parab*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_parab/GALLONS_TO_MM3);
}


/* analyze rhc */
static void
analyze_rhc(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area_hyperb, area_body, b, c, h, r, vol_hyperb,	work1;
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)ip->idb_ptr;

    RT_RHC_CK_MAGIC(rhc);

    b = MAGNITUDE(rhc->rhc_B);
    h = MAGNITUDE(rhc->rhc_H);
    r = rhc->rhc_r;
    c = rhc->rhc_c;

    /* area of one hyperbolic side (from macsyma) WRONG!!!! */
    work1 = sqrt(b*(b + 2.*c));
    area_hyperb = -2.*r*work1*(.5*(b+c) + c*c*log(c/(work1 + b + c)));

    /* volume of rhc */
    vol_hyperb = area_hyperb*h;

    /* surface area of hyperbolic body */
    area_body=0.;
#if 0
    k = (b+c)*(b+c) - c*c;
#define X_eval(y) sqrt(1. + (4.*k)/(r*r*k*k*(y)*(y) + r*r*c*c))
#define L_eval(y) .5*k*(y)*X_eval(y) \
	+ r*k*(r*r*c*c + 4.*k - r*r*c*c/k)*arcsinh((y)*sqrt(k)/c)
    area_body = 2.*(L_eval(r) - L_eval(0.));
#endif

    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  front(BxR)=%.8f  top(RxH)=%.8f  body=%.8f\n",
		  area_hyperb*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  2*r*h*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_body*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (2*area_hyperb+2*r*h+2*area_body)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_hyperb*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_hyperb/GALLONS_TO_MM3);
}


/*
 * Analyze command - prints loads of info about a solid
 * Format:	analyze [name]
 * if 'name' is missing use solid being edited
 */

/* Analyze solid in internal form */
static void
analyze_do(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_ARB8:
	    analyze_arb(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_tgc(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_ell(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_tor(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_rpc(gedp, ip);
	    break;

	case ID_RHC:
	    analyze_rhc(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_part(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "analyze: unable to process %s solid\n",
			  rt_functab[ip->idb_type].ft_name);
	    break;
    }
}


int
ged_analyze(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *ndp;
    int i;
    struct rt_db_internal intern;
    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* use the names that were input */
    for (i = 1; i < argc; i++) {
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	_ged_do_list(gedp, ndp, 1);
	analyze_do(gedp, &intern);
	rt_db_free_internal(&intern);
    }

    return GED_OK;
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
