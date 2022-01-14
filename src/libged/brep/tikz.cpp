/*                      T I K Z . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/brep/tikz.cpp
 *
 * The brep command logic for exporting Tikz plotting files.
 *
 */

#include "common.h"

#include "brep.h"
#include "raytrace.h"

#include "./ged_brep.h"

static void tikz_comb(struct ged *gedp, struct bu_vls *tikz, struct directory *dp, struct bu_vls *color, int *cnt);

static int
tikz_tree(struct ged *gedp, struct bu_vls *tikz, const union tree *oldtree, struct bu_vls *color, int *cnt)
{
    int ret = 0;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    ret |= tikz_tree(gedp, tikz, oldtree->tr_b.tb_right, color, cnt);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    ret |= tikz_tree(gedp, tikz, oldtree->tr_b.tb_left, color, cnt);
	    break;
	case OP_DB_LEAF:
	    {
		struct directory *dir = db_lookup(gedp->dbip, oldtree->tr_l.tl_name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			tikz_comb(gedp, tikz, dir, color, cnt);
		    } else {
			// It's a primitive. If it's a brep, get the wireframe.
			// TODO - support wireframes from other primitive types...
			struct rt_db_internal bintern;
			struct rt_brep_internal *b_ip = NULL;
			RT_DB_INTERNAL_INIT(&bintern)
			    if (rt_db_get_internal(&bintern, dir, gedp->dbip, NULL, &rt_uniresource) < 0) {
				return GED_ERROR;
			    }
			if (bintern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
			    ON_String s;
			    struct bu_vls cntstr = BU_VLS_INIT_ZERO;
			    (*cnt)++;
			    bu_vls_sprintf(&cntstr, "OBJ%d", *cnt);
			    b_ip = (struct rt_brep_internal *)bintern.idb_ptr;
			    (void)ON_BrepTikz(s, b_ip->brep, bu_vls_addr(color), bu_vls_addr(&cntstr));
			    const char *str = s.Array();
			    bu_vls_strcat(tikz, str);
			    bu_vls_free(&cntstr);
			}
		    }
		}
	    }
	    break;
	default:
	    bu_log("huh??\n");
	    break;
    }
    return ret;
}

static void
tikz_comb(struct ged *gedp, struct bu_vls *tikz, struct directory *dp, struct bu_vls *color, int *cnt)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb_internal = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct bu_vls color_backup = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&color_backup, "%s", bu_vls_addr(color));

    RT_DB_INTERNAL_INIT(&intern)

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	    return;
	}

    RT_CK_COMB(intern.idb_ptr);
    comb_internal = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb_internal->tree == NULL) {
	// Empty tree
	return;
    }
    RT_CK_TREE(comb_internal->tree);
    union tree *t = comb_internal->tree;


    // Get color
    if (comb_internal->rgb[0] > 0 || comb_internal->rgb[1] > 0 || comb_internal->rgb[1] > 0) {
	bu_vls_sprintf(color, "color={rgb:red,%d;green,%d;blue,%d}", comb_internal->rgb[0], comb_internal->rgb[1], comb_internal->rgb[2]);
    }

    (void)tikz_tree(gedp, tikz, t, color, cnt);

    bu_vls_sprintf(color, "%s", bu_vls_addr(&color_backup));
    bu_vls_free(&color_backup);
}

extern "C" int
brep_tikz(struct _ged_brep_info *gb, const char *outfile)
{
    int cnt = 0;
    struct bu_vls color = BU_VLS_INIT_ZERO;
    struct bu_vls tikz = BU_VLS_INIT_ZERO;

    struct ged *gedp = gb->gedp;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct rt_brep_internal *brep_ip = NULL;

    bu_vls_printf(&tikz, "\\documentclass{article}\n");
    bu_vls_printf(&tikz, "\\usepackage{tikz}\n");
    bu_vls_printf(&tikz, "\\usepackage{tikz-3dplot}\n\n");
    bu_vls_printf(&tikz, "\\begin{document}\n\n");
    // Translate view az/el into tikz-3dplot variation
    bu_vls_printf(&tikz, "\\tdplotsetmaincoords{%f}{%f}\n", 90 + -1*gedp->ged_gvp->gv_aet[1], -1*(-90 + -1 * gedp->ged_gvp->gv_aet[0]));

    // Need bbox dimensions to determine proper scale factor - do this with db_search so it will
    // work for combs as well, so long as there are no matrix transformations in the hierarchy.
    // Properly speaking this should be a bbox call in librt, but in this case we want the bbox of
    // everything - subtractions and unions.  Need to check if that's an option, and if not how
    // to handle it properly...
    ON_BoundingBox bbox;
    ON_MinMaxInit(&(bbox.m_min), &(bbox.m_max));
    struct bu_ptbl breps = BU_PTBL_INIT_ZERO;
    const char *brep_search = "-type brep";
    db_update_nref(gedp->dbip, &rt_uniresource);
    (void)db_search(&breps, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, brep_search, 1, &gb->dp, gedp->dbip, NULL);
    for(size_t i = 0; i < BU_PTBL_LEN(&breps); i++) {
	struct rt_db_internal bintern;
	struct rt_brep_internal *b_ip = NULL;
	RT_DB_INTERNAL_INIT(&bintern)
	    struct directory *d = (struct directory *)BU_PTBL_GET(&breps, i);
	if (rt_db_get_internal(&bintern, d, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	    return GED_ERROR;
	}
	b_ip = (struct rt_brep_internal *)bintern.idb_ptr;
	b_ip->brep->GetBBox(bbox[0], bbox[1], true);
    }
    // Get things roughly down to page size - not perfect, but establishes a ballpark that can be fine tuned
    // by hand after generation
    double scale = 100/bbox.Diagonal().Length();

    bu_vls_printf(&tikz, "\\begin{tikzpicture}[scale=%f,tdplot_main_coords]\n", scale);

    if (gb->dp->d_flags & RT_DIR_COMB) {
	// Assign a default color
	bu_vls_sprintf(&color, "color={rgb:red,255;green,0;blue,0}");
	tikz_comb(gedp, &tikz, gb->dp, &color, &cnt);
    } else {
	ON_String s;
	if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a B-Rep - aborting\n", gb->dp->d_namep);
	    return 1;
	} else {
	    brep_ip = (struct rt_brep_internal *)gb->intern.idb_ptr;
	}
	RT_BREP_CK_MAGIC(brep_ip);
	const ON_Brep *brep = brep_ip->brep;
	(void)ON_BrepTikz(s, brep, NULL, NULL);
	const char *str = s.Array();
	bu_vls_strcat(&tikz, str);
    }

    bu_vls_printf(&tikz, "\\end{tikzpicture}\n\n");
    bu_vls_printf(&tikz, "\\end{document}\n");

    if (outfile) {
	FILE *fp = fopen(outfile, "w");
	fprintf(fp, "%s", bu_vls_addr(&tikz));
	fclose(fp);
	bu_vls_free(&tikz);
	bu_vls_sprintf(gedp->ged_result_str, "Output written to file %s", outfile);
    } else {

	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_addr(&tikz));
	bu_vls_free(&tikz);
    }
    bu_vls_free(&tikz);
    return GED_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
