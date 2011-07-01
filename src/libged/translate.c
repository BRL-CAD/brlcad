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

/* THIS FILE IS UNDER MAJOR RECONSTRUCTION */

/* TODO:
 *  -reject paths with fp_len > 1; it's meaningless and misleading
 *  -consider merging path/object into a single argument
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
 * at some point this will be exposed, so keep a clean break from the
 * ged cmd and don't make assumptions
 */
HIDDEN int
translate(struct ged *gedp, vect_t *keypoint,
	  struct db_full_path *path,
	  struct directory *d_obj, vect_t delta,
	  int relative_pos_flag)
{
    (void) gedp;
    (void) keypoint;
    (void) path;
    (void) d_obj;
    (void) delta;
    (void) relative_pos_flag;
#if 0
    struct db_full_path full_obj_path;
    struct directory *d_to_modify = NULL;

    struct rt_db_internal intern;
    struct _ged_trace_data gtd;
    mat_t dmat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;

    /*
     * Validate parameters
     */

    /* perhaps relative positioning was enabled by mistake */
    if (relative_pos_flag && keypoint) {
	bu_vls_printf(gedp->ged_result_str,
		      "relative translations do not have keypoints");
	return GED_ERROR;
    }

    /* TODO: set reasonable default keypoint */
    if (!relative_pos_flag && !keypoint) {
	*keypoint[0] = 0.0;
	*keypoint[1] = 0.0;
	*keypoint[2] = 0.0;
    }

    /* verify existence of path */
    if (ged_path_validate(gedp, path) == GED_ERROR) {
	char *s_path = db_path_to_string(path);
	bu_vls_printf(gedp->ged_result_str, "path \"%s\" doesn't exist",
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
	bu_vls_printf(gedp->ged_result_str, "object \"%s\" not found under"
		      " path \"%s\"", d_obj->d_namep, s_path);
	bu_free((genptr_t)s_path, "path string");
	db_free_full_path(&full_obj_path);
	return GED_ERROR;
    }
    db_free_full_path(&full_obj_path);

    if (!(d_obj->d_flags & (RT_DIR_SOLID | RT_DIR_REGION | RT_DIR_COMB))) {
	bu_vls_printf(gedp->ged_result_str, "unsupported object type");
	return GED_ERROR;
    }

    /*
     * Perform translations
     */

    if (!relative_pos_flag)
	    /* 'delta' is actually an absolute position; calculate
	     * distance between it and the keypoint, so that delta
	     * really is a delta */
	    VSUB2(delta, delta, *keypoint);

    if (path->fp_len > 0) {
	/* path supplied; move obj instance only (obj's CWD
	 * modified) */
	struct rt_comb_internal *comb;
	union tree *leaf_to_modify;

	d_to_modify = DB_FULL_PATH_CUR_DIR(path);
	GED_DB_GET_INTERNAL(gedp, &intern, d_to_modify, (fastf_t *)NULL,
			    &rt_uniresource, GED_ERROR);
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	leaf_to_modify = db_find_named_leaf(comb->tree, d_obj->d_namep);
	if (leaf_to_modify == TREE_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "leaf not found where it"
			  " should be; this should not happen");
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
	
	MAT_DELTAS_ADD_VEC(leaf_to_modify->tr_l.tl_mat, delta); 
    } else {
	/* no path; move all obj instances (obj's entire tree
	 * modified) */
	d_to_modify = d_obj;
	if (_ged_get_obj_bounds2(gedp, 1, (const char **)&d_to_modify->d_namep,
				 &gtd, rpp_min, rpp_max) == GED_ERROR)
	    return GED_ERROR;
	if (!(d_to_modify->d_flags & RT_DIR_SOLID))
	    if (_ged_get_obj_bounds(gedp, 1,
				    (const char **)&d_to_modify->d_namep,
				    1, rpp_min, rpp_max) == GED_ERROR)
		return GED_ERROR;

	MAT_IDN(dmat);
	VSCALE(delta, delta, gedp->ged_wdbp->dbip->dbi_local2base);
	MAT_DELTAS_VEC(dmat, delta);

	bn_mat_inv(invXform, gtd.gtd_xform); 
	bn_mat_mul(tmpMat, invXform, dmat);
	bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

	GED_DB_GET_INTERNAL(gedp, &intern, d_to_modify, emat,
			    &rt_uniresource, GED_ERROR);
    }

    RT_CK_DB_INTERNAL(&intern);
    GED_DB_PUT_INTERNAL(gedp, d_to_modify, &intern, &rt_uniresource,
			GED_ERROR);
    rt_db_free_internal(&intern);
#endif
    return GED_OK;
}

/* ged_translate(): Proposed operations, and manual page
 *
 * Options:
 *
 * Synopsis:
 *	translate [FROM] TO object(s)
 *	translate [{-o|-c [FROM_PRIMITIVE]} | {-k x [y [z]]}] \
 *	    {{-o|-c [TO_PRIMITIVE]} | {[-r | -a] x [y [z]}} \
 *	    [PATH/]OBJECT [[PATH2/]OBJECT2] ...
 *
 * NEW Synopsis:
 *	translate [[-n] [-k {object|3dpos}]]
 *	    [[-n] -a|-r {object|3dpos}] object(s)
 *
 * Description:
 *	Used to move a primitive or combination. If all of the FROM
 *	options are ommited, the movement must be relative, and the
 *	x/y/z TO coordinates must be given. If any of the other TO
 *	options	are used, one of the FROM options must also be used.
 *	If -k is used, -a must also be used, since 
 *	Other than these two restrictions, any other combination
 *	of FROM and TO options may be used.
 *
 *	If FROM_PRIMITIVE or TO_PRIMITIVE are set to '-', the first
 *	[PATH/]OBJECT is used in its place (and must be a primitive).
 *	If any x, y, or z coordinates are set to '-', they are
 *	ignored.
 *
 * Options:
 * 	-r  relative distance (default)
 * 	-a  absolute position
 *
 * 	-o  origin of primitive
 * 	-c  center of primitive
 * 	-k  keypoint
 *
 * Examples:
 *	# Relative translations
 *
 *	    # move all instances of sph.s x+5,y+5,z+5
 *	    translate 5 5 5 /sph.s
 *
 *	    	# same as above
 *	    	translate 5 5 5 sph.s
 * 
 *	    # move instance of sph.s in bowl.c x+7,y+7
 *	    translate 7 7 bowl.c/sph.s
 *
 *	    # move instance of sph.s in bowl.c z+7
 *	    translate - - 7 bowl.c/sph.s
 * 
 *	# move all instances of bowl.c from sph.s's origin to y=5,
 *	# leaving x/z alone
 *	translate -o sph.s - 5 bowl.c	
 *
 *      # move instance of two.c from instance of sph.s's
 *      # matrix-modified origin to x=5
 *      translate -o bowl.c/sph.s 5 one.c/two.c
 *
 *	# move all bowl.c's instances and one instance of two.c from
 *	# x=-23,y=4,z=17 to x=9,y=2,z=1
 *	translate -k -23 4 17 9 2 1 bowl.c one.c/two.c
 *
 *	    # same effect as above, using relative positioning
 *	    translate 32 -2 16 bowl.c one.c/two.c
 *
 *	# move instance of sph.s from center of same instance of
 *	# sph.s to x=5,y=0,z=6
 *	translate -c bowl.c/sph.s 5 0 6 bowl.c/sph.s
 *
 *		# same as above; center this instance of sph.s on
 *		# x=5,y=0,z=6
 *		translate -c - 5 0 6 bowl.c/sph.s
 *
 *	# move instance of two.c from x=93.2 to x=-41.7
 *	translate -k 93.2 - - -41.7 - - one.c/two.c
 *
 *	    # all of these have the same end result as above
 *	    translate -k 93.2 - - -41.7 one.c/two.c
 *	    translate -k 93.2 - - -41.7 0 0 one.c/two.c
 *	    translate -k 93.2 - - -41.7 - - one.c/two.c
 *	    translate -k 93.2 0 0 -41.7 0 0 one.c/two.c
 *	    translate -k 93.2 21 32 -41.7 - - one.c/two.c
 *	    translate -k 93.2 21 32 -41.7 21 32 one.c/two.c
 * 
 *	    # same result as above, using a relative distance
 *	    translate -134.9 one.c/two.c
 *
 *	# move instance of sph.s from origin of instance of sph2.s
 *	# to center of instance of sph.s
 *	translate -o bowl.c/sph2.s -c - bowl.c/sph.s
 *
 *	# move /sph1.s from origin of instance of sph2.s to center of
 *	# instance of rcc.s 
 *	translate -o bowl.c/sph2.s -c bowl.c/rcc.s sph1.s
 */

int
ged_translate(struct ged *gedp, int argc, const char *argv[])
{
    (void) gedp;
    (void) argc;
    (void) argv;
#if 0
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    const char *const cmd_name = argv[0];
    static const char *usage = "[{-o|-c [FROM_PRIMITIVE]} | {-k x [y [z]]}]"
	" {{-o|-c [TO_PRIMITIVE]} | {x [y [z]}}"
	" [PATH/]OBJECT [[PATH2/]OBJECT2] ... )";
    static const char *skip_arg = ". ";

    int from_center_flag = 0;
    int from_origin_flag = 0;
    const char *s_from_primitive;
    struct db_full_path from_primitive;
    const char *kp_arg = NULL;        	/* keypoint argument */
    vect_t keypoint;

    int to_center_flag = 0;
    int to_origin_flag = 0;
    const char *s_to_primitive;
    struct db_full_path to_primitive;
    vect_t delta;			/* dist/pos to translate to */

    const char *s_obj[] = NULL;
    struct db_full_path obj[] = NULL;
    struct directory *d_obj[] = NULL;

    size_t i;				/* iterator */
    int c;				/* bu_getopt return value */
    char *endchr = NULL;		/* for strtod's */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    /*
     * Get short arguments
     */

    /* must be wanting help; argc < 3 is wrong too, but more helpful
     * msgs are given later, by saying which args are missing */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    bu_optind = 1; /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "o:c:k:")) != -1) {
	switch (c) {
	    case 'o':
		if (from_origin_flag) {
		    if (to_origin_flag) {
			bu_vls_printf(gedp->ged_result_str,
				      "too many -o options");
			return GED_ERROR;
		    }
		    to_origin_flag = 1;
		    s_to_primitive = bu_optarg;
		} else {
		    from_origin_flag = 1;
		    s_from_primitive = bu_optarg;
		}
		break;
	    case 'c':
		if (from_center_flag) {
		    if (to_center_flag) {
			bu_vls_printf(gedp->ged_result_str,
				      "too many -c options");
			return GED_ERROR;
		    }
		    to_origin_flag = 1;
		    s_to_primitive = bu_optarg;
		} else {
		    from_origin_flag = 1;
		    s_from_primitive = bu_optarg;
		}

		if (!(s_from_center[0] == skip_arg && kp_arg[1] == ' ')) {
		    /* that's an option, not an arg */
		    bu_vls_printf(gedp->ged_result_str,
				  "Missing argument for option -%c", bu_optopt);
		    return GED_ERROR;
		}
		break;
	    case 'k':
		rel_flag = 1;
		break;
	    default:
		/* options that require arguments */
		switch (bu_optopt) {
		    case 'k':
			bu_vls_printf(gedp->ged_result_str,
				      "Missing argument for option -%c", bu_optopt);
			return GED_ERROR;
		}

		/* unknown options */
		if (isprint(bu_optopt)) {
		    char *c2;
		    strtod((const char *)&c, &c2);
		    if (*c2 != '\0') {
			--bu_optind;
			goto no_more_args;
		    }
			/* it's neither a negative # nor an option */
			bu_vls_printf(gedp->ged_result_str,
				      "Unknown option '-%c'", bu_optopt);
			return GED_ERROR;
		} else {
		    bu_vls_printf(gedp->ged_result_str,
				  "Unknown option character '\\x%x'",
				  bu_optopt);
		    return GED_ERROR;
		}
	}
    }
no_more_args: /* for breaking out, above */

    /* 
     * Validate arguments
     */
    
    /* need to use either absolute||relative positioning; not both */
    if (abs_flag && rel_flag) {
	bu_vls_printf(gedp->ged_result_str,
		      "options '-a' and '-r' are mutually exclusive");
	return GED_ERROR;
    }

    /* set default positioning type */
    if (!abs_flag && !rel_flag)
	rel_flag = 1;
    
    /* set delta coordinates for translation */
    if ((bu_optind + 1) > argc) {
	bu_vls_printf(gedp->ged_result_str, "missing x coordinate");
	return GED_HELP;
    }
    delta[0] = strtod(argv[bu_optind], &endchr);
    if (!endchr || argv[bu_optind] == endchr) {
	bu_vls_printf(gedp->ged_result_str, "missing or invalid x coordinate");
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

    /* no args left, but more are expected */
    if ((bu_optind + 1) > argc) {
	bu_vls_printf(gedp->ged_result_str,
		      "missing object argument\n");
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    if ((bu_optind + 1) != argc)
	/* if >1 object was supplied, the first must be a path */
	s_path = argv[bu_optind++];
    if (db_string_to_path(&path, dbip, s_path) < 0) {
	bu_vls_printf(gedp->ged_result_str, "invalid path \"%s\"", s_path);
	return GED_ERROR;
    }

    /* set object (no path accepted) */
    s_obj = argv[bu_optind++];
    if (db_string_to_path(&obj, dbip, s_obj) < 0 || obj.fp_len != (size_t)1) {
	bu_vls_printf(gedp->ged_result_str, "invalid object \"%s\"",
		      s_obj);
	db_free_full_path(&path);
	return GED_ERROR;
    }

    if ((bu_optind + 1) <= argc) {
	bu_vls_printf(gedp->ged_result_str, "multiple objects not yet"
		      " supported; ");
	db_free_full_path(&path);
	db_free_full_path(&obj);
	return GED_ERROR;
    }

    /*
     * Perform translations
     */

    d_obj = DB_FULL_PATH_ROOT_DIR(&obj);
    if (!kp_arg) {
	if (translate(gedp, (vect_t *)NULL, &path, d_obj, delta,
		      rel_flag) == GED_ERROR) {
	    db_free_full_path(&path);
	    db_free_full_path(&obj);
	    bu_vls_printf(gedp->ged_result_str, "; translation failed");
	    return GED_ERROR;
	}
    } else {
	if (translate(gedp, &keypoint, &path, d_obj, delta, rel_flag) ==
	    GED_ERROR) {
	    db_free_full_path(&path);
	    db_free_full_path(&obj);
	    bu_vls_printf(gedp->ged_result_str, "; translation failed");
	    return GED_ERROR;
	}
    }

    db_free_full_path(&path);
    db_free_full_path(&obj);
#endif
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
