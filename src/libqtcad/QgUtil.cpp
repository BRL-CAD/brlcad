/*                        Q G U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QgUtil.cpp
 *
 */

#include "common.h"
#include "qtcad/defines.h"
#include "qtcad/QgUtil.h"

static int
get_arb_type(struct directory *dp, struct db_i *dbip)
{
    int type;
    const struct bn_tol arb_tol = BG_TOL_INIT;
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return 0;
    type = rt_arb_std_type(&intern, &arb_tol);
    rt_db_free_internal(&intern);
    return type;
}


static void
db_find_subregion(int *ret, union tree *tp, struct db_i *dbip, int *depth, int max_depth,
	void (*traverse_func) (int *ret, struct directory *, struct db_i *, int *, int))
{
    struct directory *dp;
    if (!tp) return;
    if (*ret) return;
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    (*depth)++;
    if (*depth > max_depth) {
	(*depth)--;
	return;
    }
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_find_subregion(ret, tp->tr_b.tb_right, dbip, depth, max_depth, traverse_func);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_find_subregion(ret, tp->tr_b.tb_left, dbip, depth, max_depth, traverse_func);
	    (*depth)--;
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		(*depth)--;
		return;
	    } else {
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    traverse_func(ret, dp, dbip, depth, max_depth);
		}
		(*depth)--;
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
    return;
}

static void
db_find_region(int *ret, struct directory *search, struct db_i *dbip, int *depth, int max_depth)
{

    /* If we have a match, we need look no further */
    if (search->d_flags & RT_DIR_REGION) {
	(*ret)++;
	return;
    }

   /* If we have a comb, open it up.  Otherwise, we're done */
    if (search->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, search, dbip, NULL, &rt_uniresource) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_find_subregion(ret, comb->tree, dbip, depth, max_depth, db_find_region);
	rt_db_free_internal(&in);
    }
    return;
}

int
QgCombType(struct directory *dp, struct db_i *dbip)
{
    struct bu_attribute_value_set avs;
    int region_flag = 0;
    int air_flag = 0;
    int region_id_flag = 0;
    int assembly_flag = 0;
    if (dp->d_flags & RT_DIR_REGION) {
	region_flag = 1;
    }

    bu_avs_init_empty(&avs);
    (void)db5_get_attributes(dbip, &avs, dp);
    const char *airval = bu_avs_get(&avs, "aircode");
    if (airval && !BU_STR_EQUAL(airval, "0")) {
	air_flag = 1;
    }
    const char *region_id = bu_avs_get(&avs, "region_id");
    if (region_id && !BU_STR_EQUAL(region_id, "0")) {
	region_id_flag = 1;
    }

    if (!region_flag && !air_flag) {
	int search_results = 0;
	int depth = 0;
	db_find_region(&search_results, dp, dbip, &depth, CADTREE_RECURSION_LIMIT);
	if (search_results) assembly_flag = 1;
    }

    if (region_flag && !air_flag) return G_REGION;
    if (!region_id_flag && air_flag) return G_AIR;
    if (region_id_flag && air_flag) return G_AIR_REGION;
    if (assembly_flag) return G_ASSEMBLY;

    return G_STANDARD_OBJ;
}

QImage
QgIcon(struct directory *dp, struct db_i *dbip)
{
    int type = 0;
    QImage raw_type_icon;
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	switch(dp->d_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_TOR:
		raw_type_icon.load(":/images/primitives/tor.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_TGC:
		raw_type_icon.load(":/images/primitives/tgc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ELL:
		raw_type_icon.load(":/images/primitives/ell.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ARB8:
		type = get_arb_type(dp, dbip);
		switch (type) {
		    case 4:
			raw_type_icon.load(":/images/primitives/arb4.png");
			break;
		    case 5:
			raw_type_icon.load(":/images/primitives/arb5.png");
			break;
		    case 6:
			raw_type_icon.load(":/images/primitives/arb6.png");
			break;
		    case 7:
			raw_type_icon.load(":/images/primitives/arb7.png");
			break;
		    case 8:
			raw_type_icon.load(":/images/primitives/arb8.png");
			break;
		    default:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_ARS:
		raw_type_icon.load(":/images/primitives/ars.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HALF:
		raw_type_icon.load(":/images/primitives/half.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_REC:
		raw_type_icon.load(":/images/primitives/tgc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_POLY:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BSPLINE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SPH:
		raw_type_icon.load(":/images/primitives/sph.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_NMG:
		raw_type_icon.load(":/images/primitives/nmg.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EBM:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_VOL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ARBN:
		raw_type_icon.load(":/images/primitives/arbn.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_PIPE:
		raw_type_icon.load(":/images/primitives/pipe.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_PARTICLE:
		raw_type_icon.load(":/images/primitives/part.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_RPC:
		raw_type_icon.load(":/images/primitives/rpc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_RHC:
		raw_type_icon.load(":/images/primitives/rhc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EPA:
		raw_type_icon.load(":/images/primitives/epa.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EHY:
		raw_type_icon.load(":/images/primitives/ehy.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ETO:
		raw_type_icon.load(":/images/primitives/eto.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_GRIP:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_JOINT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HF:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		raw_type_icon.load(":/images/primitives/dsp.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SKETCH:
		raw_type_icon.load(":/images/primitives/sketch.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		raw_type_icon.load(":/images/primitives/extrude.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SUBMODEL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_CLINE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BOT:
		raw_type_icon.load(":/images/primitives/bot.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		type = QgCombType(dp, dbip);
		switch (type) {
		    case G_REGION:
			raw_type_icon.load(":/images/primitives/region.png");
			break;
		    case G_AIR:
			raw_type_icon.load(":/images/primitives/air.png");
			break;
		    case G_AIR_REGION:
			raw_type_icon.load(":/images/primitives/airregion.png");
			break;
		    case G_ASSEMBLY:
			raw_type_icon.load(":/images/primitives/assembly.png");
			break;
		    default:
			raw_type_icon.load(":/images/primitives/comb.png");
			break;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_SUPERELL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_METABALL:
		raw_type_icon.load(":/images/primitives/metaball.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		raw_type_icon.load(":/images/primitives/brep.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HYP:
		raw_type_icon.load(":/images/primitives/hyp.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_REVOLVE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ANNOT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HRT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    default:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	}
    } else {
	raw_type_icon.load(":/images/primitives/invalid.png");
    }

    return raw_type_icon;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

