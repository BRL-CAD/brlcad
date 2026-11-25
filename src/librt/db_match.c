/*                      D B _ M A T C H . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file librt/db_match.c
 *
 * Routines to determine if a string matches a given pattern.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/path.h"
#include "vmath.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "librt_private.h"

static void
db_count_reference(struct db_i *dbip, struct directory *parent, const char *name, db_op_t op, matp_t matrix)
{
    struct directory *target = db_lookup(dbip, name, LOOKUP_QUIET);
    if (target)
	++target->d_nref;
    if (BU_PTBL_IS_INITIALIZED(&dbip->i->dbi_update_nref_clbks)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&dbip->i->dbi_update_nref_clbks); ++i) {
	    struct dbi_update_nref_clbk *cb = (struct dbi_update_nref_clbk *)BU_PTBL_GET(&dbip->i->dbi_update_nref_clbks, i);
	    (*cb->f)(dbip, parent, target, name, op, matrix, cb->u_data);
	}
    }
}

static void
db_count_grid_reference(struct db_i *dbip, struct directory *dp)
{
    if (db_version(dbip) != 5)
	return;

    /* Grid dependencies are present even if their sample data is missing.
     * Counting the edge only requires the primitive's DB5 parameters. */
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    struct db5_raw_internal raw;
    if (db_get_external(&record, dp, dbip) == 0 &&
	db5_get_raw_internal_bounded(&raw, record.ext_buf, record.ext_nbytes) &&
	raw.object_length == record.ext_nbytes && raw.major_type == dp->d_major_type &&
	raw.minor_type == dp->d_minor_type && raw.b_zzz == DB5_ZZZ_UNCOMPRESSED) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	const int found = raw.minor_type == DB5_MINORTYPE_BRLCAD_DSP ?
	    rt_dsp_database_reference(&raw.body, &name) : rt_vol_database_reference(&raw.body, &name);
	if (found)
	    db_count_reference(dbip, dp, bu_vls_cstr(&name), DB_OP_UNION, NULL);
	bu_vls_free(&name);
    }
    bu_free_external(&record);
}

static void
db_count_refs(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, void *pdp, void *UNUSED(dummy2), void *UNUSED(dummy3), void *UNUSED(dummy4))
{
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);

    db_op_t op = DB_OP_UNION;
    if (comb_leaf->tr_l.tl_op == OP_SUBTRACT)
	op = DB_OP_SUBTRACT;
    else if (comb_leaf->tr_l.tl_op == OP_INTERSECT)
	op = DB_OP_INTERSECT;
    db_count_reference(dbip, (struct directory *)pdp, comb_leaf->tr_l.tl_name, op, comb_leaf->tr_l.tl_mat);
}

void
db_update_nref(struct db_i *dbip)
{
    register struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    RT_CK_DBI(dbip);

    /* First, clear any existing counts */
    FOR_ALL_DIRECTORY_START(dp, dbip)
	dp->d_nref = 0;
    FOR_ALL_DIRECTORY_END;

    /* Do a NULL + union callback to indicate the start of an update cycle */
    for (size_t j = 0; j < BU_PTBL_LEN(&dbip->i->dbi_update_nref_clbks); j++) {
	struct dbi_update_nref_clbk *cb = (struct dbi_update_nref_clbk *)BU_PTBL_GET(&dbip->i->dbi_update_nref_clbks, j);
	(*cb->f)(dbip, NULL, NULL, NULL, DB_OP_UNION, NULL, cb->u_data);
    }

    /* Examine all COMB nodes */
    FOR_ALL_DIRECTORY_START(dp, dbip)

	    /* handle non-combination objects that reference other objects */
	    if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD) {
		/* initialize for good measure */
		RT_DB_INTERNAL_INIT(&intern);

		if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
		    struct rt_extrude_internal *extr;

		    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
			continue;
		    extr = (struct rt_extrude_internal *)intern.idb_ptr;
		    RT_EXTRUDE_CK_MAGIC(extr);
		    if (extr->sketch_name)
			db_count_reference(dbip, dp, extr->sketch_name, DB_OP_UNION, NULL);
		    rt_db_free_internal(&intern);
		} else if (dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_REVOLVE) {
		    struct rt_revolve_internal *revolve;

		    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
			continue;
		    revolve = (struct rt_revolve_internal *)intern.idb_ptr;
		    RT_REVOLVE_CK_MAGIC(revolve);
		    if (bu_vls_strlen(&revolve->sketch_name) > 0)
			db_count_reference(dbip, dp, bu_vls_cstr(&revolve->sketch_name), DB_OP_UNION, NULL);
		    rt_db_free_internal(&intern);
		} else if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP ||
			   dp->d_minor_type == DB5_MINORTYPE_BRLCAD_VOL) {
		    db_count_grid_reference(dbip, dp);
		}
	    }
	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;
	    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
		continue;
	    if (intern.idb_type != ID_COMBINATION) {
		bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n",
		       dp->d_namep);
		dp->d_flags &= ~RT_DIR_COMB;
		rt_db_free_internal(&intern);
		continue;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    db_tree_funcleaf(dbip, comb, comb->tree,
			     db_count_refs, (void *)dp,
			     (void *)NULL, (void *)NULL, (void *)NULL);
	    rt_db_free_internal(&intern);
    FOR_ALL_DIRECTORY_END;

    /* Do a NULL + subtraction callback to indicate the end of an update cycle */
    for (size_t j = 0; j < BU_PTBL_LEN(&dbip->i->dbi_update_nref_clbks); j++) {
	struct dbi_update_nref_clbk *cb = (struct dbi_update_nref_clbk *)BU_PTBL_GET(&dbip->i->dbi_update_nref_clbks, j);
	(*cb->f)(dbip, NULL, NULL, NULL, DB_OP_SUBTRACT, NULL, cb->u_data);
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
