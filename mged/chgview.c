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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <signal.h>
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
#include "./cmd.h"
#include "../librt/debug.h"	/* XXX */

extern struct db_tree_state	mged_initial_tree_state;	/* dodraw.c */

extern void color_soltab();
extern void set_absolute_tran(); /* defined in set.c */
extern void set_absolute_view_tran(); /* defined in set.c */
extern void set_absolute_model_tran(); /* defined in set.c */

void solid_list_callback();
void knob_update_rate_vars();
int mged_svbase();
int knob_tran();
int mged_tran();
int mged_vrot();
int mged_zoom();
void mged_center();
static void abs_zoom();
void usejoy();

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

extern vect_t curr_e_axes_pos;  /* from edsol.c */
extern long	nvectors;	/* from dodraw.c */

extern struct bn_tol mged_tol;	/* from ged.c */
extern vect_t e_axes_pos;

fastf_t ar_scale_factor = GED_MAX / ABS_ROT_FACTOR;
fastf_t rr_scale_factor = GED_MAX / RATE_ROT_FACTOR;
fastf_t adc_angle_scale_factor = GED_MAX / ADC_ANGLE_FACTOR;

vect_t edit_absolute_model_rotate;
vect_t edit_absolute_object_rotate;
vect_t edit_absolute_view_rotate;
vect_t last_edit_absolute_model_rotate;
vect_t last_edit_absolute_object_rotate;
vect_t last_edit_absolute_view_rotate;
vect_t edit_rate_model_rotate;
vect_t edit_rate_object_rotate;
vect_t edit_rate_view_rotate;
int edit_rateflag_model_rotate;
int edit_rateflag_object_rotate;
int edit_rateflag_view_rotate;

vect_t edit_absolute_model_tran;
vect_t edit_absolute_view_tran;
vect_t last_edit_absolute_model_tran;
vect_t last_edit_absolute_view_tran;
vect_t edit_rate_model_tran;
vect_t edit_rate_view_tran;
int edit_rateflag_model_tran;
int edit_rateflag_view_tran;

fastf_t edit_absolute_scale;
fastf_t edit_rate_scale;
int edit_rateflag_scale;

char edit_rate_model_origin;
char edit_rate_object_origin;
char edit_rate_view_origin;
char edit_rate_coords;
struct dm_list *edit_rate_mr_dm_list;
struct dm_list *edit_rate_or_dm_list;
struct dm_list *edit_rate_vr_dm_list;
struct dm_list *edit_rate_mt_dm_list;
struct dm_list *edit_rate_vt_dm_list;

struct bu_vls edit_info_vls;
struct bu_vls edit_rate_model_tran_vls[3];
struct bu_vls edit_rate_view_tran_vls[3];
struct bu_vls edit_rate_model_rotate_vls[3];
struct bu_vls edit_rate_object_rotate_vls[3];
struct bu_vls edit_rate_view_rotate_vls[3];
struct bu_vls edit_rate_scale_vls;
struct bu_vls edit_absolute_model_tran_vls[3];
struct bu_vls edit_absolute_view_tran_vls[3];
struct bu_vls edit_absolute_model_rotate_vls[3];
struct bu_vls edit_absolute_object_rotate_vls[3];
struct bu_vls edit_absolute_view_rotate_vls[3];
struct bu_vls edit_absolute_scale_vls;

double		mged_abs_tol;
double		mged_rel_tol = 0.01;		/* 1%, by default */
double		mged_nrm_tol;			/* normal ang tol, radians */

void
eraseobjpath(
     Tcl_Interp	*interp,
     int	argc,
     char	**argv,
     int	noisy,
     int	all)
{
	register struct directory *dp;
	register int i;
	struct bu_vls vls;
	Tcl_Obj *save_result;

	save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);

	bu_vls_init(&vls);
	for (i = 0; i < argc; i++) {
		int j;
		char *list;
		int ac;
		char **av, **av_orig;
		struct directory **dpp;

		bu_vls_trunc(&vls, 0);
		bu_vls_printf(&vls, "split %s /", argv[i]);
		if (Tcl_Eval(interp, bu_vls_addr(&vls)) != TCL_OK) {
			continue;
		}
		list = Tcl_GetStringResult(interp);
		Tcl_SplitList(interp, list, &ac, &av_orig);

		/* skip first element if empty */
		av = av_orig;
		if (*av[0] == '\0') {
			--ac;
			++av;
		}

		/* ignore last element if empty */
		if (*av[ac-1] == '\0')
			--ac;

		dpp = bu_calloc(ac+1, sizeof(struct directory *), "eraseobjpath: directory pointers");
		for (j = 0; j < ac; ++j)
			if ((dp = db_lookup(dbip, av[j], noisy)) != DIR_NULL)
				dpp[j] = dp;
			else
				goto end;

		dpp[j] = DIR_NULL;

		if (all)
			eraseobjall(dpp);
		else
			eraseobj(dpp);

	end:
		bu_free((genptr_t)dpp, "eraseobjpath: directory pointers");
		Tcl_Free((char *)av_orig);
	}
	bu_vls_free(&vls);
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);
}

/* Delete an object or several objects from the display */
/* Format: d object1 object2 .... objectn */
int
f_erase(
	ClientData clientData,
	Tcl_Interp *interp,
	int     argc,
	char    **argv)
{
	CHECK_DBI_NULL;

	if (argc < 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help %s", argv[0]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	eraseobjpath(interp, argc-1, argv+1, LOOKUP_NOISY, 0);
	solid_list_callback();

	return TCL_OK;
}

int
f_erase_all(
	ClientData clientData,
	Tcl_Interp *interp,
	int     argc,
	char    **argv)
{
  CHECK_DBI_NULL;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  eraseobjpath(interp, argc-1, argv+1, LOOKUP_NOISY, 1);
  solid_list_callback();

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
  struct bu_vls str;
  point_t center;

  CHECK_DBI_NULL;

  if(argc != 1 && argc != 4){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help center");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(argc == 4)  {
	/* Set center.  Value is in local units */
	VSET(center, atof(argv[1]), atof(argv[2]), atof(argv[3]));

	/* must convert to base units */
	VSCALE(center, center, local2base);
	mged_center(center);
  }

  /* Whether set or not, print current center in local units */
  MAT_DELTAS_GET_NEG(center, view_state->vs_toViewcenter);
  VSCALE(center, center, base2local);

  bu_vls_init(&str);
  bu_vls_printf(&str, "%f %f %f\n", V3ARGS(center));
  Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
  bu_vls_free(&str);

  return TCL_OK;
}

void
mged_center(center)
point_t center;
{
  MAT_DELTAS_VEC_NEG(view_state->vs_toViewcenter, center);
  new_mats();

  (void)mged_svbase();
}

/* DEBUG -- force viewsize */
/* Format: view size	*/
int
f_size(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  fastf_t f;

  CHECK_DBI_NULL;

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
  view_state->vs_Viewscale = f * 0.5 * local2base;
  new_mats();

  view_state->vs_absolute_scale = 1.0 - view_state->vs_Viewscale / view_state->vs_i_Viewscale;
  if(view_state->vs_absolute_scale < 0.0)
    view_state->vs_absolute_scale /= 9.0;

  if(view_state->vs_absolute_tran[X] != 0.0 ||
     view_state->vs_absolute_tran[Y] != 0.0 ||
     view_state->vs_absolute_tran[Z] != 0.0)
    set_absolute_tran();

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

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help B");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (f_zap(clientData, interp, 1, av) == TCL_ERROR)
    return TCL_ERROR;

  return edit_com( argc, argv, 1, 1 );
}

/* Edit something (add to visible display) */
/* Format: e object	*/
int
f_edit(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help ev");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help E");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
		VSETALL( center, 0.0 );
		VSETALL( radial, 1000.0 );	/* 1 meter */
	} else {
		VADD2SCALE( center, max, min, 0.5 );
		VSUB2( radial, max, center );
	}

	if( VNEAR_ZERO( radial , SQRT_SMALL_FASTF ) )
		VSETALL( radial , 1.0 );

	bn_mat_idn( view_state->vs_toViewcenter );
	MAT_DELTAS( view_state->vs_toViewcenter, -center[X], -center[Y], -center[Z] );
	view_state->vs_Viewscale = radial[X];
	V_MAX( view_state->vs_Viewscale, radial[Y] );
	V_MAX( view_state->vs_Viewscale, radial[Z] );

	view_state->vs_i_Viewscale = view_state->vs_Viewscale;
}

/*
 *			E D I T _ C O M
 *
 * B, e, and E commands use this area as common
 */
int
edit_com(
     int	argc,
     char	**argv,
     int	kind,
     int	catch_sigint)
{
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;
	register struct cmd_list *save_cmd_list;
	double		elapsed_time;
	int		initial_blank_screen;
	struct bu_vls vls;

	CHECK_DBI_NULL;

	bu_vls_init(&vls);
	initial_blank_screen = BU_LIST_IS_EMPTY(&HeadSolid.l);

	/*  First, delete any mention of these objects.
	 *  Silently skip any leading options (which start with minus signs).
	 */
	eraseobjpath(interp, argc-1, argv+1, LOOKUP_QUIET, 0);

	update_views = 1;

	if (setjmp(jmp_env) == 0)
		(void)signal(SIGINT, sig3);	/* allow interupts */
	else {
		bu_vls_free(&vls);
		return TCL_OK;
	}

	nvectors = 0;
	rt_prep_timer();
	drawtrees(argc, argv, kind);
	(void)rt_get_timer((struct bu_vls *)0, &elapsed_time);

	save_dmlp = curr_dm_list;
	save_cmd_list = curr_cmd_list;
	FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l) {
		curr_dm_list = dmlp;
		if (curr_dm_list->dml_tie)
			curr_cmd_list = curr_dm_list->dml_tie;
		else
			curr_cmd_list = &head_cmd_list;

		/* If we went from blank screen to non-blank, resize */
		if (mged_variables->mv_autosize  && initial_blank_screen &&
		    BU_LIST_NON_EMPTY(&HeadSolid.l)) {
			struct view_ring *vrp;

			size_reset();
			new_mats();
			(void)mged_svbase();

			for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l))
				vrp->vr_scale = view_state->vs_Viewscale;
		}
	}

	color_soltab();
	curr_dm_list = save_dmlp;
	curr_cmd_list = save_cmd_list;

	bu_vls_printf(&vls, "%ld vectors in %g sec\n", nvectors, elapsed_time);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

	bu_vls_free(&vls);
	(void)signal(SIGINT, SIG_IGN);
	solid_list_callback();
	return TCL_OK;
}

int
f_autosize(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;
	register struct cmd_list *save_cmd_list;

	if (argc != 1) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help autosize");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	save_dmlp = curr_dm_list;
	save_cmd_list = curr_cmd_list;
	FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l) {
		struct view_ring *vrp;

		curr_dm_list = dmlp;
		if (curr_dm_list->dml_tie)
			curr_cmd_list = curr_dm_list->dml_tie;
		else
			curr_cmd_list = &head_cmd_list;

		size_reset();
		new_mats();
		(void)mged_svbase();

		for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l))
			vrp->vr_scale = view_state->vs_Viewscale;
	}
	curr_dm_list = save_dmlp;
	curr_cmd_list = save_cmd_list;

	return TCL_OK;
}

void
solid_list_callback()
{
  struct bu_vls vls;
  Tcl_Obj *save_obj;

  /* save result */
  save_obj = Tcl_GetObjResult(interp);
  Tcl_IncrRefCount(save_obj);

  bu_vls_init(&vls);
  bu_vls_strcpy(&vls, "solid_list_callback");
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  /* restore result */
  Tcl_SetObjResult(interp, save_obj);
  Tcl_DecrRefCount(save_obj);
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
  if( 0 <= lvl )
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

	DM_DEBUG(dmp, regdebug);

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

	if(dbip == DBI_NULL)
	  return;

	if( (id = rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource )) < 0 )  {
		Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
			") failure\n", (char *)NULL );
		return;
	}

	bu_vls_printf( outstrp, "%s:  ", dp->d_namep );

	if( rt_functab[id].ft_describe( outstrp, &intern,
	    verbose, base2local, &rt_uniresource ) < 0 )
	  Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_db_free_internal( &intern, &rt_uniresource );
}

/*
 *			C M D _ L I S T _ G U T S
 */
static void
cmd_list_guts(clientData, interp, argc, argv, recurse)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
int recurse;
{
  register struct directory *dp;
  register int arg;
  struct bu_vls str;
  char *listeval="listeval";
  struct rt_db_internal intern;

  bu_vls_init( &str );

  for (arg = 0; arg < argc; arg++) {
    if (recurse) {
      char *tmp_argv[2];

      tmp_argv[0] = listeval;
      tmp_argv[1] = argv[arg];

      f_pathsum(clientData, interp, 2, tmp_argv);
    } else if (strchr(argv[arg], '/')) {
      struct db_tree_state ts;
      struct db_full_path path;

      db_full_path_init( &path );
      ts = mged_initial_tree_state;	/* struct copy */
      ts.ts_dbip = dbip;
      ts.ts_resp = &rt_uniresource;
      bn_mat_idn(ts.ts_mat);

      if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
	continue;

      dp = DB_FULL_PATH_CUR_DIR( &path );

      if (rt_db_get_internal(&intern, dp, dbip, ts.ts_mat, &rt_uniresource) < 0) {
	Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
			 ") failure\n", (char *)NULL );
	continue;
      }

      db_free_full_path( &path );

      bu_vls_printf( &str, "%s:  ", argv[arg] );

      if (intern.idb_meth->ft_describe(&str, &intern, 99, base2local, &rt_uniresource) < 0)
	Tcl_AppendResult(interp, dp->d_namep, ": ft_describe error\n", (char *)NULL);
    	rt_db_free_internal( &intern, &rt_uniresource );
    } else {
      if ((dp = db_lookup(dbip, argv[arg], LOOKUP_NOISY)) == DIR_NULL)
	continue;

      do_list(&str, dp, 99);	/* very verbose */
    }
  }

  Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
  bu_vls_free( &str );
}

/*
 *			C M D _ L I S T
 *
 *  List object information, verbose, in GIFT-compatible format.
 *  Format: l object
 */
int
cmd_list(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int start_arg=1;
  int recurse=0;

  CHECK_DBI_NULL;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help l");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (argc > 1 && strcmp(argv[1], "-r") == 0) {
    recurse = 1;
    start_arg = 2;
  }

#if 0
  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3 );	/* allow interupts */
  else{
    bu_vls_free( &str );
    return TCL_OK;
  }
#endif

  /* 
   * Here we have no usable arguments,
   * so we better be in an edit state.
   */
  if (argc == 1 ||
      (argc == 2 && recurse)) {
    int ac = 1;
    char *av[2];

    if (illump != SOLID_NULL) {
      struct bu_vls vls;

      bu_vls_init(&vls);

      if (state == ST_S_EDIT)
    	db_path_to_vls( &vls, &illump->s_fullpath );
      else if (state == ST_O_EDIT)  {
      	int i;
      	for( i=0; i < ipathpos; i++ )  {
      		bu_vls_printf(&vls, "/%s",
			DB_FULL_PATH_GET(&illump->s_fullpath,i)->d_namep );
      	}
      } else
	return TCL_ERROR;

    	db_path_to_vls( &vls, &illump->s_fullpath );

      av[0] = bu_vls_addr(&vls);
      av[1] = (char *)NULL;
      cmd_list_guts(clientData, interp, ac, av, recurse);

      bu_vls_free(&vls);
    } else {  /* not in an edit state */
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help l");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }
  } else { /* Here we have usable arguments. */
    argc -= start_arg;
    argv += start_arg;
    cmd_list_guts(clientData, interp, argc, argv, recurse);
  }

#if 0
  (void)signal(SIGINT, SIG_IGN);
#endif
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

  CHECK_DBI_NULL;

  if(argc < 2){
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
 *  To return all the free "struct bn_vlist" and "struct solid" items
 *  lurking on their respective freelists, back to bu_malloc().
 *  Primarily as an aid to tracking memory leaks.
 *  WARNING:  This depends on knowledge of the macro GET_SOLID in mged/solid.h
 *  and RT_GET_VLIST in h/raytrace.h.
 */
void
mged_freemem()
{
  register struct solid		*sp;
  register struct bn_vlist	*vp;

  FOR_ALL_SOLIDS(sp,&FreeSolid.l){
    GET_SOLID(sp,&FreeSolid.l);
    bu_free((genptr_t)sp, "mged_freemem: struct solid");
  }

  while( BU_LIST_NON_EMPTY( &rt_g.rtg_vlfree ) )  {
    vp = BU_LIST_FIRST( bn_vlist, &rt_g.rtg_vlfree );
    BU_LIST_DEQUEUE( &(vp->l) );
    bu_free( (genptr_t)vp, "mged_freemem: struct bn_vlist" );
  }
}

/* ZAP the display -- everything dropped */
/* Format: Z	*/
int
f_zap(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register struct solid *sp;
	register struct solid *nsp;
	struct directory	*dp;

	CHECK_DBI_NULL;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help Z");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	update_views = 1;

	/* FIRST, reject any editing in progress */
	if( state != ST_VIEW )
		button( BE_REJECT );

#ifdef DO_DISPLAY_LISTS
	freeDListsAll(BU_LIST_FIRST(solid, &HeadSolid.l)->s_dlist,
		      BU_LIST_LAST(solid, &HeadSolid.l)->s_dlist -
		      BU_LIST_FIRST(solid, &HeadSolid.l)->s_dlist + 1);
#endif

	sp = BU_LIST_NEXT(solid, &HeadSolid.l);
	while(BU_LIST_NOT_HEAD(sp, &HeadSolid.l)){
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
			if( db_dirdelete( dbip, dp ) < 0 )  {
			  Tcl_AppendResult(interp, "f_zap: db_dirdelete failed\n", (char *)NULL);
			}
		}

		nsp = BU_LIST_PNEXT(solid, sp);
		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp,&FreeSolid.l);
		sp = nsp;
	}


	/* Keeping freelists improves performance.  When debugging, give mem back */
	if( rt_g.debug )  mged_freemem();

	(void)chg_state( state, state, "zap" );

	solid_list_callback();
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

  CHECK_DBI_NULL;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help status");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(argc == 1) {
    bu_vls_init(&vls);
    start_catching_output(&vls);
		   
    bu_log("STATE=%s, ", state_str[state] );
    bu_log("Viewscale=%f (%f mm)\n", view_state->vs_Viewscale*base2local, view_state->vs_Viewscale);
    bu_log("base2local=%f\n", base2local);
    bn_mat_print("toViewcenter", view_state->vs_toViewcenter);
    bn_mat_print("Viewrot", view_state->vs_Viewrot);
    bn_mat_print("model2view", view_state->vs_model2view);
    bn_mat_print("view2model", view_state->vs_view2model);

    if( state != ST_VIEW )  {
      bn_mat_print("model2objview", view_state->vs_model2objview);
      bn_mat_print("objview2model", view_state->vs_objview2model);
    }

    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "state")){
    Tcl_AppendResult(interp, state_str[state], (char *)NULL);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "Viewscale")){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%f", view_state->vs_Viewscale*base2local);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "base2local")){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%f", base2local);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "local2base")){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%f", local2base);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "toViewcenter")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("toViewcenter", view_state->vs_toViewcenter);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "Viewrot")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("Viewrot", view_state->vs_Viewrot);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "model2view")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("model2view", view_state->vs_model2view);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "view2model")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("view2model", view_state->vs_view2model);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "model2objview")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("model2objview", view_state->vs_model2objview);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(!strcmp(argv[1], "objview2model")){
    bu_vls_init(&vls);
    start_catching_output(&vls);
    bn_mat_print("objview2model", view_state->vs_objview2model);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "help status");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  if(!strcmp(argv[1], "help"))
    return TCL_OK;

  return TCL_ERROR;
}

int
f_view(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int n;
  point_t pt;
  mat_t mat;
  struct bu_vls vls;

  if(argc < 1 || 6 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help view");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if(argc == 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help view");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
  }

  if(!strcmp(argv[1], "quat")){
    quat_t quat;

    /* return Viewrot as a quaternion */
    if(argc == 2){
      quat_mat2quat(quat, view_state->vs_Viewrot);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g %.12g %.12g %.12g", V4ARGS(quat));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    if(argc != 6){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: quat requires four parameters");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* attempt to set Viewrot given a quaternion */
    n = sscanf(argv[2], "%lf", quat);
    n += sscanf(argv[3], "%lf", quat+1);
    n += sscanf(argv[4], "%lf", quat+2);
    n += sscanf(argv[5], "%lf", quat+3);

    if(n < 4){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view quat: bad value detected - %s %s %s %s",
		    argv[2], argv[3], argv[4], argv[5]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    quat_quat2mat(view_state->vs_Viewrot, quat);
    new_mats();

    return TCL_OK;
  }

  if(!strcmp(argv[1], "ypr")){
    vect_t ypr;

    /* return Viewrot as yaw, pitch and roll */
    if(argc == 2){
      bn_mat_trn(mat, view_state->vs_Viewrot);
      anim_v_unpermute(mat);
      n = anim_mat2ypr(pt, mat);
      if(n == 2){
	Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
	return TCL_ERROR;
      }
      VSCALE(pt, pt, bn_radtodeg);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g %.12g %.12g", V3ARGS(pt));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    if(argc != 5){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: ypr requires 3 parameters");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* attempt to set Viewrot given yaw, pitch and roll */
    n = sscanf(argv[2], "%lf", ypr);
    n += sscanf(argv[3], "%lf", ypr+1);
    n += sscanf(argv[4], "%lf", ypr+2);

    if(n < 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view ypr: bad value detected - %s %s %s",
		    argv[2], argv[3], argv[4]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    anim_dy_p_r2mat(mat, V3ARGS(ypr));
    anim_v_permute(mat);
    bn_mat_trn(view_state->vs_Viewrot, mat);
    new_mats();

    return TCL_OK;
  }

  if(!strcmp(argv[1], "aet")){
    vect_t aet;

    /* return Viewrot as azimuth, elevation and twist */
    if(argc == 2){
      bn_mat_trn(mat, view_state->vs_Viewrot);
      anim_v_unpermute(mat);
      n = anim_mat2ypr(pt, mat);
      if(n == 2){
	Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
	return TCL_ERROR;
      }
      VSCALE(pt, pt, bn_radtodeg);
      if(pt[0] >= 180.0 )
	pt[0] -= 180;
      if(pt[0] < 180.0 )
	pt[0] += 180;
      pt[1] = -pt[1];
      pt[2] = -pt[2];

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g %.12g %.12g", V3ARGS(pt));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    if(argc != 5){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: aet requires 3 parameters");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* attempt to set Viewrot given azimuth, elevation and twist */
    n = sscanf(argv[2], "%lf", aet);
    n += sscanf(argv[3], "%lf", aet+1);
    n += sscanf(argv[4], "%lf", aet+2);

    if(n < 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view aet: bad value detected - %s %s %s",
		    argv[2], argv[3], argv[4]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    anim_dy_p_r2mat(mat, aet[0]+180.0, -aet[1], -aet[2]);
    anim_v_permute(mat);
    bn_mat_trn(view_state->vs_Viewrot, mat);
    new_mats();

    return TCL_OK;
  }

  if(!strcmp(argv[1], "center")){
    point_t center;

    /* return view center */
    if(argc == 2){
      MAT_DELTAS_GET_NEG(center, view_state->vs_toViewcenter);
      VSCALE(center, center, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g %.12g %.12g", V3ARGS(center));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
      return TCL_OK;
    }

    if(argc != 5){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: center requires 3 parameters");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* attempt to set the view center */
    n = sscanf(argv[2], "%lf", center);
    n += sscanf(argv[3], "%lf", center+1);
    n += sscanf(argv[4], "%lf", center+2);

    if(n < 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view center: bad value detected - %s %s %s",
		    argv[2], argv[3], argv[4]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    VSCALE(center, center, local2base);
    MAT_DELTAS_VEC_NEG(view_state->vs_toViewcenter, center);
    new_mats();

    return TCL_OK;
  }

  if(!strcmp(argv[1], "eye")){
    point_t eye;
    vect_t dir;

    /* return the eye point */
    if(argc == 2){
      VSET(pt, 0.0, 0.0, 1.0);
      MAT4X3PNT(eye, view_state->vs_view2model, pt);
      VSCALE(eye, eye, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g %.12g %.12g", V3ARGS(eye));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    if(argc != 5){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: eye requires 3 parameters");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* attempt to set view center given the eye point */
    n = sscanf(argv[2], "%lf", eye);
    n += sscanf(argv[3], "%lf", eye+1);
    n += sscanf(argv[4], "%lf", eye+2);

    if(n < 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view eye: bad value detected - %s %s %s",
		    argv[2], argv[3], argv[4]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    VSCALE(eye, eye, local2base);
    VSET(pt, 0.0, 0.0, view_state->vs_Viewscale);
    bn_mat_trn(mat, view_state->vs_Viewrot);
    MAT4X3PNT(dir, mat, pt);
    VSUB2(pt, dir, eye);
    MAT_DELTAS_VEC(view_state->vs_toViewcenter, pt);
    new_mats();

    return TCL_OK;
  }

  if(!strcmp(argv[1], "size")){
    fastf_t size;

    /* return the view size */
    if(argc == 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.12g", view_state->vs_Viewscale * 2.0 * base2local);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    if(argc != 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view: size requires 1 parameter");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* attempt to set view size */
    n = sscanf(argv[2], "%lf", &size);

    if(n < 1){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "view size: bad value detected - %s", argv[2]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    size *= local2base;
    if(size < 0.0001)
      size = 0.0001;
    view_state->vs_Viewscale = size * 0.5;
    new_mats();

    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "help view");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
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

  view_state->vs_flag = 1;		/* causes refresh() */
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
  struct bu_vls vls;

  /* grab old twist angle before it's lost */
  o_twist = view_state->vs_twist;
  o_arz = view_state->vs_absolute_rotate[Z];
  o_larz = view_state->vs_last_absolute_rotate[Z];

  /* set view using azimuth and elevation angles */
  if(iflag)
    setview( 270.0 + elev + view_state->vs_elevation,
	     0.0,
	     270.0 - azim - view_state->vs_azimuth);
  else
    setview( 270.0 + elev, 0.0, 270.0 - azim );

  bu_vls_init(&vls);

  if(iflag)
    bu_vls_printf(&vls, "knob -i -v az %f", -o_twist - twist);
  else
    bu_vls_printf(&vls, "knob -i -v az %f", -twist);

  status = Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  view_state->vs_absolute_rotate[Z] = o_arz;
  view_state->vs_last_absolute_rotate[Z] = o_larz;

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
  fastf_t twist = 0.0;  /* assumed to be 0.0 ---- unless supplied by user */

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
  }

  if(argc == 4) /* twist angle supplied */
    twist = atof(argv[3]);

  return mged_aetview(iflag, atof(argv[1]), atof(argv[2]), twist);
}


/*
 *			E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object anywhere in their 'path'
 */
void
eraseobjall(dpp)
     register struct directory **dpp;	/* this is a partial path spec. XXX should be db_full_path? */
{
	register struct directory **tmp_dpp;
	register struct solid *sp;
	register struct solid *nsp;
	struct db_full_path	subpath;

	if (dbip == DBI_NULL)
		return;

	update_views = 1;

	db_full_path_init(&subpath);
	for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
		RT_CK_DIR(*tmp_dpp);
		db_add_node_to_full_path(&subpath, *tmp_dpp);
	}

	sp = BU_LIST_NEXT(solid, &HeadSolid.l);
	while (BU_LIST_NOT_HEAD(sp, &HeadSolid.l)) {
		nsp = BU_LIST_PNEXT(solid, sp);

		if( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {
#ifdef DO_DISPLAY_LISTS
			freeDListsAll(sp->s_dlist, 1);
#endif

			if (state != ST_VIEW && illump == sp)
				button(BE_REJECT);

			BU_LIST_DEQUEUE(&sp->l);
			FREE_SOLID(sp, &FreeSolid.l);
		}
		sp = nsp;
	}

	if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR) {
		if (db_dirdelete(dbip, *dpp) < 0) {
			Tcl_AppendResult(interp, "eraseobjall: db_dirdelete failed\n", (char *)NULL);
		}
	}

	db_free_full_path(&subpath);
}


/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object at the
 * beginning of their 'path'
 */
void
eraseobj(dpp)
     register struct directory **dpp;	/* this is a partial path spec. XXX should be db_full_path? */
{
	register struct directory **tmp_dpp;
	register struct solid *sp;
	register struct solid *nsp;
	struct db_full_path	subpath;

	if (dbip == DBI_NULL)
		return;

	if (*dpp == DIR_NULL)
		return;

	update_views = 1;

	db_full_path_init(&subpath);
	for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
		RT_CK_DIR(*tmp_dpp);
		db_add_node_to_full_path(&subpath, *tmp_dpp);
	}

	sp = BU_LIST_FIRST(solid, &HeadSolid.l);
	while (BU_LIST_NOT_HEAD(sp, &HeadSolid.l)) {
		nsp = BU_LIST_PNEXT(solid, sp);

		if( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {

#ifdef DO_DISPLAY_LISTS
			freeDListsAll(sp->s_dlist, 1);
#endif

			if (state != ST_VIEW && illump == sp)
				button( BE_REJECT );

			BU_LIST_DEQUEUE(&sp->l);
			FREE_SOLID(sp, &FreeSolid.l);
		}
		sp = nsp;
	}

	if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR ) {
		if (db_dirdelete(dbip, *dpp) < 0) {
			Tcl_AppendResult(interp, "eraseobj: db_dirdelete failed\n", (char *)NULL);
		}
	}

	db_free_full_path(&subpath);
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
  register struct bn_vlist	*vp;
  int			nvlist;
  int			npts;
  struct bu_vls vls;

  if(dbip == DBI_NULL)
    return;

  bu_vls_init(&vls);

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);
  else{
    bu_vls_free(&vls);
    return;
  }

  FOR_ALL_SOLIDS(sp, &startp->l){
    if (lvl <= -2) {
      /* print only leaves */
      bu_vls_printf(&vls, "%s ", LAST_SOLID(sp)->d_namep );
      continue;
    }

    if( lvl != -1 )
	bu_vls_printf(&vls, "%s", sp->s_flag == UP ? "VIEW " : "-no- ");
    db_path_to_vls(&vls, &sp->s_fullpath);
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
    bu_vls_printf(&vls, "  basecolor=(%d,%d,%d) color=(%d,%d,%d)%s%s%s\n",
		  sp->s_basecolor[0],
		  sp->s_basecolor[1],
		  sp->s_basecolor[2],
		  sp->s_color[0],
		  sp->s_color[1],
		  sp->s_color[2],
  		  sp->s_uflag?" U":"",
  		  sp->s_dflag?" D":"",
  		  sp->s_cflag?" C":"");

    if( lvl <= 1 )  continue;

    /* Print the actual vector list */
    nvlist = 0;
    npts = 0;
    for( BU_LIST_FOR( vp, bn_vlist, &(sp->s_vlist) ) )  {
      register int	i;
      register int	nused = vp->nused;
      register int	*cmd = vp->cmd;
      register point_t *pt = vp->pt;

      BN_CK_VLIST( vp );
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
	int	c;
	int	ri = 0;
	int	nm_pieces;
	int	illum_only = 0;
	char	**path_piece = 0;
	char	*basename;
	char	*sname;

	CHECK_DBI_NULL;

#if 0
	if(argc < 2 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help ill");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if(!strcmp("-n", argv[1])){
	  illum_only = 1;
	  --argc;
	  ++argv;
	}
#else
	if (argc < 2 || 5 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help ill");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_optind = 1;
	while ((c = bu_getopt(argc, argv, "i:n")) != EOF) {
		switch (c) {
		case 'n':
			illum_only = 1;
			break;
		case 'i':
			sscanf(bu_optarg, "%d", &ri);
			if (ri <= 0) {
				Tcl_AppendResult(interp,
						 "the reference index must be greater than 0\n",
						 (char *)NULL);
				return TCL_ERROR;
			}

			break;
		default:
		case 'h':
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "help ill");
				Tcl_Eval(interp, bu_vls_addr(&vls));
				bu_vls_free(&vls);
				return TCL_ERROR;
			}
		}
	}

	argc -= (bu_optind - 1);
	argv += (bu_optind - 1);
#endif

	if(argc != 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help ill");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if(state != ST_S_PICK && state != ST_O_PICK){
	  state_err("keyboard illuminate pick");
	  goto bail_out;
	}

	path_piece = path_parse(argv[1]);
	for (nm_pieces = 0; path_piece[nm_pieces] != 0; ++nm_pieces)
	  ;

	if(nm_pieces == 0){
	  Tcl_AppendResult(interp, "Bad solid path: '", argv[1], "'\n", (char *)NULL);
	  goto bail_out;
	}

	basename = path_piece[nm_pieces - 1];

	if( (dp = db_lookup( dbip,  basename, LOOKUP_NOISY )) == DIR_NULL )
		goto bail_out;

	nmatch = 0;
	if(!(dp -> d_flags & DIR_SOLID)){
	  Tcl_AppendResult(interp, basename, " is not a solid\n", (char *)NULL);
	  goto bail_out;
	}

	FOR_ALL_SOLIDS(sp, &HeadSolid.l){
	  int	a_new_match;

/* XXX Could this make use of db_full_path_subset()? */
	  if (nmatch == 0 || nmatch != ri) {
		  i = sp -> s_fullpath.fp_len-1;
		  if (DB_FULL_PATH_GET(&sp->s_fullpath,i) == dp) {
			  a_new_match = 1;
			  j = nm_pieces - 1;
			  for (; a_new_match && (i >= 0) && (j >= 0); --i, --j) {
				  sname = DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_namep;
				  if ((*sname != *(path_piece[j]))
				      || strcmp(sname, path_piece[j]))
					  a_new_match = 0;
			  }

			  if (a_new_match && ((i >= 0) || (j < 0))) {
				  lastfound = sp;
				  ++nmatch;
			  }
		  }
	  }

	  sp->s_iflag = DOWN;
	}

	if (nmatch == 0) {
	  Tcl_AppendResult(interp, argv[1], " not being displayed\n", (char *)NULL);
	  goto bail_out;
	}

	/* preserve same old behavior */
	if (nmatch > 1 && ri == 0) {
		Tcl_AppendResult(interp, argv[1], " multiply referenced\n", (char *)NULL);
		goto bail_out;
	} else if (ri != 0 && ri != nmatch) {
		Tcl_AppendResult(interp,
				 "the reference index must be less than the number of references\n",
				 (char *)NULL);
		goto bail_out;
	}

	/* Make the specified solid the illuminated solid */
	illump = lastfound;
	illump->s_iflag = UP;

	if(!illum_only){
	  if( state == ST_O_PICK )  {
	    ipathpos = 0;
	    (void)chg_state( ST_O_PICK, ST_O_PATH, "Keyboard illuminate");
	  } else {
	    /* Check details, Init menu, set state=ST_S_EDIT */
	    init_sedit();
	  }
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
    if(state != ST_VIEW){
      struct bu_vls vls;

      bu_vls_init(&vls);

      bu_vls_printf(&vls, "%s", interp->result);
      button(BE_REJECT);
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

      bu_vls_free(&vls);
    }

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
f_sed(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
  CHECK_DBI_NULL;
  CHECK_READ_ONLY;

#if 0
  if(argc < 2 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help sed");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }
#else
  if (argc < 2 || 5 < argc) {
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help sed");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
  }
#endif

  if( not_state( ST_VIEW, "keyboard solid edit start") )
    return TCL_ERROR;
  if(BU_LIST_IS_EMPTY(&HeadSolid.l)){
    Tcl_AppendResult(interp, "no solids being displayed\n",  (char *)NULL);
    return TCL_ERROR;
  }

  update_views = 1;

  button(BE_S_ILLUMINATE);	/* To ST_S_PICK */

  argv[0] = "ill";

  /* Illuminate named solid --> ST_S_EDIT */
  if (f_ill(clientData, interp, argc, argv) == TCL_ERROR) {
	  Tcl_Obj *save_result;

	  save_result = Tcl_GetObjResult(interp);
	  Tcl_IncrRefCount(save_result);
	  button(BE_REJECT);
	  Tcl_SetObjResult(interp, save_result);
	  Tcl_DecrRefCount(save_result);
	  return TCL_ERROR;
  }

  return TCL_OK;
}

void
check_nonzero_rates()
{
  if( view_state->vs_rate_model_rotate[X] != 0.0 ||
      view_state->vs_rate_model_rotate[Y] != 0.0 ||
      view_state->vs_rate_model_rotate[Z] != 0.0 )
    view_state->vs_rateflag_model_rotate = 1;
  else
    view_state->vs_rateflag_model_rotate = 0;

  if( view_state->vs_rate_model_tran[X] != 0.0 ||
      view_state->vs_rate_model_tran[Y] != 0.0 ||
      view_state->vs_rate_model_tran[Z] != 0.0 )
    view_state->vs_rateflag_model_tran = 1;
  else
    view_state->vs_rateflag_model_tran = 0;

  if( view_state->vs_rate_rotate[X] != 0.0 ||
      view_state->vs_rate_rotate[Y] != 0.0 ||
      view_state->vs_rate_rotate[Z] != 0.0 )
    view_state->vs_rateflag_rotate = 1;
  else
    view_state->vs_rateflag_rotate = 0;

  if( view_state->vs_rate_tran[X] != 0.0 ||
      view_state->vs_rate_tran[Y] != 0.0 ||
      view_state->vs_rate_tran[Z] != 0.0 )
    view_state->vs_rateflag_tran = 1;
  else
    view_state->vs_rateflag_tran = 0;

  if( view_state->vs_rate_scale != 0.0 )
    view_state->vs_rateflag_scale = 1;
  else
    view_state->vs_rateflag_scale = 0;

  if( edit_rate_model_tran[X] != 0.0 ||
      edit_rate_model_tran[Y] != 0.0 ||
      edit_rate_model_tran[Z] != 0.0 )
    edit_rateflag_model_tran = 1;
  else
    edit_rateflag_model_tran = 0;

  if( edit_rate_view_tran[X] != 0.0 ||
      edit_rate_view_tran[Y] != 0.0 ||
      edit_rate_view_tran[Z] != 0.0 )
    edit_rateflag_view_tran = 1;
  else
    edit_rateflag_view_tran = 0;

  if( edit_rate_model_rotate[X] != 0.0 ||
      edit_rate_model_rotate[Y] != 0.0 ||
      edit_rate_model_rotate[Z] != 0.0 )
    edit_rateflag_model_rotate = 1;
  else
    edit_rateflag_model_rotate = 0;

  if( edit_rate_object_rotate[X] != 0.0 ||
      edit_rate_object_rotate[Y] != 0.0 ||
      edit_rate_object_rotate[Z] != 0.0 )
    edit_rateflag_object_rotate = 1;
  else
    edit_rateflag_object_rotate = 0;

  if( edit_rate_view_rotate[X] != 0.0 ||
      edit_rate_view_rotate[Y] != 0.0 ||
      edit_rate_view_rotate[Z] != 0.0 )
    edit_rateflag_view_rotate = 1;
  else
    edit_rateflag_view_rotate = 0;

  if( edit_rate_scale )
    edit_rateflag_scale = 1;
  else
    edit_rateflag_scale = 0;

  view_state->vs_flag = 1;	/* values changed so update faceplate */
}

void
knob_update_rate_vars()
{
  if(!mged_variables->mv_rateknobs)
    return;
}

int
mged_print_knobvals(interp)
Tcl_Interp *interp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);

  if(mged_variables->mv_rateknobs){
    if(es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e'){
      bu_vls_printf(&vls, "x = %f\n", edit_rate_model_rotate[X]);
      bu_vls_printf(&vls, "y = %f\n", edit_rate_model_rotate[Y]);
      bu_vls_printf(&vls, "z = %f\n", edit_rate_model_rotate[Z]);
    }else{
      bu_vls_printf(&vls, "x = %f\n", view_state->vs_rate_rotate[X]);
      bu_vls_printf(&vls, "y = %f\n", view_state->vs_rate_rotate[Y]);
      bu_vls_printf(&vls, "z = %f\n", view_state->vs_rate_rotate[Z]);
    }

    if(es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e')
      bu_vls_printf(&vls, "S = %f\n", edit_rate_scale);
    else
      bu_vls_printf(&vls, "S = %f\n", view_state->vs_rate_scale);

    if(es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e'){
      bu_vls_printf(&vls, "X = %f\n", edit_rate_model_tran[X]);
      bu_vls_printf(&vls, "Y = %f\n", edit_rate_model_tran[Y]);
      bu_vls_printf(&vls, "Z = %f\n", edit_rate_model_tran[Z]);
    }else{
      bu_vls_printf(&vls, "X = %f\n", view_state->vs_rate_tran[X]);
      bu_vls_printf(&vls, "Y = %f\n", view_state->vs_rate_tran[Y]);
      bu_vls_printf(&vls, "Z = %f\n", view_state->vs_rate_tran[Z]);
    }
  }else{
    if(es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e'){
      bu_vls_printf(&vls, "ax = %f\n", edit_absolute_model_rotate[X]);
      bu_vls_printf(&vls, "ay = %f\n", edit_absolute_model_rotate[Y]);
      bu_vls_printf(&vls, "az = %f\n", edit_absolute_model_rotate[Z]);
    }else{
      bu_vls_printf(&vls, "ax = %f\n", view_state->vs_absolute_rotate[X]);
      bu_vls_printf(&vls, "ay = %f\n", view_state->vs_absolute_rotate[Y]);
      bu_vls_printf(&vls, "az = %f\n", view_state->vs_absolute_rotate[Z]);
    }

    if(es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e')
      bu_vls_printf(&vls, "aS = %f\n", edit_absolute_scale);
    else
      bu_vls_printf(&vls, "aS = %f\n", view_state->vs_absolute_scale);

    if(es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e'){
      bu_vls_printf(&vls, "aX = %f\n", edit_absolute_model_tran[X]);
      bu_vls_printf(&vls, "aY = %f\n", edit_absolute_model_tran[Y]);
      bu_vls_printf(&vls, "aZ = %f\n", edit_absolute_model_tran[Z]);
    }else{
      bu_vls_printf(&vls, "aX = %f\n", view_state->vs_absolute_tran[X]);
      bu_vls_printf(&vls, "aY = %f\n", view_state->vs_absolute_tran[Y]);
      bu_vls_printf(&vls, "aZ = %f\n", view_state->vs_absolute_tran[Z]);
    }
  }

  if(adc_state->adc_draw){
    bu_vls_printf(&vls, "xadc = %d\n", adc_state->adc_dv_x);
    bu_vls_printf(&vls, "yadc = %d\n", adc_state->adc_dv_y);
    bu_vls_printf(&vls, "ang1 = %d\n", adc_state->adc_dv_a1);
    bu_vls_printf(&vls, "ang2 = %d\n", adc_state->adc_dv_a2);
    bu_vls_printf(&vls, "distadc = %d\n", adc_state->adc_dv_dist);
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
  fastf_t sf;
  vect_t tvec;
  vect_t rvec;
  char	*cmd;
  char origin = '\0';
  int do_tran = 0;
  int do_rot = 0;
  int incr_flag = 0;  /* interpret values as increments */
  int view_flag = 0;  /* manipulate view using view coords */
  int model_flag = 0; /* manipulate view using model coords */
  int edit_flag = 0;  /* force edit interpretation */

  CHECK_DBI_NULL;

  if(argc < 1){
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
    while((c = bu_getopt(argc, argv, "eimo:v")) != EOF){
      switch(c){
      case 'e':
	edit_flag = 1;
	break;
      case 'i':
	incr_flag = 1;
	break;
      case 'm':
	model_flag = 1;
	break;
      case 'o':
	origin = *bu_optarg;
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

  if(origin != 'v' && origin != 'm' && origin != 'e' && origin != 'k')
    origin = mged_variables->mv_rotate_about;

  /* print the current values */
  if(argc == 1)
    return mged_print_knobvals(interp);

  VSETALL(tvec, 0.0);
  VSETALL(rvec, 0.0);

  for(--argc, ++argv; argc; --argc, ++argv){
    cmd = *argv;
    
    if( strcmp( cmd, "zap" ) == 0 || strcmp( cmd, "zero" ) == 0 )  {
      char *av[3];

      VSETALL( view_state->vs_rate_model_rotate, 0.0 );
      VSETALL( view_state->vs_rate_model_tran, 0.0 );
      VSETALL( view_state->vs_rate_rotate, 0.0 );
      VSETALL( view_state->vs_rate_tran, 0.0 );
      view_state->vs_rate_scale = 0.0;
      VSETALL( edit_rate_model_rotate, 0.0 );
      VSETALL( edit_rate_object_rotate, 0.0 );
      VSETALL( edit_rate_view_rotate, 0.0 );
      VSETALL( edit_rate_model_tran, 0.0 );
      VSETALL( edit_rate_view_tran, 0.0 );
      edit_rate_scale = 0.0;
      knob_update_rate_vars();

      av[0] = "adc";
      av[1] = "reset";
      av[2] = (char *)NULL;

      (void)f_adc( clientData, interp, 2, av );

      (void)mged_svbase();
    } else if( strcmp( cmd, "calibrate" ) == 0 ) {
      VSETALL( view_state->vs_absolute_tran, 0.0 );
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
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[X] += f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[X] += f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[X] += f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[X] += f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[X] += f;
	      view_state->vs_rate_origin = origin;
	    }

	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[X] = f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[X] = f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[X] = f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[X] = f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[X] = f;
	      view_state->vs_rate_origin = origin;
	    }

	  }
	}

	break;
      case 'y':
	if(incr_flag){
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[Y] += f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[Y] += f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[Y] += f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[Y] += f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[Y] += f;
	      view_state->vs_rate_origin = origin;
	    }
	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[Y] = f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[Y] = f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[Y] = f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[Y] = f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[Y] = f;
	      view_state->vs_rate_origin = origin;
	    }
	  }
	}

	break;
      case 'z':
	if(incr_flag){
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[Z] += f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[Z] += f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[Z] += f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[Z] += f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[Z] += f;
	      view_state->vs_rate_origin = origin;
	    }
	  }
	}else{
	  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			      !view_flag && !model_flag) || edit_flag)){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      edit_rate_model_rotate[Z] = f;
	      edit_rate_model_origin = origin;
	      edit_rate_mr_dm_list = curr_dm_list;

	      break;
	    case 'o':
	      edit_rate_object_rotate[Z] = f;
	      edit_rate_object_origin = origin;
	      edit_rate_or_dm_list = curr_dm_list;

	      break;
	    case 'v':
	    default:
	      edit_rate_view_rotate[Z] = f;
	      edit_rate_view_origin = origin;
	      edit_rate_vr_dm_list = curr_dm_list;

	      break;
	    }
	  }else{
	    if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	      view_state->vs_rate_model_rotate[Z] = f;
	      view_state->vs_rate_model_origin = origin;
	    }else{
	      view_state->vs_rate_rotate[Z] = f;
	      view_state->vs_rate_origin = origin;
	    }
	  }
	}

      break;
    case 'X':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[X] += f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[X] += f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[X] += f;
	  else
	    view_state->vs_rate_tran[X] += f;
	}
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[X] = f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[X] = f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[X] = f;
	  else
	    view_state->vs_rate_tran[X] = f;
	}
      }

      break;
    case 'Y':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[Y] += f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[Y] += f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[Y] += f;
	  else
	    view_state->vs_rate_tran[Y] += f;
	}
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){	
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[Y] = f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[Y] = f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[Y] = f;
	  else
	    view_state->vs_rate_tran[Y] = f;
	}
      }

      break;
    case 'Z':
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[Z] += f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[Z] += f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[Z] += f;
	  else
	    view_state->vs_rate_tran[Z] += f;
	}
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_rate_model_tran[Z] = f;
	    edit_rate_mt_dm_list = curr_dm_list;

	    break;
	  case 'v':
	  default:
	    edit_rate_view_tran[Z] = f;
	    edit_rate_vt_dm_list = curr_dm_list;

	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
	    view_state->vs_rate_model_tran[Z] = f;
	  else
	    view_state->vs_rate_tran[Z] = f;
	}
      }

      break;
    case 'S':
      if(incr_flag){
	if(EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)){
	  edit_rate_scale += f;
	}else{
	  view_state->vs_rate_scale += f;
	}
      }else{
	if(EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)){
	  edit_rate_scale = f;
	}else{
	  view_state->vs_rate_scale = f;
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
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    edit_absolute_model_rotate[X] += f;
	    break;
	  case 'o':
	    edit_absolute_object_rotate[X] += f;
	    break;
	  case 'v':
	    edit_absolute_view_rotate[X] += f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    view_state->vs_absolute_model_rotate[X] += f;
	  }else{
	    view_state->vs_absolute_rotate[X] += f;
	  }
	}

	rvec[X] = f;
      }else{
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    rvec[X] = f - last_edit_absolute_model_rotate[X];
	    edit_absolute_model_rotate[X] = f;
	    break;
	  case 'o':
	    rvec[X] = f - last_edit_absolute_object_rotate[X];
	    edit_absolute_object_rotate[X] = f;
	    break;
	  case 'v':
	    rvec[X] = f - last_edit_absolute_view_rotate[X];
	    edit_absolute_view_rotate[X] = f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    rvec[X] = f - view_state->vs_last_absolute_model_rotate[X];
	    view_state->vs_absolute_model_rotate[X] = f;
	  }else{
	    rvec[X] = f - view_state->vs_last_absolute_rotate[X];
	    view_state->vs_absolute_rotate[X] = f;
	  }
	}
      }

      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	fastf_t *arp;
	fastf_t *larp;

	switch(mged_variables->mv_coords){
	case 'm':
	  arp = edit_absolute_model_rotate;
	  larp = last_edit_absolute_model_rotate;
	  break;
	case 'o':
	  arp = edit_absolute_object_rotate;
	  larp = last_edit_absolute_object_rotate;
	  break;
	case 'v':
	  arp = edit_absolute_view_rotate;
	  larp = last_edit_absolute_view_rotate;
	  break;
	default:
		bu_log("unknown mv_coords\n");
		arp = larp = NULL;
	}

	if(arp[X] < -180.0)
	  arp[X] = arp[X] + 360.0;
	else if(arp[X] > 180.0)
	  arp[X] = arp[X] - 360.0;

	larp[X] = arp[X];
      }else{
	fastf_t *arp;
	fastf_t *larp;

	if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  arp = view_state->vs_absolute_model_rotate;
	  larp = view_state->vs_last_absolute_model_rotate;
	}else{
	  arp = view_state->vs_absolute_rotate;
	  larp = view_state->vs_last_absolute_rotate;
	}

	if(arp[X] < -180.0)
	  arp[X] = arp[X] + 360.0;
	else if(arp[X] > 180.0)
	  arp[X] = arp[X] - 360.0;

	larp[X] = arp[X];
      }

      do_rot = 1;
      break;
    case 'y':
      if(incr_flag){
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    edit_absolute_model_rotate[Y] += f;
	    break;
	  case 'o':
	    edit_absolute_object_rotate[Y] += f;
	    break;
	  case 'v':
	    edit_absolute_view_rotate[Y] += f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    view_state->vs_absolute_model_rotate[Y] += f;
	  }else{
	    view_state->vs_absolute_rotate[Y] += f;
	  }
	}

	rvec[Y] = f;
      }else{
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    rvec[Y] = f - last_edit_absolute_model_rotate[Y];
	    edit_absolute_model_rotate[Y] = f;
	    break;
	  case 'o':
	    rvec[Y] = f - last_edit_absolute_object_rotate[Y];
	    edit_absolute_object_rotate[Y] = f;
	    break;
	  case 'v':
	    rvec[Y] = f - last_edit_absolute_view_rotate[Y];
	    edit_absolute_view_rotate[Y] = f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    rvec[Y] = f - view_state->vs_last_absolute_model_rotate[Y];
	    view_state->vs_absolute_model_rotate[Y] = f;
	  }else{
	    rvec[Y] = f - view_state->vs_last_absolute_rotate[Y];
	    view_state->vs_absolute_rotate[Y] = f;
	  }
	}
      }

      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	fastf_t *arp;
	fastf_t *larp;

	switch(mged_variables->mv_coords){
	case 'm':
	  arp = edit_absolute_model_rotate;
	  larp = last_edit_absolute_model_rotate;
	  break;
	case 'o':
	  arp = edit_absolute_object_rotate;
	  larp = last_edit_absolute_object_rotate;
	  break;
	case 'v':
	  arp = edit_absolute_view_rotate;
	  larp = last_edit_absolute_view_rotate;
	  break;
	default:
		bu_log("unknown mv_transform\n");
		arp = larp = NULL;
	}

	if(arp[Y] < -180.0)
	  arp[Y] = arp[Y] + 360.0;
	else if(arp[X] > 180.0)
	  arp[Y] = arp[Y] - 360.0;

	larp[Y] = arp[Y];
      }else{
	fastf_t *arp;
	fastf_t *larp;

	if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  arp = view_state->vs_absolute_model_rotate;
	  larp = view_state->vs_last_absolute_model_rotate;
	}else{
	  arp = view_state->vs_absolute_rotate;
	  larp = view_state->vs_last_absolute_rotate;
	}

	if(arp[Y] < -180.0)
	  arp[Y] = arp[Y] + 360.0;
	else if(arp[Y] > 180.0)
	  arp[Y] = arp[Y] - 360.0;

	larp[Y] = arp[Y];
      }

      do_rot = 1;
      break;
    case 'z':
      if(incr_flag){
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    edit_absolute_model_rotate[Z] += f;
	    break;
	  case 'o':
	    edit_absolute_object_rotate[Z] += f;
	    break;
	  case 'v':
	    edit_absolute_view_rotate[Z] += f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    view_state->vs_absolute_model_rotate[Z] += f;
	  }else{
	    view_state->vs_absolute_rotate[Z] += f;
	  }
	}

	rvec[Z] = f;
      }else{
	if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			     !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	    rvec[Z] = f - last_edit_absolute_model_rotate[Z];
	    edit_absolute_model_rotate[Z] = f;
	    break;
	  case 'o':
	    rvec[Z] = f - last_edit_absolute_object_rotate[Z];
	    edit_absolute_object_rotate[Z] = f;
	    break;
	  case 'v':
	    rvec[Z] = f - last_edit_absolute_view_rotate[Z];
	    edit_absolute_view_rotate[Z] = f;
	    break;
	  }
	}else{
	  if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	    rvec[Z] = f - view_state->vs_last_absolute_model_rotate[Z];
	    view_state->vs_absolute_model_rotate[Z] = f;
	  }else{
	    rvec[Z] = f - view_state->vs_last_absolute_rotate[Z];
	    view_state->vs_absolute_rotate[Z] = f;
	  }
	}
      }

      /* wrap around */
      if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	fastf_t *arp;
	fastf_t *larp;

	switch(mged_variables->mv_coords){
	case 'm':
	  arp = edit_absolute_model_rotate;
	  larp = last_edit_absolute_model_rotate;
	  break;
	case 'o':
	  arp = edit_absolute_object_rotate;
	  larp = last_edit_absolute_object_rotate;
	  break;
	case 'v':
	  arp = edit_absolute_view_rotate;
	  larp = last_edit_absolute_view_rotate;
	  break;
	default:
		bu_log("unknown mv_coords\n");
		arp = larp = NULL;
	}

	if(arp[Z] < -180.0)
	  arp[Z] = arp[Z] + 360.0;
	else if(arp[Z] > 180.0)
	  arp[Z] = arp[Z] - 360.0;

	larp[Z] = arp[Z];
      }else{
	fastf_t *arp;
	fastf_t *larp;

	if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  arp = view_state->vs_absolute_model_rotate;
	  larp = view_state->vs_last_absolute_model_rotate;
	}else{
	  arp = view_state->vs_absolute_rotate;
	  larp = view_state->vs_last_absolute_rotate;
	}

	if(arp[Z] < -180.0)
	  arp[Z] = arp[Z] + 360.0;
	else if(arp[Z] > 180.0)
	  arp[Z] = arp[Z] - 360.0;

	larp[Z] = arp[Z];
      }

      do_rot = 1;
      break;
    case 'X':
      sf = f * local2base / view_state->vs_Viewscale;
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_absolute_model_tran[X] += sf;
	    last_edit_absolute_model_tran[X] = edit_absolute_model_tran[X];

	    break;
	  case 'v':
	    edit_absolute_view_tran[X] += sf;
	    last_edit_absolute_view_tran[X] = edit_absolute_view_tran[X];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  view_state->vs_absolute_model_tran[X] += sf;
	  view_state->vs_last_absolute_model_tran[X] = view_state->vs_absolute_model_tran[X];
	}else{
	  view_state->vs_absolute_tran[X] += sf;
	  view_state->vs_last_absolute_tran[X] = view_state->vs_absolute_tran[X];
	}

	tvec[X] = f;
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    tvec[X] = f - last_edit_absolute_model_tran[X]*view_state->vs_Viewscale*base2local;
	    edit_absolute_model_tran[X] = sf;
	    last_edit_absolute_model_tran[X] = edit_absolute_model_tran[X];

	    break;
	  case 'v':
	    tvec[X] = f - last_edit_absolute_view_tran[X]*view_state->vs_Viewscale*base2local;
	    edit_absolute_view_tran[X] = sf;
	    last_edit_absolute_view_tran[X] = edit_absolute_view_tran[X];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  tvec[X] = f - view_state->vs_last_absolute_model_tran[X]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_model_tran[X] = sf;
	  view_state->vs_last_absolute_model_tran[X] = view_state->vs_absolute_model_tran[X];
	}else{
	  tvec[X] = f - view_state->vs_last_absolute_tran[X]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_tran[X] = sf;
	  view_state->vs_last_absolute_tran[X] = view_state->vs_absolute_tran[X];
	}

      }

      do_tran = 1;
      break;
    case 'Y':
      sf = f * local2base / view_state->vs_Viewscale;
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_absolute_model_tran[Y] += sf;
	    last_edit_absolute_model_tran[Y] = edit_absolute_model_tran[Y];

	    break;
	  case 'v':
	    edit_absolute_view_tran[Y] += sf;
	    last_edit_absolute_view_tran[Y] = edit_absolute_view_tran[Y];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  view_state->vs_absolute_model_tran[Y] += sf;
	  view_state->vs_last_absolute_model_tran[Y] = view_state->vs_absolute_model_tran[Y];
	}else{
	  view_state->vs_absolute_tran[Y] += sf;
	  view_state->vs_last_absolute_tran[Y] = view_state->vs_absolute_tran[Y];
	}

	tvec[Y] = f;
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    tvec[Y] = f - last_edit_absolute_model_tran[Y]*view_state->vs_Viewscale*base2local;
	    edit_absolute_model_tran[Y] = sf;
	    last_edit_absolute_model_tran[Y] = edit_absolute_model_tran[Y];

	    break;
	  case 'v':
	    tvec[Y] = f - last_edit_absolute_view_tran[Y]*view_state->vs_Viewscale*base2local;
	    edit_absolute_view_tran[Y] = sf;
	    last_edit_absolute_view_tran[Y] = edit_absolute_view_tran[Y];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  tvec[Y] = f - view_state->vs_last_absolute_model_tran[Y]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_model_tran[Y] = sf;
	  view_state->vs_last_absolute_model_tran[Y] = view_state->vs_absolute_model_tran[Y];
	}else{
	  tvec[Y] = f - view_state->vs_last_absolute_tran[Y]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_tran[Y] = sf;
	  view_state->vs_last_absolute_tran[Y] = view_state->vs_absolute_tran[Y];
	}
      }
      
      do_tran = 1;
      break;
    case 'Z':
      sf = f * local2base / view_state->vs_Viewscale;
      if(incr_flag){
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    edit_absolute_model_tran[Z] += sf;
	    last_edit_absolute_model_tran[Z] = edit_absolute_model_tran[Z];

	    break;
	  case 'v':
	    edit_absolute_view_tran[Z] += sf;
	    last_edit_absolute_view_tran[Z] = edit_absolute_view_tran[Z];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  view_state->vs_absolute_model_tran[Z] += sf;
	  view_state->vs_last_absolute_model_tran[Z] = view_state->vs_absolute_model_tran[Z];
	}else{
	  view_state->vs_absolute_tran[Z] += sf;
	  view_state->vs_last_absolute_tran[Z] = view_state->vs_absolute_tran[Z];
	}

	tvec[Z] = f;
      }else{
	if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
			  !view_flag && !model_flag) || edit_flag)){
	  switch(mged_variables->mv_coords){
	  case 'm':
	  case 'o':
	    tvec[Z] = f - last_edit_absolute_model_tran[Z]*view_state->vs_Viewscale*base2local;
	    edit_absolute_model_tran[Z] = sf;
	    last_edit_absolute_model_tran[Z] = edit_absolute_model_tran[Z];

	    break;
	  case 'v':
	    tvec[Z] = f - last_edit_absolute_view_tran[Z]*view_state->vs_Viewscale*base2local;
	    edit_absolute_view_tran[Z] = sf;
	    last_edit_absolute_view_tran[Z] = edit_absolute_view_tran[Z];

	    break;
	  }
	}else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag)){
	  tvec[Z] = f - view_state->vs_last_absolute_model_tran[Z]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_model_tran[Z] = sf;
	  view_state->vs_last_absolute_model_tran[Z] = view_state->vs_absolute_model_tran[Z];
	}else{
	  tvec[Z] = f - view_state->vs_last_absolute_tran[Z]*view_state->vs_Viewscale*base2local;
	  view_state->vs_absolute_tran[Z] = sf;
	  view_state->vs_last_absolute_tran[Z] = view_state->vs_absolute_tran[Z];
	}
      }

      do_tran = 1;
      break;
    case 'S':
      if(incr_flag){
	if(EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)){
	  edit_absolute_scale += f;
	  if(state == ST_S_EDIT)
	    sedit_abs_scale();
	  else
	    oedit_abs_scale();
	}else{
	  view_state->vs_absolute_scale += f;
	  abs_zoom();
	}
      }else{
	if(EDIT_SCALE && ((mged_variables->mv_transform == 'e' && !view_flag) || edit_flag)){
	  edit_absolute_scale = f;
	  if(state == ST_S_EDIT)
	    sedit_abs_scale();
	  else
	    oedit_abs_scale();
	}else{
	  view_state->vs_absolute_scale = f;
	  abs_zoom();
	}
      }

      break;
    default:
      goto usage;
    }
  } else if( strcmp( cmd, "xadc" ) == 0 ) {
	  char *av[5];
	  char sval[32];
	  int nargs = 3;

	  av[0] = "adc";
	  if (incr_flag) {
	    ++nargs;
	    av[1] = "-i";
	    av[2] = "x";
	    av[3] = sval;
	    av[4] = NULL;
	  } else {
	    av[1] = "x";
	    av[2] = sval;
	    av[3] = NULL;
	  }

	  sprintf(sval, "%d", i);
	  (void)f_adc(clientData, interp, nargs, av);
	} else if( strcmp( cmd, "yadc" ) == 0 )  {
	  char *av[5];
	  char sval[32];
	  int nargs = 3;

	  av[0] = "adc";
	  if (incr_flag) {
	    ++nargs;
	    av[1] = "-i";
	    av[2] = "y";
	    av[3] = sval;
	    av[4] = NULL;
	  } else {
	    av[1] = "y";
	    av[2] = sval;
	    av[3] = NULL;
	  }

	  sprintf(sval, "%d", i);
	  (void)f_adc(clientData, interp, nargs, av);
	} else if( strcmp( cmd, "ang1" ) == 0 )  {
	  char *av[5];
	  char sval[32];
	  int nargs = 3;

	  av[0] = "adc";
	  if (incr_flag) {
	    ++nargs;
	    av[1] = "-i";
	    av[2] = "a1";
	    av[3] = sval;
	    av[4] = NULL;
	  } else {
	    av[1] = "a1";
	    av[2] = sval;
	    av[3] = NULL;
	  }

	  sprintf(sval, "%f", f);
	  (void)f_adc(clientData, interp, nargs, av);
	} else if( strcmp( cmd, "ang2" ) == 0 )  {
	  char *av[5];
	  char sval[32];
	  int nargs = 3;

	  av[0] = "adc";
	  if (incr_flag) {
	    ++nargs;
	    av[1] = "-i";
	    av[2] = "a2";
	    av[3] = sval;
	    av[4] = NULL;
	  } else {
	    av[1] = "a2";
	    av[2] = sval;
	    av[3] = NULL;
	  }

	  sprintf(sval, "%f", f);
	  (void)f_adc(clientData, interp, nargs, av);
	} else if (strcmp(cmd, "distadc") == 0) {
	  char *av[5];
	  char sval[32];
	  int nargs = 3;

	  av[0] = "adc";
	  if (incr_flag) {
	    ++nargs;
	    av[1] = "-i";
	    av[2] = "odst";
	    av[3] = sval;
	    av[4] = NULL;
	  } else {
	    av[1] = "odst";
	    av[2] = sval;
	    av[3] = NULL;
	  }

	  sprintf(sval, "%d", i);
	  (void)f_adc(clientData, interp, nargs, av);
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
    (void)knob_tran(tvec, model_flag, view_flag, edit_flag);

  if(do_rot)
    (void)knob_rot(rvec, origin, model_flag, view_flag, edit_flag);
 
  check_nonzero_rates();
  return TCL_OK;
}

int
knob_tran(tvec, model_flag, view_flag, edit_flag)
vect_t tvec;
int model_flag;
int view_flag;
int edit_flag;
{
  if(EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
		    !view_flag && !model_flag) || edit_flag))
    mged_etran(tvec);
  else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
    mged_mtran(tvec);
  else if(mged_variables->mv_coords == 'o')
    mged_otran(tvec);
  else
    mged_vtran(tvec);

  return TCL_OK;
}

int
knob_rot(
	vect_t rvec,
	char origin,
	int model_flag,
	int view_flag,
	int edit_flag)
{
  if(EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
		      !view_flag && !model_flag) || edit_flag))
    mged_erot_xyz(origin, rvec);
  else if(model_flag || (mged_variables->mv_coords == 'm' && !view_flag))
    mged_vrot_xyz(origin, 'm', rvec);
  else if(mged_variables->mv_coords == 'o')
    mged_vrot_xyz(origin, 'o', rvec);
  else
    mged_vrot_xyz(origin, 'v', rvec);

  return TCL_OK;
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

	CHECK_DBI_NULL;

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
			  bu_units_string(dbip->dbi_local2base) );
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
			   mged_tol.dist*base2local, bu_units_string(local2base), mged_tol.perp,
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

/* absolute_scale's value range is [-1.0, 1.0] */
static void
abs_zoom()
{
  /* Use initial Viewscale */
  if(-SMALL_FASTF < view_state->vs_absolute_scale && view_state->vs_absolute_scale < SMALL_FASTF)
    view_state->vs_Viewscale = view_state->vs_i_Viewscale;
  else{
    /* if positive */
    if(view_state->vs_absolute_scale > 0){
      /* scale i_Viewscale by values in [0.0, 1.0] range */
      view_state->vs_Viewscale = view_state->vs_i_Viewscale * (1.0 - view_state->vs_absolute_scale);
    }else/* negative */
      /* scale i_Viewscale by values in [1.0, 10.0] range */
      view_state->vs_Viewscale = view_state->vs_i_Viewscale * (1.0 + (view_state->vs_absolute_scale * -9.0));
  }

  if( view_state->vs_Viewscale < MINVIEW )
    view_state->vs_Viewscale = MINVIEW;

  new_mats();

  if(view_state->vs_absolute_tran[X] != 0.0 ||
     view_state->vs_absolute_tran[Y] != 0.0 ||
     view_state->vs_absolute_tran[Z] != 0.0){
    set_absolute_tran();
  }
}

int
mged_zoom(val)
double val;
{
  if( val < SMALL_FASTF || val > INFINITY )  {
    Tcl_AppendResult(interp, "zoom: scale factor out of range\n", (char *)NULL);
    return TCL_ERROR;
  }

  view_state->vs_Viewscale /= val;
  if( view_state->vs_Viewscale < MINVIEW )
    view_state->vs_Viewscale = MINVIEW;

  new_mats();

  view_state->vs_absolute_scale = 1.0 - view_state->vs_Viewscale / view_state->vs_i_Viewscale;
  if(view_state->vs_absolute_scale < 0.0)
    view_state->vs_absolute_scale /= 9.0;

  if(view_state->vs_absolute_tran[X] != 0.0 ||
     view_state->vs_absolute_tran[Y] != 0.0 ||
     view_state->vs_absolute_tran[Z] != 0.0){
    set_absolute_tran();
  }

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
  quat_quat2mat( view_state->vs_Viewrot, quat );
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

  VSET(slewvec, x * INV_GED, y * INV_GED, z * INV_GED);
  VSUB2(view_state->vs_absolute_tran, view_state->vs_absolute_tran, slewvec);
  slewview( slewvec );

  return TCL_OK;
}


/* set view reference base */
int
mged_svbase()
{ 
  view_state->vs_i_Viewscale = view_state->vs_Viewscale;
  MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_toViewcenter);

  /* reset absolute slider values */
  VSETALL(view_state->vs_absolute_rotate, 0.0);
  VSETALL(view_state->vs_last_absolute_rotate, 0.0);
  VSETALL(view_state->vs_absolute_model_rotate, 0.0);
  VSETALL(view_state->vs_last_absolute_model_rotate, 0.0);
  VSETALL(view_state->vs_absolute_tran, 0.0);
  VSETALL(view_state->vs_last_absolute_tran, 0.0);
  VSETALL(view_state->vs_absolute_model_tran, 0.0);
  VSETALL(view_state->vs_last_absolute_model_tran, 0.0);
  view_state->vs_absolute_scale = 0.0;

  if(mged_variables->mv_faceplate && mged_variables->mv_orig_gui)
    curr_dm_list->dml_dirty = 1;

  return TCL_OK;
}


int
f_svbase(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int status;
  struct dm_list *dmlp;

  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel svb");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  status = mged_svbase();

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    /* if sharing view while faceplate and original gui (i.e. button menu, sliders) are on */
    if(dmlp->dml_view_state == view_state &&
       dmlp->dml_mged_variables->mv_faceplate &&
       dmlp->dml_mged_variables->mv_orig_gui)
      dmlp->dml_dirty = 1;

  return status;
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

#if 0
	if( state == ST_S_EDIT )  {
		if( sedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* solid edit claimed event */
	} else if( state == ST_O_EDIT )  {
		if( objedit_rotate( xangle, yangle, zangle ) > 0 )
			return;		/* object edit claimed event */
	}
#endif

	/* NORMAL CASE.
	 * Apply delta viewing rotation for non-edited parts.
	 * The view rotates around the VIEW CENTER.
	 */
	bn_mat_idn( newrot );
	buildHrot( newrot, xangle, yangle, zangle );

	bn_mat_mul2( newrot, view_state->vs_Viewrot );
	{
		mat_t	newinv;
		bn_mat_inv( newinv, newrot );
		wrt_view( view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta );	/* Updates ModelDelta */
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
	buildHrot( view_state->vs_Viewrot, rad[X], rad[Y], rad[Z] );
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
  buildHrot( view_state->vs_Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad );
  new_mats();

  if(view_state->vs_absolute_tran[X] != 0.0 ||
     view_state->vs_absolute_tran[Y] != 0.0 ||
     view_state->vs_absolute_tran[Z] != 0.0){
    set_absolute_tran();
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
  mat_t	delta;

  MAT_DELTAS_GET_NEG( old_model_center, view_state->vs_toViewcenter );

  MAT4X3PNT( new_model_center, view_state->vs_view2model, view_pos );
  MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, new_model_center );

  VSUB2( diff, new_model_center, old_model_center );
  bn_mat_idn( delta );
  MAT_DELTAS_VEC( delta, diff );
  bn_mat_mul2( delta, view_state->vs_ModelDelta );	/* updates ModelDelta */
  new_mats();

  set_absolute_tran();
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
	MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, eye_model );
	new_mats();

	/*  Second step:  put eye at view 0,0,1.
	 *  For eye to be at 0,0,1, the old 0,0,-1 needs to become 0,0,0.
	 */
	VSET( xlate, 0.0, 0.0, -1.0 );	/* correction factor */
	MAT4X3PNT( new_cent, view_state->vs_view2model, xlate );
	MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, new_cent );
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
	  bu_vls_printf(&vls, "helpdevel model2view");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET(model, atof(argv[1]),
		atof(argv[2]),
		atof(argv[3]) );

	MAT4X3PNT( view, view_state->vs_model2view, model );

	bu_vls_init(&str);
	bu_vls_printf(&str, "%.15e %.15e %.15e", V3ARGS(view) );
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
	  bu_vls_printf(&vls, "helpdevel view2model");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	VSET(view, atof(argv[1]), atof(argv[2]), atof(argv[3]));
	MAT4X3PNT(model, view_state->vs_view2model, view);

	bu_vls_init(&str);
	bu_vls_printf(&str, "%.15e %.15e %.15e", V3ARGS(model) );
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 *			F _ M O D E L 2 V I E W _ L U
 *
 *  Given a point in model coordinates (local units),
 *  convert it to view coordinates (local units).
 */
int
f_model2view_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;
  fastf_t f;
  point_t view_pt;
  point_t model_pt;

  bu_vls_init(&vls);

  if(argc != 4){
    bu_vls_printf(&vls, "helpdevel model2view_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  VSET(model_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
  VSCALE(model_pt, model_pt, local2base);
  MAT4X3PNT(view_pt, view_state->vs_model2view, model_pt);
  f = view_state->vs_Viewscale * base2local;
  VSCALE(view_pt, view_pt, f);

  bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(view_pt));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ V I E W 2 M O D E L _ L U 
 *
 *  Given a point in view coordinates (local units),
 *  convert it to model coordinates (local units).
 */
int
f_view2model_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;
  fastf_t sf;
  point_t view_pt;
  point_t model_pt;

  bu_vls_init(&vls);

  if(argc != 4){
    bu_vls_printf(&vls, "helpdevel view2model_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  sf = 1.0 / (view_state->vs_Viewscale * base2local);
  VSET(view_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
  VSCALE(view_pt, view_pt, sf);
  MAT4X3PNT(model_pt, view_state->vs_view2model, view_pt);
  VSCALE(model_pt, model_pt, base2local);

  bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(model_pt));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ M O D E L 2 G R I D _ L U
 *
 *  Given a point in model coordinates (local units),
 *  convert it to grid coordinates (local units).
 */
int
f_model2grid_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;
  fastf_t f;
  point_t view_pt;
  point_t model_pt;
  point_t mo_view_pt;           /* model origin in view space */
  point_t diff;

  bu_vls_init(&vls);

  if(argc != 4){
    bu_vls_printf(&vls, "helpdevel model2grid_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, view_state->vs_model2view, model_pt);

  VSET(model_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
  VSCALE(model_pt, model_pt, local2base);
  MAT4X3PNT(view_pt, view_state->vs_model2view, model_pt);

  VSUB2(diff, view_pt, mo_view_pt);
  f = view_state->vs_Viewscale * base2local;
  VSCALE(diff, diff, f);

  bu_vls_printf(&vls, "%.15e %.15e", diff[X], diff[Y]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ G R I D 2 M O D E L _ L U
 *
 *  Given a point in grid coordinates (local units),
 *  convert it to model coordinates (local units).
 */
int
f_grid2model_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;
  fastf_t f;
  point_t view_pt;
  point_t model_pt;
  point_t mo_view_pt;           /* model origin in view space */
  point_t diff;

  bu_vls_init(&vls);

  if(argc != 3){
    bu_vls_printf(&vls, "helpdevel grid2model_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  f = 1.0 / (view_state->vs_Viewscale * base2local);
  diff[X] = atof(argv[1]) * f;
  diff[Y] = atof(argv[2]) * f;
  diff[Z] = 0.0;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, view_state->vs_model2view, model_pt);

  VADD2(view_pt, mo_view_pt, diff);
  MAT4X3PNT(model_pt, view_state->vs_view2model, view_pt);
  VSCALE(model_pt, model_pt, base2local);

  bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(model_pt));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ V I E W 2 G R I D _ L U 
 *
 *  Given a point in view coordinates (local units),
 *  convert it to grid coordinates (local units).
 */
int
f_view2grid_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;
  fastf_t f;
  point_t view_pt;
  point_t model_pt;
  point_t mo_view_pt;           /* model origin in view space */
  point_t diff;

  bu_vls_init(&vls);

  if(argc != 4){
    bu_vls_printf(&vls, "helpdevel view2grid_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  VSET(view_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, view_state->vs_model2view, model_pt);
  f = view_state->vs_Viewscale * base2local;
  VSCALE(mo_view_pt, mo_view_pt, f);
  VSUB2(diff, view_pt, mo_view_pt);

  bu_vls_printf(&vls, "%.15e %.15e", diff[X], diff[Y]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ G R I D 2 V I E W _ L U
 *
 *  Given a point in grid coordinates (local units),
 *  convert it to view coordinates (local units).
 */
int
f_grid2view_lu(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;
  fastf_t f;
  point_t view_pt;
  point_t model_pt;
  point_t mo_view_pt;           /* model origin in view space */
  point_t diff;

  bu_vls_init(&vls);

  if(argc != 3){
    bu_vls_printf(&vls, "helpdevel grid2view_lu");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  diff[X] = atof(argv[1]);
  diff[Y] = atof(argv[2]);
  diff[Z] = 0.0;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, view_state->vs_model2view, model_pt);
  f = view_state->vs_Viewscale * base2local;
  VSCALE(mo_view_pt, mo_view_pt, f);
  VADD2(view_pt, mo_view_pt, diff);

  bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(view_pt));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 *			F _ V I E W 2 M O D E L _ V E C
 *
 *  Given a vector in view coordinates,
 *  convert it to model coordinates.
 */
int
f_view2model_vec(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;
  point_t model_vec;
  point_t view_vec;
  mat_t inv_Viewrot;

  bu_vls_init(&vls);

  if(argc != 4){
    bu_vls_printf(&vls, "helpdevel view2model_vec");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  VSET(view_vec, atof(argv[1]), atof(argv[2]), atof(argv[3]));
  bn_mat_inv(inv_Viewrot, view_state->vs_Viewrot);
  MAT4X3PNT(model_vec, inv_Viewrot, view_vec);

  bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(model_vec));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

  bu_vls_free(&vls);
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

  CHECK_DBI_NULL;

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

  VSET( tmp, 0.0, 0.0, 1.0 );
  MAT4X3PNT(eye, view_state->vs_view2model, tmp);

  VSUB2( dir, eye, look );
  VUNITIZE( dir );
  bn_ae_vec( &new_az, &new_el, dir );

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "ae %-15.10f %-15.10f %-15.10f", new_az, new_el, view_state->vs_twist);
  status = Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  VJOIN1( new_center, eye, -view_state->vs_Viewscale, dir );
  MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, new_center );

  new_mats();

  return( status );
}

/*
 * Initialize vsp1 using vsp2 if vsp2 is not null.
 */
void
view_ring_init(vsp1, vsp2)
struct _view_state *vsp1;
struct _view_state *vsp2;
{
  struct view_ring *vrp1;
  struct view_ring *vrp2;

  BU_LIST_INIT(&vsp1->vs_headView.l);

  if(vsp2 != (struct _view_state *)NULL){
    struct view_ring *vrp1_current_view = NULL;
    struct view_ring *vrp1_last_view = NULL;

    for(BU_LIST_FOR(vrp2, view_ring, &vsp2->vs_headView.l)){
      BU_GETSTRUCT(vrp1, view_ring);
      /* append to last list element */
      BU_LIST_APPEND(vsp1->vs_headView.l.back, &vrp1->l);

      bn_mat_copy(vrp1->vr_rot_mat, vrp2->vr_rot_mat);
      bn_mat_copy(vrp1->vr_tvc_mat, vrp2->vr_tvc_mat);
      vrp1->vr_scale = vrp2->vr_scale;
      vrp1->vr_id = vrp2->vr_id;

      if(vsp2->vs_current_view == vrp2)
	vrp1_current_view = vrp1;

      if(vsp2->vs_last_view == vrp2)
	vrp1_last_view = vrp1;
    }

    vsp1->vs_current_view = vrp1_current_view;
    vsp1->vs_last_view = vrp1_last_view;
  } else {
    BU_GETSTRUCT(vrp1, view_ring);
    BU_LIST_APPEND(&vsp1->vs_headView.l, &vrp1->l);

    vrp1->vr_id = 1;
    vsp1->vs_current_view = vrp1;
    vsp1->vs_last_view = vrp1;
  }
}

void
view_ring_destroy(struct dm_list *dlp)
{
  struct view_ring *vrp;

  while(BU_LIST_NON_EMPTY(&dlp->dml_view_state->vs_headView.l)){
    vrp = BU_LIST_FIRST(view_ring,&dlp->dml_view_state->vs_headView.l);
    BU_LIST_DEQUEUE(&vrp->l);
    bu_free((genptr_t)vrp, "view_ring_destroy: vrp");
  }
}


/*
 * SYNOPSIS
 *	view_ring add|next|prev|toggle		
 *	view_ring delete #			delete view #
 *	view_ring goto #			goto view #
 *	view_ring get [-a]			get the current view
 *
 * DESCRIPTION
 *
 * EXAMPLES
 *
 */
int
f_view_ring(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int n;
  struct view_ring *vrp;
  struct view_ring *lv;
  struct bu_vls vls;

  if(argc < 2 || 3 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel view_ring");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(!strcmp(argv[1],"add")){
    if(argc != 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* save current Viewrot */
    bn_mat_copy(view_state->vs_current_view->vr_rot_mat, view_state->vs_Viewrot);

    /* save current toViewcenter */
    bn_mat_copy(view_state->vs_current_view->vr_tvc_mat, view_state->vs_toViewcenter);

    /* save current Viewscale */
    view_state->vs_current_view->vr_scale = view_state->vs_Viewscale;

    /* allocate memory and append to list */
    BU_GETSTRUCT(vrp, view_ring);
    lv = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);
    BU_LIST_APPEND(&lv->l, &vrp->l);

    /* assign a view number */
    vrp->vr_id = lv->vr_id + 1;

    view_state->vs_last_view = view_state->vs_current_view;
    view_state->vs_current_view = vrp;
    (void)mged_svbase();

    return TCL_OK;
  }

  if(!strcmp(argv[1],"next")){
    if(argc != 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* check to see if this is the last view in the list */
    if(BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
       BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l))
      return TCL_OK;

    /* save current Viewrot */
    bn_mat_copy(view_state->vs_current_view->vr_rot_mat, view_state->vs_Viewrot);

    /* save current toViewcenter */
    bn_mat_copy(view_state->vs_current_view->vr_tvc_mat, view_state->vs_toViewcenter);

    /* save current Viewscale */
    view_state->vs_current_view->vr_scale = view_state->vs_Viewscale;

    view_state->vs_last_view = view_state->vs_current_view;
    view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_current_view);

    if(BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l))
      view_state->vs_current_view = BU_LIST_FIRST(view_ring, &view_state->vs_headView.l);

    bn_mat_copy(view_state->vs_Viewrot, view_state->vs_current_view->vr_rot_mat);
    bn_mat_copy(view_state->vs_toViewcenter, view_state->vs_current_view->vr_tvc_mat);
    view_state->vs_Viewscale = view_state->vs_current_view->vr_scale;

    new_mats();
    (void)mged_svbase();

    return TCL_OK;
  }

  if(!strcmp(argv[1],"prev")){
    if(argc != 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* check to see if this is the last view in the list */
    if(BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
       BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l))
      return TCL_OK;

    /* save current Viewrot */
    bn_mat_copy(view_state->vs_current_view->vr_rot_mat, view_state->vs_Viewrot);

    /* save current toViewcenter */
    bn_mat_copy(view_state->vs_current_view->vr_tvc_mat, view_state->vs_toViewcenter);

    /* save current Viewscale */
    view_state->vs_current_view->vr_scale = view_state->vs_Viewscale;

    view_state->vs_last_view = view_state->vs_current_view;
    view_state->vs_current_view = BU_LIST_PLAST(view_ring, view_state->vs_current_view);

    if(BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l))
      view_state->vs_current_view = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);

    bn_mat_copy(view_state->vs_Viewrot, view_state->vs_current_view->vr_rot_mat);
    bn_mat_copy(view_state->vs_toViewcenter, view_state->vs_current_view->vr_tvc_mat);
    view_state->vs_Viewscale = view_state->vs_current_view->vr_scale;

    new_mats();
    (void)mged_svbase();

    return TCL_OK;
  }

  if(!strcmp(argv[1],"toggle")){
    struct view_ring *save_last_view;

    if(argc != 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* save current Viewrot */
    bn_mat_copy(view_state->vs_current_view->vr_rot_mat, view_state->vs_Viewrot);

    /* save current toViewcenter */
    bn_mat_copy(view_state->vs_current_view->vr_tvc_mat, view_state->vs_toViewcenter);

    /* save current Viewscale */
    view_state->vs_current_view->vr_scale = view_state->vs_Viewscale;

    save_last_view = view_state->vs_last_view;
    view_state->vs_last_view = view_state->vs_current_view;
    view_state->vs_current_view = save_last_view;
    bn_mat_copy(view_state->vs_Viewrot, view_state->vs_current_view->vr_rot_mat);
    bn_mat_copy(view_state->vs_toViewcenter, view_state->vs_current_view->vr_tvc_mat);
    view_state->vs_Viewscale = view_state->vs_current_view->vr_scale;

    new_mats();
    (void)mged_svbase();

    return TCL_OK;
  }

  if(!strcmp(argv[1],"delete")){
    if(argc != 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* search for view with id of n */
    n = atoi(argv[2]);
    for(BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)){
      if(vrp->vr_id == n)
	break;
    }

    if(BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)){
      Tcl_AppendResult(interp, "view_ring delete: ", argv[2], " is not a valid view\n",
		       (char *)NULL);
      return TCL_ERROR;
    }

    /* check to see if this is the last view in the list */
    if(BU_LIST_IS_HEAD(vrp->l.forw, &view_state->vs_headView.l) &&
       BU_LIST_IS_HEAD(vrp->l.back, &view_state->vs_headView.l)){
      Tcl_AppendResult(interp, "view_ring delete: Cannot delete the only remaining view!\n", (char *)NULL);
      return TCL_ERROR;
    }

    if(vrp == view_state->vs_current_view){
      if(view_state->vs_current_view == view_state->vs_last_view){
	view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_last_view);
	view_state->vs_last_view = view_state->vs_current_view;
      }else
	view_state->vs_current_view = view_state->vs_last_view;

      bn_mat_copy(view_state->vs_Viewrot, view_state->vs_current_view->vr_rot_mat);
      bn_mat_copy(view_state->vs_toViewcenter, view_state->vs_current_view->vr_tvc_mat);
      view_state->vs_Viewscale = view_state->vs_current_view->vr_scale;
      new_mats();
      (void)mged_svbase();
    }else if(vrp == view_state->vs_last_view)
      view_state->vs_last_view = view_state->vs_current_view;

    BU_LIST_DEQUEUE(&vrp->l);
    bu_free((genptr_t)vrp, "view_ring delete");

    return TCL_OK;
  }

  if(!strcmp(argv[1],"goto")){
    if(argc != 3){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    /* search for view with id of n */
    n = atoi(argv[2]);
    for(BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)){
      if(vrp->vr_id == n)
	break;
    }

    if(BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)){
      Tcl_AppendResult(interp, "view_ring goto: ", argv[2], " is not a valid view\n",
		       (char *)NULL);
      return TCL_ERROR;
    }

    /* nothing to do */
    if(vrp == view_state->vs_current_view)
      return TCL_OK;

    /* save current Viewrot */
    bn_mat_copy(view_state->vs_current_view->vr_rot_mat, view_state->vs_Viewrot);

    /* save current toViewcenter */
    bn_mat_copy(view_state->vs_current_view->vr_tvc_mat, view_state->vs_toViewcenter);

    /* save current Viewscale */
    view_state->vs_current_view->vr_scale = view_state->vs_Viewscale;

    view_state->vs_last_view = view_state->vs_current_view;
    view_state->vs_current_view = vrp;
    bn_mat_copy(view_state->vs_Viewrot, view_state->vs_current_view->vr_rot_mat);
    bn_mat_copy(view_state->vs_toViewcenter, view_state->vs_current_view->vr_tvc_mat);
    view_state->vs_Viewscale = view_state->vs_current_view->vr_scale;

    new_mats();
    (void)mged_svbase();

    return TCL_OK;
  }

  if(!strcmp(argv[1],"get")){
    /* return current view */
    if(argc == 2){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", view_state->vs_current_view->vr_id);
      Tcl_AppendElement(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_OK;
    }

    if(strcmp("-a", argv[2])){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help view_ring");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    for(BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)){
      bu_vls_printf(&vls, "%d", vrp->vr_id);
      Tcl_AppendElement(interp, bu_vls_addr(&vls));
      bu_vls_trunc(&vls, 0);
    }

    bu_vls_free(&vls);
    return TCL_OK;
  }

  Tcl_AppendResult(interp, "view_ring: unrecognized command - ", argv[1], (char *)NULL);
  return TCL_ERROR;
}

int
mged_erot(origin, newrot)
mat_t newrot;
{
  int save_edflag;
  mat_t temp1, temp2;

  update_views = 1;

  switch(mged_variables->mv_coords){
  case 'm':
    break;
  case 'o':
    bn_mat_inv(temp1, acc_rot_sol);

    /* transform into object rotations */
    bn_mat_mul(temp2, acc_rot_sol, newrot);
    bn_mat_mul(newrot, temp2, temp1);
    break;
  case 'v':
    bn_mat_inv(temp1, view_state->vs_Viewrot);

    /* transform into model rotations */
    bn_mat_mul(temp2, temp1, newrot);
    bn_mat_mul(newrot, temp2, view_state->vs_Viewrot);
    break;
  }

  if(state == ST_S_EDIT){
    char save_origin;

    save_origin = mged_variables->mv_rotate_about;
    mged_variables->mv_rotate_about = origin;

    save_edflag = es_edflag;
    if(!SEDIT_ROTATE)
      es_edflag = SROT;

    inpara = 0;
    bn_mat_copy(incr_change, newrot);
    bn_mat_mul2(incr_change, acc_rot_sol);
    sedit();

    mged_variables->mv_rotate_about = save_origin;
    es_edflag = save_edflag;
  }else{
    point_t point;
    vect_t work;

    bn_mat_mul2(newrot, acc_rot_sol);

    /* find point for rotation to take place wrt */
    switch(origin){
    case 'v':       /* View Center */
      VSET(work, 0.0, 0.0, 0.0);
      MAT4X3PNT(point, view_state->vs_view2model, work);
      break;
    case 'e':       /* Eye */
      VSET(work, 0.0, 0.0, 1.0);
      MAT4X3PNT(point, view_state->vs_view2model, work);
      break;
    case 'm':       /* Model Center */
      VSETALL(point, 0.0);
      break;
    case 'k':
    default:
      MAT4X3PNT(point, modelchanges, es_keypoint);
    }

    /* 
     * Apply newrot to the modelchanges matrix wrt "point"
     */
    wrt_point(modelchanges, newrot, modelchanges, point);

    new_edit_mats();
  }

  return TCL_OK;
}

int
mged_erot_xyz(
	char origin,
	vect_t rvec)
{
  mat_t newrot;

  bn_mat_idn(newrot);
  buildHrot(newrot, rvec[X]*degtorad, rvec[Y]*degtorad, rvec[Z]*degtorad);

  return mged_erot(origin, newrot);
}

int
f_mrot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  vect_t rvec;

  if(argc < 4 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help mrot");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &rvec[X]) < 1){
    Tcl_AppendResult(interp, "f_mrot: bad x value - %s\n", argv[1]);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &rvec[Y]) < 1){
    Tcl_AppendResult(interp, "f_mrot: bad y value - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &rvec[Z]) < 1){
    Tcl_AppendResult(interp, "f_mrot: bad z value - %s\n", argv[3]);
    return TCL_ERROR;
  }

  return mged_vrot_xyz(mged_variables->mv_rotate_about, 'm', rvec);
}

/*
 *			M G E D _ V R O T
 */
int
mged_vrot(origin, newrot)
char origin;
mat_t newrot;
{
  mat_t   newinv;

  bn_mat_inv( newinv, newrot );

  if(origin != 'v'){
    point_t		rot_pt;
    point_t		new_origin;
    mat_t		viewchg, viewchginv;
    point_t		new_cent_view;
    point_t		new_cent_model;

    if(origin == 'e'){
      /* "VR driver" method: rotate around "eye" point (0,0,1) viewspace */
      VSET( rot_pt, 0.0, 0.0, 1.0 );		/* point to rotate around */
    }else if(origin == 'k' && state == ST_S_EDIT){
      /* rotate around keypoint */
      MAT4X3PNT(rot_pt, view_state->vs_model2view, curr_e_axes_pos);
    }else if(origin == 'k' && state == ST_O_EDIT){
      point_t kpWmc;

      MAT4X3PNT(kpWmc, modelchanges, es_keypoint);
      MAT4X3PNT(rot_pt, view_state->vs_model2view, kpWmc);
    }else{
      /* rotate around model center (0,0,0) */
      VSET( new_origin, 0.0, 0.0, 0.0 );
      MAT4X3PNT( rot_pt, view_state->vs_model2view, new_origin );  /* point to rotate around */
    }

    bn_mat_xform_about_pt( viewchg, newrot, rot_pt );
    bn_mat_inv( viewchginv, viewchg );

    /* Convert origin in new (viewchg) coords back to old view coords */
    VSET( new_origin, 0.0, 0.0, 0.0 );
    MAT4X3PNT( new_cent_view, viewchginv, new_origin );
    MAT4X3PNT( new_cent_model, view_state->vs_view2model, new_cent_view );
    MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, new_cent_model );

    /* XXX This should probably capture the translation too */
    /* XXX I think the only consumer of ModelDelta is the predictor frame */
    wrt_view( view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta );		/* pure rotation */
  } else
    /* Traditional method:  rotate around view center (0,0,0) viewspace */
    wrt_view( view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta );

  /* Update the rotation component of the model2view matrix */
  bn_mat_mul2( newrot, view_state->vs_Viewrot ); /* pure rotation */
  new_mats();

  set_absolute_tran();

  return TCL_OK;
}

int
mged_vrot_xyz(
	char origin,
	char coords,
	vect_t rvec)
{
  mat_t newrot;
  mat_t temp1, temp2;

  bn_mat_idn(newrot);
  buildHrot(newrot, rvec[X]*degtorad, rvec[Y]*degtorad, rvec[Z]*degtorad);

  if (coords == 'm') {
    /* transform model rotations into view rotations */
    bn_mat_inv(temp1, view_state->vs_Viewrot);
    bn_mat_mul(temp2, view_state->vs_Viewrot, newrot);
    bn_mat_mul(newrot, temp2, temp1);
  } else if ((state == ST_S_EDIT || state == ST_O_EDIT) &&
	     coords == 'o') {

    /* first, transform object rotations into model rotations */
    bn_mat_inv(temp1, acc_rot_sol);
    bn_mat_mul(temp2, acc_rot_sol, newrot);
    bn_mat_mul(newrot, temp2, temp1);

    /* now transform model rotations into view rotations */
    bn_mat_inv(temp1, view_state->vs_Viewrot);
    bn_mat_mul(temp2, view_state->vs_Viewrot, newrot);
    bn_mat_mul(newrot, temp2, temp1);
  } /* else assume already view rotations */

  return mged_vrot(origin, newrot);
}

int
f_vrot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  vect_t rvec;

  if(argc < 4 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help vrot");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &rvec[X]) < 1){
    Tcl_AppendResult(interp, "f_vrot: bad x value - %s\n", argv[1]);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &rvec[Y]) < 1){
    Tcl_AppendResult(interp, "f_vrot: bad y value - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &rvec[Z]) < 1){
    Tcl_AppendResult(interp, "f_vrot: bad z value - %s\n", argv[3]);
    return TCL_ERROR;
  }

  VSCALE(rvec, rvec, -1.0);

  return mged_vrot_xyz(mged_variables->mv_rotate_about, 'v', rvec);
}

int
mged_rot(origin, newrot)
char origin;
mat_t newrot;
{
  if((state == ST_S_EDIT || state == ST_O_EDIT) &&
     mged_variables->mv_transform == 'e')
    return mged_erot(origin, newrot);

  /* apply to View */
  if(mged_variables->mv_coords == 'm'){
    mat_t temp1, temp2;

    /* transform model rotations into view rotations */
    bn_mat_inv(temp1, view_state->vs_Viewrot);
    bn_mat_mul(temp2, view_state->vs_Viewrot, newrot);
    bn_mat_mul(newrot, temp2, temp1);
  }

  return mged_vrot(origin, newrot);
}

int
mged_rot_xyz(origin, rvec)
char origin;
vect_t rvec;
{
  mat_t newrot;

  bn_mat_idn(newrot);
  buildHrot(newrot, rvec[X]*degtorad, rvec[Y]*degtorad, rvec[Z]*degtorad);

  return mged_rot(origin, newrot);
}

int
f_rot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  vect_t rvec;

  if(argc != 4){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help rot");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &rvec[X]) < 1){
    Tcl_AppendResult(interp, "f_rot: bad x value - %s\n", argv[1]);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &rvec[Y]) < 1){
    Tcl_AppendResult(interp, "f_rot: bad y value - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &rvec[Z]) < 1){
    Tcl_AppendResult(interp, "f_rot: bad z value - %s\n", argv[3]);
    return TCL_ERROR;
  }

  if(state != ST_S_EDIT && state != ST_O_EDIT){
    VSCALE(rvec, rvec, -1.0);
  }

  return mged_rot_xyz(mged_variables->mv_rotate_about, rvec);
}

int
f_arot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  mat_t newrot;
  point_t pt;
  vect_t axis;
  fastf_t angle;

  if(argc != 5){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help arot");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &axis[X]) < 1){
    Tcl_AppendResult(interp, "f_arot: bad x value - %s\n", argv[1]);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &axis[Y]) < 1){
    Tcl_AppendResult(interp, "f_arot: bad y value - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &axis[Z]) < 1){
    Tcl_AppendResult(interp, "f_arot: bad z value - %s\n", argv[3]);
    return TCL_ERROR;
  }

  if(sscanf(argv[4], "%lf", &angle) < 1){
    Tcl_AppendResult(interp, "f_arot: bad angle - %s\n", argv[4]);
    return TCL_ERROR;
  }

  VSETALL(pt, 0.0);
  VUNITIZE(axis);

  bn_mat_arb_rot(newrot, pt, axis, angle*degtorad);

  return mged_rot(mged_variables->mv_rotate_about, newrot);
}

int
mged_etran(const point_t pt)
{
  point_t p2;
  int save_edflag;
  point_t delta;
  point_t vcenter;
  point_t work;
  mat_t xlatemat;

  /* compute delta */
  switch(mged_variables->mv_coords){
  case 'm':
    VSCALE(delta, pt, local2base);
    break;
  case 'o':
    VSCALE(p2, pt, local2base);
    MAT4X3PNT(delta, acc_rot_sol, p2);
    break;
  case 'v':
  default:
    VSCALE(p2, pt, local2base/view_state->vs_Viewscale);
    MAT4X3PNT(work, view_state->vs_view2model, p2);
    MAT_DELTAS_GET_NEG(vcenter, view_state->vs_toViewcenter);
    VSUB2(delta, work, vcenter);

    break;
  }

  if(state == ST_S_EDIT){
    save_edflag = es_edflag;
    if(!SEDIT_TRAN)
      es_edflag = STRANS;

    VADD2(es_para, delta, curr_e_axes_pos);
    inpara = 3;
    sedit();
    es_edflag = save_edflag;
  }else{
    bn_mat_idn(xlatemat);
    MAT_DELTAS_VEC(xlatemat, delta);
    bn_mat_mul2(xlatemat, modelchanges);

    new_edit_mats();
    update_views = 1;
  }

  return TCL_OK;
}

int
mged_otran(const vect_t tvec)
{
  vect_t work;

  if(state == ST_S_EDIT || state == ST_O_EDIT){
    /* apply acc_rot_sol to tvec */
    MAT4X3PNT(work, acc_rot_sol, tvec);
  }

  return mged_mtran(work);
}

int
mged_mtran(const vect_t tvec)
{
  point_t delta;
  point_t vc, nvc;

  VSCALE(delta, tvec, local2base);
  MAT_DELTAS_GET_NEG(vc, view_state->vs_toViewcenter);
  VSUB2(nvc, vc, delta);
  MAT_DELTAS_VEC_NEG(view_state->vs_toViewcenter, nvc);
  new_mats();

  /* calculate absolute_tran */
  set_absolute_view_tran();
  
  return TCL_OK;
}

int
mged_vtran(const vect_t tvec)
{
  vect_t  tt;
  point_t delta;
  point_t work;
  point_t vc, nvc;

  VSCALE(tt, tvec, local2base/view_state->vs_Viewscale);
  MAT4X3PNT(work, view_state->vs_view2model, tt);
  MAT_DELTAS_GET_NEG(vc, view_state->vs_toViewcenter);
  VSUB2(delta, work, vc);
  VSUB2(nvc, vc, delta);
  MAT_DELTAS_VEC_NEG(view_state->vs_toViewcenter, nvc);

  new_mats();

  /* calculate absolute_model_tran */
  set_absolute_model_tran();
  
  return TCL_OK;
}

int
mged_tran(const vect_t tvec)
{
  if((state == ST_S_EDIT || state == ST_O_EDIT) &&
      mged_variables->mv_transform == 'e')
    return mged_etran(tvec);

  /* apply to View */
  if(mged_variables->mv_coords == 'm')
    return mged_mtran(tvec);

  if(mged_variables->mv_coords == 'o')
    return mged_otran(tvec);

  return mged_vtran(tvec);
}

int
f_tra(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  vect_t tvec;

  if(argc != 4){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help tra");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &tvec[X]) < 1){
    Tcl_AppendResult(interp, "f_tra: bad x value - %s\n", argv[1]);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%lf", &tvec[Y]) < 1){
    Tcl_AppendResult(interp, "f_tra: bad y value - %s\n", argv[2]);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%lf", &tvec[Z]) < 1){
    Tcl_AppendResult(interp, "f_tra: bad z value - %s\n", argv[3]);
    return TCL_ERROR;
  }

  if(state != ST_S_EDIT && state != ST_O_EDIT){
    VSCALE(tvec, tvec, -1.0);
  }

  return mged_tran(tvec);
}

int
mged_escale(sfactor)
fastf_t sfactor;
{
  fastf_t old_scale;

  if(-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF)
    return TCL_OK;

  if(state == ST_S_EDIT){
    int save_edflag;

    save_edflag = es_edflag;
    if(!SEDIT_SCALE)
      es_edflag = SSCALE;

    es_scale = sfactor;
    old_scale = acc_sc_sol;
    acc_sc_sol *= sfactor;

    if(acc_sc_sol < MGED_SMALL_SCALE){
      acc_sc_sol = old_scale;
      es_edflag = save_edflag;
      return TCL_OK;
    }

    if(acc_sc_sol >= 1.0)
      edit_absolute_scale = (acc_sc_sol - 1.0) / 3.0;
    else
      edit_absolute_scale = acc_sc_sol - 1.0;

    sedit();

    es_edflag = save_edflag;
  }else{
    point_t temp;
    point_t pos_model;
    mat_t smat;
    fastf_t inv_sfactor;

    inv_sfactor = 1.0 / sfactor;
    bn_mat_idn(smat);

    switch(edobj){
    case BE_O_XSCALE:                            /* local scaling ... X-axis */
      smat[0] = inv_sfactor;
      old_scale = acc_sc[X];
      acc_sc[X] *= inv_sfactor;

      if(acc_sc[X] < MGED_SMALL_SCALE){
	acc_sc[X] = old_scale;
	return TCL_OK;
      }
      break;
    case BE_O_YSCALE:                            /* local scaling ... Y-axis */
      smat[5] = inv_sfactor;
      old_scale = acc_sc[Y];
      acc_sc[Y] *= inv_sfactor;

      if(acc_sc[Y] < MGED_SMALL_SCALE){
	acc_sc[Y] = old_scale;
	return TCL_OK;
      }
      break;
    case BE_O_ZSCALE:                            /* local scaling ... Z-axis */
      smat[10] = inv_sfactor;
      old_scale = acc_sc[Z];
      acc_sc[Z] *= inv_sfactor;

      if(acc_sc[Z] < MGED_SMALL_SCALE){
	acc_sc[Z] = old_scale;
	return TCL_OK;
      }
      break;
    case BE_O_SCALE:                             /* global scaling */
    default:
      smat[15] = inv_sfactor;
      old_scale = acc_sc_sol;
      acc_sc_sol *= inv_sfactor;

      if(acc_sc_sol < MGED_SMALL_SCALE){
	acc_sc_sol = old_scale;
	return TCL_OK;
      }
      break;
    }

    /* Have scaling take place with respect to keypoint,
     * NOT the view center.
     */
#if 0
    MAT4X3PNT(temp, es_mat, es_keypoint);
#else
    VMOVE(temp, es_keypoint);
#endif
    MAT4X3PNT(pos_model, modelchanges, temp);
    wrt_point(modelchanges, smat, modelchanges, pos_model);

    new_edit_mats();
  }

  return TCL_OK;
}

int
mged_vscale(sfactor)
fastf_t sfactor;
{
  fastf_t f;

  if(-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF)
    return TCL_OK;

  view_state->vs_Viewscale *= sfactor;
  if( view_state->vs_Viewscale < MINVIEW )
        view_state->vs_Viewscale = MINVIEW;
  f = view_state->vs_Viewscale / view_state->vs_i_Viewscale;

  if(f >= 1.0)
    view_state->vs_absolute_scale = (f - 1.0) / -9.0;
  else
    view_state->vs_absolute_scale = 1.0 - f;

  new_mats();
  return TCL_OK;
}

int
mged_scale(sfactor)
fastf_t sfactor;
{
  if((state == ST_S_EDIT || state == ST_O_EDIT) &&
    mged_variables->mv_transform == 'e')
    return mged_escale(sfactor);

  return mged_vscale(sfactor);
}

int
f_sca(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  fastf_t sfactor;

  if(argc != 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help sca");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lf", &sfactor) < 1){
    Tcl_AppendResult(interp, "f_sca: bad scale factor - %s\n", argv[1]);
    return TCL_ERROR;
  }

  return mged_scale(sfactor);
}
