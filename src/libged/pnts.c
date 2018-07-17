/*                         P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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
/** @file libged/pnts.c
 *
 * pnts command for simple Point Set (pnts) primitive operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/sort.h"
#include "rt/geom.h"
#include "wdb.h"
#include "./ged_private.h"

void
_pnt_to_tri(point_t *p, vect_t *n, struct rt_bot_internal *bot_ip, fastf_t scale, unsigned long pntcnt)
{
    fastf_t ty1 =  0.57735026918962573 * scale; /* tan(PI/6) */
    fastf_t ty2 = -0.28867513459481287 * scale; /* 0.5 * tan(PI/6) */
    fastf_t tx1 = 0.5 * scale;
    point_t v1, v2, v3;
    vect_t n1;
    vect_t v1p, v2p, v3p = {0.0, 0.0, 0.0};
    vect_t v1f, v2f, v3f = {0.0, 0.0, 0.0};
    mat_t rot;
    struct bn_tol btol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };

    VSET(n1, 0, 0, 1);
    VSET(v1, 0, ty1, 0);
    VSET(v2, -1*tx1, ty2, 0);
    VSET(v3, tx1, ty2, 0);

    VMOVE(v1p, v1);
    VMOVE(v2p, v2);
    VMOVE(v3p, v3);
    bn_mat_fromto(rot, n1, *n, &btol);
    MAT4X3VEC(v1f, rot, v1p);
    MAT4X3VEC(v2f, rot, v2p);
    MAT4X3VEC(v3f, rot, v3p);
    VADD2(v1p, v1f, *p);
    VADD2(v2p, v2f, *p);
    VADD2(v3p, v3f, *p);
    VMOVE(&bot_ip->vertices[pntcnt*3*3], v1p);
    VMOVE(&bot_ip->vertices[pntcnt*3*3+3], v2p);
    VMOVE(&bot_ip->vertices[pntcnt*3*3+6], v3p);
    bot_ip->faces[pntcnt*3] = pntcnt*3;
    bot_ip->faces[pntcnt*3+1] = pntcnt*3+1;
    bot_ip->faces[pntcnt*3+2] = pntcnt*3+2;
}

int
_pnts_to_bot(struct ged *gedp, struct rt_pnts_internal *pnts, const char *bot_name, fastf_t uscale)
{
    int have_normals = 1;
    unsigned long pntcnt = 0;
    struct rt_db_internal internal;
    struct rt_bot_internal *bot_ip;
    struct directory *dp;
    fastf_t scale = uscale;

    /* Sanity */
    if (!bot_name || !pnts || !gedp) return GED_ERROR;
    if (db_lookup(gedp->ged_wdbp->dbip, bot_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists!\n", bot_name);
	return GED_ERROR;
    }
    RT_PNTS_CK_MAGIC(pnts);

    /* For the moment, only generate BoTs when we have a normal to guide us.  Eventually,
     * we might add logic to find the avg center point and calculate normals radiating out
     * from that center, but for now skip anything that doesn't provide normals up front. */
    if (pnts->type == RT_PNT_TYPE_PNT) have_normals = 0;
    if (pnts->type == RT_PNT_TYPE_COL) have_normals = 0;
    if (pnts->type == RT_PNT_TYPE_SCA) have_normals = 0;
    if (pnts->type == RT_PNT_TYPE_COL_SCA) have_normals = 0;
    if (!have_normals) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point cloud data does not define normals\n");
	return GED_ERROR;
    }

    /* initialize */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (NEAR_ZERO(scale, SMALL_FASTF)) {
	switch (pnts->type) {
	    case RT_PNT_TYPE_SCA_NRM:
		scale = pnts->scale;
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		scale = pnts->scale;
		break;
	    default:
		scale = 1.0;
		break;
	}
    }

    /* Set up BoT container */
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(bot_ip, struct rt_bot_internal);
    internal.idb_ptr = (void *)bot_ip;
    bot_ip = (struct rt_bot_internal *)internal.idb_ptr;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->mode = RT_BOT_SURFACE;
    bot_ip->orientation = 2;

    /* Allocate BoT memory */
    bot_ip->num_vertices = pnts->count * 3;
    bot_ip->num_faces = pnts->count;
    bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3 + 3, sizeof(int), "bot faces");
    bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3 + 3, sizeof(fastf_t), "bot vertices");
    bot_ip->thickness = (fastf_t *)NULL;
    bot_ip->face_mode = (struct bu_bitv *)NULL;

    pntcnt = 0;
    if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    _pnt_to_tri(&(pn->v), &(pn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	struct pnt_color_normal *pcn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pcn, pnt_color_normal, &(pl->l))) {
	    _pnt_to_tri(&(pcn->v), &(pcn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *psn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(psn, pnt_scale_normal, &(pl->l))) {
	    _pnt_to_tri(&(psn->v), &(psn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	struct pnt_color_scale_normal *pcsn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pcsn, pnt_color_scale_normal, &(pl->l))) {
	    _pnt_to_tri(&(pcsn->v), &(pcsn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }

    GED_DB_DIRADD(gedp, dp, bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated BoT object %s with %d triangles", bot_name, pntcnt);

    return GED_OK;
}



HIDDEN void
_pnts_show_help(struct ged *gedp, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    const char *option_help;

    bu_vls_sprintf(&str, "Usage: pnts [options] [subcommand] [subcommand arguments]\n\n");

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free((char *)option_help, "help str");
    }
    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  get [-p][-n][-r radius] [-k px py pz] (pnt_ind|pnt_ind_min-pnt_ind_max) <pnts> [new_pnts_obj]\n");
    bu_vls_printf(&str, "    - Get specific subset (1 or more) points from a point set and\n");
    bu_vls_printf(&str, "      print information. (todo - document subcmd options...)\n\n");
    bu_vls_printf(&str, "  add <pnts> x y z [nx ny nz]\n");
    bu_vls_printf(&str, "    - Add the specified point to the point set\n\n");
    bu_vls_printf(&str, "  rm  [-r radius]  [-k px py pz] (pnt_ind|pnt_ind_min-pnt_ind_max) <pnts>\n");
    bu_vls_printf(&str, "    - Remove 1 or more points from a point set\n\n");
    bu_vls_printf(&str, "  tri [-s scale] <pnts> <output_bot>\n");
    bu_vls_printf(&str, "    - Generate unit or scaled triangles for each pnt in a point set. If no normal\n");
    bu_vls_printf(&str, "      information is present, use origin at avg of all set points to make normals.\n\n");
    bu_vls_printf(&str, "  chull <pnts> <output_bot>\n");
    bu_vls_printf(&str, "    - Store the convex hull of the point set in <output_bot>.\n\n");

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}

#if 0
HIDDEN int
pnts_get(struct ged *gedp, int argc, const char *argv[], struct bu_opt_desc *d, struct rt_db_internal *ip)
{
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    return GED_OK;
}

HIDDEN int
pnts_add(struct ged *gedp, int argc, const char *argv[], struct bu_opt_desc *d, struct rt_db_internal *ip)
{
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    return GED_OK;
}

HIDDEN int
pnts_rm(struct ged *gedp, int argc, const char *argv[], struct bu_opt_desc *d, struct rt_db_internal *ip)
{
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    return GED_OK;
}

HIDDEN int
pnts_chull(struct ged *gedp, int argc, const char *argv[], struct bu_opt_desc *d, struct rt_db_internal *ip)
{
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)ip->idb_ptr;
    return GED_OK;
}

#endif

int
ged_pnts(struct ged *gedp, int argc, const char *argv[])
{
   struct directory *pnt_dp;
    struct rt_db_internal intern;
    struct rt_pnts_internal *pnt;
    const char *cmd = argv[0];
    const char *sub = NULL;
    const char *pnt_prim = NULL;
    const char *output_obj = NULL;
    size_t len;
    int i;
    int print_help = 0;
    int opt_ret = 0;
    int opt_argc;
    struct bu_opt_desc d[2];
    const char * const pnt_subcommands[] = {"tri",NULL};
    BU_OPT(d[0], "h", "help",      "", NULL, &print_help,        "Print help and exit");
    BU_OPT_NULL(d[1]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	_pnts_show_help(gedp, d);
	return GED_ERROR;
    }

    /* See if we have any options to deal with.  Once we hit a subcommand, we're done */
    opt_argc = argc;
    for (i = 1; i < argc; ++i) {
	const char * const *subcmd = pnt_subcommands;

	for (; *subcmd != NULL; ++subcmd) {
	    if (BU_STR_EQUAL(argv[i], *subcmd)) {
		opt_argc = i;
		i = argc;
		break;
	    }
	}
    }

    if (opt_argc >= argc) {
	/* no subcommand given */
	_pnts_show_help(gedp, d);
	return GED_ERROR;
    }

    if (opt_argc > 1) {
	/* parse standard options */
	opt_ret = bu_opt_parse(NULL, opt_argc, argv, d);
	if (opt_ret < 0) _pnts_show_help(gedp, d);
    }

    /* shift past standard options to subcommand args */
    argc -= opt_argc;
    argv = &argv[opt_argc];

    if (argc < 2) {
	_pnts_show_help(gedp, d);
	return GED_ERROR;
    }

    /* determine subcommand */
    sub = argv[0];
    len = strlen(sub);
    if (bu_strncmp(sub, "tri", len) == 0) {
	output_obj = argv[argc - 1];
	pnt_prim = argv[argc - 2];
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
	return GED_ERROR;
    }

    /* get pnt */
    GED_DB_LOOKUP(gedp, pnt_dp, pnt_prim, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, pnt_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a pnts object!", cmd, pnt_prim);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    pnt = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnt);

    if (bu_strncmp(sub, "tri", len) == 0) {
	int retval = 0;

	/* must be wanting help */
	if (argc < 3) {
	    _pnts_show_help(gedp, d);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	retval = _pnts_to_bot(gedp, pnt, output_obj, 1.0);

	if (retval != GED_OK) {
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }

    rt_db_free_internal(&intern);
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
