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
 *  $Header$
 */

struct device_values  {
	int	dv_buttonpress;		/* Number of button pressed when !0 */
	float	dv_xjoy;		/* Joystick,  -1.0 <= x <= +1.0 */
	float	dv_yjoy;
	float	dv_zjoy;
	int	dv_xpen;		/* Tablet.  -2048 <= x,y <= +2047 */
	int	dv_ypen;
	int	dv_penpress;		/* !0 when tablet is pressed */
	float	dv_zoom;		/* Zoom knob.  -1.0 <= zoom <= +1.0 */
	float	dv_xslew;		/* View slew.  -1.0 <= slew <= +1.0 */
	float	dv_yslew;
	int	dv_xadc;		/* A/D cursor -2048 <= adc <= +2047 */
	int	dv_yadc;
	int	dv_1adc;		/* angle 1 for A/D cursor */
	int	dv_2adc;		/* angle 2 for A/D cursor */
	int	dv_distadc;		/* Tick distance */
	int	dv_flagadc;		/* A/D cursor "changed" flag */
};
extern struct device_values dm_values;

struct mem_map {
	struct mem_map	*m_nxtp;	/* Linking pointer to next element */
	unsigned	 m_size;	/* Size of this free element */
	unsigned long	 m_addr;	/* Address of start of this element */
};
#define MAP_NULL	((struct mem_map *) 0)

/* Interface to a specific Display Manager */
struct dm {
	void	(*dmr_open)();
	void	(*dmr_close)();
	void	(*dmr_restart)();
	int	(*dmr_input)();
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
	float	dmr_bound;		/* zoom-in limit */
	char	*dmr_name;		/* short name of device */
	char	*dmr_lname;		/* long name of device */
	struct mem_map *dmr_map;	/* displaylist mem map */
};
extern struct dm *dmp;			/* ptr to current display mgr */

/* Format of a vector list */
struct veclist {
	float	vl_pnt[3];	/* X, Y, Z of point in Model space */
	char	vl_pen;		/* PEN_DOWN==draw, PEN_UP==move */
};
extern struct veclist *vlp;	/* pointer to first free veclist element */
extern struct veclist *vlend;	/* pointer to first invalid veclist element */
#define VLIST_NULL	((struct veclist *)0)

/*
 * Record an absolute vector and "pen" position in veclist array.
 */
#define DM_GOTO(p,pen)	if(vlp>=vlend) \
	printf("%s/%d:  veclist overrun\n", __FILE__, __LINE__); \
	else { VMOVE( vlp->vl_pnt, p ); (vlp++)->vl_pen = pen; }

/* Virtual Pen settings */
#define PEN_UP		0
#define PEN_DOWN	1

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
#define BV_90_90	1
#define BV_35_25	0+32

#define BV_SHIFT	1+32
#define BE_SHIFT	2+32

#define BV_MAXFUNC	64	/* largest code used */

/*  Colors */

#define DM_BLACK	0
#define DM_RED		1
#define DM_GREEN	2
#define DM_BLUE		3
#define DM_YELLOW	4
#define DM_MAGENTA	5
#define DM_CYAN		6
#define DM_GRAY		14
#define DM_WHITE	15
