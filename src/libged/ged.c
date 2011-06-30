/*                       G E D . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @file libged/ged.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "plot3.h"
#include "mater.h"
#include "solid.h"

#include "./ged_private.h"
#include "./qray.h"


/* FIXME: this function should not exist.  passing pointers as strings
 * indicates a failure in design and lazy coding.
 */
int
ged_decode_dbip(const char *dbip_string, struct db_i **dbipp)
{
    if (sscanf(dbip_string, "%p", (void **)dbipp) != 1) {
	return GED_ERROR;
    }

    /* Could core dump */
    RT_CK_DBI(*dbipp);

    return GED_OK;
}


void
ged_close(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    if (gedp->ged_wdbp) {
	wdb_close(gedp->ged_wdbp);
	gedp->ged_wdbp = RT_WDB_NULL;
    }

    if (gedp->ged_gdp != GED_DRAWABLE_NULL) {
	qray_free(gedp->ged_gdp);
	bu_free((genptr_t)gedp->ged_gdp, "struct ged_drawable");
	gedp->ged_gdp = GED_DRAWABLE_NULL;
    }

    ged_free(gedp);
    bu_free(gedp);
}


void
ged_free(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    gedp->ged_wdbp = RT_WDB_NULL;
    gedp->ged_gdp = GED_DRAWABLE_NULL;

    if (gedp->ged_log) {
	bu_vls_free(gedp->ged_log);
	bu_free(gedp->ged_log);
	gedp->ged_log = NULL; /* sanity */
    }

    if (gedp->ged_result_str) {
	bu_vls_free(gedp->ged_result_str);
	bu_free(gedp->ged_result_str);
	gedp->ged_result_str = NULL; /* sanity */
    }

    if (gedp->ged_gdp) {
	bu_free(gedp->ged_gdp, "release ged_gdp");
	gedp->ged_gdp = NULL; /* sanity */
    }

    bu_free((genptr_t)gedp, "struct ged");
}


void
ged_init(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    BU_LIST_INIT(&gedp->l);
    gedp->ged_wdbp = RT_WDB_NULL;

    bu_vls_init(gedp->ged_log);
    bu_vls_init(gedp->ged_result_str);

    BU_GETSTRUCT(gedp->ged_gdp, ged_drawable);
    BU_LIST_INIT(&gedp->ged_gdp->gd_headDisplay);
    BU_LIST_INIT(&gedp->ged_gdp->gd_headVDraw);
    BU_LIST_INIT(&gedp->ged_gdp->gd_headRunRt.l);

    /* yuck */
    if (!BU_LIST_IS_INITIALIZED(&_FreeSolid.l)) {
	BU_LIST_INIT(&_FreeSolid.l);
    }

    gedp->ged_gdp->gd_freeSolids = &_FreeSolid;
    gedp->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    qray_init(gedp->ged_gdp);

    /* (in)sanity */
    gedp->ged_gvp = NULL;
    gedp->ged_fbsp = NULL;
    gedp->ged_dmp = NULL;
    gedp->ged_refresh_clientdata = NULL;
    gedp->ged_refresh_handler = NULL;
    gedp->ged_output_handler = NULL;
    gedp->ged_output_script = NULL;
    gedp->ged_internal_call = 0;

}


void
ged_view_init(struct ged_view *gvp)
{
    if (gvp == GED_VIEW_NULL)
	return;

    gvp->gv_scale = 500.0;
    gvp->gv_size = 2.0 * gvp->gv_scale;
    gvp->gv_isize = 1.0 / gvp->gv_size;
    VSET(gvp->gv_aet, 35.0, 25.0, 0.0);
    VSET(gvp->gv_eye_pos, 0.0, 0.0, 1.0);
    MAT_IDN(gvp->gv_rotation);
    MAT_IDN(gvp->gv_center);
    VSETALL(gvp->gv_keypoint, 0.0);
    gvp->gv_coord = 'v';
    gvp->gv_rotate_about = 'v';
    gvp->gv_minMouseDelta = -20;
    gvp->gv_maxMouseDelta = 20;
    gvp->gv_rscale = 0.4;
    gvp->gv_sscale = 2.0;

    gvp->gv_adc.gas_a1 = 45.0;
    gvp->gv_adc.gas_a2 = 45.0;
    VSET(gvp->gv_adc.gas_line_color, 255, 255, 0);
    VSET(gvp->gv_adc.gas_tick_color, 255, 255, 255);

    VSET(gvp->gv_grid.ggs_anchor, 0.0, 0.0, 0.0);
    gvp->gv_grid.ggs_res_h = 1.0;
    gvp->gv_grid.ggs_res_v = 1.0;
    gvp->gv_grid.ggs_res_major_h = 5;
    gvp->gv_grid.ggs_res_major_v = 5;
    VSET(gvp->gv_grid.ggs_color, 255, 255, 255);

    gvp->gv_rect.grs_draw = 0;
    gvp->gv_rect.grs_pos[0] = 128;
    gvp->gv_rect.grs_pos[1] = 128;
    gvp->gv_rect.grs_dim[0] = 256;
    gvp->gv_rect.grs_dim[1] = 256;
    VSET(gvp->gv_rect.grs_color, 255, 255, 255);

    gvp->gv_view_axes.gas_draw = 0;
    VSET(gvp->gv_view_axes.gas_axes_pos, 0.85, -0.85, 0.0);
    gvp->gv_view_axes.gas_axes_size = 0.2;
    gvp->gv_view_axes.gas_line_width = 0;
    gvp->gv_view_axes.gas_pos_only = 1;
    VSET(gvp->gv_view_axes.gas_axes_color, 255, 255, 255);
    VSET(gvp->gv_view_axes.gas_label_color, 255, 255, 0);
    gvp->gv_view_axes.gas_triple_color = 1;

    gvp->gv_model_axes.gas_draw = 0;
    VSET(gvp->gv_model_axes.gas_axes_pos, 0.0, 0.0, 0.0);
    gvp->gv_model_axes.gas_axes_size = 2.0;
    gvp->gv_model_axes.gas_line_width = 0;
    gvp->gv_model_axes.gas_pos_only = 0;
    VSET(gvp->gv_model_axes.gas_axes_color, 255, 255, 255);
    VSET(gvp->gv_model_axes.gas_label_color, 255, 255, 0);
    gvp->gv_model_axes.gas_triple_color = 0;
    gvp->gv_model_axes.gas_tick_enabled = 1;
    gvp->gv_model_axes.gas_tick_length = 4;
    gvp->gv_model_axes.gas_tick_major_length = 8;
    gvp->gv_model_axes.gas_tick_interval = 100;
    gvp->gv_model_axes.gas_ticks_per_major = 10;
    gvp->gv_model_axes.gas_tick_threshold = 8;
    VSET(gvp->gv_model_axes.gas_tick_color, 255, 255, 0);
    VSET(gvp->gv_model_axes.gas_tick_major_color, 255, 0, 0);

    gvp->gv_center_dot.gos_draw = 0;
    VSET(gvp->gv_center_dot.gos_line_color, 255, 255, 0);

    gvp->gv_prim_labels.gos_draw = 0;
    VSET(gvp->gv_prim_labels.gos_text_color, 255, 255, 0);

    gvp->gv_view_params.gos_draw = 0;
    VSET(gvp->gv_view_params.gos_text_color, 255, 255, 0);

    gvp->gv_view_scale.gos_draw = 0;
    VSET(gvp->gv_view_scale.gos_line_color, 255, 255, 0);
    VSET(gvp->gv_view_scale.gos_text_color, 255, 255, 255);

    /* FIXME: this causes the shaders.sh regression to fail */
    /* _ged_mat_aet(gvp); */

    ged_view_update(gvp);
}


struct ged *
ged_open(const char *dbtype, const char *filename, int existing_only)
{
    struct ged *gedp;
    struct rt_wdb *wdbp;
    struct mater *save_materp = MATER_NULL;

    save_materp = rt_material_head();
    rt_new_material_head(MATER_NULL);

    if (BU_STR_EQUAL(dbtype, "db")) {
	struct db_i *dbip;

	if ((dbip = _ged_open_dbip(filename, existing_only)) == DBI_NULL) {
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

	    return GED_NULL;
	}

	RT_CK_DBI(dbip);

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    } else if (BU_STR_EQUAL(dbtype, "file")) {
	wdbp = wdb_fopen(filename);
    } else {
	struct db_i *dbip;

	/* FIXME: this call should not exist.  passing pointers as
	 * strings indicates a failure in design and lazy coding.
	 */
	if (sscanf(filename, "%p", (void **)&dbip) != 1) {
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

	    return GED_NULL;
	}

	if (dbip == DBI_NULL) {
	    int i;

	    BU_GETSTRUCT(dbip, db_i);
	    dbip->dbi_eof = (off_t)-1L;
	    dbip->dbi_fp = NULL;
	    dbip->dbi_mf = NULL;
	    dbip->dbi_read_only = 0;

	    /* Initialize fields */
	    for (i=0; i<RT_DBNHASH; i++) {
		dbip->dbi_Head[i] = RT_DIR_NULL;
	    }

	    dbip->dbi_local2base = 1.0;		/* mm */
	    dbip->dbi_base2local = 1.0;
	    dbip->dbi_title = bu_strdup("Untitled BRL-CAD Database");
	    dbip->dbi_uses = 1;
	    dbip->dbi_filename = NULL;
	    dbip->dbi_filepath = NULL;
	    dbip->dbi_version = 5;

	    bu_ptbl_init(&dbip->dbi_clients, 128, "dbi_clients[]");
	    dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */
	}

	/* Could core dump */
	RT_CK_DBI(dbip);

	if (BU_STR_EQUAL(dbtype, "disk"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	else if (BU_STR_EQUAL(dbtype, "disk_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
	else if (BU_STR_EQUAL(dbtype, "inmem"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	else if (BU_STR_EQUAL(dbtype, "inmem_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
	else {
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

	    bu_log("wdb_open %s target type not recognized", dbtype);
	    return GED_NULL;
	}
    }

    BU_GETSTRUCT(gedp, ged);
    GED_INIT(gedp, wdbp);

    return gedp;
}


/**
 * @brief
 * Open/Create the database and build the in memory directory.
 */
struct db_i *
_ged_open_dbip(const char *filename, int existing_only)
{
    struct db_i *dbip;

    /* open database */
    if (((dbip = db_open(filename, "r+w")) == DBI_NULL) &&
	((dbip = db_open(filename, "r")) == DBI_NULL)) {

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(filename) && !bu_file_readable(filename)) {
	    bu_log("_ged_open_dbip: %s is not readable", filename);

	    return DBI_NULL;
	}

	if (existing_only)
	    return DBI_NULL;

	/* db_create does a db_dirbuild */
	if ((dbip = db_create(filename, 5)) == DBI_NULL) {
	    bu_log("_ged_open_dbip: failed to create %s\n", filename);

	    return DBI_NULL;
	}
    } else
	/* --- Scan geometry database and build in-memory directory --- */
	db_dirbuild(dbip);


    return dbip;
}


void
_ged_print_node(struct ged *gedp,
		struct directory *dp,
		size_t pathpos,
		int indentSize,
		char prefix,
		unsigned flags,
		int displayDepth,
		int currdisplayDepth)
{
    size_t i;
    struct directory *nextdp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    unsigned aflag = (flags & _GED_TREE_AFLAG);
    unsigned cflag = (flags & _GED_TREE_CFLAG);
    struct bu_vls tmp_str;

    /* cflag = don't show shapes, so return if this is not a combination */
    if (cflag && !(dp->d_flags & RT_DIR_COMB)) {
	return;
    }

    /* need another string for aflag */
    if (aflag)
	bu_vls_init(&tmp_str);

    /* set up spacing from the left margin */
    for (i = 0; i < pathpos; i++) {
	if (indentSize < 0) {
	    bu_vls_printf(gedp->ged_result_str, "\t");
	    if (aflag)
		bu_vls_printf(&tmp_str, "\t");

	} else {
	    int j;
	    for (j = 0; j < indentSize; j++) {
		bu_vls_printf(gedp->ged_result_str, " ");
		if (aflag)
		    bu_vls_printf(&tmp_str, " ");
	    }
	}
    }

    /* add the prefix if desired */
    if (prefix) {
	bu_vls_printf(gedp->ged_result_str, "%c ", prefix);
	if (aflag)
	    bu_vls_printf(&tmp_str, " ");
    }

    /* now the object name */
    bu_vls_printf(gedp->ged_result_str, "%s", dp->d_namep);

    /* suffix name if appropriate */
    /* Output Comb and Region flags (-F?) */
    if (dp->d_flags & RT_DIR_COMB)
	bu_vls_printf(gedp->ged_result_str, "/");
    if (dp->d_flags & RT_DIR_REGION)
	bu_vls_printf(gedp->ged_result_str, "R");

    bu_vls_printf(gedp->ged_result_str, "\n");

    /* output attributes if any and if desired */
    if (aflag) {
	struct bu_attribute_value_set avs;
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	    /* need a bombing macro or set an error code here: return GED_ERROR; */
	    bu_vls_free(&tmp_str);
	    return;
	}

	/* list all the attributes, if any */
	if (avs.count) {
	    struct bu_attribute_value_pair *avpp;
	    int max_attr_name_len = 0;

	    /* sort attribute-value set array by attribute name */
	    qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _ged_cmpattr);

	    for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
		int len = (int)strlen(avpp->name);
		if (len > max_attr_name_len) {
		    max_attr_name_len = len;
		}
	    }
	    for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
		bu_vls_printf(gedp->ged_result_str, "%s       @ %-*.*s    %s\n",
			      tmp_str.vls_str,
			      max_attr_name_len, max_attr_name_len,
			      avpp->name, avpp->value);
	    }
	}
	bu_vls_free(&tmp_str);
    }

    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    /*
     * This node is a combination (eg, a directory).
     * Process all the arcs (eg, directory members).
     */

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for listing");
		return;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	for (i = 0; i < actual_count; i++) {
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

	    if ((nextdp = db_lookup(gedp->ged_wdbp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
		size_t j;

		for (j=0; j<pathpos+1; j++)
		    bu_vls_printf(gedp->ged_result_str, "\t");

		bu_vls_printf(gedp->ged_result_str, "%c ", op);
		bu_vls_printf(gedp->ged_result_str, "%s\n", rt_tree_array[i].tl_tree->tr_l.tl_name);
	    } else {
		if (currdisplayDepth < displayDepth) {
		    _ged_print_node(gedp, nextdp, pathpos+1, indentSize, op, flags, displayDepth, currdisplayDepth+1);
		}
	    }
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	if (rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);
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
