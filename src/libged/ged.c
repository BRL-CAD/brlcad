/*                       G E D . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2008 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file wdb_obj.c
 *
 * A quasi-object-oriented database interface.
 *
 * A database object contains the attributes and methods for
 * controlling a BRL-CAD database.
 *
 * Also include routines to allow libwdb to use librt's import/export
 * interface, rather than having to know about the database formats directly.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "ged.h"

struct ged *ged_dbopen(struct db_i *dbip, int mode);

struct db_i *ged_open_dbip(const char *filename);
int ged_decode_dbip(const char *dbip_string, struct db_i **dbipp);

struct ged *
ged_open(const char *dbtype, const char *filename)
{
    struct ged *gedp;
    struct rt_wdb *wdbp;

    if (strcmp(dbtype, "db") == 0) {
	struct db_i	*dbip;

	if ((dbip = ged_open_dbip(filename)) == DBI_NULL)
	    return GED_NULL;

	RT_CK_DBI(dbip);

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    } else if (strcmp(dbtype, "file") == 0) {
	wdbp = wdb_fopen(filename);
    } else {
	struct db_i	*dbip;

	if (ged_decode_dbip(filename, &dbip) != GED_OK)
	    return GED_NULL;

	if (strcmp(dbtype, "disk" ) == 0)
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	else if (strcmp(dbtype, "disk_append") == 0)
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
	else if (strcmp(dbtype, "inmem" ) == 0)
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	else if (strcmp(dbtype, "inmem_append" ) == 0)
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
	else {
	    bu_log("wdb_open %s target type not recognized", dbtype);
	    return GED_NULL;
	}
    }

    BU_GETSTRUCT(gedp, ged);
    BU_GETSTRUCT(gedp->ged_gdp, ged_drawable);
    gedp->ged_wdbp = wdbp;
    ged_init(gedp);

    return gedp;
}

void
ged_init(struct ged *gedp)
{
    bu_vls_init(&gedp->ged_log);
    bu_vls_init(&gedp->ged_result_str);
    ged_drawable_init(gedp->gdp);
}

void
ged_close(struct ged *gedp)
{
    wdb_close(gedp->ged_wdbp);
    ged_drawable_close(gedp->ged_gdp);

    gedp->ged_wdbp = GED_NULL;
    gedp->ged_gdp = GED_DRAWABLE_NULL;

    bu_vls_free(&gedp->ged_log);
    bu_vls_free(&gedp->ged_result_str);

    bu_free((genptr_t)gedp, "struct ged");
}

void
ged_drawable_init(struct ged_drawable *gdp)
{
    BU_LIST_INIT(&gdp->gd_headSolid);
    BU_LIST_INIT(&gdp->gd_headVDraw);
    BU_LIST_INIT(&gdp->gd_observers.l);
    BU_LIST_INIT(&gdp->gd_headRunRt.l);
    gdp->gd_freeSolids = &FreeSolid;
    gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    ged_init_qray(gdp->gdp);
}

void
ged_drawable_close(struct ged_drawable *gdp)
{
    ged_free_qray(gdp->gdp);
    bu_free((genptr_t)gdp, "struct ged_drawable");
}

/**
 * @brief
 * Open/Create the database and build the in memory directory.
 */
struct db_i *
ged_open_dbip(const char *filename)
{
    struct db_i *dbip;

    /* open database */
    if (((dbip = db_open(filename, "r+w")) == DBI_NULL) &&
	((dbip = db_open(filename, "r"  )) == DBI_NULL)) {

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(filename) && !bu_file_readable(filename)) {
	    bu_log("ged_open_dbip: %s is not readable", filename);

	    return DBI_NULL;
	}

	/* db_create does a db_dirbuild */
	if ((dbip = db_create(filename, 5)) == DBI_NULL) {
	    bu_log("ged_open_dbip: failed to create %s\n", filename);

	    return DBI_NULL;
	}
    } else
	/* --- Scan geometry database and build in-memory directory --- */
	db_dirbuild(dbip);


    return dbip;
}

int
ged_decode_dbip(const char *dbip_string, struct db_i **dbipp)
{

    *dbipp = (struct db_i *)atol(dbip_string);

    /* Could core dump */
    RT_CK_DBI(*dbipp);

    return GED_OK;
}

void
ged_print_node(struct ged		*gedp,
	       register struct directory *dp,
	       int			pathpos,
	       int			indentSize,
	       char			prefix,
	       int			cflag,
	       int                      displayDepth,
	       int                      currdisplayDepth)
{
    register int			i;
    register struct directory	*nextdp;
    struct rt_db_internal		intern;
    struct rt_comb_internal		*comb;

    if (cflag && !(dp->d_flags & DIR_COMB))
	return;

    for (i=0; i<pathpos; i++)
	if ( indentSize < 0 ) {
	    bu_vls_printf(&gedp->ged_result_str, "\t");
	} else {
	    int j;
	    for ( j=0; j<indentSize; j++ ) {
		bu_vls_printf(&gedp->ged_result_str, " ");
	    }
	}

    if (prefix)
	bu_vls_printf(&gedp->ged_result_str, "%c ", prefix);

    bu_vls_printf(&gedp->ged_result_str, "%s", dp->d_namep);
    /* Output Comb and Region flags (-F?) */
    if (dp->d_flags & DIR_COMB)
	bu_vls_printf(&gedp->ged_result_str, "/");
    if (dp->d_flags & DIR_REGION)
	bu_vls_printf(&gedp->ged_result_str, "R");

    bu_vls_printf(&gedp->ged_result_str, "\n");

    if (!(dp->d_flags & DIR_COMB))
	return;

    /*
     *  This node is a combination (eg, a directory).
     *  Process all the arcs (eg, directory members).
     */

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting");
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	int node_count;
	int actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(&gedp->ged_result_str, "Cannot flatten tree for listing");
		return;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
							       sizeof( struct rt_tree_array ), "tree list" );
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource ) - rt_tree_array;
	    BU_ASSERT_PTR( actual_count, ==, node_count );
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	for (i=0; i<actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		default:
		    op = '?';
		    break;
	    }

	    if ((nextdp = db_lookup(gedp->ged_wdbp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL) {
		int j;

		for (j=0; j<pathpos+1; j++)
		    bu_vls_printf(&gedp->ged_result_str, "\t");

		bu_vls_printf(&gedp->ged_result_str, "%c ", op);
		bu_vls_printf(&gedp->ged_result_str, "%s\n", rt_tree_array[i].tl_tree->tr_l.tl_name);
	    } else {
		if (currdisplayDepth < displayDepth) {
		    ged_print_node(gedp, nextdp, pathpos+1, indentSize, op, cflag, displayDepth, currdisplayDepth+1);
		}
	    }
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	if (rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern, &rt_uniresource);
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
