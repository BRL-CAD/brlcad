/*                  C O L O R _ S C H E M E . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file color_scheme.c
 *
 * Functions -
 *	cs_set_dirty_flag	mark any display managers using the current color_scheme as dirty
 *	cs_update		update all colors according to the mode
 *	cs_set_bg		tell the display manager what color to use for the background
 *
 * Author -
 *	Robert G. Parker
 *
 * Source -
 *	SLAD/BND/ACST
 *	The U.S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

struct _color_scheme default_color_scheme = {
/* cs_rc */			1,
/* cs_mode */			0,
/* cs_bg */			{ 0, 0, 0 },
/* cs_bg_a */			{ 0, 0, 50 },
/* cs_bg_ia */			{ 0, 0, 0 },
/* cs_adc_line */		{ 255, 255, 0 },
/* cs_adc_line_a */		{ 255, 255, 0 },
/* cs_adc_line_ia */		{ 255, 255, 0 },
/* cs_adc_tick */		{ 255, 255, 255 },
/* cs_adc_tick_a */		{ 255, 255, 255 },
/* cs_adc_tick_ia */		{ 255, 255, 255 },
/* cs_geo_def */		{ 255, 0, 0 },
/* cs_geo_def_a */		{ 255, 0, 0 },
/* cs_geo_def_ia */		{ 255, 0, 0 },
/* cs_geo_hl */			{ 255, 255, 255 },
/* cs_geo_hl_a */		{ 255, 255, 255 },
/* cs_geo_hl_ia */		{ 255, 255, 255 },
/* cs_geo_label */		{ 255, 255, 0 },
/* cs_geo_label_a */		{ 255, 255, 0 },
/* cs_geo_label_ia */		{ 255, 255, 0 },
/* cs_model_axes */		{ 100, 255, 100 },
/* cs_model_axes_a */		{ 100, 255, 100 },
/* cs_model_axes_ia */		{ 100, 255, 100 },
/* cs_model_axes_label */	{ 255, 255, 0 },
/* cs_model_axes_label_a */	{ 255, 255, 0 },
/* cs_model_axes_label_ia */	{ 255, 255, 0 },
/* cs_view_axes */		{ 100, 100, 255 },
/* cs_view_axes_a */		{ 100, 100, 255 },
/* cs_view_axes_ia */		{ 100, 100, 255 },
/* cs_view_axes_label */	{ 255, 255, 0 },
/* cs_view_axes_label_a */	{ 255, 255, 0 },
/* cs_view_axes_label_ia */	{ 255, 255, 0 },
/* cs_edit_axes1 */		{ 255, 255, 255 },
/* cs_edit_axes1_a */		{ 255, 255, 255 },
/* cs_edit_axes1_ia */		{ 255, 255, 255 },
/* cs_edit_axes_label1 */	{ 255, 255, 0 },
/* cs_edit_axes_label1_a */	{ 255, 255, 0 },
/* cs_edit_axes_label1_ia */	{ 255, 255, 0 },
/* cs_edit_axes2 */		{ 255, 255, 255 },
/* cs_edit_axes2_a */		{ 255, 255, 255 },
/* cs_edit_axes2_ia */		{ 255, 255, 255 },
/* cs_edit_axes_label2 */	{ 255, 255, 0 },
/* cs_edit_axes_label2_a */	{ 255, 255, 0 },
/* cs_edit_axes_label2_ia */	{ 255, 255, 0 },
/* cs_rubber_band */		{ 255, 255, 255 },
/* cs_rubber_band_a */		{ 255, 255, 255 },
/* cs_rubber_band_ia */		{ 255, 255, 255 },
/* cs_grid */			{ 255, 255, 255 },
/* cs_grid_a */			{ 255, 255, 255 },
/* cs_grid_ia */		{ 255, 255, 255 },
/* cs_predictor */		{ 255, 255, 255 },
/* cs_predictor_a */		{ 255, 255, 255 },
/* cs_predictor_ia */		{ 255, 255, 255 },
/* cs_menu_line */		{ 255, 255, 0 },
/* cs_menu_line_a */		{ 255, 255, 0 },
/* cs_menu_line_ia */		{ 255, 255, 0 },
/* cs_slider_line */		{ 255, 255, 0 },
/* cs_slider_line_a */		{ 255, 255, 0 },
/* cs_slider_line_ia */		{ 255, 255, 0 },
/* cs_other_line */		{ 255, 255, 0 },
/* cs_other_line_a */		{ 255, 255, 0 },
/* cs_other_line_ia */		{ 255, 255, 0 },
/* cs_status_text1 */		{ 255, 255, 255 },
/* cs_status_text1_a */		{ 255, 255, 255 },
/* cs_status_text1_ia */	{ 255, 255, 255 },
/* cs_status_text2 */		{ 255, 255, 0 },
/* cs_status_text2_a */		{ 255, 255, 0 },
/* cs_status_text2_ia */	{ 255, 255, 0 },
/* cs_slider_text1 */		{ 255, 255, 255 },
/* cs_slider_text1_a */		{ 255, 255, 255 },
/* cs_slider_text1_ia */	{ 255, 255, 255 },
/* cs_slider_text2 */		{ 255, 0, 0 },
/* cs_slider_text2_a */		{ 255, 0, 0 },
/* cs_slider_text2_ia */	{ 255, 0, 0 },
/* cs_menu_text1 */		{ 255, 255, 255 },
/* cs_menu_text1_a */		{ 255, 255, 255 },
/* cs_menu_text1_ia */		{ 255, 255, 255 },
/* cs_menu_text2 */		{ 255, 255, 0 },
/* cs_menu_text2_a */		{ 255, 255, 0 },
/* cs_menu_text2_ia */		{ 255, 255, 0 },
/* cs_menu_title */		{ 255, 0, 0 },
/* cs_menu_title_a */		{ 255, 0, 0 },
/* cs_menu_title_ia */		{ 255, 0, 0 },
/* cs_menu_arrow */		{ 255, 255, 255 },
/* cs_menu_arrow_a */		{ 255, 255, 255 },
/* cs_menu_arrow_ia */		{ 255, 255, 255 },
/* cs_state_text1 */		{ 255, 255, 255 },
/* cs_state_text1_a */		{ 255, 255, 255 },
/* cs_state_text1_ia */		{ 255, 255, 255 },
/* cs_state_text2 */		{ 255, 255, 0 },
/* cs_state_text2_a */		{ 255, 255, 0 },
/* cs_state_text2_ia */		{ 255, 255, 0 },
/* cs_edit_info */		{ 255, 255, 0 },
/* cs_edit_info_a */		{ 255, 255, 0 },
/* cs_edit_info_ia */		{ 255, 255, 0 },
/* cs_center_dot */		{ 255, 255, 0 },
/* cs_center_dot_a */		{ 255, 255, 0 },
/* cs_center_dot_ia */		{ 255, 255, 0 }
};

#define CS_OFFSET 1	/* offset to start of colors in color_scheme_vparse */

#define CS_O(_m)        offsetof(struct _color_scheme, _m)
#define CS_OA(_m)	offsetofarray(struct _color_scheme, _m)
struct bu_structparse color_scheme_vparse[] = {
	{"%d",	1, "mode",	CS_O(cs_mode),	cs_update },
	{"%d",  3, "bg",	CS_OA(cs_bg),	cs_set_bg },
	{"%d",  3, "bg_a",	CS_OA(cs_bg_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "bg_ia",	CS_OA(cs_bg_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_line",	CS_OA(cs_adc_line),	cs_set_dirty_flag },
	{"%d",  3, "adc_line_a",	CS_OA(cs_adc_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_line_ia",	CS_OA(cs_adc_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_tick",	CS_OA(cs_adc_tick),	cs_set_dirty_flag },
	{"%d",  3, "adc_tick_a",	CS_OA(cs_adc_tick_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_tick_ia",	CS_OA(cs_adc_tick_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_def",	CS_OA(cs_geo_def),	cs_set_dirty_flag },
	{"%d",  3, "geo_def_a",	CS_OA(cs_geo_def_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_def_ia",	CS_OA(cs_geo_def_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_hl",	CS_OA(cs_geo_hl),	cs_set_dirty_flag },
	{"%d",  3, "geo_hl_a",	CS_OA(cs_geo_hl_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_hl_ia",	CS_OA(cs_geo_hl_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_label",	CS_OA(cs_geo_label),	cs_set_dirty_flag },
	{"%d",  3, "geo_label_a",	CS_OA(cs_geo_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_label_ia",	CS_OA(cs_geo_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes",	CS_OA(cs_model_axes),	cs_set_dirty_flag },
	{"%d",  3, "model_axes_a",	CS_OA(cs_model_axes_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_ia",	CS_OA(cs_model_axes_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_label",	CS_OA(cs_model_axes_label),	cs_set_dirty_flag },
	{"%d",  3, "model_axes_label_a",	CS_OA(cs_model_axes_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_label_ia",	CS_OA(cs_model_axes_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes",	CS_OA(cs_view_axes),	cs_set_dirty_flag },
	{"%d",  3, "view_axes_a",	CS_OA(cs_view_axes_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_ia",	CS_OA(cs_view_axes_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_label",	CS_OA(cs_view_axes_label),	cs_set_dirty_flag },
	{"%d",  3, "view_axes_label_a",	CS_OA(cs_view_axes_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_label_ia",	CS_OA(cs_view_axes_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes1",	CS_OA(cs_edit_axes1),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes1_a",	CS_OA(cs_edit_axes1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes1_ia",	CS_OA(cs_edit_axes1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes2",	CS_OA(cs_edit_axes2),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes2_a",	CS_OA(cs_edit_axes2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes2_ia",	CS_OA(cs_edit_axes2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label1",	CS_OA(cs_edit_axes_label1),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes_label1_a",	CS_OA(cs_edit_axes_label1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label1_ia",	CS_OA(cs_edit_axes_label1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label2",	CS_OA(cs_edit_axes_label2),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes_label2_a",	CS_OA(cs_edit_axes_label2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label2_ia",	CS_OA(cs_edit_axes_label2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "rubber_band",	CS_OA(cs_rubber_band),	cs_set_dirty_flag },
	{"%d",  3, "rubber_band_a",	CS_OA(cs_rubber_band_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "rubber_band_ia",	CS_OA(cs_rubber_band_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "grid",	CS_OA(cs_grid),	cs_set_dirty_flag },
	{"%d",  3, "grid_a",	CS_OA(cs_grid_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "grid_ia",	CS_OA(cs_grid_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "predictor",	CS_OA(cs_predictor),	cs_set_dirty_flag },
	{"%d",  3, "predictor_a",	CS_OA(cs_predictor_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "predictor_ia",	CS_OA(cs_predictor_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_line",	CS_OA(cs_menu_line),	cs_set_dirty_flag },
	{"%d",  3, "menu_line_a",	CS_OA(cs_menu_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_line_ia",	CS_OA(cs_menu_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_line",	CS_OA(cs_slider_line),	cs_set_dirty_flag },
	{"%d",  3, "slider_line_a",	CS_OA(cs_slider_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_line_ia",	CS_OA(cs_slider_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "other_line",	CS_OA(cs_other_line),	cs_set_dirty_flag },
	{"%d",  3, "other_line_a",	CS_OA(cs_other_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "other_line_ia",	CS_OA(cs_other_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "status_text1",	CS_OA(cs_status_text1),	cs_set_dirty_flag },
	{"%d",  3, "status_text1_a",	CS_OA(cs_status_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "status_text1_ia",	CS_OA(cs_status_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "status_text2",	CS_OA(cs_status_text2),	cs_set_dirty_flag },
	{"%d",  3, "status_text2_a",	CS_OA(cs_status_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "status_text2_ia",	CS_OA(cs_status_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_text1",	CS_OA(cs_slider_text1),	cs_set_dirty_flag },
	{"%d",  3, "slider_text1_a",	CS_OA(cs_slider_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_text1_ia",	CS_OA(cs_slider_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_text2",	CS_OA(cs_slider_text2),	cs_set_dirty_flag },
	{"%d",  3, "slider_text2_a",	CS_OA(cs_slider_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "slider_text2_ia",	CS_OA(cs_slider_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_text1",	CS_OA(cs_menu_text1),	cs_set_dirty_flag },
	{"%d",  3, "menu_text1_a",	CS_OA(cs_menu_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_text1_ia",	CS_OA(cs_menu_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_text2",	CS_OA(cs_menu_text2),	cs_set_dirty_flag },
	{"%d",  3, "menu_text2_a",	CS_OA(cs_menu_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_text2_ia",	CS_OA(cs_menu_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_title",	CS_OA(cs_menu_title),	cs_set_dirty_flag },
	{"%d",  3, "menu_title_a",	CS_OA(cs_menu_title_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_title_ia",	CS_OA(cs_menu_title_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_arrow",	CS_OA(cs_menu_arrow),	cs_set_dirty_flag },
	{"%d",  3, "menu_arrow_a",	CS_OA(cs_menu_arrow_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "menu_arrow_ia",	CS_OA(cs_menu_arrow_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "state_text1",	CS_OA(cs_state_text1),	cs_set_dirty_flag },
	{"%d",  3, "state_text1_a",	CS_OA(cs_state_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "state_text1_ia",	CS_OA(cs_state_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "state_text2",	CS_OA(cs_state_text2),	cs_set_dirty_flag },
	{"%d",  3, "state_text2_a",	CS_OA(cs_state_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "state_text2_ia",	CS_OA(cs_state_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_info",	CS_OA(cs_edit_info),	cs_set_dirty_flag },
	{"%d",  3, "edit_info_a",	CS_OA(cs_edit_info_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_info_ia",	CS_OA(cs_edit_info_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "center_dot",	CS_OA(cs_center_dot),	cs_set_dirty_flag },
	{"%d",  3, "center_dot_a",	CS_OA(cs_center_dot_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "center_dot_ia",	CS_OA(cs_center_dot_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

void
cs_set_dirty_flag(void)
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_color_scheme == color_scheme)
      dmlp->dml_dirty = 1;
}

void
cs_update(void)
{
  register struct bu_structparse *sp;
  struct bu_vls vls;
  int offset;

  bu_vls_init(&vls);

  if (color_scheme->cs_mode)
    offset = 1;
  else
    offset = 2;

  for (sp = &color_scheme_vparse[CS_OFFSET]; sp->sp_name != NULL; sp += 3) {
    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "rset cs %s [rset cs %s]", sp->sp_name, (sp+offset)->sp_name);
    Tcl_Eval(interp, bu_vls_addr(&vls));
  }

  cs_set_bg();
  bu_vls_free(&vls);
}

void
cs_set_bg(void)
{
  struct dm_list *dmlp;
  struct dm_list *save_curr_dmlp = curr_dm_list;
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "dm bg %d %d %d",
		color_scheme->cs_bg[0],
		color_scheme->cs_bg[1],
		color_scheme->cs_bg[2]);

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if (dmlp->dml_color_scheme == color_scheme) {
      dmlp->dml_dirty = 1;
      curr_dm_list = dmlp;
      Tcl_Eval(interp, bu_vls_addr(&vls));
    }

  bu_vls_free(&vls);
  curr_dm_list = save_curr_dmlp;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
