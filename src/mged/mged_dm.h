#ifndef SEEN_MGED_DM_H
#define SEEN_MGED_DM_H

/*
 *			M G E D _ D M . H
 *
 * Header file for communication with the display manager.
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

#include "dm.h"	/* struct dm */
#include "./menu.h" /* struct menu_item */
#include "./scroll.h" /* struct scroll_item */
#include "fb.h" /* FBIO */
#include "pkg.h" /* struct pkg_conn */

#define DO_NEW_EDIT_MATS

#define DO_DISPLAY_LISTS
#if 0
#define DO_SINGLE_DISPLAY_LIST
#endif

#if !defined(PI)      /* sometimes found in math.h */
#define PI 3.14159265358979323846264338327950288419716939937511
#endif

#define MGED_DISPLAY_VAR "mged_display"

#define RAD2DEG 57.2957795130823208767981548141051703324054724665642
#define DEG2RAD 0.01745329251994329573

/* +-2048 to +-1 */
#define GED2PM1(x) (((fastf_t)(x))*INV_GED)

#define SL_TOL 0.03125  /* size of dead spot - 64/2048 */

#define AMM_IDLE 0
#define AMM_ROT 1 
#define AMM_TRAN 2
#define AMM_SCALE 3
#define AMM_ADC_ANG1 4
#define AMM_ADC_ANG2 5
#define AMM_ADC_TRAN 6
#define AMM_ADC_DIST 7
#define AMM_CON_ROT_X 8
#define AMM_CON_ROT_Y 9
#define AMM_CON_ROT_Z 10
#define AMM_CON_TRAN_X 11
#define AMM_CON_TRAN_Y 12
#define AMM_CON_TRAN_Z 13
#define AMM_CON_SCALE_X 14
#define AMM_CON_SCALE_Y 15
#define AMM_CON_SCALE_Z 16 
#define AMM_CON_XADC 17
#define AMM_CON_YADC 18
#define AMM_CON_ANG1 19
#define AMM_CON_ANG2 20
#define AMM_CON_DIST 21

#define IS_CONSTRAINED_ROT(_mode) ( \
	(_mode) == AMM_CON_ROT_X || \
	(_mode) == AMM_CON_ROT_Y || \
	(_mode) == AMM_CON_ROT_Z )

#define IS_CONSTRAINED_TRAN(_mode) ( \
	(_mode) == AMM_CON_TRAN_X || \
	(_mode) == AMM_CON_TRAN_Y || \
	(_mode) == AMM_CON_TRAN_Z )

struct view_ring {
  struct bu_list	l;
  mat_t			vr_rot_mat;
  mat_t			vr_tvc_mat;
  fastf_t		vr_scale;
  int			vr_id;
};

#define NUM_TRAILS 8
#define MAX_TRAIL 32
struct trail {
  int		t_cur_index;      /* index of first free entry */
  int		t_nused;          /* max index in use */
  point_t	t_pt[MAX_TRAIL];
};

#ifdef MAX_CLIENTS
#	undef MAX_CLIENTS
#	define MAX_CLIENTS 32
#else
#	define MAX_CLIENTS 32
#endif

struct client {
#ifdef _WIN32
    HANDLE		c_fd;
    Tcl_Channel         c_chan;
    Tcl_FileProc        *c_handler;
#else
    int			c_fd;
#endif
    struct pkg_conn	*c_pkg;
};

/* mged command variables for affecting the user environment */
struct _mged_variables {
  int		mv_rc;
  int		mv_autosize;
  int		mv_rateknobs;
  int		mv_sliders;
  int		mv_faceplate;
  int		mv_orig_gui;
  int		mv_linewidth;
  char		mv_linestyle;
  int		mv_hot_key;
  int		mv_context;
  int		mv_dlist;
  int		mv_use_air;
  int		mv_listen;			/* nonzero to listen on port */
  int		mv_port;			/* port to listen on */
  int		mv_fb;				/* toggle image on/off */
  int		mv_fb_all;			/* 0 - use part of image as defined by the rectangle
						   1 - use the entire image */
  int		mv_fb_overlay;			/* 0 - underlay    1 - interlay    2 - overlay */
  char		mv_mouse_behavior;
  char		mv_coords;
  char		mv_rotate_about;
  char		mv_transform;
  int		mv_predictor;
  double	mv_predictor_advance;
  double	mv_predictor_length;
  double	mv_perspective;			/* used to directly set the perspective angle */
  int		mv_perspective_mode;		/* used to toggle perspective viewing on/off */
  int		mv_toggle_perspective;		/* used to toggle through values in perspective_table[] */
  double	mv_nmg_eu_dist;
  double	mv_eye_sep_dist;		/* >0 implies stereo.  units = "room" mm */
  char		mv_union_lexeme[1024];
  char		mv_intersection_lexeme[1024];
  char		mv_difference_lexeme[1024];
};

struct _axes_state {
  int		ax_rc;
  int		ax_model_draw;			/* model axes */
  int		ax_model_size;
  int		ax_model_linewidth;
  fastf_t	ax_model_pos[3];
  int		ax_view_draw;			/* view axes */
  int		ax_view_size;
  int		ax_view_linewidth;
  int		ax_view_pos[2];
  int		ax_edit_draw;			/* edit axes */
  int		ax_edit_size1;
  int		ax_edit_size2;
  int		ax_edit_linewidth1;
  int		ax_edit_linewidth2;
};

struct _dlist_state {
  int		dl_rc;
  int		dl_active;	/* 1 - actively using display lists */
  int		dl_flag;
};

struct _grid_state {
  int		gr_rc;
  int		gr_draw;	/* draw grid */
  int		gr_snap;	/* snap to grid */
  fastf_t	gr_anchor[3];
  fastf_t	gr_res_h;	/* grid resolution in h */
  fastf_t	gr_res_v;	/* grid resolution in v */
  int		gr_res_major_h;	/* major grid resolution in h */
  int		gr_res_major_v;	/* major grid resolution in v */
};

struct _adc_state {
  int		adc_rc;
  int		adc_draw;
  int		adc_dv_x;
  int		adc_dv_y;
  int		adc_dv_a1;
  int		adc_dv_a2;
  int		adc_dv_dist;
  fastf_t	adc_pos_model[3];
  fastf_t	adc_pos_view[3];
  fastf_t	adc_pos_grid[3];
  fastf_t	adc_a1;
  fastf_t	adc_a2;
  fastf_t	adc_dst;
  int		adc_anchor_pos;
  int		adc_anchor_a1;
  int		adc_anchor_a2;
  int		adc_anchor_dst;
  fastf_t	adc_anchor_pt_a1[3];
  fastf_t	adc_anchor_pt_a2[3];
  fastf_t	adc_anchor_pt_dst[3];
};

struct _rubber_band {
  int		rb_rc;
  int		rb_active;	/* 1 - actively drawing a rubber band */
  int		rb_draw;	/* draw rubber band rectangle */
  int		rb_linewidth;
  char		rb_linestyle;
  int		rb_pos[2];	/* Position in image coordinates */
  int		rb_dim[2];	/* Rectangle dimension in image coordinates */
  fastf_t	rb_x;		/* Corner of rectangle in normalized     */
  fastf_t	rb_y;		/* ------ view coordinates (i.e. +-1.0). */
  fastf_t	rb_width;	/* Width and height of rectangle in      */
  fastf_t	rb_height;	/* ------ normalized view coordinates.   */
};

struct _view_state {
  int		vs_rc;
  int		vs_flag;

  struct view_obj	*vs_vop;
  fastf_t	vs_i_Viewscale;
  mat_t		vs_model2objview;
  mat_t		vs_objview2model;
  mat_t		vs_ModelDelta;		/* changes to Viewrot this frame */

  struct view_ring	vs_headView;
  struct view_ring	*vs_current_view;
  struct view_ring	*vs_last_view;

  /* Rate stuff */
  int		vs_rateflag_model_tran;
  vect_t	vs_rate_model_tran;

  int		vs_rateflag_model_rotate;
  vect_t	vs_rate_model_rotate;
  char		vs_rate_model_origin;

  int		vs_rateflag_tran;
  vect_t	vs_rate_tran;

  int		vs_rateflag_rotate;
  vect_t	vs_rate_rotate;
  char		vs_rate_origin;
	
  int		vs_rateflag_scale;
  fastf_t	vs_rate_scale;

  /* Absolute stuff */
  vect_t	vs_absolute_tran;
  vect_t	vs_absolute_model_tran;
  vect_t	vs_last_absolute_tran;
  vect_t	vs_last_absolute_model_tran;
  vect_t	vs_absolute_rotate;
  vect_t	vs_absolute_model_rotate;
  vect_t	vs_last_absolute_rotate;
  vect_t	vs_last_absolute_model_rotate;
  fastf_t	vs_absolute_scale;

  /* Virtual trackball stuff */
  point_t	vs_orig_pos;
};

struct _color_scheme {
  int	cs_rc;
  int	cs_mode;
  int	cs_bg[3];
  int	cs_bg_a[3];
  int	cs_bg_ia[3];
  int	cs_adc_line[3];
  int	cs_adc_line_a[3];
  int	cs_adc_line_ia[3];
  int	cs_adc_tick[3];
  int	cs_adc_tick_a[3];
  int	cs_adc_tick_ia[3];
  int	cs_geo_def[3];
  int	cs_geo_def_a[3];
  int	cs_geo_def_ia[3];
  int	cs_geo_hl[3];
  int	cs_geo_hl_a[3];
  int	cs_geo_hl_ia[3];
  int	cs_geo_label[3];
  int	cs_geo_label_a[3];
  int	cs_geo_label_ia[3];
  int	cs_model_axes[3];
  int	cs_model_axes_a[3];
  int	cs_model_axes_ia[3];
  int	cs_model_axes_label[3];
  int	cs_model_axes_label_a[3];
  int	cs_model_axes_label_ia[3];
  int	cs_view_axes[3];
  int	cs_view_axes_a[3];
  int	cs_view_axes_ia[3];
  int	cs_view_axes_label[3];
  int	cs_view_axes_label_a[3];
  int	cs_view_axes_label_ia[3];
  int	cs_edit_axes1[3];
  int	cs_edit_axes1_a[3];
  int	cs_edit_axes1_ia[3];
  int	cs_edit_axes_label1[3];
  int	cs_edit_axes_label1_a[3];
  int	cs_edit_axes_label1_ia[3];
  int	cs_edit_axes2[3];
  int	cs_edit_axes2_a[3];
  int	cs_edit_axes2_ia[3];
  int	cs_edit_axes_label2[3];
  int	cs_edit_axes_label2_a[3];
  int	cs_edit_axes_label2_ia[3];
  int	cs_rubber_band[3];
  int	cs_rubber_band_a[3];
  int	cs_rubber_band_ia[3];
  int	cs_grid[3];
  int	cs_grid_a[3];
  int	cs_grid_ia[3];
  int	cs_predictor[3];
  int	cs_predictor_a[3];
  int	cs_predictor_ia[3];
  int	cs_menu_line[3];
  int	cs_menu_line_a[3];
  int	cs_menu_line_ia[3];
  int	cs_slider_line[3];
  int	cs_slider_line_a[3];
  int	cs_slider_line_ia[3];
  int	cs_other_line[3];
  int	cs_other_line_a[3];
  int	cs_other_line_ia[3];
  int	cs_status_text1[3];
  int	cs_status_text1_a[3];
  int	cs_status_text1_ia[3];
  int	cs_status_text2[3];
  int	cs_status_text2_a[3];
  int	cs_status_text2_ia[3];
  int	cs_slider_text1[3];
  int	cs_slider_text1_a[3];
  int	cs_slider_text1_ia[3];
  int	cs_slider_text2[3];
  int	cs_slider_text2_a[3];
  int	cs_slider_text2_ia[3];
  int	cs_menu_text1[3];
  int	cs_menu_text1_a[3];
  int	cs_menu_text1_ia[3];
  int	cs_menu_text2[3];
  int	cs_menu_text2_a[3];
  int	cs_menu_text2_ia[3];
  int	cs_menu_title[3];
  int	cs_menu_title_a[3];
  int	cs_menu_title_ia[3];
  int	cs_menu_arrow[3];
  int	cs_menu_arrow_a[3];
  int	cs_menu_arrow_ia[3];
  int	cs_state_text1[3];
  int	cs_state_text1_a[3];
  int	cs_state_text1_ia[3];
  int	cs_state_text2[3];
  int	cs_state_text2_a[3];
  int	cs_state_text2_ia[3];
  int	cs_edit_info[3];
  int	cs_edit_info_a[3];
  int	cs_edit_info_ia[3];
  int	cs_center_dot[3];
  int	cs_center_dot_a[3];
  int	cs_center_dot_ia[3];
};

struct _menu_state {
  int	ms_rc;
  int	ms_flag;
  int	ms_top;
  int	ms_cur_menu;
  int	ms_cur_item;
  struct menu_item	*ms_menus[NMENU];    /* base of menu items array */
};

struct dm_list {
  struct bu_list	l;
  struct dm		*dml_dmp;
  FBIO			*dml_fbp;
  int			dml_netfd;			/* socket used to listen for connections */
#ifdef _WIN32
  Tcl_Channel		dml_netchan;
#endif
  struct client		dml_clients[MAX_CLIENTS];
  int			dml_dirty;			/* true if received an expose or configuration event */
  int			dml_mapped;
  int			dml_owner;			/* true if owner of the view info */
  int			dml_am_mode;			/* alternate mouse mode */
  int			dml_ndrawn;
  int			dml_perspective_angle;
  int			*dml_zclip_ptr;
  struct bu_list	dml_p_vlist;			/* predictor vlist */
  struct trail		dml_trails[NUM_TRAILS];
  struct cmd_list	*dml_tie;

  int			dml_adc_auto;
  int			dml_grid_auto_size;
  int			_dml_mouse_dx;
  int			_dml_mouse_dy;
  int			_dml_omx;
  int			_dml_omy;
  int			_dml_knobs[8];
  point_t		_dml_work_pt;

  /* Tcl variable names for display info */
  struct bu_vls		dml_fps_name;
  struct bu_vls		dml_aet_name;
  struct bu_vls		dml_ang_name;
  struct bu_vls		dml_center_name;
  struct bu_vls		dml_size_name;
  struct bu_vls		dml_adc_name;

  /* Slider stuff */
  int			dml_scroll_top;
  int			dml_scroll_active;
  int			dml_scroll_y;
  struct scroll_item	*dml_scroll_array[6];

  /* Shareable Resources */
  struct _view_state	*dml_view_state;
  struct _adc_state	*dml_adc_state;
  struct _menu_state	*dml_menu_state;
  struct _rubber_band	*dml_rubber_band;
  struct _mged_variables *dml_mged_variables;
  struct _color_scheme	*dml_color_scheme;
  struct _grid_state	*dml_grid_state;
  struct _axes_state	*dml_axes_state;
  struct _dlist_state	*dml_dlist_state;

  /* Hooks */
  int			(*dml_cmd_hook)();
  void			(*dml_viewpoint_hook)();
  int			(*dml_eventHandler)();
};

#define DM_LIST_NULL ((struct dm_list *)NULL)
#define dmp curr_dm_list->dml_dmp
#define fbp curr_dm_list->dml_fbp
#define netfd curr_dm_list->dml_netfd
#ifdef _WIN32
#define netchan curr_dm_list->dml_netchan
#endif
#define clients curr_dm_list->dml_clients
#define pathName dmp->dm_pathName
#define tkName dmp->dm_tkName
#define dName dmp->dm_dName
#define displaylist dmp->dm_displaylist
#define dirty curr_dm_list->dml_dirty
#define mapped curr_dm_list->dml_mapped
#define owner curr_dm_list->dml_owner
#define am_mode curr_dm_list->dml_am_mode
#define ndrawn curr_dm_list->dml_ndrawn
#define perspective_angle curr_dm_list->dml_perspective_angle
#define zclip_ptr curr_dm_list->dml_zclip_ptr

#define view_state curr_dm_list->dml_view_state
#define adc_state curr_dm_list->dml_adc_state
#define menu_state curr_dm_list->dml_menu_state
#define rubber_band curr_dm_list->dml_rubber_band
#define mged_variables curr_dm_list->dml_mged_variables
#define color_scheme curr_dm_list->dml_color_scheme
#define grid_state curr_dm_list->dml_grid_state
#define axes_state curr_dm_list->dml_axes_state
#define dlist_state curr_dm_list->dml_dlist_state

#define cmd_hook curr_dm_list->dml_cmd_hook
#define viewpoint_hook curr_dm_list->dml_viewpoint_hook
#define eventHandler curr_dm_list->dml_eventHandler

#define adc_auto curr_dm_list->dml_adc_auto
#define grid_auto_size curr_dm_list->dml_grid_auto_size

/* Names of macros must be different than actual struct element */
#define dml_mouse_dx curr_dm_list->_dml_mouse_dx
#define dml_mouse_dy curr_dm_list->_dml_mouse_dy
#define dml_omx curr_dm_list->_dml_omx
#define dml_omy curr_dm_list->_dml_omy
#define dml_knobs curr_dm_list->_dml_knobs
#define dml_work_pt curr_dm_list->_dml_work_pt

#define scroll_top curr_dm_list->dml_scroll_top
#define scroll_active curr_dm_list->dml_scroll_active
#define scroll_y curr_dm_list->dml_scroll_y
#define scroll_array curr_dm_list->dml_scroll_array

#define MINVIEW		0.001				
#define VIEWSIZE	(2.0*view_state->vs_Viewscale)	/* Width of viewing cube */
#define VIEWFACTOR	(1/view_state->vs_Viewscale)	/* 2.0 / VIEWSIZE */

#define RATE_ROT_FACTOR 6.0
#define ABS_ROT_FACTOR 180.0
#define ADC_ANGLE_FACTOR 45.0

/*
 * Definitions for dealing with the buttons and lights.
 * BV are for viewing, and BE are for editing functions.
 */
#define LIGHT_OFF	0
#define LIGHT_ON	1
#define LIGHT_RESET	2		/* all lights out */

/* Function button/light codes.  Note that code 0 is reserved */
#define BV_TOP		15+16
#define BV_BOTTOM	14+16
#define BV_RIGHT	13+16
#define BV_LEFT		12+16
#define BV_FRONT	11+16
#define BV_REAR		10+16
#define BV_VRESTORE	9+16
#define BV_VSAVE	8+16
#define BE_O_ILLUMINATE	7+16
#define BE_O_SCALE	6+16
#define BE_O_X		5+16
#define BE_O_Y		4+16
#define BE_O_XY		3+16
#define BE_O_ROTATE	2+16
#define BE_ACCEPT	1+16
#define BE_REJECT	0+16

#define BE_S_EDIT	14
#define BE_S_ROTATE	13
#define BE_S_TRANS	12
#define BE_S_SCALE	11
#define BE_MENU		10
#define BV_ADCURSOR	9
#define BV_RESET	8
#define BE_S_ILLUMINATE	7
#define BE_O_XSCALE	6
#define BE_O_YSCALE	5
#define BE_O_ZSCALE	4
#define BV_ZOOM_IN	3
#define BV_ZOOM_OUT	2
#define BV_45_45	1
#define BV_35_25	0+32

#define BV_RATE_TOGGLE	1+32
#define BV_EDIT_TOGGLE  2+32
#define BV_EYEROT_TOGGLE 3+32
#define BE_S_CONTEXT    4+32

#define BV_MAXFUNC	64	/* largest code used */

#define FOR_ALL_DISPLAYS(p,hp) \
	for(BU_LIST_FOR(p,dm_list,hp))

#define GET_DM_LIST(p,id) { \
		register struct dm_list *tp; \
\
		FOR_ALL_DISPLAYS(tp, &head_dm_list.l) { \
			if((id) == tp->dml_dmp->dm_id) { \
				(p) = tp; \
				break; \
			} \
		} \
\
		if(BU_LIST_IS_HEAD(tp, &head_dm_list.l)) \
			(p) = DM_LIST_NULL; \
	}

extern double frametime;		/* defined in ged.c */
extern int doEvent();			/* defined in doevent.c */
extern int dm_pipe[];			/* defined in ged.c */
extern int update_views;		/* defined in ged.c */
extern struct dm_list head_dm_list;	/* defined in attach.c */
extern struct dm_list *curr_dm_list;	/* defined in attach.c */

struct w_dm {
  int	type;
  char	*name;
  int	(*init)();
};
extern struct w_dm which_dm[];  /* defined in attach.c */

/* indices into which_dm[] */
#define DM_PLOT_INDEX 0
#define DM_PS_INDEX 1


/* Flags indicating whether the ogl and sgi display managers have been
 * attached. Defined in dm-ogl.c. 
 * These are necessary to decide whether or not to use direct rendering
 * with ogl.
 */
extern	char	ogl_ogl_used;
extern	char	ogl_sgi_used;

#endif /* SEEN_MGED_DM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
