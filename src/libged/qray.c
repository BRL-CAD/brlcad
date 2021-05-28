/*                          Q R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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
/** @file libged/qray.c
 *
 * Routines to set and get "Query Ray" variables.
 *
 */

#include "common.h"

#include <string.h>


#include "vmath.h"
#include "ged.h"

#include "./qray.h"

struct ged_qray_color def_qray_odd_color = { 0, 255, 255 };
struct ged_qray_color def_qray_even_color = { 255, 255, 0 };
struct ged_qray_color def_qray_void_color = { 255, 0, 255 };
struct ged_qray_color def_qray_overlap_color = { 255, 255, 255 };

struct qray_fmt_data def_qray_fmt_data[] = {
    {'r', "\"Origin (x y z) = (%.2f %.2f %.2f)  (h v d) = (%.2f %.2f %.2f)\\nDirection (x y z) = (%.4f %.4f %.4f)  (az el) = (%.2f %.2f)\\n\" x_orig y_orig z_orig h v d_orig x_dir y_dir z_dir a e"},
    {'h', "\"    Region Name               Entry (x y z) LOS  Obliq_in\\n\""},
    {'p', "\"%-20s (%9.3f %9.3f %9.3f) %8.2f %8.3f\\n\" reg_name x_in y_in z_in los obliq_in"},
    {'f', "\"\""},
    {'m', "\"You missed the target\\n\""},
    {'o', "\"OVERLAP: '%s' and '%s' xyz_in=(%g %g %g) los=%g\\n\" ov_reg1_name ov_reg2_name ov_x_in ov_y_in ov_z_in ov_los"},
    {'g', "\"\""},
    {(char)0, (char *)NULL}
};


void
qray_init(struct ged_drawable *gdp)
{
    int i;
    int n = 0;
    struct qray_fmt_data *qfdp;

    bu_vls_init(&gdp->gd_qray_basename);
    bu_vls_strcpy(&gdp->gd_qray_basename, DG_QRAY_BASENAME);
    bu_vls_init(&gdp->gd_qray_script);

    gdp->gd_qray_effects = 'b';
    gdp->gd_qray_cmd_echo = 0;
    gdp->gd_qray_odd_color = def_qray_odd_color;
    gdp->gd_qray_even_color = def_qray_even_color;
    gdp->gd_qray_void_color = def_qray_void_color;
    gdp->gd_qray_overlap_color = def_qray_overlap_color;

    /* count the number of default format types */
    for (qfdp = def_qray_fmt_data; qfdp->fmt != (char *)NULL; ++qfdp)
	++n;

    gdp->gd_qray_fmts = (struct ged_qray_fmt *)bu_calloc(n+1, sizeof(struct ged_qray_fmt), "qray_fmts");

    for (i = 0; i < n; ++i) {
	gdp->gd_qray_fmts[i].type = def_qray_fmt_data[i].type;
	bu_vls_init(&gdp->gd_qray_fmts[i].fmt);
	bu_vls_strcpy(&gdp->gd_qray_fmts[i].fmt, def_qray_fmt_data[i].fmt);
    }

    gdp->gd_qray_fmts[i].type = (char)0;
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

    for (BU_LIST_FOR(ndlp, qray_dataList, &headp->l)) {
	if (do_overlaps)
	    vhead = bv_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_overlap_color.r,
				    gedp->ged_gdp->gd_qray_overlap_color.g,
				    gedp->ged_gdp->gd_qray_overlap_color.b);
	else if (i % 2)
	    vhead = bv_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_odd_color.r,
				    gedp->ged_gdp->gd_qray_odd_color.g,
				    gedp->ged_gdp->gd_qray_odd_color.b);
	else
	    vhead = bv_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_even_color.r,
				    gedp->ged_gdp->gd_qray_even_color.g,
				    gedp->ged_gdp->gd_qray_even_color.b);

	VSET(in_pt, ndlp->x_in, ndlp->y_in, ndlp->z_in);
	VJOIN1(out_pt, in_pt, ndlp->los, dir);
	VSCALE(in_pt, in_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	VSCALE(out_pt, out_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	RT_ADD_VLIST(vhead, in_pt, BV_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, out_pt, BV_VLIST_LINE_DRAW);

	if (!do_overlaps && i > 1 && !VNEAR_EQUAL(last_out_pt, in_pt, SQRT_SMALL_FASTF)) {
	    vhead = bv_vlblock_find(vbp,
				    gedp->ged_gdp->gd_qray_void_color.r,
				    gedp->ged_gdp->gd_qray_void_color.g,
				    gedp->ged_gdp->gd_qray_void_color.b);
	    RT_ADD_VLIST(vhead, last_out_pt, BV_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, in_pt, BV_VLIST_LINE_DRAW);
	}

	VMOVE(last_out_pt, out_pt);
	++i;
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
