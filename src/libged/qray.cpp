/*                        Q R A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 1998-2025 United States Government as represented by
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
/** @file libged/qray.cpp
 *
 * Routines to set and get "Query Ray" variables.
 *
 */

#include "common.h"

#include <string>
#include <fstream>
#include <vector>

#include "vmath.h"
#include "bu/app.h"
#include "ged.h"

#include "./qray.h"

struct ged_qray_color def_qray_odd_color = { 0, 255, 255 };
struct ged_qray_color def_qray_even_color = { 255, 255, 0 };
struct ged_qray_color def_qray_void_color = { 255, 0, 255 };
struct ged_qray_color def_qray_overlap_color = { 255, 255, 255 };

void
qray_init(struct ged_drawable *gdp)
{
    bu_vls_init(&gdp->gd_qray_basename);
    bu_vls_strcpy(&gdp->gd_qray_basename, DG_QRAY_BASENAME);
    bu_vls_init(&gdp->gd_qray_script);

    gdp->gd_qray_effects = 'b';
    gdp->gd_qray_cmd_echo = 0;
    gdp->gd_qray_odd_color = def_qray_odd_color;
    gdp->gd_qray_even_color = def_qray_even_color;
    gdp->gd_qray_void_color = def_qray_void_color;
    gdp->gd_qray_overlap_color = def_qray_overlap_color;

    /* Load the default GED formatting template */
    struct bu_vls ged_fmt_file = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ged_fmt_file, "%s/ged.nrt", bu_dir(NULL, 0, BU_DIR_DATA, "nirt", NULL));
    std::vector<std::string> fmt_lines;
    std::string s;
    std::ifstream file;
    file.open(bu_vls_cstr(&ged_fmt_file));
    while (std::getline(file, s)) {
	if (!s.length())
	    continue;
	if (s[0] == '#')
	    continue;
	fmt_lines.push_back(s);
    }
    file.close();
    bu_vls_free(&ged_fmt_file);

    /* Allocate memory for the default format types */
    gdp->gd_qray_fmts = (struct ged_qray_fmt *)bu_calloc(fmt_lines.size()+1, sizeof(struct ged_qray_fmt), "qray_fmts");

    for (size_t i = 0; i < fmt_lines.size(); ++i) {
	std::string fmt_str = fmt_lines[i];
	fmt_str.erase(0, 4); // Remove "fmt_"
	gdp->gd_qray_fmts[i].type = fmt_str[0];
	fmt_str.erase(0, 2); // remove "t "
	bu_vls_init(&gdp->gd_qray_fmts[i].fmt);
	bu_vls_sprintf(&gdp->gd_qray_fmts[i].fmt, "%s", fmt_str.c_str());
    }

    gdp->gd_qray_fmts[fmt_lines.size()].type = (char)0;
}


void
qray_free(struct ged_drawable *gdp)
{
    int i;

    bu_vls_free(&gdp->gd_qray_basename);
    bu_vls_free(&gdp->gd_qray_script);
    for (i = 0; gdp->gd_qray_fmts[i].type != (char)0; ++i)
	bu_vls_free(&gdp->gd_qray_fmts[i].fmt);
    bu_free(gdp->gd_qray_fmts, "dgo_free_qray");
}


void
qray_data_to_vlist(struct ged *gedp,
	struct bv_vlblock *vbp,
	struct qray_dataList *headp,
	vect_t dir,
	int do_overlaps)
{
    int i = 1;			/* start out odd */
    struct bu_list *vhead;
    struct qray_dataList *ndlp;
    vect_t in_pt, out_pt;
    vect_t last_out_pt = { 0, 0, 0 };
    struct bu_list *vlfree = &rt_vlfree;

    for (BU_LIST_FOR(ndlp, qray_dataList, &headp->l)) {
	if (do_overlaps)
	    vhead = bv_vlblock_find(vbp,
		    gedp->i->ged_gdp->gd_qray_overlap_color.r,
		    gedp->i->ged_gdp->gd_qray_overlap_color.g,
		    gedp->i->ged_gdp->gd_qray_overlap_color.b);
	else if (i % 2)
	    vhead = bv_vlblock_find(vbp,
		    gedp->i->ged_gdp->gd_qray_odd_color.r,
		    gedp->i->ged_gdp->gd_qray_odd_color.g,
		    gedp->i->ged_gdp->gd_qray_odd_color.b);
	else
	    vhead = bv_vlblock_find(vbp,
		    gedp->i->ged_gdp->gd_qray_even_color.r,
		    gedp->i->ged_gdp->gd_qray_even_color.g,
		    gedp->i->ged_gdp->gd_qray_even_color.b);

	VSET(in_pt, ndlp->x_in, ndlp->y_in, ndlp->z_in);
	VJOIN1(out_pt, in_pt, ndlp->los, dir);
	VSCALE(in_pt, in_pt, gedp->dbip->dbi_local2base);
	VSCALE(out_pt, out_pt, gedp->dbip->dbi_local2base);
	BV_ADD_VLIST(vlfree, vhead, in_pt, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, out_pt, BV_VLIST_LINE_DRAW);

	if (!do_overlaps && i > 1 && !VNEAR_EQUAL(last_out_pt, in_pt, SQRT_SMALL_FASTF)) {
	    vhead = bv_vlblock_find(vbp,
		    gedp->i->ged_gdp->gd_qray_void_color.r,
		    gedp->i->ged_gdp->gd_qray_void_color.g,
		    gedp->i->ged_gdp->gd_qray_void_color.b);
	    BV_ADD_VLIST(vlfree, vhead, last_out_pt, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, in_pt, BV_VLIST_LINE_DRAW);
	}

	VMOVE(last_out_pt, out_pt);
	++i;
    }
}


/** @} */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
