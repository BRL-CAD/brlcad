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

#include "rtstring.h"		/* for vls string support */

struct device_values  {
	struct rt_vls	dv_string;	/* newline-separated "commands" from dm */
};
extern struct device_values dm_values;

/* Interface to a specific Display Manager */
struct dm {
	int	(*dmr_open)();
	void	(*dmr_close)();
	void	(*dmr_input)MGED_ARGS((fd_set *input, int noblock));
	void	(*dmr_prolog)();
	void	(*dmr_epilog)();
	void	(*dmr_normal)();
	void	(*dmr_newrot)();
	void	(*dmr_update)();
	void	(*dmr_puts)();
	void	(*dmr_2d_line)();
	void	(*dmr_light)();
	int	(*dmr_object)();	/* Invoke an object subroutine */
	unsigned (*dmr_cvtvecs)();	/* returns size requirement of subr */
	unsigned (*dmr_load)();		/* DMA the subr to device */
	void	(*dmr_statechange)();	/* called on editor state change */
	void	(*dmr_viewchange)();	/* add/drop solids from view */
	void	(*dmr_colorchange)();	/* called when color table changes */
	void	(*dmr_window)();	/* Change window boundry */
	void	(*dmr_debug)();		/* Set DM debug level */
	int	dmr_displaylist;	/* !0 means device has displaylist */
	int	dmr_releasedisplay;	/* !0 release for other programs */
	double	dmr_bound;		/* zoom-in limit */
	char	*dmr_name;		/* short name of device */
	char	*dmr_lname;		/* long name of device */
	struct mem_map *dmr_map;	/* displaylist mem map */
	int	(*dmr_cmd)();		/* dm-specific cmds to perform */
};

#ifdef MULTI_ATTACH

struct dm_list {
  struct rt_list l;
  struct dm *_dmp;

/* New stuff to allow more than one active display manager */
  char *_dm_vars;   /* pointer to dependant display manager variables */
  struct rt_vls _pathName; /* full name of drawing window */
  int     _dmaflag;
  fastf_t _Viewscale;
  mat_t   _Viewrot;
  mat_t   _toViewcenter;
  mat_t   _model2view;
  mat_t   _view2model;
  mat_t   _model2objview;
  mat_t   _objview2model;
  struct _mged_variables _mged_variables;

/* Angle/distance cursor stuff */
  int     _adcflag;
  int     _dv_xadc;
  int     _dv_yadc;
  int     _dv_1adc;
  int     _dv_2adc;
  int     _dv_distadc;
  fastf_t _curs_x;
  fastf_t _curs_y;
  fastf_t _c_tdist;
  fastf_t _angle1;
  fastf_t _angle2;

/* Rate stuff */
  int     _rateflag_slew;
  vect_t  _rate_slew;
  vect_t  _absolute_slew;

  int     _rateflag_rotate;
  vect_t  _rate_rotate;
  vect_t  _absolute_rotate;
	
  int     _rateflag_zoom;
  fastf_t  _rate_zoom;
  fastf_t  _absolute_zoom;

#ifdef VIRTUAL_TRACKBALL
  double _rot_x;
  double _rot_y;
  double _rot_z;
  double _tran_x;
  double _tran_y;
  double _tran_z;
  point_t _orig_pos;
#endif
};

extern int update_views;   /* from dm-X.h */
extern struct dm_list head_dm_list;  /* list of active display managers */
extern struct dm_list *curr_dm_list;

#define DM_LIST_NULL ((struct dm_list *)NULL)
#define dmp curr_dm_list->_dmp
#define dm_vars curr_dm_list->_dm_vars
#define pathName curr_dm_list->_pathName
#define mged_variables curr_dm_list->_mged_variables
#define dmaflag curr_dm_list->_dmaflag

#define adcflag curr_dm_list->_adcflag
#define curs_x curr_dm_list->_curs_x
#define curs_y curr_dm_list->_curs_y
#define c_tdist curr_dm_list->_c_tdist
#define angle1 curr_dm_list->_angle1
#define angle2 curr_dm_list->_angle2
#define dv_xadc curr_dm_list->_dv_xadc
#define dv_yadc curr_dm_list->_dv_yadc
#define dv_1adc curr_dm_list->_dv_1adc
#define dv_2adc curr_dm_list->_dv_2adc
#define dv_distadc curr_dm_list->_dv_distadc

#define rateflag_slew curr_dm_list->_rateflag_slew
#define rate_slew curr_dm_list->_rate_slew
#define absolute_slew curr_dm_list->_absolute_slew
#define rateflag_rotate curr_dm_list->_rateflag_rotate
#define rate_rotate curr_dm_list->_rate_rotate
#define absolute_rotate curr_dm_list->_absolute_rotate
#define rateflag_zoom curr_dm_list->_rateflag_zoom
#define rate_zoom curr_dm_list->_rate_zoom
#define absolute_zoom curr_dm_list->_absolute_zoom

#define Viewscale curr_dm_list->_Viewscale
#define Viewrot curr_dm_list->_Viewrot
#define toViewcenter curr_dm_list->_toViewcenter
#define model2view curr_dm_list->_model2view
#define view2model curr_dm_list->_view2model
#define model2objview curr_dm_list->_model2objview
#define objview2model curr_dm_list->_objview2model

#ifdef VIRTUAL_TRACKBALL
#define rot_x curr_dm_list->_rot_x
#define rot_y curr_dm_list->_rot_y
#define rot_z curr_dm_list->_rot_z
#define tran_x curr_dm_list->_tran_x
#define tran_y curr_dm_list->_tran_y
#define tran_z curr_dm_list->_tran_z
#define orig_pos curr_dm_list->_orig_pos
#endif

#else
/* Not MULTI_ATTACH */

extern struct dm *dmp;			/* ptr to current display mgr */

#endif

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


#define BV_MAXFUNC	64	/* largest code used */

/*  Colors */

#define DM_BLACK	0
#define DM_RED		1
#define DM_BLUE		2
#define DM_YELLOW	3
#define DM_WHITE	4

/* Command parameter to dmr_viewchange() */
#define DM_CHGV_REDO	0	/* Display has changed substantially */
#define DM_CHGV_ADD	1	/* Add an object to the display */
#define DM_CHGV_DEL	2	/* Delete an object from the display */
#define DM_CHGV_REPL	3	/* Replace an object */
#define DM_CHGV_ILLUM	4	/* Make new object the illuminated object */

#ifdef XMGED
#define NMG_1           78+128
#define NMG_2           77+128
#define NMG_3           76+128
#define NMG_4           75+128
#define NMG_5           74+128
#define NMG_6           73+128
#define NMG_7           72+128
#define NMG_8           71+128
#define EHY_1		70+128
#define EHY_2		69+128
#define EHY_3		68+128
#define EHY_4		67+128
#define EPA_1		66+128
#define EPA_2		65+128
#define EPA_3		64+128
#define RHC_1		63+128
#define RHC_2		62+128
#define RHC_3		61+128
#define RHC_4		60+128
#define RPC_1		59+128
#define RPC_2		58+128
#define RPC_3		57+128
#define ROT4_1		56+128
#define ROT4_2		55+128
#define ROT4_3		54+128
#define ROT4_4		53+128
#define ROT5_1		52+128
#define ROT5_2		51+128
#define ROT5_3		50+128
#define ROT5_4		49+128
#define ROT5_5		48+128
#define ROT6_1		47+128
#define ROT6_2		46+128
#define ROT6_3		45+128
#define ROT6_4		44+128
#define ROT6_5		43+128
#define ROT7_1		42+128
#define ROT7_2		41+128
#define ROT7_3		40+128
#define ROT7_4		39+128
#define ROT7_5		38+128
#define ROT7_6		37+128
#define ROT8_1		36+128
#define ROT8_2		35+128
#define ROT8_3		34+128
#define ROT8_4		33+128
#define ROT8_5		32+128
#define ROT8_6		31+128
#define MV4_1		30+128
#define MV4_2		29+128
#define MV4_3		28+128
#define MV4_4		27+128
#define MV5_1		26+128
#define MV5_2		25+128
#define MV5_3		24+128
#define MV5_4		23+128
#define MV5_5		22+128
#define MV6_1		21+128
#define MV6_2		20+128
#define MV6_3		19+128
#define MV6_4		18+128
#define MV6_5		17+128
#define MV7_1		16+128
#define MV7_2		15+128
#define MV8_1		14+128
#define MV8_2		13+128
#define MV8_3		12+128
#define MV8_4		11+128
#define MV8_5		10+128
#define MV8_6		9+128
#define SPLINE_1	8+128
#define SPLINE_2	7+128
#define ARS_1		6+128
#define ELL_1		5+128
#define ELL_2		4+128
#define ELL_3		3+128
#define ELL_4		2+128
#define ETO_1		1+128
#define ETO_2		0+128
#define ETO_3		63+64
#define ETO_4		62+64
#define TOR_1		61+64
#define TOR_2		60+64
#define TGC_1		59+64
#define TGC_2		58+64
#define TGC_3		57+64
#define TGC_4		56+64
#define TGC_5		55+64
#define TGC_6		54+64
#define TGC_7		53+64
#define TGC_8		52+64
#define TGC_9		51+64
#define TGC_10		50+64
#define TGC_11		49+64
#define TGC_12		48+64
#define POINT4_1	47+64
#define POINT4_2	46+64
#define POINT4_3	45+64
#define POINT4_4	44+64
#define EDGE5_1		43+64
#define EDGE5_2		42+64
#define EDGE5_3		41+64
#define EDGE5_4		40+64
#define EDGE5_5		39+64
#define EDGE5_6		38+64
#define EDGE5_7		37+64
#define EDGE5_8		36+64
#define EDGE5_9		35+64
#define EDGE6_1		34+64
#define EDGE6_2		33+64
#define EDGE6_3		32+64
#define EDGE6_4		31+64
#define EDGE6_5		30+64
#define EDGE6_6		29+64
#define EDGE6_7		28+64
#define EDGE6_8		27+64
#define EDGE6_9		26+64
#define EDGE6_10	25+64
#define EDGE7_1		24+64
#define EDGE7_2		23+64
#define EDGE7_3		22+64
#define EDGE7_4		21+64
#define EDGE7_5		20+64
#define EDGE7_6		19+64
#define EDGE7_7		18+64
#define EDGE7_8		17+64
#define EDGE7_9		16+64
#define EDGE7_10	15+64
#define EDGE7_11	14+64
#define EDGE7_12	13+64
#define EDGE8_1		12+64
#define EDGE8_2		11+64
#define EDGE8_3		10+64
#define EDGE8_4		9+64
#define EDGE8_5		8+64
#define EDGE8_6		7+64
#define EDGE8_7		6+64
#define EDGE8_8		5+64
#define EDGE8_9		4+64
#define EDGE8_10	3+64
#define EDGE8_11	2+64
#define EDGE8_12	1+64
#define P_HELP		0+64
#endif
