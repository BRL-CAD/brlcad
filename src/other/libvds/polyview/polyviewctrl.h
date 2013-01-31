/** Header file generated with fdesign on Wed Jun 16 14:44:28 1999.**/

#ifndef FD_PolyViewCtrl_h_
#define FD_PolyViewCtrl_h_

/** Callbacks, globals and object handlers **/
extern void cb_quit(FL_OBJECT *, long);
extern void cb_mat_amb(FL_OBJECT *, long);
extern void cb_mat_dif(FL_OBJECT *, long);
extern void cb_mat_spec(FL_OBJECT *, long);
extern void cb_mat_emis(FL_OBJECT *, long);
extern void cb_glb_amb(FL_OBJECT *, long);
extern void cb_mat_shiny(FL_OBJECT *, long);
extern void cb_light_pos(FL_OBJECT *, long);
extern void cb_zoom(FL_OBJECT *, long);
extern void cb_wireframe(FL_OBJECT *, long);
extern void cb_shadedlit(FL_OBJECT *, long);
extern void cb_walkthru(FL_OBJECT *, long);
extern void cb_inspect(FL_OBJECT *, long);
extern void cb_threshold(FL_OBJECT *, long);
extern void cb_vds(FL_OBJECT *, long);
extern void cb_reset(FL_OBJECT *, long);
extern void cb_load(FL_OBJECT *, long);
extern void cb_save(FL_OBJECT *, long);


/**** Forms and Objects ****/
typedef struct {
	FL_FORM *PolyViewCtrl;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *btn_quit;
	FL_OBJECT *sld_mat_amb_r;
	FL_OBJECT *sld_mat_amb_g;
	FL_OBJECT *sld_mat_amb_b;
	FL_OBJECT *sld_mat_dif_r;
	FL_OBJECT *sld_mat_dif_g;
	FL_OBJECT *sld_mat_dif_b;
	FL_OBJECT *sld_mat_spec_r;
	FL_OBJECT *sld_mat_spec_g;
	FL_OBJECT *sld_mat_spec_b;
	FL_OBJECT *sld_mat_emis_r;
	FL_OBJECT *sld_mat_emis_g;
	FL_OBJECT *sld_mat_emis_b;
	FL_OBJECT *sld_glb_amb_r;
	FL_OBJECT *sld_glb_amb_g;
	FL_OBJECT *sld_glb_amb_b;
	FL_OBJECT *sld_mat_shiny;
	FL_OBJECT *light_pos;
	FL_OBJECT *sld_zoom;
	FL_OBJECT *grp_render_mode;
	FL_OBJECT *btn_wireframe;
	FL_OBJECT *btn_shaded;
	FL_OBJECT *grp_view_mode;
	FL_OBJECT *btn_walkthru;
	FL_OBJECT *btn_inspect;
	FL_OBJECT *sld_threshold;
	FL_OBJECT *btn_vds;
	FL_OBJECT *btn_reset;
	FL_OBJECT *Load;
} FD_PolyViewCtrl;

extern FD_PolyViewCtrl * create_form_PolyViewCtrl(void);

#endif /* FD_PolyViewCtrl_h_ */
