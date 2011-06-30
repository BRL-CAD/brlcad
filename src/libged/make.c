/*                         M A K E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/make.c
 *
 * The make command.
 *
 */

#include "common.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "rtgeom.h"
#include "wdb.h"

#include "./ged_private.h"


int
ged_make(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    int k;
    int save_bu_optind;
    struct directory *dp;
    fastf_t scale = 1;
    point_t origin = {0.0, 0.0, 0.0};
    struct rt_db_internal internal;
    struct rt_arb_internal *arb_ip;
    struct rt_ars_internal *ars_ip;
    struct rt_tgc_internal *tgc_ip;
    struct rt_ell_internal *ell_ip;
    struct rt_tor_internal *tor_ip;
    struct rt_grip_internal *grp_ip;
    struct rt_half_internal *half_ip;
    struct rt_rpc_internal *rpc_ip;
    struct rt_rhc_internal *rhc_ip;
    struct rt_epa_internal *epa_ip;
    struct rt_ehy_internal *ehy_ip;
    struct rt_eto_internal *eto_ip;
    struct rt_part_internal *part_ip;
    struct rt_pipe_internal *pipe_ip;
    struct rt_sketch_internal *sketch_ip;
    struct rt_extrude_internal *extrude_ip;
    struct rt_bot_internal *bot_ip;
    struct rt_arbn_internal *arbn_ip;
    struct rt_superell_internal *superell_ip;
    struct rt_metaball_internal *metaball_ip;
    struct rt_pnts_internal *pnts_ip;
    static const char *usage = "-h | -t | -o origin -s sf name <arb8|arb7|arb6|arb5|arb4|arbn|ars|bot|ehy|ell|ell1|epa|eto|extrude|grip|half|hyp|nmg|part|pipe|pnts|rcc|rec|rhc|rpc|rpp|sketch|sph|tec|tgc|tor|trc>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;

    /* Process arguments */
    while ((k = bu_getopt(argc, (char * const *)argv, "hHo:O:s:S:tT")) != -1) {
	switch (k) {
	    case 'o':
	    case 'O':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &origin[X],
			   &origin[Y],
			   &origin[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		}
		break;
	    case 's':
	    case 'S':
		if (sscanf(bu_optarg, "%lf", &scale) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return GED_ERROR;
		}
		break;
	    case 't':
	    case 'T':
		if (argc == 2) {
		    bu_vls_printf(gedp->ged_result_str, "arb8 arb7 arb6 arb5 arb4 arbn ars bot ehy ell ell1 epa eto extrude grip half hyp nmg part pipe pnts rcc rec rhc rpc rpp sketch sph tec tgc tor trc superell metaball");
		    return GED_HELP;
		}

		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	    case 'h':
	    case 'H':
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_HELP;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    argc -= bu_optind;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    save_bu_optind = bu_optind;

    GED_CHECK_EXISTS(gedp, argv[bu_optind], LOOKUP_QUIET, GED_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    if (BU_STR_EQUAL(argv[bu_optind+1], "arb8") ||
	BU_STR_EQUAL(argv[bu_optind+1],  "rpp")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARB8;
	internal.idb_meth = &rt_functab[ID_ARB8];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
	arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
	arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
	VSET(arb_ip->pt[0] ,
	     origin[X] + 0.5*scale,
	     origin[Y] - 0.5*scale,
	     origin[Z] - 0.5*scale);
	for (i=1; i<8; i++)
	    VMOVE(arb_ip->pt[i], arb_ip->pt[0]);
	arb_ip->pt[1][Y] += scale;
	arb_ip->pt[2][Y] += scale;
	arb_ip->pt[2][Z] += scale;
	arb_ip->pt[3][Z] += scale;
	for (i=4; i<8; i++)
	    arb_ip->pt[i][X] -= scale;
	arb_ip->pt[5][Y] += scale;
	arb_ip->pt[6][Y] += scale;
	arb_ip->pt[6][Z] += scale;
	arb_ip->pt[7][Z] += scale;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "arb7")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARB8;
	internal.idb_meth = &rt_functab[ID_ARB8];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
	arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
	arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
	VSET(arb_ip->pt[0] ,
	     origin[X] + 0.5*scale,
	     origin[Y] - 0.5*scale,
	     origin[Z] - 0.25*scale);
	for (i=1; i<8; i++)
	    VMOVE(arb_ip->pt[i], arb_ip->pt[0]);
	arb_ip->pt[1][Y] += scale;
	arb_ip->pt[2][Y] += scale;
	arb_ip->pt[2][Z] += scale;
	arb_ip->pt[3][Z] += 0.5*scale;
	for (i=4; i<8; i++)
	    arb_ip->pt[i][X] -= scale;
	arb_ip->pt[5][Y] += scale;
	arb_ip->pt[6][Y] += scale;
	arb_ip->pt[6][Z] += 0.5*scale;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "arb6")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARB8;
	internal.idb_meth = &rt_functab[ID_ARB8];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
	arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
	arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
	VSET(arb_ip->pt[0],
	     origin[X] + 0.5*scale,
	     origin[Y] - 0.5*scale,
	     origin[Z] - 0.5*scale);
	for (i=1; i<8; i++)
	    VMOVE(arb_ip->pt[i], arb_ip->pt[0]);
	arb_ip->pt[1][Y] += scale;
	arb_ip->pt[2][Y] += scale;
	arb_ip->pt[2][Z] += scale;
	arb_ip->pt[3][Z] += scale;
	for (i=4; i<8; i++)
	    arb_ip->pt[i][X] -= scale;
	arb_ip->pt[4][Y] += 0.5*scale;
	arb_ip->pt[5][Y] += 0.5*scale;
	arb_ip->pt[6][Y] += 0.5*scale;
	arb_ip->pt[6][Z] += scale;
	arb_ip->pt[7][Y] += 0.5*scale;
	arb_ip->pt[7][Z] += scale;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "arb5")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARB8;
	internal.idb_meth = &rt_functab[ID_ARB8];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
	arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
	arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
	VSET(arb_ip->pt[0] ,
	     origin[X] + 0.5*scale,
	     origin[Y] - 0.5*scale,
	     origin[Z] - 0.5*scale);
	for (i=1; i<8; i++)
	    VMOVE(arb_ip->pt[i], arb_ip->pt[0]);
	arb_ip->pt[1][Y] += scale;
	arb_ip->pt[2][Y] += scale;
	arb_ip->pt[2][Z] += scale;
	arb_ip->pt[3][Z] += scale;
	for (i=4; i<8; i++)
	{
	    arb_ip->pt[i][X] -= scale;
	    arb_ip->pt[i][Y] += 0.5*scale;
	    arb_ip->pt[i][Z] += 0.5*scale;
	}
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "arb4")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARB8;
	internal.idb_meth = &rt_functab[ID_ARB8];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
	arb_ip = (struct rt_arb_internal *)internal.idb_ptr;
	arb_ip->magic = RT_ARB_INTERNAL_MAGIC;
	VSET(arb_ip->pt[0] ,
	     origin[X] + 0.5*scale,
	     origin[Y] - 0.5*scale,
	     origin[Z] - 0.5*scale);
	for (i=1; i<8; i++)
	    VMOVE(arb_ip->pt[i], arb_ip->pt[0]);
	arb_ip->pt[1][Y] += scale;
	arb_ip->pt[2][Y] += scale;
	arb_ip->pt[2][Z] += scale;
	arb_ip->pt[3][Y] += scale;
	arb_ip->pt[3][Z] += scale;
	for (i=4; i<8; i++)
	{
	    arb_ip->pt[i][X] -= scale;
	    arb_ip->pt[i][Y] += scale;
	}
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "arbn")) {
	point_t view_center;

	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARBN;
	internal.idb_meth = &rt_functab[ID_ARBN];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arbn_internal), "rt_arbn_internal");
	arbn_ip = (struct rt_arbn_internal *)internal.idb_ptr;
	arbn_ip->magic = RT_ARBN_INTERNAL_MAGIC;
	arbn_ip->neqn = 8;
	arbn_ip->eqn = (plane_t *)bu_calloc(arbn_ip->neqn,
					    sizeof(plane_t), "arbn plane eqns");
	VSET(arbn_ip->eqn[0], 1, 0, 0);
	arbn_ip->eqn[0][W] = 0.5*scale;
	VSET(arbn_ip->eqn[1], -1, 0, 0);
	arbn_ip->eqn[1][W] = 0.5*scale;
	VSET(arbn_ip->eqn[2], 0, 1, 0);
	arbn_ip->eqn[2][W] = 0.5*scale;
	VSET(arbn_ip->eqn[3], 0, -1, 0);
	arbn_ip->eqn[3][W] = 0.5*scale;
	VSET(arbn_ip->eqn[4], 0, 0, 1);
	arbn_ip->eqn[4][W] = 0.5*scale;
	VSET(arbn_ip->eqn[5], 0, 0, -1);
	arbn_ip->eqn[5][W] = 0.5*scale;
	VSET(arbn_ip->eqn[6], 0.57735, 0.57735, 0.57735);
	arbn_ip->eqn[6][W] = 0.5*scale;
	VSET(arbn_ip->eqn[7], -0.57735, -0.57735, -0.57735);
	arbn_ip->eqn[7][W] = 0.5*scale;
	VSET(view_center,
	     origin[X],
	     origin[Y],
	     origin[Z]);
	for (i=0; i<arbn_ip->neqn; i++) {
	    arbn_ip->eqn[i][W] +=
		VDOT(view_center, arbn_ip->eqn[i]);
	}
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "ars")) {
	size_t curve;
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ARS;
	internal.idb_meth = &rt_functab[ID_ARS];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ars_internal), "rt_ars_internal");
	ars_ip = (struct rt_ars_internal *)internal.idb_ptr;
	ars_ip->magic = RT_ARS_INTERNAL_MAGIC;
	ars_ip->ncurves = 3;
	ars_ip->pts_per_curve = 3;
	ars_ip->curves = (fastf_t **)bu_malloc((ars_ip->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs");

	for (curve=0; curve < ars_ip->ncurves; curve++) {
	    ars_ip->curves[curve] = (fastf_t *)bu_calloc(
		(ars_ip->pts_per_curve + 1) * 3,
		sizeof(fastf_t), "ARS points");

	    if (curve == 0) {
		VSET(&(ars_ip->curves[0][0]),
		     origin[X],
		     origin[Y],
		     origin[Z]);
		VMOVE(&(ars_ip->curves[curve][3]), &(ars_ip->curves[curve][0]));
		VMOVE(&(ars_ip->curves[curve][6]), &(ars_ip->curves[curve][0]));
	    } else if (curve == (ars_ip->ncurves - 1)) {
		VSET(&(ars_ip->curves[curve][0]),
		     origin[X],
		     origin[Y],
		     origin[Z]+curve*0.5*scale);
		VMOVE(&(ars_ip->curves[curve][3]), &(ars_ip->curves[curve][0]));
		VMOVE(&(ars_ip->curves[curve][6]), &(ars_ip->curves[curve][0]));

	    } else {
		fastf_t x, y, z;
		x = origin[X]+curve*0.5*scale;
		y = origin[Y]+curve*0.5*scale;
		z = origin[Z]+curve*0.5*scale;

		VSET(&ars_ip->curves[curve][0],
		     origin[X],
		     origin[Y],
		     z);
		VSET(&ars_ip->curves[curve][3],
		     x,
		     origin[Y],
		     z);
		VSET(&ars_ip->curves[curve][6],
		     x, y, z);
	    }
	}

    } else if (BU_STR_EQUAL(argv[bu_optind+1], "sph")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ELL;
	internal.idb_meth = &rt_functab[ID_ELL];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ell_internal), "rt_ell_internal");
	ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
	ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell_ip->v, origin[X], origin[Y], origin[Z]);
	VSET(ell_ip->a, 0.5*scale, 0.0, 0.0);	/* A */
	VSET(ell_ip->b, 0.0, 0.5*scale, 0.0);	/* B */
	VSET(ell_ip->c, 0.0, 0.0, 0.5*scale);	/* C */
    } else if ((BU_STR_EQUAL(argv[bu_optind+1], "grp")) ||
	       (BU_STR_EQUAL(argv[bu_optind+1], "grip"))) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_GRIP;
	internal.idb_meth = &rt_functab[ID_GRIP];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_grip_internal), "rt_grp_internal");
	grp_ip = (struct rt_grip_internal *) internal.idb_ptr;
	grp_ip->magic = RT_GRIP_INTERNAL_MAGIC;
	VSET(grp_ip->center, origin[X], origin[Y],
	     origin[Z]);
	VSET(grp_ip->normal, 1.0, 0.0, 0.0);
	grp_ip->mag = 0.375*scale;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "ell1")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ELL;
	internal.idb_meth = &rt_functab[ID_ELL];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ell_internal), "rt_ell_internal");
	ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
	ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell_ip->v, origin[X], origin[Y], origin[Z]);
	VSET(ell_ip->a, 0.5*scale, 0.0, 0.0);	/* A */
	VSET(ell_ip->b, 0.0, 0.25*scale, 0.0);	/* B */
	VSET(ell_ip->c, 0.0, 0.0, 0.25*scale);	/* C */
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "ell")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ELL;
	internal.idb_meth = &rt_functab[ID_ELL];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ell_internal), "rt_ell_internal");
	ell_ip = (struct rt_ell_internal *)internal.idb_ptr;
	ell_ip->magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell_ip->v, origin[X], origin[Y], origin[Z]);
	VSET(ell_ip->a, 0.5*scale, 0.0, 0.0);		/* A */
	VSET(ell_ip->b, 0.0, 0.25*scale, 0.0);	/* B */
	VSET(ell_ip->c, 0.0, 0.0, 0.125*scale);	/* C */
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "tor")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TOR;
	internal.idb_meth = &rt_functab[ID_TOR];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tor_internal), "rt_tor_internal");
	tor_ip = (struct rt_tor_internal *)internal.idb_ptr;
	tor_ip->magic = RT_TOR_INTERNAL_MAGIC;
	VSET(tor_ip->v, origin[X], origin[Y], origin[Z]);
	VSET(tor_ip->h, 1.0, 0.0, 0.0);	/* unit normal */
	tor_ip->r_h = 0.1*scale;
	tor_ip->r_a = 0.4*scale;
	tor_ip->r_b = tor_ip->r_a;
	VSET(tor_ip->a, 0.0, 1, 0.0);
	VSET(tor_ip->b, 0.0, 0.0, 1);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "tgc")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TGC;
	internal.idb_meth = &rt_functab[ID_TGC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
	tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc_ip->v, origin[X], origin[Y], origin[Z]-0.5*scale);
	VSET(tgc_ip->h,  0.0, 0.0, scale);
	VSET(tgc_ip->a,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->b,  0.0, 0.125*scale, 0.0);
	VSET(tgc_ip->c,  0.125*scale, 0.0, 0.0);
	VSET(tgc_ip->d,  0.0, 0.25*scale, 0.0);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "tec")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TGC;
	internal.idb_meth = &rt_functab[ID_TGC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
	tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc_ip->v, origin[X], origin[Y], origin[Z]-0.5*scale);
	VSET(tgc_ip->h,  0.0, 0.0, scale);
	VSET(tgc_ip->a,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->b,  0.0, 0.125*scale, 0.0);
	VSET(tgc_ip->c,  0.125*scale, 0.0, 0.0);
	VSET(tgc_ip->d,  0.0, (0.0625*scale), 0.0);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "rec")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TGC;
	internal.idb_meth = &rt_functab[ID_TGC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
	tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc_ip->v, origin[X], origin[Y], origin[Z]- 0.5*scale);
	VSET(tgc_ip->h,  0.0, 0.0, scale);
	VSET(tgc_ip->a,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->b,  0.0, 0.125*scale, 0.0);
	VSET(tgc_ip->c,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->d,  0.0, 0.125*scale, 0.0);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "trc")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TGC;
	internal.idb_meth = &rt_functab[ID_TGC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
	tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc_ip->v, origin[X], origin[Y], origin[Z]-0.5*scale);
	VSET(tgc_ip->h,  0.0, 0.0, scale);
	VSET(tgc_ip->a,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->b,  0.0, 0.25*scale, 0.0);
	VSET(tgc_ip->c,  0.125*scale, 0.0, 0.0);
	VSET(tgc_ip->d,  0.0, 0.125*scale, 0.0);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "rcc")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_TGC;
	internal.idb_meth = &rt_functab[ID_TGC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_tgc_internal), "rt_tgc_internal");
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;
	tgc_ip->magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc_ip->v, origin[X], origin[Y], origin[Z]- 0.5*scale);
	VSET(tgc_ip->h,  0.0, 0.0, scale);
	VSET(tgc_ip->a,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->b,  0.0, 0.25*scale, 0.0);
	VSET(tgc_ip->c,  0.25*scale, 0.0, 0.0);
	VSET(tgc_ip->d,  0.0, 0.25*scale, 0.0);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "half")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_HALF;
	internal.idb_meth = &rt_functab[ID_HALF];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_half_internal), "rt_half_internal");
	half_ip = (struct rt_half_internal *)internal.idb_ptr;
	half_ip->magic = RT_HALF_INTERNAL_MAGIC;
	VSET(half_ip->eqn, 0.0, 0.0, 1.0);
	half_ip->eqn[W] = (origin[Z]);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "rpc")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_RPC;
	internal.idb_meth = &rt_functab[ID_RPC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_rpc_internal), "rt_rpc_internal");
	rpc_ip = (struct rt_rpc_internal *)internal.idb_ptr;
	rpc_ip->rpc_magic = RT_RPC_INTERNAL_MAGIC;
	VSET(rpc_ip->rpc_V, origin[X], origin[Y]-scale*0.25, origin[Z]-scale*0.5);
	VSET(rpc_ip->rpc_H, 0.0, 0.0, scale);
	VSET(rpc_ip->rpc_B, 0.0, (scale*0.5), 0.0);
	rpc_ip->rpc_r = scale*0.25;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "rhc")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_RHC;
	internal.idb_meth = &rt_functab[ID_RHC];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_rhc_internal), "rt_rhc_internal");
	rhc_ip = (struct rt_rhc_internal *)internal.idb_ptr;
	rhc_ip->rhc_magic = RT_RHC_INTERNAL_MAGIC;
	VSET(rhc_ip->rhc_V, origin[X], origin[Y]-0.25*scale, origin[Z]-0.25*scale);
	VSET(rhc_ip->rhc_H, 0.0, 0.0, 0.5*scale);
	VSET(rhc_ip->rhc_B, 0.0, 0.5*scale, 0.0);
	rhc_ip->rhc_r = scale*0.25;
	rhc_ip->rhc_c = scale*0.10;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "epa")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_EPA;
	internal.idb_meth = &rt_functab[ID_EPA];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_epa_internal), "rt_epa_internal");
	epa_ip = (struct rt_epa_internal *)internal.idb_ptr;
	epa_ip->epa_magic = RT_EPA_INTERNAL_MAGIC;
	VSET(epa_ip->epa_V, origin[X], origin[Y], origin[Z]-scale*0.5);
	VSET(epa_ip->epa_H, 0.0, 0.0, scale);
	VSET(epa_ip->epa_Au, 0.0, 1.0, 0.0);
	epa_ip->epa_r1 = scale*0.5;
	epa_ip->epa_r2 = scale*0.25;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "ehy")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_EHY;
	internal.idb_meth = &rt_functab[ID_EHY];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_ehy_internal), "rt_ehy_internal");
	ehy_ip = (struct rt_ehy_internal *)internal.idb_ptr;
	ehy_ip->ehy_magic = RT_EHY_INTERNAL_MAGIC;
	VSET(ehy_ip->ehy_V, origin[X], origin[Y], origin[Z]-scale*0.5);
	VSET(ehy_ip->ehy_H, 0.0, 0.0, scale);
	VSET(ehy_ip->ehy_Au, 0.0, 1.0, 0.0);
	ehy_ip->ehy_r1 = scale*0.5;
	ehy_ip->ehy_r2 = scale*0.25;
	ehy_ip->ehy_c = ehy_ip->ehy_r2;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "eto")) {
	fastf_t mag;
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_ETO;
	internal.idb_meth = &rt_functab[ID_ETO];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_eto_internal), "rt_eto_internal");
	eto_ip = (struct rt_eto_internal *)internal.idb_ptr;
	eto_ip->eto_magic = RT_ETO_INTERNAL_MAGIC;
	VSET(eto_ip->eto_V, origin[X], origin[Y], origin[Z]);
	VSET(eto_ip->eto_N, 0.0, 0.0, 1.0);
	VSET(eto_ip->eto_C, scale*0.1, 0.0, scale*0.1);
	mag = MAGNITUDE(eto_ip->eto_C);
	/* Close enough for now.*/
	eto_ip->eto_r = scale*0.5 - mag*cos(M_PI_4);
	eto_ip->eto_rd = scale*0.05;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "hyp")) {
	vect_t vertex, height, vectA;
	VSET(vertex, origin[X], origin[Y], origin[Z] - scale*0.5);
	VSET(height, 0.0, 0.0, scale);
	VSET(vectA, 0.0, scale*0.5, 0.0);
	return mk_hyp(gedp->ged_wdbp, argv[save_bu_optind], vertex, height, vectA, scale*0.25, 0.4);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "part")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_PARTICLE;
	internal.idb_meth = &rt_functab[ID_PARTICLE];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_part_internal), "rt_part_internal");
	part_ip = (struct rt_part_internal *)internal.idb_ptr;
	part_ip->part_magic = RT_PART_INTERNAL_MAGIC;
	VSET(part_ip->part_V, origin[X], origin[Y], origin[Z]-scale*0.25);
	VSET(part_ip->part_H, 0.0, 0.0, 0.5*scale);
	part_ip->part_vrad = scale*0.25;
	part_ip->part_hrad = scale*0.125;
	part_ip->part_type = RT_PARTICLE_TYPE_CONE;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "nmg")) {
	struct model *m;
	struct nmgregion *r;
	struct shell *s;

	m = nmg_mm();
	r = nmg_mrsv(m);
	s = BU_LIST_FIRST(shell, &r->s_hd);
	nmg_vertex_g(s->vu_p->v_p, origin[X], origin[Y], origin[Z]);
	(void)nmg_meonvu(s->vu_p);
	(void)nmg_ml(s);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_NMG;
	internal.idb_meth = &rt_functab[ID_NMG];
	internal.idb_ptr = (genptr_t)m;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "pipe")) {
	struct wdb_pipept *ps;

	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_PIPE;
	internal.idb_meth = &rt_functab[ID_PIPE];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_pipe_internal), "rt_pipe_internal");
	pipe_ip = (struct rt_pipe_internal *)internal.idb_ptr;
	pipe_ip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	BU_LIST_INIT(&pipe_ip->pipe_segs_head);
	BU_GETSTRUCT(ps, wdb_pipept);
	ps->l.magic = WDB_PIPESEG_MAGIC;
	VSET(ps->pp_coord, origin[X], origin[Y], origin[Z]-0.5*scale);
	ps->pp_od = 0.25*scale;
	ps->pp_id = 0.25*ps->pp_od;
	ps->pp_bendradius = ps->pp_od;
	BU_LIST_INSERT(&pipe_ip->pipe_segs_head, &ps->l);
	BU_GETSTRUCT(ps, wdb_pipept);
	ps->l.magic = WDB_PIPESEG_MAGIC;
	VSET(ps->pp_coord, origin[X], origin[Y], origin[Z]+0.5*scale);
	ps->pp_od = 0.25*scale;
	ps->pp_id = 0.25*ps->pp_od;
	ps->pp_bendradius = ps->pp_od;
	BU_LIST_INSERT(&pipe_ip->pipe_segs_head, &ps->l);
    } else if (BU_STR_EQUAL(argv[bu_optind + 1], "pnts")) {
	struct pnt *point;
	struct pnt *headPoint;

	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_PNTS;
	internal.idb_meth = &rt_functab[ID_PNTS];
	internal.idb_ptr = (genptr_t) bu_malloc(sizeof(struct rt_pnts_internal), "rt_pnts_internal");

	pnts_ip = (struct rt_pnts_internal *) internal.idb_ptr;
	pnts_ip->magic = RT_PNTS_INTERNAL_MAGIC;
	pnts_ip->count = 1;
	pnts_ip->type = RT_PNT_TYPE_PNT;
	pnts_ip->scale = 0;

	BU_GETSTRUCT(pnts_ip->point, pnt);
	headPoint = pnts_ip->point;
	BU_LIST_INIT(&headPoint->l);
	BU_GETSTRUCT(point, pnt);
	VSET(point->v, origin[X], origin[Y], origin[Z]);
	BU_LIST_PUSH(&headPoint->l, &point->l);

    } else if (BU_STR_EQUAL(argv[bu_optind+1], "bot")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_BOT;
	internal.idb_meth = &rt_functab[ID_BOT];
	BU_GETSTRUCT(bot_ip, rt_bot_internal);
	internal.idb_ptr = (genptr_t)bot_ip;
	bot_ip = (struct rt_bot_internal *)internal.idb_ptr;
	bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
	bot_ip->mode = RT_BOT_SOLID;
	bot_ip->orientation = RT_BOT_UNORIENTED;
	bot_ip->num_vertices = 4;
	bot_ip->num_faces = 4;
	bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3, sizeof(int), "BOT faces");
	bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3, sizeof(fastf_t), "BOT vertices");
	bot_ip->thickness = (fastf_t *)NULL;
	bot_ip->face_mode = (struct bu_bitv *)NULL;
	VSET(&bot_ip->vertices[0],  origin[X], origin[Y], origin[Z]);
	VSET(&bot_ip->vertices[3], origin[X]-0.5*scale, origin[Y]+0.5*scale, origin[Z]-scale);
	VSET(&bot_ip->vertices[6], origin[X]-0.5*scale, origin[Y]-0.5*scale, origin[Z]-scale);
	VSET(&bot_ip->vertices[9], origin[X]+0.5*scale, origin[Y]+0.5*scale, origin[Z]-scale);
	VSET(&bot_ip->faces[0], 0, 1, 3);
	VSET(&bot_ip->faces[3], 0, 1, 2);
	VSET(&bot_ip->faces[6], 0, 2, 3);
	VSET(&bot_ip->faces[9], 1, 2, 3);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "extrude")) {
	char *av[8];
	char center_str[512];
	char scale_str[128];

	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_EXTRUDE;
	internal.idb_meth = &rt_functab[ID_EXTRUDE];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_extrude_internal), "rt_extrude_internal");
	extrude_ip = (struct rt_extrude_internal *)internal.idb_ptr;
	extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;
	VSET(extrude_ip->V, origin[X], origin[Y], origin[Z]);
	VSET(extrude_ip->h, 0.0, 0.0, scale);
	VSET(extrude_ip->u_vec, 1.0, 0.0, 0.0);
	VSET(extrude_ip->v_vec, 0.0, 1.0, 0.0);
	extrude_ip->keypoint = 0;
	av[0] = "make_name";
	av[1] = "skt_";
	ged_make_name(gedp, 2, (const char **)av);
	extrude_ip->sketch_name = bu_strdup(bu_vls_addr(gedp->ged_result_str));
	extrude_ip->skt = (struct rt_sketch_internal *)NULL;

	sprintf(center_str, "%f %f %f", V3ARGS(origin));
	sprintf(scale_str, "%f", scale);
	av[0] = "make";
	av[1] = "-o";
	av[2] = center_str;
	av[3] = "-s";
	av[4] = scale_str;
	av[5] = extrude_ip->sketch_name;
	av[6] = "sketch";
	av[7] = (char *)0;
	ged_make(gedp, 7, (const char **)av);
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "sketch")) {
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_SKETCH;
	internal.idb_meth = &rt_functab[ID_SKETCH];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_sketch_internal), "rt_sketch_internal");
	sketch_ip = (struct rt_sketch_internal *)internal.idb_ptr;
	sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;
	VSET(sketch_ip->u_vec, 1.0, 0.0, 0.0);
	VSET(sketch_ip->v_vec, 0.0, 1.0, 0.0);
	VSET(sketch_ip->V, origin[X], origin[Y], origin[Z]);
#if 0
	/* XXX this creates a "default" sketch object -- this
	   code should probably be made optional somewhere
	   else now? */
	sketch_ip->vert_count = 7;
	sketch_ip->verts = (point2d_t *)bu_calloc(sketch_ip->vert_count, sizeof(point2d_t), "sketch_ip->verts");
	sketch_ip->verts[0][0] = 0.25*scale;
	sketch_ip->verts[0][1] = 0.0;
	sketch_ip->verts[1][0] = 0.5*scale;
	sketch_ip->verts[1][1] = 0.0;
	sketch_ip->verts[2][0] = 0.5*scale;
	sketch_ip->verts[2][1] = 0.5*scale;
	sketch_ip->verts[3][0] = 0.0;
	sketch_ip->verts[3][1] = 0.5*scale;
	sketch_ip->verts[4][0] = 0.0;
	sketch_ip->verts[4][1] = 0.25*scale;
	sketch_ip->verts[5][0] = 0.25*scale;
	sketch_ip->verts[5][1] = 0.25*scale;
	sketch_ip->verts[6][0] = 0.125*scale;
	sketch_ip->verts[6][1] = 0.125*scale;
	sketch_ip->skt_curve.seg_count = 6;
	sketch_ip->skt_curve.reverse = (int *)bu_calloc(sketch_ip->skt_curve.seg_count, sizeof(int), "sketch_ip->skt_curve.reverse");
	sketch_ip->skt_curve.segments = (genptr_t *)bu_calloc(sketch_ip->skt_curve.seg_count, sizeof(genptr_t), "sketch_ip->skt_curve.segments");

	csg = (struct carc_seg *)bu_calloc(1, sizeof(struct carc_seg), "segments");
	sketch_ip->skt_curve.segments[0] = (genptr_t)csg;
	csg->magic = CURVE_CARC_MAGIC;
	csg->start = 4;
	csg->end = 0;
	csg->radius = 0.25*scale;
	csg->center_is_left = 1;
	csg->orientation = 0;

	lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segments");
	sketch_ip->skt_curve.segments[1] = (genptr_t)lsg;
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 0;
	lsg->end = 1;

	lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segments");
	sketch_ip->skt_curve.segments[2] = (genptr_t)lsg;
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 1;
	lsg->end = 2;

	lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segments");
	sketch_ip->skt_curve.segments[3] = (genptr_t)lsg;
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 2;
	lsg->end = 3;

	lsg = (struct line_seg *)bu_calloc(1, sizeof(struct line_seg), "segments");
	sketch_ip->skt_curve.segments[4] = (genptr_t)lsg;
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 3;
	lsg->end = 4;

	csg = (struct carc_seg *)bu_calloc(1, sizeof(struct carc_seg), "segments");
	sketch_ip->skt_curve.segments[5] = (genptr_t)csg;
	csg->magic = CURVE_CARC_MAGIC;
	csg->start = 6;
	csg->end = 5;
	csg->radius = -1.0;
	csg->center_is_left = 1;
	csg->orientation = 0;
#else
	sketch_ip->vert_count = 0;
	sketch_ip->verts = (point2d_t *)NULL;
	sketch_ip->skt_curve.seg_count = 0;
	sketch_ip->skt_curve.reverse = (int *)NULL;
	sketch_ip->skt_curve.segments = (genptr_t *)NULL;
#endif
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "superell")) {


	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_SUPERELL;
	internal.idb_meth = &rt_functab[ID_SUPERELL];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_superell_internal), "rt_superell_internal");
	superell_ip = (struct rt_superell_internal *)internal.idb_ptr;
	superell_ip->magic = RT_SUPERELL_INTERNAL_MAGIC;
	VSET(superell_ip->v, origin[X], origin[Y], origin[Z]);
	VSET(superell_ip->a, 0.5*scale, 0.0, 0.0);	/* A */
	VSET(superell_ip->b, 0.0, 0.25*scale, 0.0);	/* B */
	VSET(superell_ip->c, 0.0, 0.0, 0.125*scale);	/* C */
	superell_ip->n = 1.0;
	superell_ip->e = 1.0;
	fprintf(stdout, "superell being made with %f and %f\n", superell_ip->n, superell_ip->e);

    } else if (BU_STR_EQUAL(argv[bu_optind+1], "hf")) {
	bu_vls_printf(gedp->ged_result_str, "make: the height field is deprecated and not supported by this command.\nUse the dsp primitive.\n");
	return GED_ERROR;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "pg") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "poly")) {
	bu_vls_printf(gedp->ged_result_str, "make: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.");
	return GED_ERROR;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "cline") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "dsp") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "ebm") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "nurb") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "spline") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "submodel") ||
	       BU_STR_EQUAL(argv[bu_optind+1], "vol")) {
	bu_vls_printf(gedp->ged_result_str, "make: the %s primitive is not supported by this command", argv[bu_optind+1]);
	return GED_ERROR;
    } else if (BU_STR_EQUAL(argv[bu_optind+1], "metaball")) {
	struct wdb_metaballpt *mbpt;
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_METABALL;
	internal.idb_meth = &rt_functab[ID_METABALL];
	internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_internal");
	metaball_ip = (struct rt_metaball_internal *)internal.idb_ptr;
	metaball_ip->magic = RT_METABALL_INTERNAL_MAGIC;
	metaball_ip->threshold = 1.0;
	metaball_ip->method = 1;
	BU_LIST_INIT(&metaball_ip->metaball_ctrl_head);

	mbpt = (struct wdb_metaballpt *)malloc(sizeof(struct wdb_metaballpt));
	mbpt->fldstr = 1.0;
	mbpt->sweat = 1.0;
	VSET(mbpt->coord, origin[X] - 1.0, origin[Y], origin[Z]);
	BU_LIST_INSERT(&metaball_ip->metaball_ctrl_head, &mbpt->l);

	mbpt = (struct wdb_metaballpt *)malloc(sizeof(struct wdb_metaballpt));
	mbpt->fldstr = 1.0;
	mbpt->sweat = 1.0;
	VSET(mbpt->coord, origin[X] + 1.0, origin[Y], origin[Z]);
	BU_LIST_INSERT(&metaball_ip->metaball_ctrl_head, &mbpt->l);

	bu_log("metaball being made with %f threshold and two points using the %s rendering method\n",
	       metaball_ip->threshold, rt_metaball_lookup_type_name(metaball_ip->method));
    } else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    GED_DB_DIRADD(gedp, dp, argv[save_bu_optind], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
