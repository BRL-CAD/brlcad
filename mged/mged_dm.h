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

#define DV_PICK		1	/* dv_penpress for pick function */
#define DV_INZOOM	2	/* dv_penpress for zoom in */
#define DV_OUTZOOM	4	/* dv_penpress for zoom out */
#define DV_SLEW		8	/* dv_penpress for view slew */
struct device_values  {
	int	dv_buttonpress;		/* Number of button pressed when !0 */
	double	dv_xjoy;		/* Joystick,  -1.0 <= x <= +1.0 */
	double	dv_yjoy;
	double	dv_zjoy;
	int	dv_xpen;		/* Tablet.  -2048 <= x,y <= +2047 */
	int	dv_ypen;
	int	dv_penpress;		/* !0 when tablet is pressed */
	double	dv_zoom;		/* Zoom knob.  -1.0 <= zoom <= +1.0 */
	double	dv_xslew;		/* View slew.  -1.0 <= slew <= +1.0 */
	double	dv_yslew;
	double	dv_zslew;
	int	dv_xadc;		/* A/D cursor -2048 <= adc <= +2047 */
	int	dv_yadc;
	int	dv_1adc;		/* angle 1 for A/D cursor */
	int	dv_2adc;		/* angle 2 for A/D cursor */
	int	dv_distadc;		/* Tick distance */
	int	dv_flagadc;		/* A/D cursor "changed" flag */
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
extern struct dm *dmp;			/* ptr to current display mgr */

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
