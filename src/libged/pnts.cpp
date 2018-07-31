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

/* TODO - merge make_pnts in with this... */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

#include "bu/color.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/units.h"
#include "rt/geom.h"
#include "wdb.h"
#include "analyze.h"
#include "./ged_private.h"

#define PNT_V_IN(_pt, _key, _v)               \
    switch (_key) {                           \
        case 'x':                             \
            ((struct _pt *)point)->v[X] = _v; \
            break;                            \
        case 'y':                             \
            ((struct _pt *)point)->v[Y] = _v; \
            break;                            \
        case 'z':                             \
            ((struct _pt *)point)->v[Z] = _v; \
            break;                            \
    }

#define PNT_C_IN(_pt, _key, _v)                       \
    switch (_key) {                                   \
        case 'r':                                     \
            ((struct _pt *)point)->c.buc_rgb[0] = _v; \
            break;                                    \
        case 'g':                                     \
            ((struct _pt *)point)->c.buc_rgb[1] = _v; \
            break;                                    \
        case 'b':                                     \
            ((struct _pt *)point)->c.buc_rgb[2] = _v; \
            break;                                    \
    }

#define PNT_S_IN(_pt, _key, _v)           \
    switch (_key) {                       \
	case 's':                         \
	   ((struct _pt *)point)->s = _v; \
	break;                            \
    }

#define PNT_N_IN(_pt, _key, _v)              \
    switch (_key) {                          \
	case 'i':                            \
	   ((struct _pt *)point)->n[X] = _v; \
	break;                               \
	case 'j':                            \
	   ((struct _pt *)point)->n[Y] = _v; \
	break;                               \
	case 'k':                            \
	   ((struct _pt *)point)->n[Z] = _v; \
	break;                               \
    }

HIDDEN const char *
_pnt_default_fmt_str(rt_pnt_type type) {
    static const char *pntfmt = "xyz";
    static const char *colfmt = "xyzrgb";
    static const char *scafmt = "xyzs";
    static const char *nrmfmt = "xyzijk";
    static const char *colscafmt = "xyzsrgb";
    static const char *colnrmfmt = "xyzijkrgb";
    static const char *scanrmfmt = "xyzijks";
    static const char *colscanrmfmt = "xyzijksrgb";

    switch (type) {
	case RT_PNT_TYPE_PNT:
	    return pntfmt;
	    break;
	case RT_PNT_TYPE_COL:
	    return colfmt;
	    break;
	case RT_PNT_TYPE_SCA:
	    return scafmt;
	    break;
	case RT_PNT_TYPE_NRM:
	    return nrmfmt;
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    return colscafmt;
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    return colnrmfmt;
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    return scanrmfmt;
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    return colscanrmfmt;
	    break;
	default:
	    return NULL;
	    break;
    }
}

void
pnt_v_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_PNT:
	    PNT_V_IN(pnt, key, val);
	    break;
	case RT_PNT_TYPE_COL:
	    PNT_V_IN(pnt_color, key, val);
	    break;
	case RT_PNT_TYPE_SCA:
	    PNT_V_IN(pnt_scale, key, val);
	    break;
	case RT_PNT_TYPE_NRM:
	    PNT_V_IN(pnt_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_V_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_V_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_V_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_V_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
pnt_c_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_COL:
	    PNT_C_IN(pnt_color, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_C_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_C_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_C_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
pnt_s_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_SCA:
	    PNT_S_IN(pnt_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_S_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_S_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_S_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
pnt_n_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_NRM:
	    PNT_N_IN(pnt_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_N_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_N_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_N_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}


HIDDEN void
_pnts_cmd_help(struct ged *gedp, const char *usage, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    const char *option_help;

    bu_vls_sprintf(&str, "%s", usage);

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free((char *)option_help, "help str");
    }

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}


HIDDEN void
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

/* TODO - need some generic version of this logic in libbn - 
 * used in libanalyze's NIRT as well */
void _pnts_fastf_t_to_vls(struct bu_vls *o, fastf_t d, int p)
{
    // TODO - once we enable C++ 11 switch the precision below to std::numeric_limits<double>::max_digits10
    size_t prec = (p > 0) ? (size_t)p : std::numeric_limits<fastf_t>::digits10 + 2;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    bu_vls_sprintf(o, "%s", sd.c_str());
}

HIDDEN int
_pnts_to_bot(struct ged *gedp, int argc, const char **argv)
{
    int have_normals = 1;
    unsigned long pntcnt = 0;
    struct rt_db_internal intern, internal;
    struct rt_bot_internal *bot_ip;
    struct directory *pnt_dp;
    struct directory *dp;
    int print_help = 0;
    int opt_ret = 0;
    fastf_t scale;
    struct rt_pnts_internal *pnts = NULL;
    const char *pnt_prim= NULL;
    const char *bot_name = NULL;
    const char *usage = "Usage: pnts tri [options] <pnts> <output_bot>\n\n";
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",      "",  NULL,            &print_help,   "Print help and exit");
    BU_OPT(d[1], "s", "scale",     "#", &bu_opt_fastf_t, &scale,        "Specify scale factor to apply to unit triangle - will scale the triangle size, with the triangle centered on the original point.");
    BU_OPT_NULL(d[2]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc != 2) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    pnt_prim = argv[0];
    bot_name = argv[1];

    /* get pnt */
    GED_DB_LOOKUP(gedp, pnt_dp, pnt_prim, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, pnt_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "pnts tri: %s is not a pnts object!", pnt_prim);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* Sanity */
    if (!bot_name || !gedp) return GED_ERROR;
    if (db_lookup(gedp->ged_wdbp->dbip, bot_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists!\n", bot_name);
	return GED_ERROR;
    }

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

    rt_db_free_internal(&intern);
    return GED_OK;
}

HIDDEN int
_obj_to_pnts(struct ged *gedp, int argc, const char **argv)
{
    struct directory *dp;
    int print_help = 0;
    int opt_ret = 0;
    fastf_t len_tol = 0.0;
    int pnt_mode = 0;
    struct rt_db_internal internal;
    struct bn_tol btol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
    struct rt_pnts_internal *pnts = NULL;
    const char *pnt_prim= NULL;
    const char *obj_name = NULL;
    const char *usage = "Usage: pnts gen [options] <obj> <output_pnts>\n\n";
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",      "",  NULL,            &print_help,   "Print help and exit");
    BU_OPT(d[1], "t", "tolerance", "#", &bu_opt_fastf_t, &len_tol,      "Specify sampling grid spacing (in mm).");
    BU_OPT(d[2], "S", "surface",   "",  NULL,            &pnt_mode,     "Save only first and last points along ray.");
    BU_OPT_NULL(d[3]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc != 2) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    obj_name = argv[0];
    pnt_prim = argv[1];

    /* Sanity */
    if (db_lookup(gedp->ged_wdbp->dbip, obj_name, LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s doesn't exist!\n", obj_name);
	return GED_ERROR;
    }
    if (db_lookup(gedp->ged_wdbp->dbip, pnt_prim, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists!\n", pnt_prim);
	return GED_ERROR;
    }

    /* If we don't have a tolerance, try to guess something sane from the bbox */
    if (NEAR_ZERO(len_tol, RT_LEN_TOL)) {
	point_t rpp_min, rpp_max;
	point_t obj_min, obj_max;
	VSETALL(rpp_min, INFINITY);
	VSETALL(rpp_max, -INFINITY);
	ged_get_obj_bounds(gedp, 1, (const char **)&obj_name, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	len_tol = DIST_PT_PT(rpp_max, rpp_min) * 0.01;
	bu_log("Note - no tolerance specified, using %f\n", len_tol);
    }
    btol.dist = len_tol;

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = RT_PNT_TYPE_NRM;

    if (analyze_obj_to_pnts(pnts, gedp->ged_wdbp->dbip, obj_name, &btol, pnt_mode)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point generation failed\n");
	return GED_ERROR;
    }

    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %d points", pnt_prim, pnts->count);

    return GED_OK;
}

rt_pnt_type
_pnts_fmt_type(struct bu_vls *f)
{
    int has_pnt = 0;
    int has_nrm = 0;
    int has_s = 0;
    int has_c = 0;
    const char *fc = bu_vls_addr(f);

    if (!f || !bu_vls_strlen(f)) return RT_PNT_UNKNOWN;
    if (strchr(fc, 'x') && strchr(fc, 'y') && strchr(fc, 'z')) has_pnt = 1;
    if (strchr(fc, 'i') && strchr(fc, 'j') && strchr(fc, 'k')) has_nrm = 1;
    if (strchr(fc, 's')) has_s = 1;
    if (strchr(fc, 'r') && strchr(fc, 'g') && strchr(fc, 'b')) has_c = 1;

    if (has_pnt && has_nrm && has_s && has_c) return RT_PNT_TYPE_COL_SCA_NRM;
    if (has_pnt && has_nrm && has_s) return RT_PNT_TYPE_SCA_NRM;
    if (has_pnt && has_nrm && has_c) return RT_PNT_TYPE_COL_NRM;
    if (has_pnt && has_nrm) return RT_PNT_TYPE_NRM;
    if (has_pnt && has_s && has_c) return RT_PNT_TYPE_COL_SCA;
    if (has_pnt && has_s) return RT_PNT_TYPE_SCA;
    if (has_pnt && has_c) return RT_PNT_TYPE_SCA;
    if (has_pnt) return RT_PNT_TYPE_PNT;

    return RT_PNT_UNKNOWN;
}

rt_pnt_type
_pnts_fmt_guess(int numcnt) {
    switch (numcnt) {
	case 3:
	    return RT_PNT_TYPE_PNT;
	    break;
	case 4:
	    return RT_PNT_TYPE_SCA;
	    break;
	case 6:
	    bu_log("Warning - found 6 numbers, which is ambiguous - assuming RT_PNT_TYPE_NRM.  To read in as RT_PNT_TYPE_COL, specify the format string \"xyzrgb\"\n");
	    return RT_PNT_TYPE_NRM;
	    break;
	case 7:
	    bu_log("Warning - found 7 numbers, which is ambiguous - assuming RT_PNT_TYPE_COL_SCA.  To read in as RT_PNT_TYPE_SCA_NRM, specify the format string \"xyzijks\"\n");
	    return RT_PNT_TYPE_COL_SCA;
	    break;
	case 9:
	    return RT_PNT_TYPE_COL_NRM;
	    break;
	case 10:
	    return RT_PNT_TYPE_COL_SCA_NRM;
	    break;
	default:
	    return RT_PNT_UNKNOWN;
	    break;
    }
}

int
_pnts_fmt_match(rt_pnt_type t, int numcnt)
{
    if (t == RT_PNT_UNKNOWN || numcnt < 3 || numcnt > 10) return 0;

    if (numcnt == 3 && t == RT_PNT_TYPE_PNT) return 1;
    if (numcnt == 4 && t == RT_PNT_TYPE_SCA) return 1;
    if (numcnt == 6 && t == RT_PNT_TYPE_NRM) return 1;
    if (numcnt == 6 && t == RT_PNT_TYPE_COL) return 1;
    if (numcnt == 7 && t == RT_PNT_TYPE_COL_SCA) return 1;
    if (numcnt == 7 && t == RT_PNT_TYPE_SCA_NRM) return 1;
    if (numcnt == 9 && t == RT_PNT_TYPE_COL_NRM) return 1;
    if (numcnt == 10 && t == RT_PNT_TYPE_COL_SCA_NRM) return 1;

    return 0;
}

void
_pnts_init_head_pnt(struct rt_pnts_internal *pnts)
{
    if (!pnts) return;
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_INIT(&(((struct pnt *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_INIT(&(((struct pnt_color *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_INIT(&(((struct pnt_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_INIT(&(((struct pnt_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_INIT(&(((struct pnt_color_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_scale_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_scale_normal *)pnts->point)->l));
	    break;
	default:
	    break;
    }
}


void *
_pnts_new_pnt(rt_pnt_type t)
{
    void *point = NULL;
    if (t == RT_PNT_UNKNOWN) return NULL;
    switch (t) {
	case RT_PNT_TYPE_PNT:
	    BU_ALLOC(point, struct pnt);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_ALLOC(point, struct pnt_color);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_ALLOC(point, struct pnt_scale);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_ALLOC(point, struct pnt_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_ALLOC(point, struct pnt_color_scale);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_ALLOC(point, struct pnt_color_normal);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_ALLOC(point, struct pnt_scale_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_ALLOC(point, struct pnt_color_scale_normal);
	    break;
	default:
	    break;
    }
    return point;
}

void
_pnts_add(struct rt_pnts_internal *pnts, void *point)
{
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_PUSH(&(((struct pnt *)pnts->point)->l), &((struct pnt *)point)->l);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_PUSH(&(((struct pnt_color *)pnts->point)->l), &((struct pnt_color *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_PUSH(&(((struct pnt_scale *)pnts->point)->l), &((struct pnt_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_PUSH(&(((struct pnt_normal *)pnts->point)->l), &((struct pnt_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_PUSH(&(((struct pnt_color_scale *)pnts->point)->l), &((struct pnt_color_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_PUSH(&(((struct pnt_color_normal *)pnts->point)->l), &((struct pnt_color_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_PUSH(&(((struct pnt_scale_normal *)pnts->point)->l), &((struct pnt_scale_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_PUSH(&(((struct pnt_color_scale_normal *)pnts->point)->l), &((struct pnt_color_scale_normal *)point)->l);
	    break;
	default:
	    break;
    }
}


int
_pnt_read(struct rt_pnts_internal *pnts, int numcnt, const char **nums, const char *fmt, fastf_t conv_factor)
{
    int i = 0;
    char fc = '\0';
    void *point = _pnts_new_pnt(pnts->type);
    for (i = 0; i < numcnt; i++) {
	fastf_t val;
	fc = fmt[i];
	if (bu_opt_fastf_t(NULL, 1, (const char **)&nums[i], (void *)&val) == -1) {
	    bu_log("Error - failed to read number %s\n", nums[i]);
	    return GED_ERROR;
	}
	if ((fc == 'x') || (fc == 'y') || (fc == 'z')) {
	    pnt_v_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'i') || (fc == 'j') || (fc == 'k')) {
	    pnt_n_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'r') || (fc == 'g') || (fc == 'b')) {
	    pnt_c_set(point, pnts->type, fc, val);
	    continue;
	}
	if ((fc == 's')) {
	    pnt_s_set(point, pnts->type, fc, val);
	    continue;
	}
    }

    _pnts_add(pnts, point);

    return GED_OK;
}


HIDDEN int
_read_pnts(struct ged *gedp, int argc, const char **argv)
{
    int print_help = 0;
    int opt_ret = 0;
    FILE *fp;
    struct rt_db_internal internal;
    struct rt_pnts_internal *pnts = NULL;
    struct directory *dp;
    const char *pnt_prim = NULL;
    const char *filename = NULL;
    const char *usage = "Usage: pnts read [options] <input_file> <pnts_obj> \n\nReads in point data.\n\n";
    struct bu_vls fmt = BU_VLS_INIT_ZERO;
    struct bu_vls fl  = BU_VLS_INIT_ZERO;
    struct bu_vls unit = BU_VLS_INIT_ZERO;
    char **nums = NULL;
    int numcnt = 0;
    int pnts_cnt = 0;
    int array_size = 0;
    fastf_t conv_factor = 1.0;
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",      "",              NULL,         &print_help,   "Print help and exit");
    BU_OPT(d[1], "f", "format",    "[xyzijksrgb]",  &bu_opt_vls,  &fmt,          "Format of input data");
    BU_OPT(d[2], "u", "units",     "unit",          &bu_opt_vls,  &unit,         "Either a named unit (e.g. in) or a unit expression (.15m)");
    BU_OPT_NULL(d[3]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc != 2) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    /* got a unit, see if we can do something with it */
    if (bu_vls_strlen(&unit)) {
	const char *cunit = bu_vls_addr(&unit);
	if (bu_opt_fastf_t(NULL, 1, (const char **)&cunit, (void *)&conv_factor) == -1) {
	    conv_factor = bu_mm_value(bu_vls_addr(&unit));
	}
	if (conv_factor < 0) {
	    bu_vls_sprintf(gedp->ged_result_str, "invalid unit specification: %s\n", bu_vls_addr(&unit));
	    bu_vls_free(&unit);
	    bu_vls_free(&fmt);
	    return GED_ERROR;
	}
    }

    filename = argv[0];
    pnt_prim = argv[1];

    if (!bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s does not exist\n", filename);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return GED_ERROR;
    }

    if (db_lookup(gedp->ged_wdbp->dbip, pnt_prim, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists\n", pnt_prim);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return GED_ERROR;
    }

    if ((fp=fopen(filename, "rb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Could not open file '%s'.\n", filename);
	bu_vls_free(&unit);
	bu_vls_free(&fmt);
	return GED_ERROR;
    }

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = _pnts_fmt_type(&fmt);
    if (pnts->type != RT_PNT_UNKNOWN) {
	pnts->point = _pnts_new_pnt(pnts->type);
	_pnts_init_head_pnt(pnts);
    }

    while (!(bu_vls_gets(&fl, fp) < 0)) {

	char *input = bu_strdup(bu_vls_addr(&fl));
	/* Because a valid point array will always have many fewer numbers than characters,
	 * we should only need to allocate this array once.  However, guard against a
	 * pathologic case by making sure our array size is at least strlen(input) large */
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
	    /* If we dont' have a specified format, try to guess */
	    bu_log("Warning - no point format specified, trying to deduce format from input according to the template xyz[ijk][s][rgb]\n");
	    pnts->type = _pnts_fmt_guess(numcnt);
	    if (pnts->type != RT_PNT_UNKNOWN) {
		pnts->point = _pnts_new_pnt(pnts->type);
		_pnts_init_head_pnt(pnts);
		bu_vls_sprintf(&fmt, "%s", _pnt_default_fmt_str(pnts->type));
		bu_log("Assuming format %s\n", bu_vls_addr(&fmt));
	    }
	}

	/* Validate numcnt against type */
	if (!_pnts_fmt_match(pnts->type, numcnt)) {
	    if (pnts->type == RT_PNT_UNKNOWN) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: could not determine point type, aborting\n");
	    } else {
		bu_vls_sprintf(gedp->ged_result_str, "found invalid number count %d for point type, aborting:\n%s\n", numcnt, bu_vls_addr(&fl));
	    }
	    rt_db_free_internal(&internal);
	    bu_vls_free(&fmt);
	    bu_vls_free(&unit);
	    bu_free(input, "input cpy");
	    bu_free(nums, "nums array");
	    fclose(fp);
	    return GED_ERROR;
	}

	_pnt_read(pnts, numcnt, (const char **)nums, bu_vls_addr(&fmt), conv_factor); 
	pnts_cnt++;
	bu_vls_trunc(&fl, 0);
	bu_free(input, "input cpy");
    }

    pnts->count = pnts_cnt;
    fclose(fp);

    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %d points", pnt_prim, pnts->count);

    bu_vls_free(&fmt);
    if (nums) bu_free(nums, "free old nums array");
    return GED_OK;
}

HIDDEN int
_write_pnts(struct ged *gedp, int argc, const char **argv)
{
    int print_help = 0;
    int opt_ret = 0;
    FILE *fp;
    struct rt_db_internal intern;
    struct rt_pnts_internal *pnts = NULL;
    struct directory *pnt_dp;
    struct bu_vls pnt_str = BU_VLS_INIT_ZERO;
    const char *pnt_prim = NULL;
    const char *filename = NULL;
    const char *usage = "Usage: pnts write [options] <pnts_obj> <output_file>\n\nWrites out data based on the point type, one row per point, using a format of x y z [i j k] [scale] [R G B] (bracketed groups may or may not be present depending on point type.)\n\n";
    struct bu_opt_desc d[3];
    int precis = 0;
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,   "Print help and exit");
    BU_OPT(d[1], "p", "precision", "#",  &bu_opt_int,  &precis,       "Number of digits after decimal to use when printing out numbers (default 17)");
    BU_OPT_NULL(d[2]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc != 2) {
	_pnts_cmd_help(gedp, usage, d);
	return GED_ERROR;
    }

    pnt_prim = argv[0];
    filename = argv[1];

    if (bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s already exists\n", filename);
	return GED_ERROR;
    }

    /* get pnt */
    GED_DB_LOOKUP(gedp, pnt_dp, pnt_prim, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, pnt_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "pnts write: %s is not a pnts object!", pnt_prim);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->type != RT_PNT_TYPE_NRM) {
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    /* Write points */
    if ((fp=fopen(filename, "wb+")) == NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: cannot open file %s for writing\n", filename);
	return GED_ERROR;
    }

    if (pnts->type == RT_PNT_TYPE_PNT) {
	struct pnt *pn = NULL;
	struct pnt *pl = (struct pnt *)pnts->point;
	for (BU_LIST_FOR(pn, pnt, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		if (i != 2) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }


    if (pnts->type == RT_PNT_TYPE_COL) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color *pn = NULL;
	struct pnt_color *pl = (struct pnt_color *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		return GED_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_SCA) {
	struct pnt_scale *pn = NULL;
	struct pnt_scale *pl = (struct pnt_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		if (i != 2) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_SCA) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale *pn = NULL;
	struct pnt_color_scale *pl = (struct pnt_color_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    if (bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return GED_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_normal *pn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return GED_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *pn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale_normal *pn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], precis);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    _pnts_fastf_t_to_vls(&pnt_str, pn->s, precis);
	    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    if (bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return GED_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Error - pnts write: unsupported point type");
    rt_db_free_internal(&intern);
    fclose(fp);
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
    bu_vls_printf(&str, "  read  - Read data from an input file into a pnts object\n\n");
    bu_vls_printf(&str, "  write - Write data from a point set into a file\n\n");
    bu_vls_printf(&str, "  gen   - Generate a point set from the object and store the set in a points object.\n\n");
    bu_vls_printf(&str, "  tri   - Generate unit or scaled triangles for each pnt in a point set. If no normal\n");
    bu_vls_printf(&str, "          information is present, use origin at avg of all set points to make normals.\n\n");
    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}

int
ged_pnts(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd = argv[0];
    size_t len;
    int i;
    int print_help = 0;
    int opt_ret = 0;
    int opt_argc = argc;
    struct bu_opt_desc d[2];
    const char * const pnt_subcommands[] = {"gen", "read", "tri", "write", NULL};
    const char * const *subcmd;

    BU_OPT(d[0], "h", "help",      "", NULL, &print_help,        "Print help and exit");
    BU_OPT_NULL(d[1]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_show_help(gedp, d);
	return GED_OK;
    }

    /* See if we have any options to deal with.  Once we hit a subcommand, we're done */
    for (i = 0; i < argc; ++i) {
	subcmd = pnt_subcommands;
	for (; *subcmd != NULL; ++subcmd) {
	    if (BU_STR_EQUAL(argv[i], *subcmd)) {
		opt_argc = i;
		i = argc;
		break;
	    }
	}
    }

    if (opt_argc > 0) {
	/* parse standard options */
	opt_ret = bu_opt_parse(NULL, opt_argc, argv, d);
	if (opt_ret < 0) {
	    _pnts_show_help(gedp, d);
	    return GED_ERROR;
	}
    }

    if (print_help) {
	_pnts_show_help(gedp, d);
	return GED_OK;
    }

    /* shift argv to subcommand */
    argc -= opt_argc;
    argv = &argv[opt_argc];

    /* If we don't have a subcommand, we're done */
    if (argc < 1) {
	_pnts_show_help(gedp, d);
	return GED_ERROR;
    }

    len = strlen(argv[0]);
    if (bu_strncmp(argv[0], "tri", len) == 0) return _pnts_to_bot(gedp, argc, argv);

    if (bu_strncmp(argv[0], "gen", len) == 0) return _obj_to_pnts(gedp, argc, argv);

    if (bu_strncmp(argv[0], "write", len) == 0) return _write_pnts(gedp, argc, argv);

    if (bu_strncmp(argv[0], "read", len) == 0) return _read_pnts(gedp, argc, argv);

    /* If we don't have a valid subcommand, we're done */
    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!\n", cmd, argv[0]);
    _pnts_show_help(gedp, d);
    return GED_ERROR;
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
