/*
 *			C O L O R _ S C H E M E . C
 *
 * Functions -
 *	f_cs_set		set/get color scheme values
 *	f_cs_def		set/get default color scheme values
 *	cs_set_dirty_flag	mark any display managers using the current color_scheme as dirty
 *	cs_update		update all colors according to the state (i.e. active/inactive)
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

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

void cs_set_dirty_flag();
void cs_update();
void cs_set_bg();

struct _color_scheme default_color_scheme = {
/* rc */			0,
/* active */			0,
/* bg */			{ 0, 0, 0 },
/* bg_a */			{ 0, 0, 50 },
/* bg_ia */			{ 0, 0, 0 },
/* adc_line */			{ 255, 255, 0 },
/* adc_line_a */		{ 255, 255, 0 },
/* adc_line_ia */		{ 255, 255, 0 },
/* adc_tick */			{ 255, 255, 255 },
/* adc_tick_a */		{ 255, 255, 255 },
/* adc_tick_ia */		{ 255, 255, 255 },
/* geo_def */			{ 255, 0, 0 },
/* geo_def_a */			{ 255, 0, 0 },
/* geo_def_ia */		{ 255, 0, 0 },
/* geo_hl */			{ 255, 255, 255 },
/* geo_hl_a */			{ 255, 255, 255 },
/* geo_hl_ia */			{ 255, 255, 255 },
/* geo_label */			{ 255, 255, 0 },
/* geo_label_a */		{ 255, 255, 0 },
/* geo_label_ia */		{ 255, 255, 0 },
/* model_axes */		{ 100, 255, 100 },
/* model_axes_a */		{ 100, 255, 100 },
/* model_axes_ia */		{ 100, 255, 100 },
/* model_axes_label */		{ 255, 255, 0 },
/* model_axes_label_a */	{ 255, 255, 0 },
/* model_axes_label_ia */	{ 255, 255, 0 },
/* view_axes */			{ 100, 100, 255 },
/* view_axes_a */		{ 100, 100, 255 },
/* view_axes_ia */		{ 100, 100, 255 },
/* view_axes_label */		{ 255, 255, 0 },
/* view_axes_label_a */		{ 255, 255, 0 },
/* view_axes_label_ia */	{ 255, 255, 0 },
/* edit_axes1 */		{ 255, 255, 255 },
/* edit_axes1_a */		{ 255, 255, 255 },
/* edit_axes1_ia */		{ 255, 255, 255 },
/* edit_axes_label1 */		{ 255, 255, 0 },
/* edit_axes_label1_a */	{ 255, 255, 0 },
/* edit_axes_label1_ia */	{ 255, 255, 0 },
/* edit_axes2 */		{ 255, 255, 255 },
/* edit_axes2_a */		{ 255, 255, 255 },
/* edit_axes2_ia */		{ 255, 255, 255 },
/* edit_axes_label2 */		{ 255, 255, 0 },
/* edit_axes_label2_a */	{ 255, 255, 0 },
/* edit_axes_label2_ia */	{ 255, 255, 0 },
/* rubber_band */		{ 255, 255, 255 },
/* rubber_band_a */		{ 255, 255, 255 },
/* rubber_band_ia */		{ 255, 255, 255 },
/* grid */			{ 255, 255, 255 },
/* grid_a */			{ 255, 255, 255 },
/* grid_ia */			{ 255, 255, 255 },
/* predictor */			{ 255, 255, 255 },
/* predictor_a */		{ 255, 255, 255 },
/* predictor_ia */		{ 255, 255, 255 },
/* fp_menu_line */		{ 255, 255, 0 },
/* fp_menu_line_a */		{ 255, 255, 0 },
/* fp_menu_line_ia */		{ 255, 255, 0 },
/* fp_slider_line */		{ 255, 255, 0 },
/* fp_slider_line_a */		{ 255, 255, 0 },
/* fp_slider_line_ia */		{ 255, 255, 0 },
/* fp_other_line */		{ 255, 255, 0 },
/* fp_other_line_a */		{ 255, 255, 0 },
/* fp_other_line_ia */		{ 255, 255, 0 },
/* fp_status_text1 */		{ 255, 255, 255 },
/* fp_status_text1_a */		{ 255, 255, 255 },
/* fp_status_text1_ia */	{ 255, 255, 255 },
/* fp_status_text2 */		{ 255, 255, 0 },
/* fp_status_text2_a */		{ 255, 255, 0 },
/* fp_status_text2_ia */	{ 255, 255, 0 },
/* fp_slider_text1 */		{ 255, 255, 255 },
/* fp_slider_text1_a */		{ 255, 255, 255 },
/* fp_slider_text1_ia */	{ 255, 255, 255 },
/* fp_slider_text2 */		{ 255, 0, 0 },
/* fp_slider_text2_a */		{ 255, 0, 0 },
/* fp_slider_text2_ia */	{ 255, 0, 0 },
/* fp_menu_text1 */		{ 255, 255, 255 },
/* fp_menu_text1_a */		{ 255, 255, 255 },
/* fp_menu_text1_ia */		{ 255, 255, 255 },
/* fp_menu_text2 */		{ 255, 255, 0 },
/* fp_menu_text2_a */		{ 255, 255, 0 },
/* fp_menu_text2_ia */		{ 255, 255, 0 },
/* fp_menu_title */		{ 255, 0, 0 },
/* fp_menu_title_a */		{ 255, 0, 0 },
/* fp_menu_title_ia */		{ 255, 0, 0 },
/* fp_menu_arrow */		{ 255, 255, 255 },
/* fp_menu_arrow_a */		{ 255, 255, 255 },
/* fp_menu_arrow_ia */		{ 255, 255, 255 },
/* fp_state_text1 */		{ 255, 255, 255 },
/* fp_state_text1_a */		{ 255, 255, 255 },
/* fp_state_text1_ia */		{ 255, 255, 255 },
/* fp_state_text2 */		{ 255, 255, 0 },
/* fp_state_text2_a */		{ 255, 255, 0 },
/* fp_state_text2_ia */		{ 255, 255, 0 },
/* fp_edit_info */		{ 255, 255, 0 },
/* fp_edit_info_a */		{ 255, 255, 0 },
/* fp_edit_info_ia */		{ 255, 255, 0 },
/* fp_center_dot */		{ 255, 255, 0 },
/* fp_center_dot_a */		{ 255, 255, 0 },
/* fp_center_dot_ia */		{ 255, 255, 0 }
};

#define CS_OFFSET 1	/* offset to start of colors in color_scheme_vparse */

#define MV_O(_m)        offsetof(struct _color_scheme, _m)
#define MV_OA(_m)	offsetofarray(struct _color_scheme, _m)
struct bu_structparse color_scheme_vparse[] = {
	{"%d",	1, "active",	MV_O(active),	cs_update },
	{"%d",  3, "bg",	MV_OA(bg),	cs_set_bg },
	{"%d",  3, "bg_a",	MV_OA(bg_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "bg_ia",	MV_OA(bg_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_line",	MV_OA(adc_line),	cs_set_dirty_flag },
	{"%d",  3, "adc_line_a",	MV_OA(adc_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_line_ia",	MV_OA(adc_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_tick",	MV_OA(adc_tick),	cs_set_dirty_flag },
	{"%d",  3, "adc_tick_a",	MV_OA(adc_tick_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "adc_tick_ia",	MV_OA(adc_tick_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_def",	MV_OA(geo_def),	cs_set_dirty_flag },
	{"%d",  3, "geo_def_a",	MV_OA(geo_def_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_def_ia",	MV_OA(geo_def_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_hl",	MV_OA(geo_hl),	cs_set_dirty_flag },
	{"%d",  3, "geo_hl_a",	MV_OA(geo_hl_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_hl_ia",	MV_OA(geo_hl_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_label",	MV_OA(geo_label),	cs_set_dirty_flag },
	{"%d",  3, "geo_label_a",	MV_OA(geo_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "geo_label_ia",	MV_OA(geo_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes",	MV_OA(model_axes),	cs_set_dirty_flag },
	{"%d",  3, "model_axes_a",	MV_OA(model_axes_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_ia",	MV_OA(model_axes_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_label",	MV_OA(model_axes_label),	cs_set_dirty_flag },
	{"%d",  3, "model_axes_label_a",	MV_OA(model_axes_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "model_axes_label_ia",	MV_OA(model_axes_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes",	MV_OA(view_axes),	cs_set_dirty_flag },
	{"%d",  3, "view_axes_a",	MV_OA(view_axes_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_ia",	MV_OA(view_axes_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_label",	MV_OA(view_axes_label),	cs_set_dirty_flag },
	{"%d",  3, "view_axes_label_a",	MV_OA(view_axes_label_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "view_axes_label_ia",	MV_OA(view_axes_label_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes1",	MV_OA(edit_axes1),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes1_a",	MV_OA(edit_axes1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes1_ia",	MV_OA(edit_axes1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes2",	MV_OA(edit_axes2),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes2_a",	MV_OA(edit_axes2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes2_ia",	MV_OA(edit_axes2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label1",	MV_OA(edit_axes_label1),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes_label1_a",	MV_OA(edit_axes_label1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label1_ia",	MV_OA(edit_axes_label1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label2",	MV_OA(edit_axes_label2),	cs_set_dirty_flag },
	{"%d",  3, "edit_axes_label2_a",	MV_OA(edit_axes_label2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "edit_axes_label2_ia",	MV_OA(edit_axes_label2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "rubber_band",	MV_OA(rubber_band),	cs_set_dirty_flag },
	{"%d",  3, "rubber_band_a",	MV_OA(rubber_band_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "rubber_band_ia",	MV_OA(rubber_band_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "grid",	MV_OA(grid),	cs_set_dirty_flag },
	{"%d",  3, "grid_a",	MV_OA(grid_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "grid_ia",	MV_OA(grid_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "predictor",	MV_OA(predictor),	cs_set_dirty_flag },
	{"%d",  3, "predictor_a",	MV_OA(predictor_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "predictor_ia",	MV_OA(predictor_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_line",	MV_OA(fp_menu_line),	cs_set_dirty_flag },
	{"%d",  3, "fp_menu_line_a",	MV_OA(fp_menu_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_line_ia",	MV_OA(fp_menu_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_line",	MV_OA(fp_slider_line),	cs_set_dirty_flag },
	{"%d",  3, "fp_slider_line_a",	MV_OA(fp_slider_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_line_ia",	MV_OA(fp_slider_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_other_line",	MV_OA(fp_other_line),	cs_set_dirty_flag },
	{"%d",  3, "fp_other_line_a",	MV_OA(fp_other_line_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_other_line_ia",	MV_OA(fp_other_line_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_status_text1",	MV_OA(fp_status_text1),	cs_set_dirty_flag },
	{"%d",  3, "fp_status_text1_a",	MV_OA(fp_status_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_status_text1_ia",	MV_OA(fp_status_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_status_text2",	MV_OA(fp_status_text2),	cs_set_dirty_flag },
	{"%d",  3, "fp_status_text2_a",	MV_OA(fp_status_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_status_text2_ia",	MV_OA(fp_status_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_text1",	MV_OA(fp_slider_text1),	cs_set_dirty_flag },
	{"%d",  3, "fp_slider_text1_a",	MV_OA(fp_slider_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_text1_ia",	MV_OA(fp_slider_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_text2",	MV_OA(fp_slider_text2),	cs_set_dirty_flag },
	{"%d",  3, "fp_slider_text2_a",	MV_OA(fp_slider_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_slider_text2_ia",	MV_OA(fp_slider_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_text1",	MV_OA(fp_menu_text1),	cs_set_dirty_flag },
	{"%d",  3, "fp_menu_text1_a",	MV_OA(fp_menu_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_text1_ia",	MV_OA(fp_menu_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_text2",	MV_OA(fp_menu_text2),	cs_set_dirty_flag },
	{"%d",  3, "fp_menu_text2_a",	MV_OA(fp_menu_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_text2_ia",	MV_OA(fp_menu_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_title",	MV_OA(fp_menu_title),	cs_set_dirty_flag },
	{"%d",  3, "fp_menu_title_a",	MV_OA(fp_menu_title_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_title_ia",	MV_OA(fp_menu_title_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_arrow",	MV_OA(fp_menu_arrow),	cs_set_dirty_flag },
	{"%d",  3, "fp_menu_arrow_a",	MV_OA(fp_menu_arrow_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_menu_arrow_ia",	MV_OA(fp_menu_arrow_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_state_text1",	MV_OA(fp_state_text1),	cs_set_dirty_flag },
	{"%d",  3, "fp_state_text1_a",	MV_OA(fp_state_text1_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_state_text1_ia",	MV_OA(fp_state_text1_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_state_text2",	MV_OA(fp_state_text2),	cs_set_dirty_flag },
	{"%d",  3, "fp_state_text2_a",	MV_OA(fp_state_text2_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_state_text2_ia",	MV_OA(fp_state_text2_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_edit_info",	MV_OA(fp_edit_info),	cs_set_dirty_flag },
	{"%d",  3, "fp_edit_info_a",	MV_OA(fp_edit_info_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_edit_info_ia",	MV_OA(fp_edit_info_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_center_dot",	MV_OA(fp_center_dot),	cs_set_dirty_flag },
	{"%d",  3, "fp_center_dot_a",	MV_OA(fp_center_dot_a),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  3, "fp_center_dot_ia",	MV_OA(fp_center_dot_ia),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

void
cs_set_dirty_flag()
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->_color_scheme == color_scheme)
      dmlp->_dirty = 1;
}

void
cs_update()
{
  register struct bu_structparse *sp;
  struct bu_vls vls;
  int offset;

  bu_vls_init(&vls);

  if (color_scheme->active)
    offset = 1;
  else
    offset = 2;

  for (sp = &color_scheme_vparse[CS_OFFSET]; sp->sp_name != NULL; sp += 3) {
    bu_vls_trunc(&vls, 0);
    bu_vls_printf(&vls, "cs_set %s [cs_set %s]", sp->sp_name, (sp+offset)->sp_name);
    Tcl_Eval(interp, bu_vls_addr(&vls));
  }

  cs_set_bg();
  bu_vls_free(&vls);
}

void
cs_set_bg()
{
  struct dm_list *dmlp;
  struct dm_list *save_curr_dmlp = curr_dm_list;
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "dm bg %d %d %d",
		color_scheme->bg[0],
		color_scheme->bg[1],
		color_scheme->bg[2]);

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if (dmlp->_color_scheme == color_scheme) {
      dmlp->_dirty = 1;
      curr_dm_list = dmlp;
      Tcl_Eval(interp, bu_vls_addr(&vls));
    }

  bu_vls_free(&vls);
  curr_dm_list = save_curr_dmlp;
}

int
f_cs_set (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;
  struct bu_vls tmp_vls;

  bu_vls_init(&vls);

  if(argc < 1 || 5 < argc){
    bu_vls_printf(&vls, "help cs_set");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  bu_vls_init(&tmp_vls);
  start_catching_output(&tmp_vls);

  if (argc < 2) {
    /* Bare set command, print out current settings */
    bu_struct_print("Color Scheme", color_scheme_vparse,
		    (const char *)color_scheme);
  } else if (argc == 2) {
    bu_vls_struct_item_named(&vls, color_scheme_vparse, argv[1],
			     (const char *)color_scheme, ' ');
    bu_log("%s", bu_vls_addr(&vls));
  } else {
    bu_vls_printf(&vls, "%s=\"", argv[1]);
    bu_vls_from_argv(&vls, argc-2, argv+2);
    bu_vls_putc(&vls, '\"');
    bu_struct_parse(&vls, color_scheme_vparse, (char *)color_scheme);
  }

  stop_catching_output(&tmp_vls);
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);

  bu_vls_free(&vls);
  bu_vls_free(&tmp_vls);

  return TCL_OK;
}

int
f_cs_def (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;
  struct bu_vls tmp_vls;

  bu_vls_init(&vls);

  if(argc < 1 || 5 < argc){
    bu_vls_printf(&vls, "help cs_def");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  bu_vls_init(&tmp_vls);
  start_catching_output(&tmp_vls);

  if (argc < 2) {
    /* Bare set command, print out current settings */
    bu_struct_print("Default Color Scheme", color_scheme_vparse,
		    (const char *)&default_color_scheme);
  } else if (argc == 2) {
    bu_vls_struct_item_named(&vls, color_scheme_vparse, argv[1],
			     (const char *)&default_color_scheme, ' ');
    bu_log("%s", bu_vls_addr(&vls));
  } else {
    bu_vls_printf(&vls, "%s=\"", argv[1]);
    bu_vls_from_argv(&vls, argc-2, argv+2);
    bu_vls_putc(&vls, '\"');
    bu_struct_parse(&vls, color_scheme_vparse, (char *)&default_color_scheme);
  }

  stop_catching_output(&tmp_vls);
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);

  bu_vls_free(&vls);
  bu_vls_free(&tmp_vls);

  return TCL_OK;
}
