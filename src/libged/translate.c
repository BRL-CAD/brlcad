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

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "ged.h"

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
    const char *s_obj;
    const char *s_path;
    struct db_full_path obj;
    struct db_full_path path;
    struct db_full_path full_obj_path;	/* full path to object */
    struct directory *d_obj;
    char *endchr = NULL;		/* for strtod's */

    /* testing */
    mat_t modelchanges, incr, old;
    vect_t model_incr, ed_sol_pt, new_vertex;

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

    
    /* need to use either absolute or relative positioning, but not both */
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

    /* testing */
    keypoint[0] = 0;
    keypoint[1] = 0;
    keypoint[2] = 0;

    /* set 3d coords */
    if ((bu_optind + 1) > argc) {
	bu_vls_printf(&gedp->ged_result_str, "missing x coordinate");
	return GED_HELP;
    }
    new_vertex[0] = strtod(argv[bu_optind], &endchr);
    if (!endchr || argv[bu_optind] == endchr) {
	bu_vls_printf(&gedp->ged_result_str, "missing or invalid x coordinate");
	return GED_ERROR;
    }
    ++bu_optind;
    for (i = 1; i < 3; ++i, ++bu_optind) {
	if ((bu_optind + 1) > argc)
	    break;
	new_vertex[i] = strtod(argv[bu_optind], &endchr);
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

    /* verify existence of path */
    if (ged_path_validate(gedp, path) == GED_ERROR) {
	bu_vls_printf(&gedp->ged_result_str, "path \"%s\" doesn't exist",
		      s_path);
	db_free_full_path(&path);
	db_free_full_path(&obj);
	return GED_ERROR;
    }

    /* verify that object exists under current directory in path */
    db_full_path_init(&full_obj_path);
    db_dup_path_tail(&full_obj_path, &path, path.fp_len - 1);
    db_append_full_path(&full_obj_path, &obj);
    if (ged_path_validate(gedp, full_obj_path) == GED_ERROR) {
	bu_vls_printf(&gedp->ged_result_str, "object \"%s\" not found"
		      " under path \"%s\"", s_obj, s_path);
	db_free_full_path(&path);
	db_free_full_path(&obj);
	db_free_full_path(&full_obj_path);
	return GED_ERROR;
    }

    /* 
     * Perform transations
     */
    d_obj = DB_FULL_PATH_ROOT_DIR(&obj);
    if (d_obj->d_flags & RT_DIR_SOLID) {
       /* TODO: translation of solids (note:'p' cmd is not yet available) */
	bu_vls_printf(&gedp->ged_result_str, "translation of solids not yet"
					     " supported; ");
	goto disabled;
    }
    if (d_obj->d_flags & (RT_DIR_REGION | RT_DIR_COMB)) {
       /* translation of combinations */
	bu_vls_printf(&gedp->ged_result_str, "translation of combinations not"
					     " yet supported; ");
	goto disabled;
    }

    /* XXX disabled until functional */
    if ((bu_optind + 1) <= argc)
	bu_vls_printf(&gedp->ged_result_str, "multiple objects not yet"
					     " supported; ");
    goto disabled;

    /* do translation */
    MAT_IDN(incr);
    MAT_IDN(old);
    MAT4X3PNT(ed_sol_pt, modelchanges, keypoint);
    VSUB2(model_incr, new_vertex, ed_sol_pt);
    MAT_DELTAS_VEC(incr, model_incr);
    MAT_COPY(old, modelchanges);
    bn_mat_mul(modelchanges, incr, old);
    /* new_edit_mats(); */

    db_free_full_path(&path);
    db_free_full_path(&obj);
    db_free_full_path(&full_obj_path);
    return GED_OK;

    disabled:
    db_free_full_path(&path);
    db_free_full_path(&obj);
    db_free_full_path(&full_obj_path);
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
