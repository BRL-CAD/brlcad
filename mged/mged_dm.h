/*
 *			D M . H
 *
 * Header file for communication with the display manager.
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

/* Declarations of functions in the Display Manager module */
extern void	dm_light();


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
