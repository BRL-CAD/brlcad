/*
 *			A D C . C
 *
 * Functions -
 *	adcursor	implement the angle/distance cursor
 *	f_adc		control angle/distance cursor from keyboard
 *
 * Authors -
 *	Gary Steven Moss
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

/*
 *			A D C U R S O R
 *
 * Compute and display the angle/distance cursor.
 */
void
adcursor()
{
	static double	pi = 3.14159265358979323264;
	static fastf_t	x1, Y1;	/* not "y1", due to conflict with math lib */
	static fastf_t	x2, y2;
	static fastf_t	x3, y3;
	static fastf_t	x4, y4;
	static fastf_t	d1, d2;
	static fastf_t	t1, t2;
	static long	idxy[2];

	/*
	 * Calculate a-d cursor displacement.
	 */
#define MINVAL	-2048
#define MAXVAL	 2047
	idxy[0] = dv_xadc;
	idxy[1] = dv_yadc;
	idxy[0] = (idxy[0] < MINVAL ? MINVAL : idxy[0]);
	idxy[0] = (idxy[0] > MAXVAL ? MAXVAL : idxy[0]);
	idxy[1] = (idxy[1] < MINVAL ? MINVAL : idxy[1]);
	idxy[1] = (idxy[1] > MAXVAL ? MAXVAL : idxy[1]);

	dmp->dm_setColor(dmp, DM_YELLOW, 1);
	dmp->dm_setLineAttr(dmp, 1, 0);    /* linewidth - 1, not dashed */
	dmp->dm_drawLine2D( dmp, MINVAL, idxy[1], MAXVAL, idxy[1] ); /* Horizontal */
	dmp->dm_drawLine2D( dmp, idxy[0], MAXVAL, idxy[0], MINVAL );  /* Vertical */

	curs_x = (fastf_t) (idxy[0]);
	curs_y = (fastf_t) (idxy[1]);

	/*
	 * Calculate a-d cursor rotation.
	 */
	/* - to make rotation match knob direction */
	idxy[0] = -dv_1adc;	/* solid line */
	idxy[1] = -dv_2adc;	/* dashed line */
	angle1 = ((2047.0 + (fastf_t) (idxy[0])) * pi) / (4.0 * 2047.0);
	angle2 = ((2047.0 + (fastf_t) (idxy[1])) * pi) / (4.0 * 2047.0);

	/* sin for X and cos for Y to reverse sense of knob */
	d1 = cos (angle1) * 8000.0;
	d2 = sin (angle1) * 8000.0;
	x1 = curs_x + d1;
	Y1 = curs_y + d2;
	x2 = curs_x - d1;
	y2 = curs_y - d2;
	(void)clip ( &x1, &Y1, &x2, &y2 );

	x3 = curs_x + d2;
	y3 = curs_y - d1;
	x4 = curs_x - d2;
	y4 = curs_y + d1;
	(void)clip ( &x3, &y3, &x4, &y4 );

	dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	dmp->dm_drawLine2D( dmp, (int)x3, (int)y3, (int)x4, (int)y4 );

	d1 = cos (angle2) * 8000.0;
	d2 = sin (angle2) * 8000.0;
	x1 = curs_x + d1;
	Y1 = curs_y + d2;
	x2 = curs_x - d1;
	y2 = curs_y - d2;
	(void)clip ( &x1, &Y1, &x2, &y2 );

	x3 = curs_x + d2;
	y3 = curs_y - d1;
	x4 = curs_x - d2;
	y4 = curs_y + d1;
	(void)clip ( &x3, &y3, &x4, &y4 );

	dmp->dm_setLineAttr(dmp, 1, 1);  /* linewidth - 1, dashed */
	dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	dmp->dm_drawLine2D( dmp, (int)x3, (int)y3, (int)x4, (int)y4 );
	dmp->dm_setLineAttr(dmp, 1, 0);  /* linewidth - 1, not dashed */

	/*
	 * Position tic marks from dial 9.
	 */
	/* map -2048 - 2047 into 0 - 2048 * sqrt (2) */
	/* Tick distance */
	c_tdist = ((fastf_t)(dv_distadc) + 2047.0) * M_SQRT2_DIV2;

	d1 = c_tdist * cos (angle1);
	d2 = c_tdist * sin (angle1);
	t1 = 20.0 * sin (angle1);
	t2 = 20.0 * cos (angle1);

	/* Quadrant 1 */
	x1 = curs_x + d1 + t1;
	Y1 = curs_y + d2 - t2;
	x2 = curs_x + d1 -t1;
	y2 = curs_y + d2 + t2;
	if (clip ( &x1, &Y1, &x2, &y2 ) == 0) {
	  dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	}

	/* Quadrant 2 */
	x1 = curs_x - d2 + t2;
	Y1 = curs_y + d1 + t1;
	x2 = curs_x - d2 - t2;
	y2 = curs_y + d1 - t1;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
	  dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	}

	/* Quadrant 3 */
	x1 = curs_x - d1 - t1;
	Y1 = curs_y - d2 + t2;
	x2 = curs_x - d1 + t1;
	y2 = curs_y - d2 - t2;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
	  dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	}

	/* Quadrant 4 */
	x1 = curs_x + d2 - t2;
	Y1 = curs_y - d1 - t1;
	x2 = curs_x + d2 + t2;
	y2 = curs_y - d1 + t1;
	if (clip (&x1, &Y1, &x2, &y2) == 0) {
	  dmp->dm_drawLine2D( dmp, (int)x1, (int)Y1, (int)x2, (int)y2 );
	}
}

/*
 *			F _ A D C
 */

static char	adc_syntax[] = "\
 adc		toggle display of angle/distance cursor\n\
 adc a1 #	set angle1\n\
 adc a2 #	set angle2\n\
 adc dst #	set radius (distance) of tick\n\
 adc dh #	reposition horizontally\n\
 adc dv #	reposition vertically\n\
 adc dx #	reposition in X direction\n\
 adc dy #	reposition in Y direction\n\
 adc dz #	reposition in Z direction\n\
 adc hv # #	reposition (view coordinates)\n\
 adc xyz # # #	reposition in front of a point (model coordinates)\n\
 adc reset	reset angles, location, and tick distance\n\
";
int
f_adc (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	char	*parameter;
	point_t	center_model;
	point_t	center_view;
	point_t	pt;		/* Value(s) provided by user */
	point_t	pt2, pt3;	/* Extra points as needed */
	fastf_t	view2dm = 2047.0 / Viewscale;
	int	i;
	int     iadc = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if(strstr(argv[0], "iadc"))
	   iadc = 1;

	if (argc == 1)
	{
	  if (mged_variables.adcflag)  {
#if 0
	    dmp->dm_light( dmp, LIGHT_OFF, BV_ADCURSOR );
#endif
	    mged_variables.adcflag = 0;
	  } else {
#if 0
	    dmp->dm_light( dmp, LIGHT_ON, BV_ADCURSOR );
#endif
	    mged_variables.adcflag = 1;
	  }

	  if(mged_variables.scroll_enabled){
	    /* 
	     * We need to send this through the interpreter in case
	     * the built-in "sliders" command has been redefined.
	     */
	    Tcl_Eval(interp, "sliders on");
	  }

	  dmaflag = 1;
	  return TCL_OK;
	}

	parameter = argv[1];
	argc -= 2;
	for (i = 0; i < argc; ++i)
		pt[i] = atof(argv[i + 2]);
	VSET(center_model,
	    -toViewcenter[MDX], -toViewcenter[MDY], -toViewcenter[MDZ]);
	MAT4X3VEC(center_view, Viewrot, center_model);

	if( strcmp( parameter, "a1" ) == 0 )  {
	  if (argc == 1) {
	    if(iadc)
	      dv_1adc += (1.0 - pt[0] / 45.0) * 2047.0;
	    else
	      dv_1adc = (1.0 - pt[0] / 45.0) * 2047.0;

	    dmaflag = 1;
	    adc_a1_deg = ((2047.0 + (fastf_t)-dv_1adc) * bn_pi) / (4.0 * 2047.0) * radtodeg;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&ang1_vls));

	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc a1' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "a2" ) == 0 )  {
	  if (argc == 1) {
	    if(iadc)
	      dv_2adc += (1.0 - pt[0] / 45.0) * 2047.0;
	    else
	      dv_2adc = (1.0 - pt[0] / 45.0) * 2047.0;

	    dmaflag = 1;
	    adc_a2_deg = ((2047.0 + (fastf_t)-dv_2adc) * bn_pi) / (4.0 * 2047.0) * radtodeg;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&ang2_vls));

	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc a2' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if(strcmp(parameter, "dst") == 0)  {
	  if (argc == 1) {
	    if(iadc)
	      dv_distadc += (pt[0] /
			  (Viewscale * base2local * M_SQRT2_DIV2) - 1.0) * 2047.0;
	    else
	      dv_distadc = (pt[0] /
			    (Viewscale * base2local * M_SQRT2_DIV2) - 1.0) * 2047.0;

	    dmaflag = 1;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&distadc_vls));

	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dst' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "dh" ) == 0 )  {
	  if (argc == 1) {
	    dv_xadc += pt[0] * 2047.0 / (Viewscale * base2local);
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dh' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "dv" ) == 0 )  {
	  if (argc == 1) {
	    dv_yadc += pt[0] * 2047.0 / (Viewscale * base2local);
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dv' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "dx" ) == 0 )  {
	  if (argc == 1) {
	    VSET(pt2, pt[0]*local2base, 0.0, 0.0);
	    MAT4X3VEC(pt3, Viewrot, pt2);
	    dv_xadc += pt3[X] * view2dm;
	    dv_yadc += pt3[Y] * view2dm;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dx' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "dy" ) == 0 )  {
	  if (argc == 1) {
	    VSET(pt2, 0.0, pt[0]*local2base, 0.0);
	    MAT4X3VEC(pt3, Viewrot, pt2);
	    dv_xadc += pt3[X] * view2dm;
	    dv_yadc += pt3[Y] * view2dm;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dy' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp( parameter, "dz" ) == 0 )  {
	  if (argc == 1) {
	    VSET(pt2, 0.0, 0.0, pt[0]*local2base);
	    MAT4X3VEC(pt3, Viewrot, pt2);
	    dv_xadc += pt3[X] * view2dm;
	    dv_yadc += pt3[Y] * view2dm;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc dz' command accepts only 1 argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "hv") == 0)  {
	  if (argc == 2) {
	    VSCALE(pt, pt, local2base);
	    VSUB2(pt3, pt, center_view);
	    dv_xadc = pt3[X] * view2dm;
	    dv_yadc = pt3[Y] * view2dm;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc hv' command requires 2 arguments\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "xyz") == 0)  {
	  if (argc == 3) {
	    VSCALE(pt, pt, local2base);
	    VSUB2(pt2, pt, center_model);
	    MAT4X3VEC(pt3, Viewrot, pt2);
	    dv_xadc = pt3[X] * view2dm;
	    dv_yadc = pt3[Y] * view2dm;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc xyz' command requires 2 arguments\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "x") == 0 ) {
	  if( argc == 1 ) {
	    if(iadc)
	      dv_xadc += pt[0];
	    else
	      dv_xadc = pt[0];

	    dmaflag = 1;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&xadc_vls));

	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc x' command requires one argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "y") == 0 ) {
	  if( argc == 1 ) {
	    if(iadc)
	      dv_yadc += pt[0];
	    else
	      dv_yadc = pt[0];

	    dmaflag = 1;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&yadc_vls));
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc y' command requires one argument\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "reset") == 0)  {
	  if (argc == 0) {
	    dv_xadc = dv_yadc = 0;
	    dv_1adc = dv_2adc = 0;
	    dv_distadc = 0;
	    dmaflag = 1;
	    return TCL_OK;
	  }
	  Tcl_AppendResult(interp, "The 'adc reset' command accepts no arguments\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( strcmp(parameter, "help") == 0)  {
	  Tcl_AppendResult(interp, "Usage:\n", adc_syntax, (char *)NULL);
	  return TCL_OK;
	} else {
	  Tcl_AppendResult(interp, "ADC: unrecognized command: '",
			   argv[1], "'\nUsage:\n", adc_syntax, (char *)NULL);
	}
	return TCL_ERROR;
}
