#ifndef SEEN_MGED_DM_H
#define SEEN_MGED_DM_H

/*
 *			D M . H
 *
 * Header file for communication with the display manager.
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#include "dm.h"	/* struct dm */
#include "./menu.h" /* struct menu_item */
#define TRY_NEW_MGED_VARS 0
#define DO_NEW_EDIT_MATS

struct device_values  {
	struct bu_vls	dv_string;	/* newline-separated "commands" from dm */
};
extern struct device_values dm_values;

/* defined in ged.c */
extern int dm_pipe[];

#define SL_TOL 0.031265266  /* size of dead spot - 64/2047 */

#define ALT_MOUSE_MODE_IDLE 0
#define ALT_MOUSE_MODE_ROTATE 1 
#define ALT_MOUSE_MODE_TRANSLATE 2
#define ALT_MOUSE_MODE_ZOOM 3

#define VIEW_TABLE_SIZE 5    /* enough to hold the menu's view selections */

struct shared_info {
  fastf_t _Viewscale;
  fastf_t _i_Viewscale;
  fastf_t azimuth;
  fastf_t elevation;
  fastf_t twist;
  mat_t   _Viewrot;
  mat_t   _toViewcenter;
  mat_t   _model2view;
  mat_t   _view2model;
  mat_t   _model2objview;
  mat_t   _objview2model;

  mat_t	  _viewrot_table[VIEW_TABLE_SIZE];
  fastf_t _viewscale_table[VIEW_TABLE_SIZE];
  int	  _current_view;

/* Angle/distance cursor stuff */
  int	  _dv_xadc;
  int	  _dv_yadc;
  int	  _dv_1adc;
  int	  _dv_2adc;
  int	  _dv_distadc;
  fastf_t _curs_x;
  fastf_t _curs_y;
  fastf_t _c_tdist;
  fastf_t _angle1;
  fastf_t _angle2;
  fastf_t _adc_a1_deg;
  fastf_t _adc_a2_deg;

/* Rate stuff */
  int	  _rateflag_tran;
  vect_t  _rate_tran;

  int	  _rateflag_rotate;
  vect_t  _rate_rotate;
	
  int	  _rateflag_scale;
  fastf_t _rate_scale;

/* Absolute stuff */
  vect_t  _absolute_tran;
  vect_t  _absolute_rotate;
  vect_t  _last_absolute_rotate;
  fastf_t _absolute_scale;

/* Virtual trackball stuff */
  point_t _orig_pos;

  int _dmaflag;
  int _rc;         /* reference count */

#if TRY_NEW_MGED_VARS
  /* Tcl variable names for mged_variables */
  struct bu_vls mged_variable_names[27];
#endif

  /* Tcl variable names for display info */
  struct bu_vls _aet_name;
  struct bu_vls _ang_name;
  struct bu_vls _center_name;
  struct bu_vls _size_name;
  struct bu_vls _adc_name;

  /* Tcl variable names for sliders */
  struct bu_vls _rate_tran_vls[3];
  struct bu_vls _rate_rotate_vls[3];
  struct bu_vls _rate_scale_vls;
  struct bu_vls _absolute_tran_vls[3];
  struct bu_vls _absolute_rotate_vls[3];
  struct bu_vls _absolute_scale_vls;
  struct bu_vls _xadc_vls;
  struct bu_vls _yadc_vls;
  struct bu_vls _ang1_vls;
  struct bu_vls _ang2_vls;
  struct bu_vls _distadc_vls;

  /* Convenient pointer to the owner's (of the shared_info) tkName */
  struct bu_vls *opp;
};


struct dm_list {
  struct bu_list l;
  struct dm *_dmp;

/* New stuff to allow more than one active display manager */
  struct shared_info *s_info;  /* info that can be used by display managers that are tied */
  int _dirty;      /* true if received an expose or configuration event */
  int _owner;      /* true if owner of the shared info */
  int _am_mode;    /* alternate mouse mode */
  int _ndrawn;
  double _frametime;/* time needed to draw last frame */
  struct bu_vls _fps_name;
  struct cmd_list *aim;
  struct  _mged_variables _mged_variables;
  struct menu_item *_menu_array[NMENU];    /* base of array of menu items */
  int _menuflag;
  int _menu_top;
  int _cur_menu;
  int _cur_menu_item;

/* Slider stuff */
  int _scroll_top;
  int _scroll_active;
  int _scroll_y;
  int _scroll_edit;
  struct scroll_item *_scroll_array[6];
  struct bu_vls _scroll_edit_vls;

  int _last_v_axes;
  void (*_knob_hook)();
  void (*_axes_color_hook)();
  int (*_cmd_hook)();
  void (*_state_hook)();
  void (*_viewpoint_hook)();
  int (*dm_init)();
};

extern int update_views;   /* from ged.c */
extern struct dm_list head_dm_list;  /* list of active display managers */
extern struct dm_list *curr_dm_list;

#define DM_LIST_NULL ((struct dm_list *)NULL)
#define dmp curr_dm_list->_dmp
#define pathName dmp->dm_pathName
#define tkName dmp->dm_tkName
#define dName dmp->dm_dName
#define dirty curr_dm_list->_dirty
#define owner curr_dm_list->_owner
#define am_mode curr_dm_list->_am_mode
#define ndrawn curr_dm_list->_ndrawn
#define frametime curr_dm_list->_frametime
#define fps_name curr_dm_list->_fps_name
#define knob_hook curr_dm_list->_knob_hook
#define axes_color_hook curr_dm_list->_axes_color_hook
#define cmd_hook curr_dm_list->_cmd_hook
#define state_hook curr_dm_list->_state_hook
#define viewpoint_hook curr_dm_list->_viewpoint_hook

#if 0
#define mged_variables curr_dm_list->s_info->_mged_variables
#else
#define mged_variables curr_dm_list->_mged_variables
#endif

#define menu_array curr_dm_list->_menu_array
#define menuflag curr_dm_list->_menuflag
#define menu_top curr_dm_list->_menu_top
#define cur_menu curr_dm_list->_cur_menu
#define cur_item curr_dm_list->_cur_menu_item

#define curs_x curr_dm_list->s_info->_curs_x
#define curs_y curr_dm_list->s_info->_curs_y
#define c_tdist curr_dm_list->s_info->_c_tdist
#define angle1 curr_dm_list->s_info->_angle1
#define angle2 curr_dm_list->s_info->_angle2
#define adc_a1_deg curr_dm_list->s_info->_adc_a1_deg
#define adc_a2_deg curr_dm_list->s_info->_adc_a2_deg
#define dv_xadc curr_dm_list->s_info->_dv_xadc
#define dv_yadc curr_dm_list->s_info->_dv_yadc
#define dv_1adc curr_dm_list->s_info->_dv_1adc
#define dv_2adc curr_dm_list->s_info->_dv_2adc
#define dv_distadc curr_dm_list->s_info->_dv_distadc

#define rateflag_slew curr_dm_list->s_info->_rateflag_tran
#define rateflag_tran curr_dm_list->s_info->_rateflag_tran
#define rate_slew curr_dm_list->s_info->_rate_tran
#define rate_tran curr_dm_list->s_info->_rate_tran
#define absolute_slew curr_dm_list->s_info->_absolute_tran
#define absolute_tran curr_dm_list->s_info->_absolute_tran
#define rateflag_rotate curr_dm_list->s_info->_rateflag_rotate
#define rate_rotate curr_dm_list->s_info->_rate_rotate
#define absolute_rotate curr_dm_list->s_info->_absolute_rotate
#define last_absolute_rotate curr_dm_list->s_info->_last_absolute_rotate
#define rateflag_zoom curr_dm_list->s_info->_rateflag_scale
#define rate_zoom curr_dm_list->s_info->_rate_scale
#define absolute_zoom curr_dm_list->s_info->_absolute_scale

#define Viewscale curr_dm_list->s_info->_Viewscale
#define i_Viewscale curr_dm_list->s_info->_i_Viewscale
#define Viewrot curr_dm_list->s_info->_Viewrot
#define toViewcenter curr_dm_list->s_info->_toViewcenter
#define model2view curr_dm_list->s_info->_model2view
#define view2model curr_dm_list->s_info->_view2model
#define model2objview curr_dm_list->s_info->_model2objview
#define objview2model curr_dm_list->s_info->_objview2model
#define viewrot_table curr_dm_list->s_info->_viewrot_table
#define viewscale_table curr_dm_list->s_info->_viewscale_table
#define current_view curr_dm_list->s_info->_current_view

#define rot_x curr_dm_list->s_info->_rot_x
#define rot_y curr_dm_list->s_info->_rot_y
#define rot_z curr_dm_list->s_info->_rot_z
#define tran_x curr_dm_list->s_info->_tran_x
#define tran_y curr_dm_list->s_info->_tran_y
#define tran_z curr_dm_list->s_info->_tran_z
#define orig_pos curr_dm_list->s_info->_orig_pos

#define dmaflag curr_dm_list->s_info->_dmaflag
#define rc curr_dm_list->s_info->_rc

#define aet_name curr_dm_list->s_info->_aet_name
#define ang_name curr_dm_list->s_info->_ang_name
#define center_name curr_dm_list->s_info->_center_name
#define size_name curr_dm_list->s_info->_size_name
#define adc_name curr_dm_list->s_info->_adc_name

#define rate_tran_vls curr_dm_list->s_info->_rate_tran_vls
#define rate_rotate_vls curr_dm_list->s_info->_rate_rotate_vls
#define rate_scale_vls curr_dm_list->s_info->_rate_scale_vls
#define absolute_tran_vls curr_dm_list->s_info->_absolute_tran_vls
#define absolute_rotate_vls curr_dm_list->s_info->_absolute_rotate_vls
#define absolute_scale_vls curr_dm_list->s_info->_absolute_scale_vls
#define xadc_vls curr_dm_list->s_info->_xadc_vls
#define yadc_vls curr_dm_list->s_info->_yadc_vls
#define ang1_vls curr_dm_list->s_info->_ang1_vls
#define ang2_vls curr_dm_list->s_info->_ang2_vls
#define distadc_vls curr_dm_list->s_info->_distadc_vls

#if 0
#define scroll_top curr_dm_list->s_info->_scroll_top
#define scroll_active curr_dm_list->s_info->_scroll_active
#define scroll_y curr_dm_list->s_info->_scroll_y
#define scroll_edit curr_dm_list->s_info->_scroll_edit
#define scroll_array curr_dm_list->s_info->_scroll_array
#else
#define scroll_top curr_dm_list->_scroll_top
#define scroll_active curr_dm_list->_scroll_active
#define scroll_y curr_dm_list->_scroll_y
#define scroll_edit curr_dm_list->_scroll_edit
#define scroll_array curr_dm_list->_scroll_array
#define scroll_edit_vls curr_dm_list->_scroll_edit_vls
#endif

#define last_v_axes curr_dm_list->_last_v_axes

#define MINVIEW		0.001				
#define VIEWSIZE	(2.0*Viewscale)	/* Width of viewing cube */
#define VIEWFACTOR	(1/Viewscale)	/* 2.0 / VIEWSIZE */

#define RATE_ROT_FACTOR 6.0
#define ABS_ROT_FACTOR 180.0
#define ADC_ANGLE_FACTOR 45.0

#define ALT_MOUSE_MODE_NOT_ACTIVE(_type,_name)\
  ((_type)dm_vars)->_name == ALT_MOUSE_MODE_OFF ||\
  ((_type)dm_vars)->_name == ALT_MOUSE_MODE_ON

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

#define BV_SLICEMODE	15
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

#define GET_DM_LIST(p,structure,w) { \
	register struct dm_list *tp; \
	for(BU_LIST_FOR(tp, dm_list, &head_dm_list.l)) { \
		if(w == ((struct structure *)tp->_dmp->dm_vars)->win) { \
			(p) = tp; \
			break; \
		} \
	} \
\
	if(BU_LIST_IS_HEAD(tp, &head_dm_list.l))\
		p = DM_LIST_NULL;\
	}

#endif /* SEEN_MGED_DM_H */
