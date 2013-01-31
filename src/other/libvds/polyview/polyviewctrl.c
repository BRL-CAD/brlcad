/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "polyviewctrl.h"

FD_PolyViewCtrl *create_form_PolyViewCtrl(void)
{
  FL_OBJECT *obj;
  FD_PolyViewCtrl *fdui = (FD_PolyViewCtrl *) fl_calloc(1, sizeof(*fdui));

  fdui->PolyViewCtrl = fl_bgn_form(FL_NO_BOX, 470, 230);
  obj = fl_add_box(FL_UP_BOX,0,0,470,230,"");
  obj = fl_add_box(FL_DOWN_BOX,10,160,190,60,"");
  fdui->btn_quit = obj = fl_add_button(FL_NORMAL_BUTTON,410,200,50,20,"Quit");
    fl_set_object_callback(obj,cb_quit,0);
  fdui->sld_mat_amb_r = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,10,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_amb,0);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_amb_g = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,20,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_amb,1);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_amb_b = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,30,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_amb,2);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_dif_r = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,50,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_dif,0);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_dif_g = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,60,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_dif,1);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 1);
  fdui->sld_mat_dif_b = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,70,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_dif,2);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_spec_r = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,90,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_spec,0);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_spec_g = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,100,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_spec,1);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_spec_b = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,110,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_spec,2);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_emis_r = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,130,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_emis,0);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_emis_g = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,140,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_emis,1);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_mat_emis_b = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,150,50,10,90,"");
    fl_set_object_callback(obj,cb_mat_emis,2);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0);
  fdui->sld_glb_amb_r = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,220,50,10,150,"");
    fl_set_object_callback(obj,cb_glb_amb,0);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0.2);
  fdui->sld_glb_amb_g = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,230,50,10,150,"");
    fl_set_object_callback(obj,cb_glb_amb,1);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0.2);
  fdui->sld_glb_amb_b = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,240,50,10,150,"");
    fl_set_object_callback(obj,cb_glb_amb,2);
    fl_set_slider_bounds(obj, 1, 0);
    fl_set_slider_value(obj, 0.2);
  obj = fl_add_text(FL_NORMAL_TEXT,10,10,190,20,"Material Properties");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_EMBOSSED_STYLE);
  obj = fl_add_text(FL_NORMAL_TEXT,10,30,30,20,"Amb");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,130,30,30,20,"Emis");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,50,30,30,20,"Dif");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,90,30,30,20,"Spec");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,210,20,50,30,"Global\nLight");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_EMBOSSED_STYLE);
  obj = fl_add_text(FL_NORMAL_TEXT,170,30,30,20,"Shiny");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,10,140,30,20,"RGB");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,50,140,30,20,"RGB");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,90,140,30,20,"RGB");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
  obj = fl_add_text(FL_NORMAL_TEXT,130,140,30,20,"RGB");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
  fdui->sld_mat_shiny = obj = fl_add_valslider(FL_VERT_BROWSER_SLIDER,170,50,30,90,"");
    fl_set_object_callback(obj,cb_mat_shiny,0);
    fl_set_slider_bounds(obj, 100, 0);
    fl_set_slider_value(obj, 0);
    fl_set_slider_size(obj, 0.11);
    fl_set_slider_step(obj, 0.05);
  obj = fl_add_text(FL_NORMAL_TEXT,220,200,30,20,"RGB");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
  fdui->light_pos = obj = fl_add_positioner(FL_NORMAL_POSITIONER,270,30,100,100,"");
    fl_set_object_boxtype(obj,FL_FRAME_BOX);
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_callback(obj,cb_light_pos,0);
    fl_set_positioner_xbounds(obj, -1, 1);
    fl_set_positioner_ybounds(obj, -1, 1);
    fl_set_positioner_xvalue(obj, 0);
    fl_set_positioner_yvalue(obj, 0);
  obj = fl_add_text(FL_NORMAL_TEXT,270,10,100,20,"Light Position");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_EMBOSSED_STYLE);
  fdui->sld_zoom = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,380,30,20,190,"");
    fl_set_object_callback(obj,cb_zoom,0);
    fl_set_slider_bounds(obj, 1, -1);
    fl_set_slider_value(obj, 0);
  obj = fl_add_text(FL_NORMAL_TEXT,370,10,40,20,"Zoom");
    fl_set_object_lalign(obj,FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_EMBOSSED_STYLE);

  fdui->grp_render_mode = fl_bgn_group();
  fdui->btn_wireframe = obj = fl_add_lightbutton(FL_RADIO_BUTTON,270,140,100,20,"Wireframe");
    fl_set_object_callback(obj,cb_wireframe,0);
    fl_set_button(obj, 1);
  fdui->btn_shaded = obj = fl_add_lightbutton(FL_RADIO_BUTTON,270,160,100,20,"Shaded/Lit");
    fl_set_object_callback(obj,cb_shadedlit,0);
  fl_end_group();


  fdui->grp_view_mode = fl_bgn_group();
  fdui->btn_walkthru = obj = fl_add_lightbutton(FL_RADIO_BUTTON,270,200,100,20,"Walkthru");
    fl_set_object_callback(obj,cb_walkthru,0);
  fdui->btn_inspect = obj = fl_add_lightbutton(FL_RADIO_BUTTON,270,180,100,20,"Inspect");
    fl_set_object_callback(obj,cb_inspect,0);
    fl_set_button(obj, 1);
  fl_end_group();

  fdui->sld_threshold = obj = fl_add_valslider(FL_HOR_BROWSER_SLIDER,20,170,170,20,"");
    fl_set_object_callback(obj,cb_threshold,0);
  fdui->btn_vds = obj = fl_add_lightbutton(FL_PUSH_BUTTON,100,195,90,20,"VDS Enable");
    fl_set_object_callback(obj,cb_vds,0);
  obj = fl_add_text(FL_NORMAL_TEXT,20,195,70,20,"Threshold");
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE);
  fdui->btn_reset = obj = fl_add_button(FL_NORMAL_BUTTON,410,175,50,20,"Reset");
    fl_set_object_callback(obj,cb_reset,0);
  fdui->Load = obj = fl_add_button(FL_NORMAL_BUTTON,410,30,50,20,"Load...");
    fl_set_object_callback(obj,cb_load,0);
  obj = fl_add_button(FL_NORMAL_BUTTON,410,30,50,20,"Load...");
    fl_set_object_callback(obj,cb_load,0);
  obj = fl_add_button(FL_NORMAL_BUTTON,410,55,50,20,"Save...");
    fl_set_object_callback(obj,cb_save,0);
  fl_end_form();

  fdui->PolyViewCtrl->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

