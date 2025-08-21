/*                         P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

extern "C" {
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/units.h"
#include "bu/cmd.h"
#include "rt/geom.h"
#include "wdb.h"
#include "analyze.h"
}
#include "../ged_private.h"
#include "../pnts_util.h"

struct _ged_pnts_info {
    struct ged *gedp;
    struct bu_opt_desc *gopts;
};

static int
_ged_pnts_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gpi->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gpi->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

/* =================== Common helpers =================== */

static int
_pnts_db_lookup_internal(struct ged *gedp, const char *obj_name, struct rt_db_internal *intern, int minor_type)
{
    struct directory *dp = NULL;
    GED_DB_LOOKUP(gedp, dp, obj_name, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, intern, dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

    if (intern->idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	(minor_type >= 0 && intern->idb_minor_type != minor_type)) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not the expected object type!", __func__, obj_name);
	rt_db_free_internal(intern);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

/* TODO - need some generic version of this logic in libbn -
 * used in libanalyze's NIRT as well */
static void
_pnts_fastf_t_to_vls(struct bu_vls *o, fastf_t d, int p)
{
    size_t prec = (p > 0) ? (size_t)p : std::numeric_limits<fastf_t>::max_digits10;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    bu_vls_sprintf(o, "%s", sd.c_str());
}

// Used for 'read' and internal logic to add a point from an array of number strings
static int
_pnts_read_point(struct rt_pnts_internal *pnts, int numcnt, const char **nums, const char *fmt, fastf_t conv_factor)
{
    int i = 0;
    char fc = '\0';
    void *point = _ged_pnts_new_pnt(pnts->type);
    for (i = 0; i < numcnt; i++) {
	fastf_t val;
	fc = fmt[i];
	char* num = (char*)nums[i];
	int j = strlen(nums[i]) - 1;
	while (j > -1) {
	    if (num[j] == ' ') j--;
	    else {
		num[j+1] = '\0';
		break;
	    }
	}
	if (bu_opt_fastf_t(NULL, 1, (const char **)&num, (void *)&val) == -1) return BRLCAD_ERROR;
	if ((fc == 'x') || (fc == 'y') || (fc == 'z')) {
	    _ged_pnt_v_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'i') || (fc == 'j') || (fc == 'k')) {
	    _ged_pnt_n_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'r') || (fc == 'g') || (fc == 'b')) {
	    _ged_pnt_c_set(point, pnts->type, fc, val / 255.0);
	    continue;
	}
	if (fc == 's') {
	    _ged_pnt_s_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
    }
    _ged_pnts_add(pnts, point);
    return BRLCAD_OK;
}

// Common logic for writing points in text format, used by 'write'
static int
_pnts_write_text(FILE *fp, struct rt_pnts_internal *pnts, int precis)
{
    struct bu_vls pnt_str = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_OK;

    if (pnts->type == RT_PNT_TYPE_PNT) {
	struct pnt *pn = NULL;
	struct pnt *pl = (struct pnt *)pnts->point;
	for (BU_LIST_FOR(pn, pnt, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s%c", bu_vls_addr(&pnt_str), (i == 2 ? '\n' : ' '));
	    }
	}
    } else if (pnts->type == RT_PNT_TYPE_COL) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color *pn = NULL;
	struct pnt_color *pl = (struct pnt_color *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		ret = BRLCAD_ERROR;
		break;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
    } else if (pnts->type == RT_PNT_TYPE_SCA) {
	struct pnt_scale *pn = NULL;
	struct pnt_scale *pl = (struct pnt_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	}
    } else if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s%c", bu_vls_addr(&pnt_str), (i == 2 ? '\n' : ' '));
	    }
	}
    } else if (pnts->type == RT_PNT_TYPE_COL_SCA) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale *pn = NULL;
	struct pnt_color_scale *pl = (struct pnt_color_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		ret = BRLCAD_ERROR;
		break;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
    } else if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_normal *pn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_normal, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		ret = BRLCAD_ERROR;
		break;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
    } else if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *pn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale_normal, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	}
    } else if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale_normal *pn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale_normal, &(pl->l))) {
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (int i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		ret = BRLCAD_ERROR;
		break;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
    } else {
	ret = BRLCAD_ERROR;
    }

    bu_vls_free(&pnt_str);
    return ret;
}

/* ================ Subcommand implementations ================ */

static int
_ged_pnts_cmd_tri(void *bs, int argc, const char **argv)
{
    const char *usage = "Usage: pnts tri [options] <pnts> <output_bot>\n\n";
    const char *purpose = "Generate unit or scaled triangles for each point in a set";
    if (_ged_pnts_cmd_msgs(bs, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    struct ged *gedp = gpi->gedp;

    int print_help = 0;
    int opt_ret = 0;
    fastf_t scale = 0.0;
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",      "",  NULL,            &print_help,   "Print help and exit");
    BU_OPT(d[1], "s", "scale",     "#", &bu_opt_fastf_t, &scale,        "Specify scale factor to apply to unit triangle - will scale the triangle size, with the triangle centered on the original point");
    BU_OPT_NULL(d[2]);

    argc-=(argc>0);
    argv+=(argc>0); // skip subcommand

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    argc = opt_ret;

    if (argc != 2) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    const char *pnt_prim = argv[0];
    const char *bot_name = argv[1];

    struct rt_db_internal intern, internal;
    if (_pnts_db_lookup_internal(gedp, pnt_prim, &intern, DB5_MINORTYPE_BRLCAD_PNTS) != BRLCAD_OK)
	return BRLCAD_ERROR;

    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (!bot_name || !gedp) return BRLCAD_ERROR;
    GED_CHECK_EXISTS(gedp, bot_name, LOOKUP_QUIET, BRLCAD_ERROR);

    int have_normals = 1;
    if (pnts->type == RT_PNT_TYPE_PNT || pnts->type == RT_PNT_TYPE_COL ||
	pnts->type == RT_PNT_TYPE_SCA || pnts->type == RT_PNT_TYPE_COL_SCA)
	have_normals = 0;
    if (!have_normals) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point cloud data does not define normals\n");
	return BRLCAD_ERROR;
    }

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (NEAR_ZERO(scale, SMALL_FASTF)) {
	switch (pnts->type) {
	    case RT_PNT_TYPE_SCA_NRM:
	    case RT_PNT_TYPE_COL_SCA_NRM:
		scale = pnts->scale;
		break;
	    default:
		scale = 1.0;
		break;
	}
    }

    struct rt_bot_internal *bot_ip;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(bot_ip, struct rt_bot_internal);
    internal.idb_ptr = (void *)bot_ip;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->mode = RT_BOT_SURFACE;
    bot_ip->orientation = 2;

    bot_ip->num_vertices = pnts->count * 3;
    bot_ip->num_faces = pnts->count;
    bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3 + 3, sizeof(int), "bot faces");
    bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3 + 3, sizeof(fastf_t), "bot vertices");
    bot_ip->thickness = (fastf_t *)NULL;
    bot_ip->face_mode = (struct bu_bitv *)NULL;

    // Helper for adding triangles
    auto pnt_to_tri = [](point_t *p, vect_t *n, struct rt_bot_internal *botp, fastf_t tscale, unsigned long pntcnt) {
	fastf_t ty1 =  0.57735026918962573 * tscale; // tan(PI/6)
	fastf_t ty2 = -0.28867513459481287 * tscale; // 0.5 * tan(PI/6)
	fastf_t tx1 = 0.5 * tscale;
	point_t v1, v2, v3;
	vect_t n1;
	vect_t v1pp, v2pp, v3pp = {0.0, 0.0, 0.0};
	vect_t v1fp, v2fp, v3fp = {0.0, 0.0, 0.0};
	mat_t rot;
	struct bn_tol btol = BN_TOL_INIT_TOL;

	VSET(n1, 0, 0, 1);
	VSET(v1, 0, ty1, 0);
	VSET(v2, -1*tx1, ty2, 0);
	VSET(v3, tx1, ty2, 0);

	VMOVE(v1pp, v1);
	VMOVE(v2pp, v2);
	VMOVE(v3pp, v3);
	bn_mat_fromto(rot, n1, *n, &btol);
	MAT4X3VEC(v1fp, rot, v1pp);
	MAT4X3VEC(v2fp, rot, v2pp);
	MAT4X3VEC(v3fp, rot, v3pp);
	VADD2(v1pp, v1fp, *p);
	VADD2(v2pp, v2fp, *p);
	VADD2(v3pp, v3fp, *p);
	VMOVE(&botp->vertices[pntcnt*3*3], v1pp);
	VMOVE(&botp->vertices[pntcnt*3*3+3], v2pp);
	VMOVE(&botp->vertices[pntcnt*3*3+6], v3pp);
	botp->faces[pntcnt*3] = pntcnt*3;
	botp->faces[pntcnt*3+1] = pntcnt*3+1;
	botp->faces[pntcnt*3+2] = pntcnt*3+2;
    };

    unsigned long pntcnt = 0;
    if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    pnt_to_tri(&(pn->v), &(pn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	struct pnt_color_normal *pcn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pcn, pnt_color_normal, &(pl->l))) {
	    pnt_to_tri(&(pcn->v), &(pcn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *psn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(psn, pnt_scale_normal, &(pl->l))) {
	    pnt_to_tri(&(psn->v), &(psn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	struct pnt_color_scale_normal *pcsn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pcsn, pnt_color_scale_normal, &(pl->l))) {
	    pnt_to_tri(&(pcsn->v), &(pcsn->n), bot_ip, scale, pntcnt);
	    pntcnt++;
	}
    }

    struct directory *dp;
    GED_DB_DIRADD(gedp, dp, bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, BRLCAD_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated BoT object %s with %ld triangles", bot_name, pntcnt);

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}

static int
_ged_pnts_cmd_gen(void *bs, int argc, const char **argv)
{
    const char *usage = "Usage: pnts gen [options] <obj> <output_pnts>\n\n";
    const char *purpose = "Generate a point set from an object and store it in a pnts object";
    if (_ged_pnts_cmd_msgs(bs, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    struct ged *gedp = gpi->gedp;

    int print_help = 0, opt_ret = 0;
    fastf_t len_tol = 0.0;
    int pnt_surf_mode = 0, pnt_grid_mode = 0, pnt_rand_mode = 0, pnt_sobol_mode = 0;
    int max_pnts = 0, max_time = 0, flags = 0;
    double avg_thickness = 0.0;
    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h", "help",      "",  NULL,            &print_help,     "Print help and exit");
    BU_OPT(d[1], "t", "tolerance", "#", &bu_opt_fastf_t, &len_tol,        "Specify sampling grid spacing (in mm).");
    BU_OPT(d[2], "",  "surface",   "",  NULL,            &pnt_surf_mode,  "Save only first and last points along ray.");
    BU_OPT(d[3], "",  "grid",      "",  NULL,            &pnt_grid_mode,  "Sample using a gridded ray pattern (default).");
    BU_OPT(d[4], "",  "rand",      "",  NULL,            &pnt_rand_mode,  "Sample using a random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(d[5], "",  "sobol",     "",  NULL,            &pnt_sobol_mode, "Sample using a Sobol pseudo-random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(d[6], "",  "max-pnts",  "",  &bu_opt_int,     &max_pnts,       "Maximum number of pnts to return per non-grid sampling method.");
    BU_OPT(d[7], "",  "max-time",  "",  &bu_opt_int,     &max_time,       "Maximum time to spend per-method (in seconds) when using non-grid sampling.");
    BU_OPT_NULL(d[8]);

    argc-=(argc>0);
    argv+=(argc>0); // skip subcommand

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    argc = opt_ret;
    if (argc != 2) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    const char *obj_name = argv[0];
    const char *pnt_prim = argv[1];

    if (db_lookup(gedp->dbip, obj_name, LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s doesn't exist!\n", obj_name);
	return BRLCAD_ERROR;
    }
    GED_CHECK_EXISTS(gedp, pnt_prim, LOOKUP_QUIET, BRLCAD_ERROR);

    if (pnt_surf_mode) flags |= RT_GEN_OBJ_PNTS_SURF;
    if (!pnt_grid_mode && !pnt_rand_mode && !pnt_sobol_mode) {
	flags |= RT_GEN_OBJ_PNTS_GRID;
    } else {
	if (pnt_grid_mode)  flags |= RT_GEN_OBJ_PNTS_GRID;
	if (pnt_rand_mode)  flags |= RT_GEN_OBJ_PNTS_RAND;
	if (pnt_sobol_mode) flags |= RT_GEN_OBJ_PNTS_SOBOL;
    }

    if (NEAR_ZERO(len_tol, RT_LEN_TOL)) {
	point_t rpp_min, rpp_max, obj_min, obj_max;
	VSETALL(rpp_min, INFINITY);
	VSETALL(rpp_max, -INFINITY);
	rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&obj_name, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	len_tol = DIST_PNT_PNT(rpp_max, rpp_min) * 0.01;
	bu_log("Note - no tolerance specified, using %f\n", len_tol);
    }
    struct bn_tol btol = BN_TOL_INIT_TOL;
    btol.dist = len_tol;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = RT_PNT_TYPE_NRM;

    if (rt_gen_obj_pnts(pnts, &avg_thickness, gedp->dbip, obj_name, &btol, flags, max_pnts, max_time, 2)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point generation failed\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %ld points, avg. partition thickness %g", pnt_prim, pnts->count, avg_thickness);

    struct directory *dp;
    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
}

static int
_ged_pnts_cmd_read(void *bs, int argc, const char **argv)
{
    const char *usage = "Usage: pnts read [options] <input_file> <pnts_obj> \n\nReads in point data.\n\n";
    const char *purpose = "Read data from an input file into a pnts object";
    if (_ged_pnts_cmd_msgs(bs, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    struct ged *gedp = gpi->gedp;

    int print_help = 0, opt_ret = 0;
    struct bu_vls fmt = BU_VLS_INIT_ZERO;
    struct bu_vls fl  = BU_VLS_INIT_ZERO;
    struct bu_vls unit = BU_VLS_INIT_ZERO;
    char **nums = NULL;
    int numcnt = 0, pnts_cnt = 0, array_size = 0;
    fastf_t conv_factor = 1.0;
    fastf_t psize = 0.0;
    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",      "",              NULL,            &print_help,  "Print help and exit");
    BU_OPT(d[1], "f", "format",    "[xyzijksrgb]",  &bu_opt_vls,     &fmt,         "Format of input data");
    BU_OPT(d[2], "u", "units",     "unit",          &bu_opt_vls,     &unit,        "Either a named unit (e.g. in), number (implicit unit is mm) or a unit expression (.15m)");
    BU_OPT(d[3], "",  "size",      "#",             &bu_opt_fastf_t, &psize,       "Default size to use for points");
    BU_OPT_NULL(d[4]);

    argc-=(argc>0);
    argv+=(argc>0); // skip subcommand

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    argc = opt_ret;
    if (argc != 2) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (bu_vls_strlen(&unit)) {
	const char *cunit = bu_vls_addr(&unit);
	if (bu_opt_fastf_t(NULL, 1, (const char **)&cunit, (void *)&conv_factor) == -1) {
	    conv_factor = bu_mm_value(bu_vls_addr(&unit));
	}
	if (conv_factor < 0) {
	    bu_vls_sprintf(gedp->ged_result_str, "invalid unit specification: %s\n", bu_vls_addr(&unit));
	    bu_vls_free(&unit);
	    bu_vls_free(&fmt);
	    return BRLCAD_ERROR;
	}
    }

    const char *filename = argv[0];
    const char *pnt_prim = argv[1];

    if (!bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s does not exist\n", filename);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return BRLCAD_ERROR;
    }

    if (db_lookup(gedp->dbip, pnt_prim, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists\n", pnt_prim);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return BRLCAD_ERROR;
    }

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Could not open file '%s'.\n", filename);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = psize;
    pnts->type = _ged_pnts_fmt_type(bu_vls_addr(&fmt));
    if (pnts->type != RT_PNT_UNKNOWN) {
	pnts->point = _ged_pnts_new_pnt(pnts->type);
	_ged_pnts_init_head_pnt(pnts);
    }

    while (!(bu_vls_gets(&fl, fp) < 0)) {
	char *input = bu_strdup(bu_vls_addr(&fl));
	if (!nums || ((int)strlen(input) > array_size)) {
	    if (nums) bu_free(nums, "free old nums array");
	    nums = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	    array_size = strlen(input);
	}
	numcnt = bu_argv_from_string(nums, strlen(input), input);

	if (!numcnt) {
	    bu_vls_trunc(&fl, 0);
	    bu_free(input, "input cpy");
	    continue;
	}

	if (pnts->type == RT_PNT_UNKNOWN) {
	    bu_log("Warning - no point format specified, trying to match template xyz[ijk][s][rgb]\n");
	    pnts->type = _ged_pnts_fmt_guess(numcnt);
	    if (pnts->type != RT_PNT_UNKNOWN) {
		pnts->point = _ged_pnts_new_pnt(pnts->type);
		_ged_pnts_init_head_pnt(pnts);
		bu_vls_sprintf(&fmt, "%s", _ged_pnt_default_fmt_str(pnts->type));
		bu_log("Assuming format %s\n", bu_vls_addr(&fmt));
	    }
	}

	if (!_ged_pnts_fmt_match(pnts->type, numcnt)) {
	    if (pnts->type == RT_PNT_UNKNOWN)
		bu_vls_sprintf(gedp->ged_result_str, "Error: could not determine point type, aborting\n");
	    else
		bu_vls_sprintf(gedp->ged_result_str, "found invalid number count %d for point type, aborting:\n%s\n", numcnt, bu_vls_addr(&fl));
	    rt_db_free_internal(&internal);
	    bu_vls_free(&fmt);
	    bu_vls_free(&unit);
	    bu_free(input, "input cpy");
	    bu_free(nums, "nums array");
	    fclose(fp);
	    return BRLCAD_ERROR;
	}

	_pnts_read_point(pnts, numcnt, (const char **)nums, bu_vls_addr(&fmt), conv_factor);
	pnts_cnt++;
	bu_vls_trunc(&fl, 0);
	bu_free(input, "input cpy");
    }

    pnts->count = pnts_cnt;
    fclose(fp);

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %ld points", pnt_prim, pnts->count);

    struct directory *dp;
    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, BRLCAD_ERROR);

    bu_vls_free(&fmt);
    if (nums) bu_free(nums, "free old nums array");
    return BRLCAD_OK;
}

static int
_ged_pnts_cmd_write(void *bs, int argc, const char **argv)
{
    const char *usage = "Usage: pnts write [options] <pnts_obj> <output_file>\n\nWrites out data based on the point type, one row per point, using a format of x y z [i j k] [scale] [R G B] (bracketed parts optional)\n";
    const char *purpose = "Write data from a point set into a file";
    if (_ged_pnts_cmd_msgs(bs, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    struct ged *gedp = gpi->gedp;

    int print_help = 0, ply_out = 0, opt_ret = 0, precis = 0;
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,   "Print help and exit");
    BU_OPT(d[1], "p", "precision", "#",  &bu_opt_int,  &precis,       "Number of digits after decimal to use when printing out numbers (default 17)");
    BU_OPT(d[2], "",  "ply",       "",   NULL,         &ply_out,      "Write output using PLY format instead of x y z [i j k] [scale] [R G B] text file");
    BU_OPT_NULL(d[3]);

    argc-=(argc>0);
    argv+=(argc>0); // skip subcommand

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    argc = opt_ret;
    if (argc != 2) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    const char *pnt_prim = argv[0];
    const char *filename = argv[1];
    if (bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s already exists\n", filename);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal intern;
    if (_pnts_db_lookup_internal(gedp, pnt_prim, &intern, DB5_MINORTYPE_BRLCAD_PNTS) != BRLCAD_OK)
	return BRLCAD_ERROR;

    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->type == RT_PNT_UNKNOWN) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: unknown pnts type\n");
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    FILE *fp = fopen(filename, "wb+");
    if (fp == NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: cannot open file %s for writing\n", filename);
	return BRLCAD_ERROR;
    }

    int ret = BRLCAD_OK;
    if (!ply_out) {
	ret = _pnts_write_text(fp, pnts, precis);
    } else {
	// Write PLY header, then data. This is a simplified version.
	fprintf(fp, "ply\nformat ascii 1.0\ncomment %s\n", pnt_prim);
	fprintf(fp, "element vertex %ld\n", (long)pnts->count);
	fprintf(fp, "property double x\nproperty double y\nproperty double z\n");
	if (pnts->type == RT_PNT_TYPE_NRM || pnts->type == RT_PNT_TYPE_SCA_NRM ||
	    pnts->type == RT_PNT_TYPE_COL_NRM || pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	    fprintf(fp, "property double nx\nproperty double ny\nproperty double nz\n");
	}
	if (pnts->type == RT_PNT_TYPE_COL || pnts->type == RT_PNT_TYPE_COL_SCA ||
	    pnts->type == RT_PNT_TYPE_COL_NRM || pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	    fprintf(fp, "property uchar red\nproperty uchar green\nproperty uchar blue\n");
	}
	fprintf(fp, "element face 0\nend_header\n");
	ret = _pnts_write_text(fp, pnts, precis); // For now, just reuse text output for data rows
    }

    rt_db_free_internal(&intern);
    fclose(fp);
    return ret;
}

static int
_ged_pnts_cmd_wn(void *bs, int argc, const char **argv)
{
    struct _ged_pnts_info *gpi = (struct _ged_pnts_info *)bs;
    if (_ged_pnts_cmd_msgs(bs, argc, argv, "pnts wn ...", "UNIMPLEMENTED")) {
	return BRLCAD_OK;
    }
    bu_vls_printf(gpi->gedp->ged_result_str, "pnts: wn subcommand is unimplemented\n");
    return BRLCAD_ERROR;
}

static void
_pnts_show_help(struct ged *gedp, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "Usage: pnts [options] <subcommand> [subcommand arguments]\n\n");

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(&str, "Subcommands:\n\n");
    bu_vls_printf(&str, "  read  - Read data from an input file into a pnts object\n\n");
    bu_vls_printf(&str, "  write - Write data from a point set into a file\n\n");
    bu_vls_printf(&str, "  gen   - Generate a point set from the object and store the set in a points object.\n\n");
    bu_vls_printf(&str, "  tri   - Generate unit or scaled triangles for each pnt in a point set. If no normal\n");
    bu_vls_printf(&str, "          information is present, use origin at avg of all set points to make normals.\n\n");
    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}

static const struct bu_cmdtab _pnts_cmds[] = {
    { "gen",    _ged_pnts_cmd_gen },
    { "read",   _ged_pnts_cmd_read },
    { "tri",    _ged_pnts_cmd_tri },
    { "wn",     _ged_pnts_cmd_wn },
    { "write",  _ged_pnts_cmd_write },
    { (char *)NULL, NULL }
};

extern "C" int
ged_pnts_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    struct _ged_pnts_info gpi;
    gpi.gedp = gedp;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT_NULL(d[1]);
    gpi.gopts = d;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    argc--;
    argv++; // skip command name

    if (argc < 1) {
	_pnts_show_help(gedp, d);
	return BRLCAD_OK;
    }

    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_pnts_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }
    int opt_argc = (cmd_pos >= 0) ? cmd_pos : argc;
    int opt_ret = bu_opt_parse(NULL, opt_argc, argv, d);

    if (print_help) {
	_pnts_show_help(gedp, d);
	return BRLCAD_OK;
    }

    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, "pnts: no valid subcommand specified\n");
	_pnts_show_help(gedp, d);
	return BRLCAD_ERROR;
    }
    if (opt_ret < 0) {
	_pnts_show_help(gedp, d);
	return BRLCAD_ERROR;
    }

    for (int i = cmd_pos; i < argc; i++) {
	argv[i - cmd_pos] = argv[i];
    }
    argc = argc - cmd_pos;

    int ret = BRLCAD_ERROR;
    if (bu_cmd(_pnts_cmds, argc, argv, 0, (void *)&gpi, &ret) == BRLCAD_OK) {
	return ret;
    }

    bu_vls_printf(gedp->ged_result_str, "pnts: subcommand %s not defined\n", argv[0]);
    _pnts_show_help(gedp, d);
    return BRLCAD_ERROR;
}

/* Compatibility wrapper for the old make_pnts command ordering/prompting:
 *
 * Input values:
 * argv[1] object name
 * argv[2] filename with path
 * argv[3] point data file format string
 * argv[4] point data file units string or conversion factor to millimeters
 * argv[5] default size of each point
 */
int
ged_make_pnts_core(struct ged *gedp, int argc, const char *argv[]) 
{
    double conv_factor = -1.0;
    double psize = -1.0;
    char *endp = (char *)NULL;
    static const char *usage = "point_cloud_name filename_with_path file_format file_data_units default_point_size";
    static const char *prompt[] = {
	"Enter point-cloud name: ",
	"Enter point file path and name: ",
	"Enter file data format (xyzrgbsijk?): ",
	"Enter file data units (um|mm|cm|m|km|in|ft|yd|mi)\nor conversion factor from file data units to millimeters: ",
	"Enter default point size: "
    };
    static const char *r_cmd = "read";
    static const char *s_opt = "--size";
    static const char *f_opt = "--format";
    static const char *u_opt = "--units";
    const char *nargv[10];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* prompt for point-cloud name */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[0]);
	return GED_MORE;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    /* prompt for data file name with path */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[1]);
	return GED_MORE;
    }

    /* prompt for data file format */
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[2]);
	return GED_MORE;
    }

    /* Validate 'point file data format string' and return point-cloud type. */
    if ((_ged_pnts_fmt_type(argv[3]) == RT_PNT_UNKNOWN)) {
	bu_vls_printf(gedp->ged_result_str, "could not validate format string: %s\n", argv[3]);
	return BRLCAD_ERROR;
    }

    /* prompt for data file units */
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[3]);
	return GED_MORE;
    }

    /* Validate unit */
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[4], (void *)&conv_factor) == -1) {
	conv_factor = bu_mm_value(argv[4]);
    }
    if (conv_factor < 0) {
	bu_vls_sprintf(gedp->ged_result_str, "invalid unit specification: %s\n", argv[4]);
	return BRLCAD_ERROR;
    }

    /* prompt for default point size */
    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[4]);
	return GED_MORE;
    }

    psize = strtod(argv[5], &endp);
    if ((endp != argv[5]) && (*endp == '\0')) {
	/* convert to double success */
	if (psize < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "Default point size '%lf' must be non-negative.\n", psize);
	    return BRLCAD_ERROR;
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid default point size '%s'\n", argv[5]);
	return BRLCAD_ERROR;
    }

    nargv[0] = "pnts";
    nargv[1] = r_cmd;
    nargv[2] = s_opt;
    nargv[3] = argv[5];
    nargv[4] = f_opt;
    nargv[5] = argv[3];
    nargv[6] = u_opt;
    nargv[7] = argv[4];
    nargv[8] = argv[2];
    nargv[9] = argv[1];

    return ged_exec_pnts(gedp, 10, (const char **)nargv);
}



#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl pnts_cmd_impl = { "pnts", ged_pnts_core, GED_CMD_DEFAULT };
    const struct ged_cmd pnts_cmd = { &pnts_cmd_impl };

    struct ged_cmd_impl make_pnts_cmd_impl = { "make_pnts", ged_make_pnts_core, GED_CMD_DEFAULT };
    const struct ged_cmd make_pnts_cmd = { &make_pnts_cmd_impl };

    const struct ged_cmd *pnts_cmds[] = { &make_pnts_cmd,  &pnts_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  pnts_cmds, 2 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
