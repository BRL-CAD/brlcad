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

/*  TODO:
 *    -reject paths with fp_len > 1; it's meaningless and misleading
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "ged.h"

/* 
 * it's concievable that this could be exposed, so keep a clean break
 * from the ged cmd and don't make assumptions
 */
HIDDEN int
translate(struct ged *gedp, point_t * const keypoint,
	  struct db_full_path * const path, struct directory * const d_obj,
	  int * const rel_flag) { 

    struct db_full_path full_obj_path;
    struct directory *d_comb_to_modify = NULL;
    struct rt_comb_internal *comb_to_modify;
    char **argv = NULL;
    int argc = 0;

    /* for retrieving a tree */
    struct rt_db_internal old_intern;
    union tree *old_ntp = NULL;
    struct rt_tree_array *old_rt_tree_array = NULL;
    size_t old_node_count = (size_t)0;
    RT_DB_INTERNAL_INIT(&old_intern);


    /* XXX quiet compiler for now */
    (void)keypoint;
    (void)rel_flag;
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

    /* build ged_combmem command arguments:
     *   -everything in obj's tree is fetched and coords modified 
     *    as necessary
     *   -build argv
     *   -call ged_combmem
     */

    /* determine what is being translated */
    if (d_obj->d_flags & RT_DIR_SOLID) {
	/* TODO: translation of solids (note:ged_p cmd is not yet
	 * available) */
	db_free_full_path(&full_obj_path);
	bu_vls_printf(&gedp->ged_result_str, "translation of solids not yet"
					     " supported");
	return GED_ERROR;
    } else if (d_obj->d_flags & (RT_DIR_REGION | RT_DIR_COMB)) {
	if (path->fp_len > 0)
	    /* path supplied; move obj instance only (obj's parent
	     * modified) */
	    d_comb_to_modify = DB_FULL_PATH_CUR_DIR(path);
	else
	    /* no path; move all obj instances (obj's entire tree
	     * modified) */
	    d_comb_to_modify = d_obj;

	/* get comb tree */
	if (rt_db_get_internal(&old_intern, d_comb_to_modify,
	    		       gedp->ged_wdbp->dbip, (matp_t)NULL,
			       &rt_uniresource) < 0) {
	    bu_vls_printf(&(gedp)->ged_result_str,
			  "Database read error, aborting");
	    return GED_ERROR;
	}
	comb_to_modify = (struct rt_comb_internal *)(old_intern).idb_ptr;
	RT_CK_COMB(comb_to_modify);
	if (comb_to_modify->tree) {
	    old_ntp = db_dup_subtree(comb_to_modify->tree, &rt_uniresource);
	    RT_CK_TREE(old_ntp);

	    /* Convert to "v4 / GIFT style", so that the
	     * flatten makes sense. */
	    if (db_ck_v4gift_tree(old_ntp) < 0)
		db_non_union_push((old_ntp), &rt_uniresource);
	    RT_CK_TREE(old_ntp);

	    old_node_count = db_tree_nleaves(old_ntp);
	    old_rt_tree_array = (struct rt_tree_array *)bu_calloc(
		(old_node_count), sizeof(struct rt_tree_array),
		"rt_tree_array");

	    /* free=0 means that the tree won't have any leaf nodes freed */
	    (void)db_flatten_tree((old_rt_tree_array), (old_ntp), OP_UNION, 0,
				  &rt_uniresource);
	} else {
	    old_ntp = TREE_NULL;
	    old_node_count = 0;
	    old_rt_tree_array = (struct rt_tree_array *)0;
	}
    } else {
	db_free_full_path(&full_obj_path);
	bu_vls_printf(&gedp->ged_result_str, "unsupported object type; ");
	return GED_ERROR;
    }
    
    /* ged_combmem(gedp, argc, argv); */

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
    vect_t new_vertex;			/* distance/position to translate to */
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

    /* XXX testing */
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
    if ((bu_optind + 1) <= argc) {
	bu_vls_printf(&gedp->ged_result_str, "multiple objects not yet"
					     " supported; ");
	goto disabled;
    }

    /* 
     * Perform translations
     */
    d_obj = DB_FULL_PATH_ROOT_DIR(&obj);
    if (translate(gedp, &keypoint, &path, d_obj, &rel_flag) == GED_ERROR) {
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
