/*
 *			C H G V I E W . C
 *
 * Functions -
 *	f_center	(DEBUG) force view center
 *	f_vrot		(DEBUG) force view rotation
 *	f_view		(DEBUG) force view size
 *	f_blast		zap the display, then edit anew
 *	f_edit		edit something (add to visible display)
 *	f_evedit	Evaluated edit something (add to visible display)
 *	f_delobj	delete an object or several from the display
 *	f_debug		(DEBUG) print solid info?
 *	f_regdebug	toggle debugging state
 *	cmd_list	list object information
 *	f_zap		zap the display -- everything dropped
 *	f_status	print view info
 *	f_fix		fix display processor after hardware error
 *	eraseobj	Drop an object from the visible list
 *	pr_schain	Print info about visible list
 *	f_ill		illuminate the named object
 *	f_sed		simulate pressing "solid edit" then illuminate
 *	f_knob		simulate knob twist
 *	f_slewview	Slew the view
 *	slewview	guts for f_setview
 *	f_setview	Set the current view
 *	setview		guts for f_setview
 *	usejoy		Apply joystick to viewing perspective
 *      absview_v       Absolute view rotation about view center
 *      f_vrot_center   Set the center of rotation -- not ready yet
 *      cmd_getknob     returns knob/slider value
 *      f_svbase        Set view base references (i.e. i_Viewscale and orig_pos)
 *      mged_svbase     Guts for f_svbase
 *      f_tran          Translate view center
 *      mged_tran       Guts for f_tran
 *      f_qvrot         Set view from direction vector and twist angle
 *      f_orientation   Set current view direction from a quaternion
 *      f_zoom          zoom view
 *      mged_zoom       guts for f_zoom
 *      abs_zoom        absolute zoom
 *      f_tol           set or display tolerance
 *      knob_tran       handle translations for f_knob
 *      f_aetview       set view using azimuth, elevation and twist angles
 *
 *  Author -
 *	Michael John Muuss
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

#include <signal.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "mater.h"
#include "raytrace.h"
#include "nmg.h"
#include "externs.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "../librt/debug.h"	/* XXX */

extern int mged_param();
extern void color_soltab();
extern void set_scroll();   /* defined in set.c */

void knob_update_rate_vars();
int mged_svbase();
int knob_tran();
int mged_tran();
int mged_vrot();
int mged_zoom();
static void abs_zoom();
void usejoy();

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

extern mat_t    ModelDelta;     /* from ged.c */
extern long	nvectors;	/* from dodraw.c */

extern struct bn_tol mged_tol;	/* from ged.c */
extern vect_t e_axes_pos;

fastf_t ar_scale_factor = 2047.0 / ABS_ROT_FACTOR;
fastf_t rr_scale_factor = 2047.0 / RATE_ROT_FACTOR;
fastf_t adc_angle_scale_factor = 2047.0 / ADC_ANGLE_FACTOR;

#if 1
int edit_abs_tran[3];
int abs_tran[3];
#endif

vect_t edit_absolute_rotate;
vect_t last_edit_absolute_rotate;
vect_t edit_rate_rotate;
int edit_rateflag_rotate;

vect_t edit_absolute_tran;
vect_t edit_rate_tran;
int edit_rateflag_tran;

fastf_t edit_absolute_scale;
fastf_t edit_rate_scale;
int edit_rateflag_scale;

struct bu_vls edit_info_vls;
struct bu_vls edit_rate_tran_vls[3];
struct bu_vls edit_rate_rotate_vls[3];
struct bu_vls edit_rate_scale_vls;
struct bu_vls edit_absolute_tran_vls[3];
struct bu_vls edit_absolute_rotate_vls[3];
struct bu_vls edit_absolute_scale_vls;

double		mged_abs_tol;
double		mged_rel_tol = 0.01;		/* 1%, by default */
double		mged_nrm_tol;			/* normal ang tol, radians */

BU_EXTERN(int	edit_com, (int argc, char **argv, int kind, int catch_sigint));

/* Delete an object or several objects from the display */
/* Format: d object1 object2 .... objectn */
int
f_erase(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  register struct directory *dp;
  register int i;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for( i = 1; i < argc; i++ )  {
    if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) != DIR_NULL )
      eraseobj(dp);
  }

  no_memory = 0;
  return TCL_OK;
}

f_erase_all(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  register struct directory *dp;
  register int i;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for( i = 1; i < argc; i++ )  {
    if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) != DIR_NULL )
      eraseobjall(dp);
  }

  no_memory = 0;
  return TCL_OK;
}

/* DEBUG -- force view center */
/* Format: C x y z	*/
int
f_center(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 4 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help center");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* must convert from the local unit to the base unit */
  toViewcenter[MDX] = -atof( argv[1] ) * local2base;
  toViewcenter[MDY] = -atof( argv[2] ) * local2base;
  toViewcenter[MDZ] = -atof( argv[3] ) * local2base;
  new_mats();

  (void)mged_svbase();

  return TCL_OK;
}

/*
 *			M G E D _ V R O T
 */
int
mged_vrot(x, y, z)
double x, y, z;
{
  mat_t newrot;
  vect_t new_pos;
  mat_t   newinv;

  bn_mat_idn( newrot );
  buildHrot( newrot, x * degtorad, y * degtorad, z * degtorad);
  bn_mat_inv( newinv, newrot );

  if( mged_variables.eyerot == 0 )  {
    /* Traditional method:  rotate around view center (0,0,0) viewspace */
    wrt_view( ModelDelta, newinv, ModelDelta );

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2( newrot, Viewrot );			/* pure rotation */
    new_mats();

    if(absolute_slew[X] != 0.0 ||
       absolute_slew[Y] != 0.0 ||
       absolute_slew[Z] != 0.0){
      VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
      MAT4X3PNT(absolute_slew, model2view, new_pos);
    }
  } else {
  	/* "VR driver" method: rotate around "eye" point (0,0,1) viewspace */
  	point_t		eye_view;
  	point_t		new_origin;
  	mat_t		viewchg, viewchginv;
  	point_t		new_cent_view;
  	point_t		new_cent_model;

  	VSET( eye_view, 0, 0, 1 );		/* point to rotate around */
  	bn_mat_xform_about_pt( viewchg, newrot, eye_view );
  	bn_mat_inv( viewchginv, viewchg );

  	/* Convert origin in new (viewchg) coords back to old view coords */
  	VSET( new_origin, 0, 0, 0 );
  	MAT4X3PNT( new_cent_view, viewchginv, new_origin );
  	MAT4X3PNT( new_cent_model, view2model, new_cent_view );
  	MAT_DELTAS_VEC_NEG( toViewcenter, new_cent_model );

  	/* XXX This should probably capture the translation too */
  	/* XXX I think the only consumer of ModelDelta is the predictor frame */
	wrt_view( ModelDelta, newinv, ModelDelta );		/* pure rotation */

	/* Update the rotation component of the model2view matrix */
	bn_mat_mul2( newrot, Viewrot );			/* pure rotation */
	new_mats();

	VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
	MAT4X3PNT(absolute_slew, model2view, new_pos);
  }

  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));

  return TCL_OK;
}

int
f_vrot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int status;
  struct bu_vls vls;

  if(argc < 4 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help vrot");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  status = mged_vrot(atof(argv[1]), atof(argv[2]), atof(argv[3]));

  return status;
}

/* DEBUG -- force viewsize */
/* Format: view size	*/
int
f_view(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  fastf_t f;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help size");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  f = atof( argv[1] );
  if( f < 0.0001 ) f = 0.0001;
  Viewscale = f * 0.5 * local2base;
  new_mats();

  return TCL_OK;
}

/* ZAP the display -- then edit anew */
/* Format: B object	*/
int
f_blast(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  char *av[2];

  av[0] = "Z";
  av[1] = (char *)NULL;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help B");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (f_zap(clientData, interp, 1, av) == TCL_ERROR)
    return TCL_ERROR;

  if( dmp->dm_displaylist )  {
    /*
     * Force out the control list with NO solids being drawn,
     * then the display processor will not mind when we start
     * writing new subroutines out there...
     */
    update_views = 1;
    refresh();
  }

  return edit_com( argc, argv, 1, 1 );
}

/* Edit something (add to visible display) */
/* Format: e object	*/
int
f_edit(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  update_views = 1;

  return edit_com( argc, argv, 1, 1 );
}

/* Format: ev objects	*/
int
f_ev(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help ev");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  update_views = 1;

  return edit_com( argc, argv, 3, 1 );
}

#if 0
/* XXX until NMG support is complete,
 * XXX the old Big-E command is retained in all its glory in
 * XXX the file proc_reg.c, including the command processing.
 */
/*
 *			F _ E V E D I T
 *
 *  The "Big E" command.
 *  Evaluated Edit something (add to visible display)
 *  Usage: E object(s)
 */
int
f_evedit(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help E");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  update_views = 1;

  return edit_com( argc, argv, 2, 1 );
}
#endif

/*
 *			S I Z E _ R E S E T
 *
 *  Reset view size and view center so that all solids in the solid table
 *  are in view.
 *  Caller is responsible for calling new_mats().
 */
void
size_reset()
{
	register struct solid	*sp;
	vect_t		min, max;
	vect_t		minus, plus;
	vect_t		center;
	vect_t		radial;

	VSETALL( min,  INFINITY );
	VSETALL( max, -INFINITY );

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN( min, minus );
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX( max, plus );
	}

	if(BU_LIST_IS_EMPTY(&HeadSolid.l)) {
		/* Nothing is in view */
		VSETALL( center, 0 );
		VSETALL( radial, 1000 );	/* 1 meter */
	} else {
		VADD2SCALE( center, max, min, 0.5 );
		VSUB2( radial, max, center );
	}

	if( VNEAR_ZERO( radial , SQRT_SMALL_FASTF ) )
		VSETALL( radial , 1.0 );

	bn_mat_idn( toViewcenter );
	MAT_DELTAS( toViewcenter, -center[X], -center[Y], -center[Z] );
	Viewscale = radial[X];
	V_MAX( Viewscale, radial[Y] );
	V_MAX( Viewscale, radial[Z] );

	i_Viewscale = Viewscale;
}

/*
 *			E D I T _ C O M
 *
 * B, e, and E commands use this area as common
 */
int
edit_com(argc, argv, kind, catch_sigint)
int	argc;
char	**argv;
int	kind;
int	catch_sigint;
{
  register struct directory *dp;
  register int	i;
  double		elapsed_time;
  int		initial_blank_screen;
  struct bu_vls vls;

  bu_vls_init(&vls);
  initial_blank_screen = BU_LIST_IS_EMPTY(&HeadSolid.l);

  /*  First, delete any mention of these objects.
   *  Silently skip any leading options (which start with minus signs).
   */
  for( i = 1; i < argc; i++ )  {
    if( (dp = db_lookup( dbip,  argv[i], LOOKUP_QUIET )) != DIR_NULL )  {
      eraseobj( dp );
      no_memory = 0;
    }
  }

  if( dmp->dm_displaylist )  {
    /* Force displaylist update before starting new drawing */
    update_views = 1;
    refresh();
  }

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else {
    bu_vls_free(&vls);
    return TCL_OK;
  }

  nvectors = 0;
  rt_prep_timer();
  drawtrees( argc, argv, kind );
  (void)rt_get_timer( (struct bu_vls *)0, &elapsed_time );

  bu_vls_printf(&vls, "%ld vectors in %g sec\n", nvectors, elapsed_time);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  {
    register struct dm_list *p;
    struct dm_list *save_dm_list;
    struct cmd_list *save_cmd_list;

    save_dm_list = curr_dm_list;
    save_cmd_list = curr_cmd_list;
    for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
      curr_dm_list = p;
      if(curr_dm_list->aim)
	curr_cmd_list = curr_dm_list->aim;
      else
	curr_cmd_list = &head_cmd_list;

      /* If we went from blank screen to non-blank, resize */
      if (mged_variables.autosize  && initial_blank_screen &&
	  BU_LIST_NON_EMPTY(&HeadSolid.l)) {
	size_reset();
	new_mats();
	(void)mged_svbase();

	for(i = 0; i < VIEW_TABLE_SIZE; ++i)
	  viewscale_table[i] = Viewscale;
      }

      color_soltab();
#if 0
      dmp->dm_colorchange(dmp);
#endif
    }

    curr_dm_list = save_dm_list;
    curr_cmd_list = save_cmd_list;
  }

  bu_vls_free(&vls);
  (void)signal( SIGINT, SIG_IGN );
  return TCL_OK;
}

/*
 *			F _ D E B U G
 *
 *  Print information about solid table, and per-solid VLS
 */
int
f_debug(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int	lvl = 0;
  struct bu_vls vls;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help x");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( argc > 1 )  lvl = atoi(argv[1]);

  bu_vls_init(&vls);
  if( lvl != -1 )
      bu_vls_printf(&vls, "ndrawn=%d\n", ndrawn);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  pr_schain( &HeadSolid, lvl );

  return TCL_OK;
}

/*
 *			F _ R E G D E B U G
 *
 *  Display-manager specific "hardware register" debugging.
 */
int
f_regdebug(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	static int regdebug = 0;
	static char debug_str[10];

	if(argc < 1 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help regdebug");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( argc == 1 )
		regdebug = !regdebug;	/* toggle */
	else
		regdebug = atoi( argv[1] );

	sprintf( debug_str, "%d", regdebug );

	Tcl_AppendResult(interp, "regdebug=", debug_str, "\n", (char *)NULL);

	dmp->dm_debug(dmp, regdebug);

	return TCL_OK;
}

/*
 *			F _ D E B U G B U
 *
 *  Provide user-level access to LIBBU debug bit vector.
 */
int
f_debugbu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help debugbu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  start_catching_output(&vls);

  if( argc >= 2 )  {
    sscanf( argv[1], "%x", &bu_debug );
  } else {
    bu_printb( "Possible flags", 0xffffffffL, BU_DEBUG_FORMAT );
    bu_log("\n");
  }
  bu_printb( "bu_debug", bu_debug, BU_DEBUG_FORMAT );
  bu_log("\n");

  stop_catching_output(&vls);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

/*
 *			F _ D E B U G L I B
 *
 *  Provide user-level access to LIBRT debug bit vector
 */
int
f_debuglib(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help debuglib");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  start_catching_output(&vls);

  if( argc >= 2 )  {
    sscanf( argv[1], "%x", &rt_g.debug );
    if( rt_g.debug )  bu_debug |= BU_DEBUG_COREDUMP;
  } else {
    bu_printb( "Possible flags", 0xffffffffL, DEBUG_FORMAT );
    bu_log("\n");
  }
  bu_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
  bu_log("\n");

  stop_catching_output(&vls);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

/*
 *			F _ D E B U G M E M
 *
 *  Provide user-level access to LIBBU bu_prmem() routine.
 *  Must be used in concert with BU_DEBUG_MEM_CHECK flag.
 */
int
f_debugmem(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help debugmem");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3 );	/* allow interupts */
  else
    return TCL_OK;

  bu_prmem("Invoked via MGED command");

  (void)signal(SIGINT, SIG_IGN);
  return TCL_OK;
}

/*
 *			F _ D E B U G N M G
 *
 *  Provide user-level access to LIBRT NMG_debug flags.
 */
int
f_debugnmg(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help debugnmg");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  start_catching_output(&vls);

  if( argc >= 2 )  {
    sscanf( argv[1], "%x", &rt_g.NMG_debug );
  } else {
    bu_printb( "possible flags", 0xffffffffL, NMG_DEBUG_FORMAT );
    bu_log("\n");
  }
  bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
  bu_log("\n");

  stop_catching_output(&vls);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

/*
 *			D O _ L I S T
 */
void
do_list( outstrp, dp, verbose )
struct bu_vls	*outstrp;
register struct directory *dp;
int	verbose;
{
	int			id;
	struct rt_db_internal	intern;

	bu_vls_printf( outstrp, "%s:  ", dp->d_namep );

	if( (id = rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL )) < 0 )  {
		Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
			") failure", (char *)NULL );
		return;
	}

	if( rt_functab[id].ft_describe( outstrp, &intern,
	    verbose, base2local ) < 0 )
	  Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_functab[id].ft_ifree( &intern );
}

/*
 *			C M D _ L I S T
 *
 *  List object information, verbose
 *  Format: l object
 */
int
cmd_list(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register struct directory *dp;
  register int arg;
  struct bu_vls str;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help l");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init( &str );

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3 );	/* allow interupts */
  else{
    bu_vls_free( &str );
    return TCL_OK;
  }

  for( arg = 1; arg < argc; arg++ )  {
    if( (dp = db_lookup( dbip, argv[arg], LOOKUP_NOISY )) == DIR_NULL )
      continue;

    do_list( &str, dp, 99 );	/* very verbose */
  }

  Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
  bu_vls_free( &str );

  (void)signal(SIGINT, SIG_IGN);
  return TCL_OK;
}

/* List object information, briefly */
/* Format: cat object	*/
int
f_cat(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register struct directory *dp;
  register int arg;
  struct bu_vls str;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help cat");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init( &str );

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3 );	/* allow interupts */
  else{
    bu_vls_free( &str );
    return TCL_OK;
  }

  for( arg = 1; arg < argc; arg++ )  {
    if( (dp = db_lookup( dbip, argv[arg], LOOKUP_NOISY )) == DIR_NULL )
      continue;

    bu_vls_trunc( &str, 0 );
    do_list( &str, dp, 0 );	/* non-verbose */
    Tcl_AppendResult(interp, bu_vls_addr(&str), "\n", (char *)NULL);
  }

  bu_vls_free( &str );

  (void)signal(SIGINT, SIG_IGN);
  return TCL_OK;
}

/*
 *  To return all the free "struct rt_vlist" and "struct solid" items
 *  lurking on their respective freelists, back to bu_malloc().
 *  Primarily as an aid to tracking memory leaks.
 *  WARNING:  This depends on knowledge of the macro GET_SOLID in mged/solid.h
 *  and RT_GET_VLIST in h/raytrace.h.
 */
void
mged_freemem()
{
  register struct solid		*sp;
  register struct rt_vlist	*vp;

  FOR_ALL_SOLIDS(sp,&FreeSolid.l){
    GET_SOLID(sp,&FreeSolid.l);
    bu_free((genptr_t)sp, "mged_freemem: struct solid");
  }

  while( BU_LIST_NON_EMPTY( &rt_g.rtg_vlfree ) )  {
    vp = BU_LIST_FIRST( rt_vlist, &rt_g.rtg_vlfree );
    BU_LIST_DEQUEUE( &(vp->l) );
    bu_free( (genptr_t)vp, "mged_freemem: struct rt_vlist" );
  }
}

/* ZAP the display -- everything dropped */
/* Format: Z	*/
int
f_zap(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct solid *sp;
	register struct solid *nsp;
	struct directory	*dp;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help Z");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	update_views = 1;
	no_memory = 0;

	/* FIRST, reject any editing in progress */
	if( state != ST_VIEW )
		button( BE_REJECT );

	sp = BU_LIST_NEXT(solid, &HeadSolid.l);
	while(BU_LIST_NOT_HEAD(sp, &HeadSolid.l)){
		rt_memfree( &(dmp->dm_map), sp->s_bytes, (unsigned long)sp->s_addr );
		dp = sp->s_path[0];
		RT_CK_DIR(dp);
		if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
			if( db_dirdelete( dbip, dp ) < 0 )  {
			  Tcl_AppendResult(interp, "f_zap: db_dirdelete failed\n", (char *)NULL);
			}
		}
		sp->s_addr = sp->s_bytes = 0;
		nsp = BU_LIST_PNEXT(solid, sp);
		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp,&FreeSolid.l);
		sp = nsp;
	}

	/* Keeping freelists improves performance.  When debugging, give mem back */
	if( rt_g.debug )  mged_freemem();

	(void)chg_state( state, state, "zap" );

	return TCL_OK;
}

int
f_status(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;

  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help status");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  start_catching_output(&vls);
		   
  bu_log("STATE=%s, ", state_str[state] );
  bu_log("Viewscale=%f (%f mm)\n", Viewscale*base2local, Viewscale);
  bu_log("base2local=%f\n", base2local);
  bn_mat_print("toViewcenter", toViewcenter);
  bn_mat_print("Viewrot", Viewrot);
  bn_mat_print("model2view", model2view);
  bn_mat_print("view2model", view2model);
  if( state != ST_VIEW )  {
    bn_mat_print("model2objview", model2objview);
    bn_mat_print("objview2model", objview2model);
  }

  stop_catching_output(&vls);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  return TCL_OK;
}

/* Fix the display processor after a hardware error by re-attaching */
int
f_fix(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help fix");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  return reattach();
}

int
f_refresh(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help refresh");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  dmaflag = 1;		/* causes refresh() */
  return TCL_OK;
}

int
mged_aetview(iflag, azim, elev, twist)
int iflag;
fastf_t azim, elev, twist;
{
  int status = TCL_OK;
  fastf_t o_twist;
  fastf_t o_arz;
  fastf_t o_larz;
  char *av[5];
  struct bu_vls vls;

  /* grab old twist angle before it's lost */
  o_twist = curr_dm_list->s_info->twist;
  o_arz = absolute_rotate[Z];
  o_larz = last_absolute_rotate[Z];

  /* set view using azimuth and elevation angles */
  if(iflag)
    setview( 270.0 + elev + curr_dm_list->s_info->elevation,
	     0.0,
	     270.0 - azim - curr_dm_list->s_info->azimuth);
  else
    setview( 270.0 + elev, 0.0, 270.0 - azim );

  bu_vls_init(&vls);

  if(iflag)
    bu_vls_printf(&vls, "knob -i -v az %f", -o_twist - twist);
  else
    bu_vls_printf(&vls, "knob -i -v az %f", -twist);

  status = Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  absolute_rotate[Z] = o_arz;
  last_absolute_rotate[Z] = o_larz;

  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[Z]));

  return status;
}

/* set view using azimuth, elevation and twist angles */
int
f_aetview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int iflag = 0;
  struct bu_vls vls;
  fastf_t twist = 0.0;

  if(argc < 3 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help ae");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Check for -i option */
  if(argv[1][0] == '-' && argv[1][1] == 'i'){
    iflag = 1;  /* treat arguments as incremental values */
    ++argv;
    --argc;
  }else
    twist = curr_dm_list->s_info->twist;

  if(argc == 4) /* twist angle supplied */
    twist = atof(argv[3]);

  return mged_aetview(iflag, atof(argv[1]), atof(argv[2]), twist);
}


/*
 *			E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all displays
 * which contain the specified object in their 'path'
 */
void
eraseobjall( dp )
register struct directory *dp;
{
  register struct solid *sp;
  static struct solid *nsp;
  register int i;

  update_views = 1;

  RT_CK_DIR(dp);
  sp = BU_LIST_NEXT(solid, &HeadSolid.l);
  while(BU_LIST_NOT_HEAD(sp, &HeadSolid.l)){
    nsp = BU_LIST_PNEXT(solid, sp);
    for( i=0; i<=sp->s_last; i++ )  {
      if( sp->s_path[i] != dp )  continue;

      if( state != ST_VIEW && illump == sp )
	button( BE_REJECT );
      rt_memfree( &(dmp->dm_map), sp->s_bytes, (unsigned long)sp->s_addr );
      BU_LIST_DEQUEUE(&sp->l);
      FREE_SOLID(sp, &FreeSolid.l);

      break;
    }
    sp = nsp;
  }

  if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
    if( db_dirdelete( dbip, dp ) < 0 )  {
      Tcl_AppendResult(interp, "eraseobjall: db_dirdelete failed\n", (char *)NULL);
    }
  }
}


/*
 *			E R A S E O B J
 *
 * This routine removes only the specified object from the solid list
 */
void
eraseobj( dp )
register struct directory *dp;
{
  register struct solid *sp;
  register struct solid *nsp;

  update_views = 1;
  RT_CK_DIR(dp);

  sp = BU_LIST_FIRST(solid, &HeadSolid.l);
  while(BU_LIST_NOT_HEAD(sp, &HeadSolid.l)){
    nsp = BU_LIST_PNEXT(solid, sp);
    if(*sp->s_path != dp){
      sp = nsp;
      continue;
    }

    if(state != ST_VIEW && illump == sp)
      button( BE_REJECT );

    rt_memfree(&(dmp->dm_map), sp->s_bytes, (unsigned long)sp->s_addr);
    BU_LIST_DEQUEUE(&sp->l);
    FREE_SOLID(sp, &FreeSolid.l);
    sp = nsp;
  }

  if( dp->d_addr == RT_DIR_PHONY_ADDR ){
    if( db_dirdelete( dbip, dp ) < 0 ){
      Tcl_AppendResult(interp, "eraseobj: db_dirdelete failed\n", (char *)NULL);
    }
  }
}


/*
 *			P R _ S C H A I N
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
void
pr_schain( startp, lvl )
struct solid *startp;
int		lvl;			/* debug level */
{
  register struct solid	*sp;
  register int		i;
  register struct rt_vlist	*vp;
  int			nvlist;
  int			npts;
  struct bu_vls vls;

  bu_vls_init(&vls);

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);
  else{
    bu_vls_free(&vls);
    return;
  }

  FOR_ALL_SOLIDS(sp, &startp->l){
    if( lvl != -1 )
	bu_vls_printf(&vls, "%s", sp->s_flag == UP ? "VIEW " : "-no- ");
    for( i=0; i <= sp->s_last; i++ )
      bu_vls_printf(&vls, "/%s", sp->s_path[i]->d_namep);
    if(( lvl != -1 ) && ( sp->s_iflag == UP ))
      bu_vls_printf(&vls, " ILLUM");

    bu_vls_printf(&vls, "\n");

    if( lvl <= 0 )  continue;

    /* convert to the local unit for printing */
    bu_vls_printf(&vls, "  cent=(%.3f,%.3f,%.3f) sz=%g ",
		  sp->s_center[X]*base2local,
		  sp->s_center[Y]*base2local, 
		  sp->s_center[Z]*base2local,
		  sp->s_size*base2local );
    bu_vls_printf(&vls, "reg=%d\n",sp->s_regionid );
    bu_vls_printf(&vls, "  color=(%d,%d,%d) %d,%d,%d\n",
		  sp->s_basecolor[0],
		  sp->s_basecolor[1],
		  sp->s_basecolor[2],
		  sp->s_color[0],
		  sp->s_color[1],
		  sp->s_color[2]);

    if( lvl <= 1 )  continue;

    /* Print the actual vector list */
    nvlist = 0;
    npts = 0;
    for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
      register int	i;
      register int	nused = vp->nused;
      register int	*cmd = vp->cmd;
      register point_t *pt = vp->pt;

      RT_CK_VLIST( vp );
      nvlist++;
      npts += nused;
      if( lvl <= 2 )  continue;

      for( i = 0; i < nused; i++,cmd++,pt++ )  {
	bu_vls_printf(&vls, "  %s (%g, %g, %g)\n",
		      rt_vlist_cmd_descriptions[*cmd],
		      V3ARGS( *pt ) );
      }
    }

    bu_vls_printf(&vls, "  %d vlist structures, %d pts\n", nvlist, npts );
    bu_vls_printf(&vls, "  %d pts (via rt_ck_vlist)\n", rt_ck_vlist( &(sp->s_vlist) ) );
  }

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  (void)signal( SIGINT, SIG_IGN );
}

static char ** path_parse ();

/* Illuminate the named object */
int
f_ill(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register struct solid *sp;
	struct solid *lastfound = SOLID_NULL;
	register int i, j;
	int nmatch;
	int	nm_pieces;
	char	**path_piece = 0;
	char	*basename;
	char	*sname;

	if(argc < 2 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help ill");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if (state == ST_S_PICK)
	{
	    path_piece = path_parse(argv[1]);
	    for (nm_pieces = 0; path_piece[nm_pieces] != 0; ++nm_pieces)
		;

	    if (nm_pieces == 0)
	    {
	      Tcl_AppendResult(interp, "Bad solid path: '", argv[1], "'\n", (char *)NULL);
	      goto bail_out;
	    }
	    basename = path_piece[nm_pieces - 1];
	}
	else
	    basename = argv[1];

	if( (dp = db_lookup( dbip,  basename, LOOKUP_NOISY )) == DIR_NULL )
		goto bail_out;

	nmatch = 0;
	switch (state)
	{
	    case ST_S_PICK:
		if (!(dp -> d_flags & DIR_SOLID))
		{
		  Tcl_AppendResult(interp, basename, " is not a solid\n", (char *)NULL);
		  goto bail_out;
		}
		FOR_ALL_SOLIDS(sp, &HeadSolid.l)
		{
		    int	a_new_match;

		    i = sp -> s_last;
		    if (sp -> s_path[i] == dp)
		    {
			a_new_match = 1;
			j = nm_pieces - 1;
			for ( ; a_new_match && (i >= 0) && (j >= 0); --i, --j)
			{
			    sname = sp -> s_path[i] -> d_namep;
			    if ((*sname != *(path_piece[j]))
			     || strcmp(sname, path_piece[j]))
			        a_new_match = 0;
			}
			if (a_new_match && ((i >= 0) || (j < 0)))
			{
			    lastfound = sp;
			    ++nmatch;
			}
		    }
		    sp->s_iflag = DOWN;
		}
		break;
	    case ST_O_PICK:
		FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
			for( i=0; i<=sp->s_last; i++ )  {
				if( sp->s_path[i] == dp )  {
					lastfound = sp;
					nmatch++;
					break;
				}
			}
			sp->s_iflag = DOWN;
		}
		break;
	    default:
		state_err("keyboard illuminate pick");
		goto bail_out;
	}
	if( nmatch <= 0 )  {
	  Tcl_AppendResult(interp, argv[1], " not being displayed\n", (char *)NULL);
	  goto bail_out;
	}
	if( nmatch > 1 )  {
	  Tcl_AppendResult(interp, argv[1], " multiply referenced\n", (char *)NULL);
	  goto bail_out;
	}
	/* Make the specified solid the illuminated solid */
	illump = lastfound;
	illump->s_iflag = UP;
	if( state == ST_O_PICK )  {
		ipathpos = 0;
		(void)chg_state( ST_O_PICK, ST_O_PATH, "Keyboard illuminate");
	} else {
		/* Check details, Init menu, set state=ST_S_EDIT */
		init_sedit();
	}
	update_views = 1;
	if (path_piece)
	{
	    for (i = 0; path_piece[i] != 0; ++i)
		bu_free((genptr_t)path_piece[i], "f_ill: char *");
	    bu_free((genptr_t) path_piece, "f_ill: char **");
	}
	return TCL_OK;

bail_out:
    if (state != ST_VIEW)
	button(BE_REJECT);
    if (path_piece)
    {
	for (i = 0; path_piece[i] != 0; ++i)
	    bu_free((genptr_t)path_piece[i], "f_ill: char *");
	bu_free((genptr_t) path_piece, "f_ill: char **");
    }
    return TCL_ERROR;
}

/* Simulate pressing "Solid Edit" and doing an ILLuminate command */
int
f_sed(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  CHECK_READ_ONLY;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help sed");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( not_state( ST_VIEW, "keyboard solid edit start") )
    return TCL_ERROR;
  if(BU_LIST_IS_EMPTY(&HeadSolid.l)){
    Tcl_AppendResult(interp, "no solids being displayed\n",  (char *)NULL);
    return TCL_ERROR;
  }

  update_views = 1;

  button(BE_S_ILLUMINATE);	/* To ST_S_PICK */

  argv[0] = "ill";
  return f_ill(clientData, interp, argc, argv);	/* Illuminate named solid --> ST_S_EDIT */
}

void
check_nonzero_rates()
{
  if( rate_rotate[X] != 0.0 ||
      rate_rotate[Y] != 0.0 ||
      rate_rotate[Z] != 0.0 )
    rateflag_rotate = 1;
  else
    rateflag_rotate = 0;

  if( rate_slew[X] != 0.0 ||
      rate_slew[Y] != 0.0 ||
      rate_slew[Z] != 0.0 )
    rateflag_slew = 1;
  else
    rateflag_slew = 0;

  if( rate_zoom != 0.0 )
    rateflag_zoom = 1;
  else
    rateflag_zoom = 0;

  if(EDIT_TRAN){
    if( edit_rate_tran[X] != 0.0 ||
	edit_rate_tran[Y] != 0.0 ||
	edit_rate_tran[Z] != 0.0 )
      edit_rateflag_tran = 1;
    else
      edit_rateflag_tran = 0;
  }else 
    edit_rateflag_tran = 0;

  if(EDIT_ROTATE){
    if( edit_rate_rotate[X] != 0.0 ||
	edit_rate_rotate[Y] != 0.0 ||
	edit_rate_rotate[Z] != 0.0 )
      edit_rateflag_rotate = 1;
    else
      edit_rateflag_rotate = 0;
  }else
    edit_rateflag_rotate = 0;

  if(EDIT_SCALE){
    if( edit_rate_scale )
      edit_rateflag_scale = 1;
    else
      edit_rateflag_scale = 0;
  }else
    edit_rateflag_scale = 0;

  if( rate_azimuth )
    rateflag_azimuth = 1;
  else
    rateflag_azimuth = 0;
    
  dmaflag = 1;	/* values changed so update faceplate */
}

void
knob_update_rate_vars()
{
  if(!mged_variables.rateknobs)
    return;

  if(scroll_edit == EDIT_CLASS_ROTATE){
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Z]));
  }else{
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Z]));
  }

  if(scroll_edit == EDIT_CLASS_SCALE)
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_scale_vls));
  else
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_scale_vls));

  if(es_edclass == EDIT_CLASS_TRAN){
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Z]));
  }else{
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Z]));
  }
}

int
mged_print_knobvals(interp)
Tcl_Interp *interp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);

  if(mged_variables.rateknobs){
    if(scroll_edit == EDIT_CLASS_ROTATE){
      bu_vls_printf(&vls, "x = %f\n", edit_rate_rotate[X]);
      bu_vls_printf(&vls, "y = %f\n", edit_rate_rotate[Y]);
      bu_vls_printf(&vls, "z = %f\n", edit_rate_rotate[Z]);
    }else{
      bu_vls_printf(&vls, "x = %f\n", rate_rotate[X]);
      bu_vls_printf(&vls, "y = %f\n", rate_rotate[Y]);
      bu_vls_printf(&vls, "z = %f\n", rate_rotate[Z]);
    }

    if(scroll_edit == EDIT_CLASS_SCALE)
      bu_vls_printf(&vls, "S = %f\n", edit_rate_scale);
    else
      bu_vls_printf(&vls, "S = %f\n", rate_zoom);

    if(es_edclass == EDIT_CLASS_TRAN){
      bu_vls_printf(&vls, "X = %f\n", edit_rate_tran[X]);
      bu_vls_printf(&vls, "Y = %f\n", edit_rate_tran[Y]);
      bu_vls_printf(&vls, "Z = %f\n", edit_rate_tran[Z]);
    }else{
      bu_vls_printf(&vls, "X = %f\n", rate_slew[X]);
      bu_vls_printf(&vls, "Y = %f\n", rate_slew[Y]);
      bu_vls_printf(&vls, "Z = %f\n", rate_slew[Z]);
    }
  }else{
    if(scroll_edit == EDIT_CLASS_ROTATE){
      bu_vls_printf(&vls, "ax = %f\n", edit_absolute_rotate[X]);
      bu_vls_printf(&vls, "ay = %f\n", edit_absolute_rotate[Y]);
      bu_vls_printf(&vls, "az = %f\n", edit_absolute_rotate[Z]);
    }else{
      bu_vls_printf(&vls, "ax = %f\n", absolute_rotate[X]);
      bu_vls_printf(&vls, "ay = %f\n", absolute_rotate[Y]);
      bu_vls_printf(&vls, "az = %f\n", absolute_rotate[Z]);
    }

    if(scroll_edit == EDIT_CLASS_SCALE)
      bu_vls_printf(&vls, "aS = %f\n", edit_absolute_scale);
    else
      bu_vls_printf(&vls, "aS = %f\n", absolute_zoom);

    if(es_edclass == EDIT_CLASS_TRAN){
      bu_vls_printf(&vls, "aX = %f\n", edit_absolute_tran[X]);
      bu_vls_printf(&vls, "aY = %f\n", edit_absolute_tran[Y]);
      bu_vls_printf(&vls, "aZ = %f\n", edit_absolute_tran[Z]);
    }else{
      bu_vls_printf(&vls, "aX = %f\n", absolute_slew[X]);
      bu_vls_printf(&vls, "aY = %f\n", absolute_slew[Y]);
      bu_vls_printf(&vls, "aZ = %f\n", absolute_slew[Z]);
    }
  }

  if(mged_variables.adcflag){
    bu_vls_printf(&vls, "xadc = %d\n", dv_xadc);
    bu_vls_printf(&vls, "yadc = %d\n", dv_yadc);
    bu_vls_printf(&vls, "ang1 = %d\n", dv_1adc);
    bu_vls_printf(&vls, "ang2 = %d\n", dv_2adc);
    bu_vls_printf(&vls, "distadc = %d\n", dv_distadc);
  }

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_RETURN;
}

/* Main processing of knob twists.  "knob id val id val ..." */
int
f_knob(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int	i;
  fastf_t f;
  vect_t tvec;
  char	*cmd;
  char  knob_val_pair[128];
  int do_tran = 0;
  int incr_flag = 0;  /* interpret values as increments */
  int view_flag = 0;  /* force view interpretation */
  int edit_flag = 0;  /* force edit interpretation */
  struct bu_vls vls;

  if(argc < 1 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help knob");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Check for options */
  {
    int c;

    bu_optind = 1;
    while((c = bu_getopt(argc, argv, "eiv")) != EOF){
      switch(c){
      case 'e':
	edit_flag = 1;
	break;
      case 'i':
	incr_flag = 1;
	break;
      case 'v':
	view_flag = 1;
	break;
      default:
	break;
      }
    }
   
    argv += bu_optind - 1;
    argc -= bu_optind - 1;
  }

  /* print the current values */
  if(argc == 1)
    return mged_print_knobvals(interp);

  for(--argc, ++argv; argc; --argc, ++argv){
    cmd = *argv;
    
    if( strcmp( cmd, "zap" ) == 0 || strcmp( cmd, "zero" ) == 0 )  {
      char *av[3];

      VSETALL( rate_rotate, 0 );
      VSETALL( rate_slew, 0 );
      rate_zoom = 0;
      VSETALL( edit_rate_rotate, 0 );
      VSETALL( edit_rate_tran, 0 );
      edit_rate_scale = 0.0;
      rate_azimuth = 0.0;
      knob_update_rate_vars();

      av[0] = "adc";
      av[1] = "reset";
      av[2] = (char *)NULL;

      (void)f_adc( clientData, interp, 2, av );

      if(knob_hook)
	knob_hook();

      (void)mged_svbase();
    } else if( strcmp( cmd, "calibrate" ) == 0 ) {
      VSETALL( absolute_slew, 0.0 );
    }else{
      if(argc - 1){
	i = atoi(argv[1]);
	f = atof(argv[1]);
#if 0
	if( f < -1.0 )
	  f = -1.0;
	else if( f > 1.0 )
	  f = 1.0;
#endif
      }else
	goto usage;

      --argc;
      ++argv;

    if( cmd[1] == '\0' )  {
#if 0
      if( f < -1.0 )
	f = -1.0;
      else if( f > 1.0 )
	f = 1.0;
#endif

      switch( cmd[0] )  {
      case 'x':
	if(incr_flag){
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[X] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[X]));
	  }else{
	    rate_rotate[X] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[X]));
	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[X] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[X]));
	  }else{
	    rate_rotate[X] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[X]));
	  }
	}

	break;
      case 'y':
	if(incr_flag){
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[Y] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Y]));
	  }else{
	    rate_rotate[Y] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Y]));
	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[Y] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Y]));
	  }else{
	    rate_rotate[Y] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Y]));
	  }
	}

	break;
      case 'z':
	if(incr_flag){
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[Z] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Z]));
	  }else{
	    rate_rotate[Z] += f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Z]));
	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    edit_rate_rotate[Z] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_rotate_vls[Z]));
	  }else{
	    rate_rotate[Z] = f;
	    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_rotate_vls[Z]));
	  }
	}

      break;
    case 'X':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[X] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[X]));
	}else{
	  rate_slew[X] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[X]));
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[X] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[X]));
	}else{
	  rate_slew[X] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[X]));
	}
      }

      break;
    case 'Y':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[Y] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Y]));
	}else{
	  rate_slew[Y] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Y]));
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[Y] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Y]));
	}else{
	  rate_slew[Y] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Y]));
	}
      }

      break;
    case 'Z':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[Z] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Z]));
	}else{
	  rate_slew[Z] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Z]));
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_tran[Z] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_tran_vls[Z]));
	}else{
	  rate_slew[Z] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_tran_vls[Z]));
	}
      }

      break;
    case 'S':
      if(incr_flag){
	if(EDIT_SCALE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_scale += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_scale_vls));
	}else{
	  rate_zoom += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_scale_vls));
	}
      }else{
	if(EDIT_SCALE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_rate_scale = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_rate_scale_vls));
	}else{
	  rate_zoom = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_scale_vls));
	}
      }

      break;
    default:
      goto usage;
    }
  } else if( cmd[0] == 'a' && cmd[1] != '\0' && cmd[2] == '\0' ) {
    switch( cmd[1] ) {
    case 'x':
      if(incr_flag){
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[X] += f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = f;
	    tvec[Y] = 0.0;
	    tvec[Z] = 0.0;
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[X] += f;
	  }else{
	    (void)mged_vrot(f, 0.0, 0.0);
	    absolute_rotate[X] += f;
	  }
	}
      }else{
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[X] = f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = f - last_edit_absolute_rotate[X];
	    tvec[Y] = 0.0;
	    tvec[Z] = 0.0;
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[X] = f;
	  }else{
	    mged_vrot(f - last_absolute_rotate[X], 0.0, 0.0);
	    absolute_rotate[X] = f;
	  }
	}
      }
	  
      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	if(edit_absolute_rotate[X] < -180.0)
	  edit_absolute_rotate[X] = edit_absolute_rotate[X] + 360.0;
	else if(edit_absolute_rotate[X] > 180.0)
	  edit_absolute_rotate[X] = edit_absolute_rotate[X] - 360.0;

	last_edit_absolute_rotate[X] = edit_absolute_rotate[X];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_rotate_vls[X]));
      }else{
	if(absolute_rotate[X] < -180.0)
	  absolute_rotate[X] = absolute_rotate[X] + 360.0;
	else if(absolute_rotate[X] > 180.0)
	  absolute_rotate[X] = absolute_rotate[X] - 360.0;

	last_absolute_rotate[X] = absolute_rotate[X];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[X]));
      }

      break;
    case 'y':
      if(incr_flag){
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[Y] += f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = 0.0;
	    tvec[Y] = f;
	    tvec[Z] = 0.0;
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[Y] += f;
	  }else{
	    (void)mged_vrot(0.0, f, 0.0);
	    absolute_rotate[Y] += f;
	  }
	}
      }else{
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[Y] = f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = 0.0;
	    tvec[Y] = f - last_edit_absolute_rotate[Y];
	    tvec[Z] = 0.0;
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[Y] = f;
	  }else{
	    mged_vrot(0.0, f - last_absolute_rotate[Y], 0.0);
	    absolute_rotate[Y] = f;
	  }
	}
      }
	  
      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	if(edit_absolute_rotate[Y] < -180.0)
	  edit_absolute_rotate[Y] = edit_absolute_rotate[Y] + 360.0;
	else if(edit_absolute_rotate[Y] > 180.0)
	  edit_absolute_rotate[Y] = edit_absolute_rotate[Y] - 360.0;

	last_edit_absolute_rotate[Y] = edit_absolute_rotate[Y];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_rotate_vls[Y]));
      }else{
	if(absolute_rotate[Y] < -180.0)
	  absolute_rotate[Y] = absolute_rotate[Y] + 360.0;
	else if(absolute_rotate[Y] > 180.0)
	  absolute_rotate[Y] = absolute_rotate[Y] - 360.0;

	last_absolute_rotate[Y] = absolute_rotate[Y];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[Y]));
      }

      break;
    case 'z':
      if(incr_flag){
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[Z] += f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = 0.0;
	    tvec[Y] = 0.0;
	    tvec[Z] = f;
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[Z] += f;
	  }else{
	    (void)mged_vrot(0.0, 0.0, f);
	    absolute_rotate[Z] += f;
	  }
	}
      }else{
	if(SEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_rotate[Z] = f;
	  (void)mged_param(interp, 3, edit_absolute_rotate);
	}else {
	  if(OEDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	    tvec[X] = 0.0;
	    tvec[Y] = 0.0;
	    tvec[Z] = f - last_edit_absolute_rotate[Z];
	    mged_rot_obj(interp, 1, tvec);
	    edit_absolute_rotate[Z] = f;
	  }else{
	    mged_vrot(0.0, 0.0, f - last_absolute_rotate[Z]);
	    absolute_rotate[Z] = f;
	  }
	}
      }
	  
      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables.edit && !view_flag) || edit_flag)){
	if(edit_absolute_rotate[Z] < -180.0)
	  edit_absolute_rotate[Z] = edit_absolute_rotate[Z] + 360.0;
	else if(edit_absolute_rotate[Z] > 180.0)
	  edit_absolute_rotate[Z] = edit_absolute_rotate[Z] - 360.0;

	last_edit_absolute_rotate[Z] = edit_absolute_rotate[Z];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_rotate_vls[Z]));
      }else{
	if(absolute_rotate[Z] < -180.0)
	  absolute_rotate[Z] = absolute_rotate[Z] + 360.0;
	else if(absolute_rotate[Z] > 180.0)
	  absolute_rotate[Z] = absolute_rotate[Z] - 360.0;

	last_absolute_rotate[Z] = absolute_rotate[Z];
	Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[Z]));
      }

      break;
    case 'X':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[X] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[X]));
	}else{
	  absolute_slew[X] += f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
#endif
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[X] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[X]));
	}else{
	  absolute_slew[X] = f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
#endif
	}
      }

      do_tran = 1;
      break;
    case 'Y':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[Y] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[Y]));
	}else{
	  absolute_slew[Y] += f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
#endif
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[Y] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[Y]));
	}else{
	  absolute_slew[Y] = f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
#endif
	}
      }
      
      do_tran = 1;
      break;
    case 'Z':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[Z] += f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[Z]));
	}else{
	  absolute_slew[Z] += f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
#endif
	}
      }else{
	if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_tran[Z] = f;
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_tran_vls[Z]));
	}else{
	  absolute_slew[Z] = f;
#if 0
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
#endif
	}
      }

      do_tran = 1;
      break;
    case 'S':
      if(incr_flag){
	if(EDIT_SCALE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_scale += f;
	  if(state == ST_S_EDIT)
	    sedit_abs_scale();
	  else
	    oedit_abs_scale();
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_scale_vls));
	}else{
	  absolute_zoom += f;
	  abs_zoom();
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_scale_vls));
	}
      }else{
	if(EDIT_SCALE && ((mged_variables.edit && !view_flag) || edit_flag)){
	  edit_absolute_scale = f;
	  if(state == ST_S_EDIT)
	    sedit_abs_scale();
	  else
	    oedit_abs_scale();
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&edit_absolute_scale_vls));
	}else{
	  absolute_zoom = f;
	  abs_zoom();
	  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_scale_vls));
	}
      }

      break;
    default:
      goto usage;
    }
  } else if( strcmp( cmd, "azim" ) == 0 ) {
    if(incr_flag)
      rate_azimuth += f;
    else
      rate_azimuth = f;

    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&rate_azimuth_vls));
  } else if( strcmp( cmd, "aazim" ) == 0 ) {
    if(incr_flag)
      (void)mged_aetview(1, f, 0.0, 0.0);
    else
      (void)mged_aetview(0, f, curr_dm_list->s_info->elevation, curr_dm_list->s_info->twist);

    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_azimuth_vls));
  } else if( strcmp( cmd, "xadc" ) == 0 ) {
	  char *av[4];
	  char    sval[32];

	  av[1] = "x";
	  av[3] = NULL;

	  if(incr_flag)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%d", i);
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "yadc" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "y";
	  av[3] = NULL;

	  if(incr_flag)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%d", i);
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "ang1" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "a1";
	  av[3] = NULL;

	  if(incr_flag)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
#if 1
	  sprintf(sval, "%f", f);
#else
	  sprintf(sval, "%f", 45.0*(1.0-(double)i/2047.0));
#endif
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "ang2" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "a2";
	  av[3] = NULL;

	  if(incr_flag)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
#if 1
	  sprintf(sval, "%f", f);
#else
	  sprintf(sval, "%f", 45.0*(1.0-(double)i/2047.0));
#endif
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "distadc" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "dst";
	  av[3] = NULL;

	  if(incr_flag)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%f", ((double)i/2047.0 + 1.0)*Viewscale * base2local * M_SQRT2_DIV2);
	  (void)f_adc(clientData, interp, 3, av);
	} else {
usage:
	  Tcl_AppendResult(interp,
		"knob: x,y,z for rotation in degrees\n",
		"knob: S for scale, X,Y,Z for slew (rates, range -1..+1)\n",
		"knob: ax,ay,az for absolute rotation in degrees, aS for absolute scale,\n",
		"knob: aX,aY,aZ for absolute slew.  calibrate to set current slew to 0\n",
		"knob: xadc, yadc, distadc (values, range -2048..+2047)\n",
		"knob: ang1, ang2 for adc angles in degrees\n",
		"knob: zero (cancel motion)\n", (char *)NULL);

	  return TCL_ERROR;
	}
      }
  }

  if(do_tran)
    (void)knob_tran(interp, view_flag, edit_flag);
 
  check_nonzero_rates();
  return TCL_OK;
}

int
knob_tran(interp, view_flag, edit_flag)
Tcl_Interp *interp;
int view_flag;
int edit_flag;
{
  point_t new_pos;
  point_t diff;
  point_t model_pos;

  if(EDIT_TRAN && ((mged_variables.edit && !view_flag) || edit_flag)){
    VSCALE(diff, edit_absolute_tran, Viewscale);
    VADD2(model_pos, diff, e_axes_pos);
    MAT4X3PNT(new_pos, model2view, model_pos);

    if(state == ST_S_EDIT)
      sedit_trans(new_pos);
    else
      oedit_trans(new_pos);

  }else /* slew the view */
    mged_tran(interp, absolute_slew);

  return TCL_OK;
}

void
mged_do_rate_rotate()
{
  (void)mged_vrot(rate_rotate[X], rate_rotate[Y], rate_rotate[Z]);
  absolute_rotate[X] += rate_rotate[X];
  absolute_rotate[Y] += rate_rotate[Y];
  absolute_rotate[Z] += rate_rotate[Z];

  if(absolute_rotate[X] < -180.0)
    absolute_rotate[X] = absolute_rotate[X] + 360.0;
  else if(absolute_rotate[X] > 180.0)
    absolute_rotate[X] = absolute_rotate[X] - 360.0;

  last_absolute_rotate[X] = absolute_rotate[X];
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[X]));

  if(absolute_rotate[Y] < -180.0)
    absolute_rotate[Y] = absolute_rotate[Y] + 360.0;
  else if(absolute_rotate[Y] > 180.0)
    absolute_rotate[Y] = absolute_rotate[Y] - 360.0;

  last_absolute_rotate[Y] = absolute_rotate[Y];
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[Y]));

  if(absolute_rotate[Z] < -180.0)
    absolute_rotate[Z] = absolute_rotate[Z] + 360.0;
  else if(absolute_rotate[Z] > 180.0)
    absolute_rotate[Z] = absolute_rotate[Z] - 360.0;

  last_absolute_rotate[Z] = absolute_rotate[Z];
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_rotate_vls[Z]));
}

void
mged_do_rate_slew()
{
  absolute_slew[X] += rate_slew[X] * 0.1;
  absolute_slew[Y] += rate_slew[Y] * 0.1;
  absolute_slew[Z] += rate_slew[Z] * 0.1;
  mged_tran(interp, absolute_slew);
}



/*
 *			F _ T O L
 *
 *  "tol"	displays current settings
 *  "tol abs #"	sets absolute tolerance.  # > 0.0
 *  "tol rel #"	sets relative tolerance.  0.0 < # < 1.0
 *  "tol norm #" sets normal tolerance, in degrees.
 *  "tol dist #" sets calculational distance tolerance
 *  "tol perp #" sets calculational normal tolerance.
 */
int
f_tol(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	double	f;
	int argind=1;

	if(argc < 1 || 11 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help tol");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( argc < 3 )  {
	  Tcl_AppendResult(interp, "Current tolerance settings are:\n", (char *)NULL);
	  Tcl_AppendResult(interp, "Tesselation tolerances:\n", (char *)NULL );
	  if( mged_abs_tol > 0.0 )  {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\tabs %g %s\n", mged_abs_tol * base2local,
			  rt_units_string(dbip->dbi_local2base) );
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	  } else {
	    Tcl_AppendResult(interp, "\tabs None\n", (char *)NULL);
	  }
	  if( mged_rel_tol > 0.0 )  {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "\trel %g (%g%%)\n", mged_rel_tol, mged_rel_tol * 100.0 );
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	  } else {
	    Tcl_AppendResult(interp, "\trel None\n", (char *)NULL);
	  }
	  if( mged_nrm_tol > 0.0 )  {
	    int	deg, min;
	    double	sec;
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    sec = mged_nrm_tol * bn_radtodeg;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  mged_nrm_tol * bn_radtodeg, deg, min, sec );
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	  } else {
	    Tcl_AppendResult(interp, "\tnorm None\n", (char *)NULL);
	  }

	  {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls,"Calculational tolerances:\n");
	    bu_vls_printf(&vls,
			  "\tdistance = %g %s\n\tperpendicularity = %g (cosine of %g degrees)\n",
			   mged_tol.dist*base2local, rt_units_string(local2base), mged_tol.perp,
			  acos(mged_tol.perp)*bn_radtodeg);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	  }

	  return TCL_OK;
	}

	while( argind < argc )
	{

		f = atof(argv[argind+1]);
		if( argv[argind][0] == 'a' )  {
			/* Absolute tol */
			if( f <= 0.0 )
			        mged_abs_tol = 0.0;	/* None */
			else
			        mged_abs_tol = f * local2base;
			return TCL_OK;
		}
		else if( argv[argind][0] == 'r' )  {
			/* Relative */
			if( f < 0.0 || f >= 1.0 )  {
			   Tcl_AppendResult(interp, "relative tolerance must be between 0 and 1, not changed\n", (char *)NULL);
			   return TCL_ERROR;
			}
			/* Note that a value of 0.0 will disable relative tolerance */
			mged_rel_tol = f;
		}
		else if( argv[argind][0] == 'n' )  {
			/* Normal tolerance, in degrees */
			if( f < 0.0 || f > 90.0 )  {
			  Tcl_AppendResult(interp, "Normal tolerance must be in positive degrees, < 90.0\n", (char *)NULL);
			  return TCL_ERROR;
			}
			/* Note that a value of 0.0 or 360.0 will disable this tol */
			mged_nrm_tol = f * bn_degtorad;
		}
		else if( argv[argind][0] == 'd' ) {
			/* Calculational distance tolerance */
			if( f < 0.0 ) {
			  Tcl_AppendResult(interp, "Calculational distance tolerance must be positive\n", (char *)NULL);
			  return TCL_ERROR;
			}
			mged_tol.dist = f*local2base;
			mged_tol.dist_sq = mged_tol.dist * mged_tol.dist;
		}
		else if( argv[argind][0] == 'p' ) {
			/* Calculational perpendicularity tolerance */
			if( f < 0.0 || f > 1.0 ) {
			  Tcl_AppendResult(interp, "Calculational perpendicular tolerance must be from 0 to 1\n", (char *)NULL);
			  return TCL_ERROR;
			}
			mged_tol.perp = f;
			mged_tol.para = 1.0 - f;
		}
		else
		  Tcl_AppendResult(interp, "Error, tolerance '", argv[argind],
				   "' unknown\n", (char *)NULL);

		argind += 2;
	}
	return TCL_OK;
}

/* absolute_zoom's value range is [-1.0, 1.0] */
static void
abs_zoom()
{
  vect_t new_pos;

  /* Use initial Viewscale */
  if(-SMALL_FASTF < absolute_zoom && absolute_zoom < SMALL_FASTF)
    Viewscale = i_Viewscale;
  else{
    /* if positive */
    if(absolute_zoom > 0){
      /* scale i_Viewscale by values in [0.0, 1.0] range */
      Viewscale = i_Viewscale * (1.0 - absolute_zoom);
    }else/* negative */
      /* scale i_Viewscale by values in [1.0, 10.0] range */
      Viewscale = i_Viewscale * (1.0 + (absolute_zoom * -9.0));
  }

  if( Viewscale < MINVIEW )
    Viewscale = MINVIEW;

  new_mats();

  if(absolute_slew[X] != 0.0 ||
     absolute_slew[Y] != 0.0 ||
     absolute_slew[Z] != 0.0){
    VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
    MAT4X3PNT(absolute_slew, model2view, new_pos);

    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
  }

  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_scale_vls));
}

int
mged_zoom(val)
double val;
{
  vect_t new_pos;

  if( val < SMALL_FASTF || val > INFINITY )  {
    Tcl_AppendResult(interp, "zoom: scale factor out of range\n", (char *)NULL);
    return TCL_ERROR;
  }

  Viewscale /= val;
  if( Viewscale < MINVIEW )
    Viewscale = MINVIEW;

  new_mats();

  absolute_zoom = 1.0 - Viewscale / i_Viewscale;
  if(absolute_zoom < 0.0)
    absolute_zoom /= 9.0;

  if(absolute_slew[X] != 0.0 ||
     absolute_slew[Y] != 0.0 ||
     absolute_slew[Z] != 0.0){
    VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
    MAT4X3PNT(absolute_slew, model2view, new_pos);

    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
  }

  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_scale_vls));
  return TCL_OK;
}


/*
 *			F _ Z O O M
 *
 *  A scale factor of 2 will increase the view size by a factor of 2,
 *  (i.e., a zoom out) which is accomplished by reducing Viewscale in half.
 */
int
f_zoom(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help zoom");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  return mged_zoom(atof(argv[1]));
}

/*
 *			F _ O R I E N T A T I O N
 *
 *  Set current view direction from a quaternion,
 *  such as might be found in a "saveview" script.
 */
int
f_orientation(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register int	i;
  quat_t		quat;

  if(argc < 5 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help orientation");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for( i=0; i<4; i++ )
    quat[i] = atof( argv[i+1] );
  quat_quat2mat( Viewrot, quat );
  new_mats();

  return TCL_OK;
}

/*
 *			F _ Q V R O T
 *
 *  Set view from direction vector and twist angle
 */
int
f_qvrot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
    double	dx, dy, dz;
    double	az;
    double	el;
    double	theta;

    if(argc < 5 || 5 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help qvrot");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    dx = atof(argv[1]);
    dy = atof(argv[2]);
    dz = atof(argv[3]);

    if (NEAR_ZERO(dy, 0.00001) && NEAR_ZERO(dx, 0.00001))
    {
	if (NEAR_ZERO(dz, 0.00001))
	{
	  Tcl_AppendResult(interp, "f_qvrot: (dx, dy, dz) may not be the zero vector\n", (char *)NULL);
	  return TCL_ERROR;
	}
	az = 0.0;
    }
    else
	az = atan2(dy, dx);
    
    el = atan2(dz, sqrt(dx * dx + dy * dy));

    setview( 270.0 + el * radtodeg, 0.0, 270.0 - az * radtodeg );
    theta = atof(argv[4]) * degtorad;
    usejoy(0.0, 0.0, theta);

    return TCL_OK;
}

/*
 *			P A T H _ P A R S E
 *
 *	    Break up a path string into its constituents.
 *
 *	This function has one parameter:  a slash-separated path.
 *	path_parse() allocates storage for and copies each constituent
 *	of path.  It returns a null-terminated array of these copies.
 *
 *	It is the caller's responsibility to free the copies and the
 *	pointer to them.
 */
static char **
path_parse (path)

char	*path;

{
    int		nm_constituents;
    int		i;
    char	*pp;
    char	*start_addr;
    char	**result;
    char	*copy;

    nm_constituents = ((*path != '/') && (*path != '\0'));
    for (pp = path; *pp != '\0'; ++pp)
	if (*pp == '/')
	{
	    while (*++pp == '/')
		;
	    if (*pp != '\0')
		++nm_constituents;
	}
    
    result = (char **) bu_malloc((nm_constituents + 1) * sizeof(char *),
			"array of strings");
    
    for (i = 0, pp = path; i < nm_constituents; ++i)
    {
	while (*pp == '/')
	    ++pp;
	start_addr = pp;
	while ((*++pp != '/') && (*pp != '\0'))
	    ;
	result[i] = (char *) bu_malloc((pp - start_addr + 1) * sizeof(char),
			"string");
	strncpy(result[i], start_addr, (pp - start_addr));
	result[i][pp - start_addr] = '\0';
    }
    result[nm_constituents] = 0;

    return(result);
}


int
f_setview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    *argv[];
{
  double x, y, z;

  if(argc < 4 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help setview");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &x) < 1){
    Tcl_AppendResult(interp, "f_setview: bad x value - ",
		     argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &y) < 1){
    Tcl_AppendResult(interp, "f_setview: bad y value - ",
		     argv[2], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &z) < 1){
    Tcl_AppendResult(interp, "f_setview: bad z value - ",
		     argv[3], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  setview(x, y, z);

  return TCL_OK;
}


int
mged_tran(interp, tranvec)
Tcl_Interp *interp;
vect_t tranvec;
{
  vect_t old_pos;
  vect_t new_pos;
  vect_t diff;

  VMOVE(absolute_slew, tranvec);
  MAT4X3PNT( new_pos, view2model, absolute_slew );
  MAT_DELTAS_GET_NEG( old_pos, toViewcenter );
  VSUB2( diff, new_pos, old_pos );
  VADD2(new_pos, orig_pos, diff);
  MAT_DELTAS_VEC( toViewcenter, new_pos);
#if 0
  MAT_DELTAS_VEC( ModelDelta, new_pos);		/* #if 0 */
#endif
  new_mats();

#if 1
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
#endif
  return TCL_OK;
}


int
f_tran(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  int incr = 0;
  int x, y, z;
  vect_t tranvec;

  if(argc < 3 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help tran");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Check for -i option */
  if(argv[1][0] == '-' && argv[1][1] == 'i'){
    incr = 1;  /* treat arguments as incremental values */
    ++argv;
    --argc;
  }

  if(sscanf(argv[1], "%d", &x) < 1){
    Tcl_AppendResult(interp, "f_tran: bad x value - ",
		     argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%d", &y) < 1){
    Tcl_AppendResult(interp, "f_tran: bad y value - ",
		     argv[2], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(argc == 4){
    if(sscanf(argv[3], "%d", &z) < 1){
      Tcl_AppendResult(interp, "f_tran: bad z value - ",
		       argv[3], "\n", (char *)NULL);
      return TCL_ERROR;
    }
  }else{
    if(incr)
      z = 0.0;
    else
      z = absolute_slew[Z];
  }

  if(incr){
    point_t tpoint;

    VSET(tpoint, x, y, z)
    VADD2(tranvec, absolute_slew, tpoint);
  }else{
    VSET(tranvec, x, y, z);
  }

  return mged_tran(interp, tranvec);
}


int
f_slewview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  int x, y, z;
  vect_t slewvec;

  if(argc < 3 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help sv");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%d", &x) < 1){
    Tcl_AppendResult(interp, "f_slewview: bad x value - ",
		     argv[1], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%d", &y) < 1){
    Tcl_AppendResult(interp, "f_slewview: bad y value - ",
		     argv[2], "\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(argc == 4){
    if(sscanf(argv[3], "%d", &z) < 1){
      Tcl_AppendResult(interp, "f_slewview: bad z value - ",
		       argv[3], "\n", (char *)NULL);
      return TCL_ERROR;
    }
  }else
    z = 0;

  VSET(slewvec, x/2047.0, y/2047.0, z/2047.0);
  VSUB2(absolute_slew, absolute_slew, slewvec);
  slewview( slewvec );

  return TCL_OK;
}


/* set view reference base */
int
mged_svbase()
{ 
  i_Viewscale = Viewscale;
  MAT_DELTAS_GET(orig_pos, toViewcenter);

  /* reset absolute slider values */
  VSETALL( absolute_rotate, 0.0);
  VSETALL( last_absolute_rotate, 0.0);
  VSETALL( absolute_slew, 0.0);
  absolute_zoom = 0.0;

  set_scroll();

  return TCL_OK;
}


int
f_svbase(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help svb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  return mged_svbase();
}


/*
 *	G E T K N O B
 *
 *	Procedure called by the Tcl/Tk interface code to find the values
 *	of the knobs/sliders.
 */

int
cmd_getknob(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  int len;
  fastf_t f;
  char *cp;

  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help getknob");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  cp  = argv[1];
  len = strlen(cp);
  if(len == 1){
    switch(*cp){
    case 'x':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_rate_rotate[X];
      else
	f = rate_rotate[X];
      break;
    case 'y':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_rate_rotate[Y];
      else
	f = rate_rotate[Y];
      break;
    case 'z':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_rate_rotate[Z];
      else
	f = rate_rotate[Z];
      break;
    case 'X':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_rate_tran[X];
      else
	f = rate_slew[X];
      break;
    case 'Y':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_rate_tran[Y];
      else
	f = rate_slew[Y];
      break;
    case 'Z':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_rate_tran[Z];
      else
	f = rate_slew[Z];
      break;
    case 'S':
      if(EDIT_SCALE && mged_variables.edit)
	f = edit_rate_scale;
      else
	f = rate_zoom;
      break;
    default:
      Tcl_AppendResult(interp, "getknob: bad value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }
  }else if(len == 2){
    if(*cp++ != 'a'){
      Tcl_AppendResult(interp, "getknob: bad value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    switch(*cp){
    case 'x':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_absolute_rotate[X];
      else
	f = absolute_rotate[X];
      break;
    case 'y':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_absolute_rotate[Y];
      else
	f = absolute_rotate[Y];
      break;
    case 'z':
      if(EDIT_ROTATE && mged_variables.edit)
	f = edit_absolute_rotate[Z];
      else
	f = absolute_rotate[Z];
      break;
    case 'X':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_absolute_tran[X];
      else
	f = absolute_slew[X];
      break;
    case 'Y':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_absolute_tran[Y];
      else
	f = absolute_slew[Y];
      break;
    case 'Z':
      if(EDIT_TRAN && mged_variables.edit)
	f = edit_absolute_tran[Z];
      else
	f = absolute_slew[Z];
      break;
    case 'S':
      if(EDIT_SCALE && mged_variables.edit)
	f = edit_absolute_scale;
      else
	f = absolute_zoom;
      break;
    default:
      Tcl_AppendResult(interp, "getknob: bad value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }
  }else{
    if(strcmp(argv[1], "xadc") == 0)
      f = dv_xadc;
    else if(strcmp(argv[1], "yadc") == 0)
      f = dv_yadc;
    else if(strcmp(argv[1], "ang1") == 0)
      f = dv_1adc;
    else if(strcmp(argv[1], "ang2") == 0)
      f = dv_2adc;
    else if(strcmp(argv[1], "distadc") == 0)
      f = dv_distadc;
    else{
      Tcl_AppendResult(interp, "getknob: bad value - ", argv[1], "\n", (char *)NULL);
       return TCL_ERROR;
    }
  }

  sprintf(interp->result, "%lf", f);
  return TCL_OK;
}

/*
 *			F _ V R O T _ C E N T E R
 *
 *  Set the center of rotation, either in model coordinates, or
 *  in view (+/-1) coordinates.
 *  The default is to rotate around the view center: v=(0,0,0).
 */
int
f_vrot_center(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(argc < 5 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help vrot_center");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, "Not ready until tomorrow.\n", (char *)NULL);
  return TCL_OK;
}

/*
 *			U S E J O Y
 *
 *  Apply the "joystick" delta rotation to the viewing direction,
 *  where the delta is specified in terms of the *viewing* axes.
 *  Rotation is performed about the view center, for now.
 *  Angles are in radians.
 */
void
usejoy( xangle, yangle, zangle )
double	xangle, yangle, zangle;
{
	mat_t	newrot;		/* NEW rot matrix, from joystick */

	if( state == ST_S_EDIT )  {
		if( sedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* solid edit claimed event */
	} else if( state == ST_O_EDIT )  {
		if( objedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* object edit claimed event */
	}

	/* NORMAL CASE.
	 * Apply delta viewing rotation for non-edited parts.
	 * The view rotates around the VIEW CENTER.
	 */
	bn_mat_idn( newrot );
	buildHrot( newrot, xangle, yangle, zangle );

	bn_mat_mul2( newrot, Viewrot );
	{
		mat_t	newinv;
		bn_mat_inv( newinv, newrot );
		wrt_view( ModelDelta, newinv, ModelDelta );	/* Updates ModelDelta */
	}
	new_mats();
}

/*
 *			A B S V I E W _ V
 *
 *  The "angle" ranges from -1 to +1.
 *  Assume rotation around view center, for now.
 */
void
absview_v( ang )
CONST point_t	ang;
{
	point_t	rad;

	VSCALE( rad, ang, bn_pi );	/* range from -pi to +pi */
	buildHrot( Viewrot, rad[X], rad[Y], rad[Z] );
	new_mats();
}

/*
 *			S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
setview( a1, a2, a3 )
double a1, a2, a3;		/* DOUBLE angles, in degrees */
{
  point_t model_pos;
  point_t temp;

  buildHrot( Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
  new_mats();

  if(absolute_slew[X] != 0.0 ||
     absolute_slew[Y] != 0.0 ||
     absolute_slew[Z] != 0.0){
    VSET(temp, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
    MAT4X3PNT(absolute_slew, model2view, temp);
#if 1
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
    Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
#endif
  }
}


/*
 *			S L E W V I E W
 *
 *  Given a position in view space,
 *  make that point the new view center.
 */
void
slewview( view_pos )
vect_t view_pos;
{
  point_t old_model_center;
  point_t new_model_center;
  vect_t diff;
  vect_t temp;
  mat_t	delta;

  MAT_DELTAS_GET_NEG( old_model_center, toViewcenter );

  MAT4X3PNT( new_model_center, view2model, view_pos );
  MAT_DELTAS_VEC_NEG( toViewcenter, new_model_center );

  VSUB2( diff, new_model_center, old_model_center );
  bn_mat_idn( delta );
  MAT_DELTAS_VEC( delta, diff );
  bn_mat_mul2( delta, ModelDelta );	/* updates ModelDelta */
  new_mats();

  VSET(temp, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
  MAT4X3PNT(absolute_slew, model2view, temp);

#if 1
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[X]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Y]));
  Tcl_UpdateLinkedVar(interp, bu_vls_addr(&absolute_tran_vls[Z]));
#endif
}

/*
 *			F _ E Y E _ P T
 *
 *  Perform same function as mged command that 'eye_pt' performs
 *  in rt animation script -- put eye at specified point.
 */
int
f_eye_pt(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	point_t	eye_model;
	vect_t	xlate;
	vect_t	new_cent;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help eye_pt");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET(eye_model, atof(argv[1]), atof(argv[2]), atof(argv[3]) );

	/* First step:  put eye at view center (view 0,0,0) */
	MAT_DELTAS_VEC_NEG( toViewcenter, eye_model );
	new_mats();

	/*  Second step:  put eye at view 0,0,1.
	 *  For eye to be at 0,0,1, the old 0,0,-1 needs to become 0,0,0.
	 */
	VSET( xlate, 0, 0, -1 );	/* correction factor */
	MAT4X3PNT( new_cent, view2model, xlate );
	MAT_DELTAS_VEC_NEG( toViewcenter, new_cent );
	new_mats();

	return TCL_OK;
}

/*
 *			F _ M O D E L 2 V I E W
 *
 *  Given a point in model space coordinates (in mm)
 *  convert it to view (screen) coordinates.
 */
int
f_model2view(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	point_t	model;
	point_t	view;
	struct bu_vls	str;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help model2view");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET(model, atof(argv[1]),
		atof(argv[2]),
		atof(argv[3]) );

	MAT4X3PNT( view, model2view, model );

	bu_vls_init(&str);
	bu_vls_printf(&str, "%.15e %.15e %.15e\n", V3ARGS(view) );
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 *			F _ V I E W 2 M O D E L
 *
 *  Given a point in view (screen) space coordinates,
 *  convert it to model coordinates (in mm).
 */
int
f_view2model(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	point_t	model;
	point_t	view;
	struct bu_vls	str;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help view2model");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET(view, atof(argv[1]),
		atof(argv[2]),
		atof(argv[3]) );

	MAT4X3PNT( model, view2model, view );

	bu_vls_init(&str);
	bu_vls_printf(&str, "%.15e %.15e %.15e\n", V3ARGS(model) );
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
}

int
f_lookat(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	point_t look;
	point_t eye;
	point_t tmp;
	point_t new_center;
	vect_t dir;
	fastf_t new_az, new_el;
	int status;
	struct bu_vls vls;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help lookat");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET( look, atof(argv[1]),
		atof(argv[2]),
		atof(argv[3]) );

	VSCALE( look, look, local2base );

	VSET( tmp, 0, 0, 1 );
	MAT4X3PNT(eye, view2model, tmp);

	VSUB2( dir, eye, look );
	VUNITIZE( dir );
	mat_ae_vec( &new_az, &new_el, dir );

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "ae %-15.10f %-15.10f %-15.10f", new_az, new_el, curr_dm_list->s_info->twist);
	status = Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	VJOIN1( new_center, eye, -Viewscale, dir );
	MAT_DELTAS_VEC_NEG( toViewcenter, new_center );

	new_mats();

	return( status );
}
