/*                   D B _ F U L L P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2022 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_fullpath.c
 *
 * Routines to manipulate "db_full_path" structures
 *
 */

#include "common.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "raytrace.h"


void
db_full_path_init(struct db_full_path *pathp)
{
    if (!pathp)
	return;

    pathp->fp_len = 0;
    pathp->fp_maxlen = 0;
    pathp->fp_names = (struct directory **)NULL;
    pathp->fp_bool = (int *)NULL;
    pathp->fp_cinst = (int *)NULL;
    pathp->magic = DB_FULL_PATH_MAGIC;
}


void
db_add_node_to_full_path(struct db_full_path *pp, struct directory *dp)
{
    RT_CK_FULL_PATH(pp);

    if (!dp)
	return;

    RT_CK_DIR(dp);

    if (pp->fp_maxlen <= 0) {
	pp->fp_maxlen = 32;
	pp->fp_names = (struct directory **)bu_malloc(
	    pp->fp_maxlen * sizeof(struct directory *),
	    "db_full_path array");
	pp->fp_bool = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_full_path bool array");
	pp->fp_cinst = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_full_path comb index array");
    } else if (pp->fp_len >= pp->fp_maxlen) {
	pp->fp_maxlen *= 4;
	pp->fp_names = (struct directory **)bu_realloc(
	    (char *)pp->fp_names,
	    pp->fp_maxlen * sizeof(struct directory *),
	    "enlarged db_full_path array");
	pp->fp_bool = (int *)bu_realloc(
	    (char *)pp->fp_bool,
	    pp->fp_maxlen * sizeof(int),
	    "enlarged db_full_path bool array");
	pp->fp_cinst = (int *)bu_realloc(
	    (char *)pp->fp_cinst,
	    pp->fp_maxlen * sizeof(int),
	    "enlarged db_full_path cinst array");
    }
    pp->fp_names[pp->fp_len++] = dp;
}


void
db_dup_full_path(struct db_full_path *newp, const struct db_full_path *oldp)
{
    RT_CK_FULL_PATH(newp);
    RT_CK_FULL_PATH(oldp);

    newp->fp_maxlen = oldp->fp_maxlen;
    newp->fp_len = oldp->fp_len;
    if (oldp->fp_len <= 0) {
	newp->fp_names = (struct directory **)0;
	newp->fp_bool = (int *)0;
	newp->fp_cinst = (int *)0;
	return;
    }
    newp->fp_names = (struct directory **)bu_malloc(
	newp->fp_maxlen * sizeof(struct directory *),
	"db_full_path array (duplicate)");
    memcpy((char *)newp->fp_names, (char *)oldp->fp_names, newp->fp_len * sizeof(struct directory *));
    newp->fp_bool = (int *)bu_malloc(
	newp->fp_maxlen * sizeof(int),
	"db_full_path bool array (duplicate)");
    memcpy((char *)newp->fp_bool, (char *)oldp->fp_bool, newp->fp_len * sizeof(int));
    newp->fp_cinst = (int *)bu_malloc(
	newp->fp_maxlen * sizeof(int),
	"db_full_path cinst array (duplicate)");
    memcpy((char *)newp->fp_cinst, (char *)oldp->fp_cinst, newp->fp_len * sizeof(int));
}


void
db_extend_full_path(struct db_full_path *pathp, size_t incr)
{
    size_t newlen;

    RT_CK_FULL_PATH(pathp);

    if (pathp->fp_maxlen <= 0) {
	pathp->fp_len = 0;
	pathp->fp_maxlen = incr;
	pathp->fp_names = (struct directory **)bu_malloc(
	    pathp->fp_maxlen * sizeof(struct directory *),
	    "empty fp_names extension");
	pathp->fp_bool = (int *)bu_malloc(
	    pathp->fp_maxlen * sizeof(int),
	    "empty fp_bool bool extension");
	pathp->fp_cinst = (int *)bu_malloc(
	    pathp->fp_maxlen * sizeof(int),
	    "empty fp_cinst cinst extension");
	return;
    }

    newlen = pathp->fp_len + incr;
    if (pathp->fp_maxlen < newlen) {
	pathp->fp_maxlen = newlen+1;
	pathp->fp_names = (struct directory **)bu_realloc(
	    (char *)pathp->fp_names,
	    pathp->fp_maxlen * sizeof(struct directory *),
	    "fp_names extension");
	pathp->fp_bool = (int *)bu_realloc(
	    (char *)pathp->fp_bool,
	    pathp->fp_maxlen * sizeof(int),
	    "fp_names bool extension");
	pathp->fp_cinst = (int *)bu_realloc(
		(char *)pathp->fp_cinst,
		pathp->fp_maxlen * sizeof(int),
		"fp_names cinst extension");
    }
}


void
db_append_full_path(struct db_full_path *dest, const struct db_full_path *src)
{
    if (!src)
	return;

    RT_CK_FULL_PATH(dest);
    RT_CK_FULL_PATH(src);

    db_extend_full_path(dest, src->fp_len);
    memcpy((char *)&dest->fp_names[dest->fp_len],
	   (char *)&src->fp_names[0],
	   src->fp_len * sizeof(struct directory *));
    memcpy((char *)&dest->fp_bool[dest->fp_len],
	   (char *)&src->fp_bool[0],
	   src->fp_len * sizeof(int));
    memcpy((char *)&dest->fp_cinst[dest->fp_len],
	   (char *)&src->fp_cinst[0],
	   src->fp_len * sizeof(int));
    dest->fp_len += src->fp_len;
}


void
db_dup_path_tail(struct db_full_path *newp, const struct db_full_path *oldp, b_off_t start)
{
    RT_CK_FULL_PATH(newp);
    RT_CK_FULL_PATH(oldp);

    if (start < 0 || (size_t)start > oldp->fp_len-1)
	bu_bomb("db_dup_path_tail: start offset out of range\n");

    newp->fp_maxlen = newp->fp_len = oldp->fp_len - start;
    if (newp->fp_len <= 0) {
	newp->fp_names = (struct directory **)0;
	newp->fp_bool = (int *)0;
	return;
    }
    newp->fp_names = (struct directory **)bu_malloc(
	newp->fp_maxlen * sizeof(struct directory *),
	"db_full_path array (duplicate)");
    memcpy((char *)newp->fp_names, (char *)&oldp->fp_names[start], newp->fp_len * sizeof(struct directory *));
    newp->fp_bool = (int *)bu_malloc(
	newp->fp_maxlen * sizeof(int),
	"db_full_path bool array (duplicate)");
    memcpy((char *)newp->fp_bool, (char *)&oldp->fp_bool[start], newp->fp_len * sizeof(int));
    newp->fp_cinst = (int *)bu_malloc(
	newp->fp_maxlen * sizeof(int),
	"db_full_path cinst array (duplicate)");
    memcpy((char *)newp->fp_cinst, (char *)&oldp->fp_cinst[start], newp->fp_len * sizeof(int));
}


void
db_fullpath_to_vls(struct bu_vls *vls, const struct db_full_path *full_path, const struct db_i *dbip, int fp_flags)
{
    size_t i;
    int type;
    const struct bn_tol tol = BG_TOL_INIT;

    if (!full_path || full_path->fp_len == 0)
	return;

    BU_CK_VLS(vls);
    RT_CK_FULL_PATH(full_path);

    if (full_path->fp_names == NULL) {
	bu_vls_strcat(vls, "**NULL**");
	return;
    }

    if ((fp_flags & DB_FP_PRINT_TYPE) && !dbip) {
	bu_log("WARNING: requested object type printing, but database is unavailable\n\tObject types will not be printed!");
    }

    for (i = 0; i < full_path->fp_len; i++) {
	bu_vls_putc(vls, '/');
	if (fp_flags & DB_FP_PRINT_BOOL) {
	    switch (full_path->fp_bool[i]) {
		case 2:
		    bu_vls_strcat(vls, "u ");
		    break;
		case 3:
		    bu_vls_strcat(vls, "+ ");
		    break;
		case 4:
		    bu_vls_strcat(vls, "- ");
		    break;
	    }
	}
	bu_vls_strcat(vls, full_path->fp_names[i]->d_namep);
	if (fp_flags & DB_FP_PRINT_COMB_INDEX) {
	    if (full_path->fp_cinst[i])
		bu_vls_printf(vls, "@%d", full_path->fp_cinst[i]);
	}

	if ((fp_flags & DB_FP_PRINT_TYPE) && dbip) {
	    struct rt_db_internal intern;
	    if (!(rt_db_get_internal(&intern, full_path->fp_names[i], dbip, NULL, &rt_uniresource) < 0)) {
		    bu_vls_putc(vls, '(');
		    switch (intern.idb_minor_type) {
			case DB5_MINORTYPE_BRLCAD_ARB8:
			    type = rt_arb_std_type(&intern, &tol);
			    switch (type) {
				case 4:
				    bu_vls_strcat(vls, "arb4");
				    break;
				case 5:
				    bu_vls_strcat(vls, "arb5");
				    break;
				case 6:
				    bu_vls_strcat(vls, "arb6");
				    break;
				case 7:
				    bu_vls_strcat(vls, "arb7");
				    break;
				case 8:
				    bu_vls_strcat(vls, "arb8");
				    break;
				default:
				    break;
			    }
			    break;
			case DB5_MINORTYPE_BRLCAD_COMBINATION:
			    if (full_path->fp_names[i]->d_flags & RT_DIR_REGION) {
				bu_vls_putc(vls, 'r');
			    } else {
				bu_vls_putc(vls, 'c');
			    }
			    break;
			default:
			    bu_vls_strcat(vls, intern.idb_meth->ft_label);
			    break;
		    }

		bu_vls_putc(vls, ')');
		rt_db_free_internal(&intern);
	    }
	}

    }
}


void
db_pr_full_path(const char *msg, const struct db_full_path *pathp)
{
    char *sofar;

    if (!msg)
	msg = "FULL PATH: ";

    if (pathp) {
	sofar = db_path_to_string(pathp);
	bu_log("%s%s\n", msg, sofar);
	bu_free(sofar, "path string");
    } else {
	bu_log("%s**NULL**\n", msg);
    }

}

void
db_free_full_path(struct db_full_path *pp)
{
    if (!pp)
	return;

    RT_CK_FULL_PATH(pp);

    if (pp->fp_maxlen > 0) {
	if (pp->fp_names)
	    bu_free((char *)pp->fp_names, "db_full_path array");
	if (pp->fp_bool)
	    bu_free((char *)pp->fp_bool, "db_full_path bool array");
	if (pp->fp_cinst)
	    bu_free((char *)pp->fp_cinst, "db_full_path cinst array");
	pp->fp_maxlen = pp->fp_len = 0;
	pp->fp_names = (struct directory **)0;
    }
}


int
db_identical_full_paths(const struct db_full_path *a, const struct db_full_path *b)
{
    long i;

    if (!a && !b)
	return 1;
    else if (!a || !b)
	return 0;

    RT_CK_FULL_PATH(a);
    RT_CK_FULL_PATH(b);
    BU_ASSERT(a->fp_len < LONG_MAX);

    if (a->fp_len != b->fp_len) return 0;

    for (i = a->fp_len-1; i >= 0; i--) {
	if (a->fp_names[i] != b->fp_names[i]) return 0;
	if (a->fp_cinst[i] != b->fp_cinst[i]) return 0;
    }
    return 1;
}


int
db_full_path_subset(
    const struct db_full_path *a,
    const struct db_full_path *b,
    const int skip_first)
{
    size_t i = 0;

    RT_CK_FULL_PATH(a);
    RT_CK_FULL_PATH(b);

    if (b->fp_len > a->fp_len)
	return 0;

    if (skip_first)
	i = 1;
    else
	i = 0;

    for (; i < a->fp_len; i++) {
	size_t j;

	if (a->fp_names[i] != b->fp_names[0])
	    continue;

	if (a->fp_cinst[i] != b->fp_cinst[0])
	    continue;

	/* First element matches, check remaining length */
	if (b->fp_len > a->fp_len - i)
	    return 0;

	/* Check remainder of 'b' */
	for (j = 1; j < b->fp_len; j++) {
	    if (a->fp_names[i+j] != b->fp_names[j]) goto step;
	    if (a->fp_cinst[i+j] != b->fp_cinst[j]) goto step;
	}

	/* 'b' is a proper subset */
	return 1;

    step:
	;
    }
    return 0;
}


int
db_full_path_match_top(
    const struct db_full_path *a,
    const struct db_full_path *b)
{
    size_t i;

    RT_CK_FULL_PATH(a);
    RT_CK_FULL_PATH(b);

    if (a->fp_len > b->fp_len) return 0;

    for (i = 0; i < a->fp_len; i++) {
	if (a->fp_names[i] != b->fp_names[i]) return 0;
	if (a->fp_cinst[i] != b->fp_cinst[i]) return 0;
    }

    return 1;
}


int
db_full_path_search(const struct db_full_path *a, const struct directory *dp)
{
    long i;

    if (!dp)
	return 0;

    RT_CK_FULL_PATH(a);
    RT_CK_DIR(dp);
    BU_ASSERT(a->fp_len < LONG_MAX);

    for (i = a->fp_len-1; i >= 0; i--) {
	if (a->fp_names[i] == dp) return 1;
    }
    return 0;
}

int
cyclic_path(const struct db_full_path *fp, const char *test_name, long int depth)
{
    if (!test_name || !fp || depth < 0)
	return 0;

    /* Check the path starting at depth to see if we have a match. */
    while (depth >= 0) {
	if (BU_STR_EQUAL(test_name, fp->fp_names[depth]->d_namep)) {
	    return 1;
	}
	depth--;
    }

    /* not cyclic */
    return 0;
}

int
db_full_path_cyclic(const struct db_full_path *fp, const char *lname, int full_check)
{
    long int depth;
    const char *test_name;
    const struct directory *dp;

    if (!fp)
	return 0;

    RT_CK_FULL_PATH(fp);

    depth = fp->fp_len - 1;

    if (!full_check) {
	if (lname) {
	    test_name = lname;
	} else {
	    dp = DB_FULL_PATH_CUR_DIR(fp);
	    if (!dp)
		return 0;

	    test_name = dp->d_namep;
	    depth--;
	}

	return cyclic_path(fp, test_name, depth);
    }

    /* full_check is set - check everything */
    if (lname) {
	if (cyclic_path(fp, lname, depth)) return 1;
    }


    /* Check each element in the path against all elements
     * above it - the first instance of a cycle ends
     * the check. */
    while (depth > 0) {
	test_name = fp->fp_names[depth]->d_namep;
	depth--;
	if (cyclic_path(fp, test_name, depth)) return 1;
    }

    /* not cyclic */
    return 0;
}


// NOTE - this will probably wind up being expensive enough that we'll
// still want to track it through the draw tree walk - the point of this
// function is to a) let us get the information locally when we're NOT
// doing a tree walk and b) concisely and clearly outline ALL the rules
// for setting object color.
//
// See if we can just use the attributes - not sure if we need to crack
// the comb, but it's possible (particularly if attributes aren't synced)
void
db_full_path_color(
	struct bu_color *c,
	struct db_full_path *pathp,
	struct db_i *dbip,
	struct resource *UNUSED(resp))
{
    RT_CHECK_DBI(dbip);
    RT_CK_FULL_PATH(pathp);

    if (!c)
	return;

    // If nothing else is found, the color is set to the default
    unsigned char rgb_default[3] = {255, 0, 0};
    bu_color_from_rgb_chars(c, rgb_default);

    // Things we have to check for:
    //
    // 1. region_id attribute (only with region flag?) + rt_material_head table (see color_soltab)
    // 2. color attribute
    // 3. rgb attribute
    // 4. inherit attribute - if set, children don't override parent
    for (size_t i = 0; i < pathp->fp_len; i++) {
	int have_color = 0;
	struct bu_attribute_value_set c_avs;
	db5_get_attributes(dbip, &c_avs, pathp->fp_names[i]);

	// Inherit flag tells us whether this dp overrides its children
	int inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;

	if (rt_material_head()) {
	    // TODO - if region_id is set but region flag isn't, do we still
	    // use rt_material_head to color?
	    int region_id = -1;
	    const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	    if (region_id_val) {
		bu_opt_int(NULL, 1, &region_id_val, (void *)&region_id);
	    } else if (pathp->fp_names[i]->d_flags & RT_DIR_REGION) {
		// If we have a region flag but no region_id, for color table
		// purposes treat the region_id as 0
		region_id = 0;
	    }
	    if (region_id >= 0) {
		// If we have both a region_id and an rt_material_head table, that is (?) highest precedence
		// for color?
		const struct mater *mp;
		for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		    if (region_id > mp->mt_high || region_id < mp->mt_low) {
			continue;
		    }
		    unsigned char mt[3];
		    mt[0] = mp->mt_r;
		    mt[1] = mp->mt_g;
		    mt[2] = mp->mt_b;
		    bu_color_from_rgb_chars(c, mt);
		}
		// TODO - do we stop only if we have a color with inherit?
		if (have_color && inherit) {
		    // Inherit flag set, no point checking child nodes
		    bu_avs_free(&c_avs);
		    return;
		} else {
		    continue;
		}
	    }

	}

	// If we aren't overridden by region_id, check for locally set colors
	const char *color_val = bu_avs_get(&c_avs, "color");
	if (!color_val) {
	    color_val = bu_avs_get(&c_avs, "rgb");
	}
	if (color_val) {
	    bu_opt_color(NULL, 1, &color_val, (void *)c);
	    have_color = 1;
	}

	bu_avs_free(&c_avs);

	// If we have inherit (TODO - do we also have to have a non-default
	// color?), there's no point in checking children for more settings -
	// they will not override.
	if (have_color && inherit)
	    return;
    }

}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
