/*                         T R A N S L A T E . C
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
/** @file libged/translate.c
 *
 * The translate command.
 *
 */

/* TODO:
 *  -reject paths with fp_len > 1; it's meaningless and misleading
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "ged.h"
#include "./ged_private.h"

/*
 * it's concievable that this could be exposed, so keep a clean break
 * from the ged cmd and don't make assumptions
 */
HIDDEN int
translate(struct ged *gedp, point_t * const keypoint,
	  struct db_full_path * const path, struct directory * const d_obj,
	  vect_t delta) {

    struct db_full_path full_obj_path;
    struct directory *d_comb_to_modify = NULL;
    char **argv = NULL;
    int argc = 0;

    struct rt_db_internal intern;
    struct _ged_trace_data gtd;
    mat_t dmat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;

    /* XXX quiet compiler for now */
    (void)keypoint;
    (void)argv;
    (void)argc;

    /* verify existence of path */
    if (ged_path_validate(gedp, path) == GED_ERROR) {
	char *s_path = db_path_to_string(path);
	bu_vls_printf(&gedp->ged_result_str, "path \"%s\" doesn't exist",
		      s_path);
	bu_free((genptr_t)s_path, "path string");
	return GED_ERROR;
    }

    /* verify that object exists under current directory in path */
    db_full_path_init(&full_obj_path);
    if (path->fp_len > 0) /* if there's no path, obj is at root */
	db_dup_path_tail(&full_obj_path, path, path->fp_len - (size_t)1);
    db_add_node_to_full_path(&full_obj_path, d_obj);
    if (ged_path_validate(gedp, &full_obj_path) == GED_ERROR) {
	char *s_path = db_path_to_string(path);
	bu_vls_printf(&gedp->ged_result_str, "object \"%s\" not found under"
		      " path \"%s\"", d_obj->d_namep, s_path);
	db_free_full_path(&full_obj_path);
	bu_free((genptr_t)s_path, "path string");
	return GED_ERROR;
    }

    /*
     * Determine what is being translated
     */
    if (d_obj->d_flags & RT_DIR_SOLID) {
	/* TODO: translation of solids */
	db_free_full_path(&full_obj_path);
	bu_vls_printf(&gedp->ged_result_str, "translation of solids not yet"
		      " supported");
	return GED_ERROR;
    } else if (d_obj->d_flags & (RT_DIR_REGION | RT_DIR_COMB)) {
	if (path->fp_len > 0)
	    /* path supplied; move obj instance only (obj's CWD
	     * modified) */
	    d_comb_to_modify = DB_FULL_PATH_CUR_DIR(path);
	else
	    /* no path; move all obj instances (obj's entire tree
	     * modified) */
	    d_comb_to_modify = d_obj;
    } else {
	db_free_full_path(&full_obj_path);
	bu_vls_printf(&gedp->ged_result_str, "unsupported object type");
	return GED_ERROR;
    }

    /*
     * Perform translations
     */
    if (d_comb_to_modify == d_obj) {
	/* move all obj instances */
	if (_ged_get_obj_bounds2(gedp, 1,
				 (const char **)&d_comb_to_modify->d_namep,
				 &gtd, rpp_min, rpp_max) == GED_ERROR)
	    return GED_ERROR;
	d_comb_to_modify = gtd.gtd_obj[gtd.gtd_objpos-1];
	if (!(d_comb_to_modify->d_flags & RT_DIR_SOLID))
	    if (_ged_get_obj_bounds(gedp, 1,
		(const char **)&d_comb_to_modify->d_namep, 1, rpp_min, rpp_max)
		== GED_ERROR)
		return GED_ERROR;
	MAT_IDN(dmat);
	VSCALE(delta, delta, gedp->ged_wdbp->dbip->dbi_local2base);
	MAT_DELTAS_VEC(dmat, delta);

	bn_mat_inv(invXform, gtd.gtd_xform);
	bn_mat_mul(tmpMat, invXform, dmat);
	bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

	GED_DB_GET_INTERNAL(gedp, &intern, d_comb_to_modify, emat,
			    &rt_uniresource, GED_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	GED_DB_PUT_INTERNAL(gedp, d_comb_to_modify, &intern, &rt_uniresource,
			    GED_ERROR);
    } else {
	bu_vls_printf(&gedp->ged_result_str, "function not yet implemented");
	return GED_ERROR;
    }

    db_free_full_path(&full_obj_path);
    return GED_OK;
}


int
ged_translate(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    const char *cmd_name = argv[0];
    static const char *usage = "[-k keypoint:object]"
	" [[-a] | [-r]]"
	" x [y [z]]"
	" path"
	" object";
    size_t i;				/* iterator */
    int c;				/* bu_getopt return value */
    int abs_flag = 0;			/* use absolute position */
    int rel_flag = 0;			/* use relative distance */
    char *kp_arg = NULL;        	/* keypoint argument */
    point_t keypoint;
    vect_t delta;			/* dist/pos to translate to */
    const char *s_obj;
    const char *s_path;
    struct db_full_path obj;
    struct db_full_path path;
    struct directory *d_obj;
    char *endchr = NULL;		/* for strtod's */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help -- argc < 4 is wrong too, but more helpful
       msgs are given later, by saying which args are missing */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    /* get short arguments */
    bu_optind = 1; /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "ak:r")) != -1) {
	switch (c) {
	    case 'a':
		abs_flag = 1;
		break;
	    case 'k':
		kp_arg = bu_optarg;
		if (kp_arg[0] == '-') {
		    /* that's an option, not an arg */
		    bu_vls_printf(&gedp->ged_result_str,
				  "Missing argument for option -%c", bu_optopt);
		    return GED_ERROR;
		}
		break;
	    case 'r':
		rel_flag = 1;
		break;
	    default:
		/* options that require arguments */
		switch (bu_optopt) {
		    case 'k':
			bu_vls_printf(&gedp->ged_result_str,
				      "Missing argument for option -%c", bu_optopt);
			return GED_ERROR;
		}

		/* unknown options */
		if (isprint(bu_optopt)) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Unknown option '-%c'", bu_optopt);
		    return GED_ERROR;
		} else {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Unknown option character '\\x%x'",
				  bu_optopt);
		    return GED_ERROR;
		}
	}
    }


    /* need to use either absolute||relative positioning; not both */
    if (abs_flag && rel_flag) {
	bu_vls_printf(&gedp->ged_result_str,
		      "options '-a' and '-r' are mutually exclusive");
	return GED_ERROR;
    }
    /* perhaps rel_flag was set by mistake */
    if (rel_flag && kp_arg) {
	bu_vls_printf(&gedp->ged_result_str,
		      "relative translations do not have keypoints");
	return GED_ERROR;
    }
    /* default to relative positioning */
    if (!abs_flag && !rel_flag)
	rel_flag = 1;

    /* TODO: set reasonable default keypoint */
#if 0
    if (!keypoint)
	;
#endif

    /* XXX testing */
    keypoint[0] = 0;
    keypoint[1] = 0;
    keypoint[2] = 0;

    /* set 3d coords */
    if ((bu_optind + 1) > argc) {
	bu_vls_printf(&gedp->ged_result_str, "missing x coordinate");
	return GED_HELP;
    }
    delta[0] = strtod(argv[bu_optind], &endchr);
    if (!endchr || argv[bu_optind] == endchr) {
	bu_vls_printf(&gedp->ged_result_str, "missing or invalid x coordinate");
	return GED_ERROR;
    }
    ++bu_optind;
    for (i = 1; i < 3; ++i, ++bu_optind) {
	if ((bu_optind + 1) > argc)
	    break;
	delta[i] = strtod(argv[bu_optind], &endchr);
	if (!endchr || argv[bu_optind] == endchr)
	    /* invalid y or z coord */
	    break;
    }

    /* set path */
    if ((bu_optind + 1) >= argc) {
	bu_vls_printf(&gedp->ged_result_str,
		      "missing path and/or object\n");
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }
    s_path = argv[bu_optind++];
    if (db_string_to_path(&path, dbip, s_path) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "invalid path \"%s\"", s_path);
	return GED_ERROR;
    }

    /* set object (no path accepted) */
    s_obj = argv[bu_optind++];
    if (db_string_to_path(&obj, dbip, s_obj) < 0 || obj.fp_len != (size_t)1) {
	bu_vls_printf(&gedp->ged_result_str, "invalid object \"%s\"",
		      s_obj);
	db_free_full_path(&path);
	return GED_ERROR;
    }
    if ((bu_optind + 1) <= argc) {
	bu_vls_printf(&gedp->ged_result_str, "multiple objects not yet"
		      " supported; ");
	goto disabled;
    }

    /*
     * Perform translations
     */
    d_obj = DB_FULL_PATH_ROOT_DIR(&obj);
    if (translate(gedp, &keypoint, &path, d_obj, delta) == GED_ERROR) {
	db_free_full_path(&path);
	db_free_full_path(&obj);
	bu_vls_printf(&gedp->ged_result_str, "; translation failed");
	return GED_ERROR;
    }

    /* XXX disabled until functional */
    goto disabled;

    db_free_full_path(&path);
    db_free_full_path(&obj);
    return GED_OK;

disabled:
    db_free_full_path(&path);
    db_free_full_path(&obj);
    bu_vls_printf(&gedp->ged_result_str, "function not yet implemented");
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
