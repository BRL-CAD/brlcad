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
 *	f_refresh	request display refresh
 *	f_attach	attach display device
 *	f_release	release display device
 *	eraseobj	Drop an object from the visible list
 *	pr_schain	Print info about visible list
 *	f_ill		illuminate the named object
 *	f_sed		simulate pressing "solid edit" then illuminate
 *	f_knob		simulate knob twist
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

#include <math.h>
#include <signal.h>
#include <stdio.h>

#include "tcl.h"
#include "tk.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "nmg.h"
#include "externs.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "../librt/debug.h"	/* XXX */

extern void color_soltab();
int mged_vrot();
static void abs_zoom();

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

extern mat_t    ModelDelta;     /* from ged.c */
extern long	nvectors;	/* from dodraw.c */

extern struct rt_tol mged_tol;	/* from ged.c */

vect_t edit_absolute_rotate;
vect_t edit_rate_rotate;
int edit_rateflag_rotate;

vect_t edit_absolute_tran;
vect_t edit_rate_tran;
int edit_rateflag_tran;

fastf_t edit_absolute_scale;
fastf_t edit_rate_scale;
int edit_rateflag_scale;

double		mged_abs_tol;
double		mged_rel_tol = 0.01;		/* 1%, by default */
double		mged_nrm_tol;			/* normal ang tol, radians */

BU_EXTERN(int	edit_com, (int argc, char **argv, int kind, int catch_sigint));

/* Delete an object or several objects from the display */
/* Format: d object1 object2 .... objectn */
int
f_delobj(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) != DIR_NULL )
			eraseobj( dp );
	}
	no_memory = 0;

	update_views = 1;

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  /* must convert from the local unit to the base unit */
  toViewcenter[MDX] = -atof( argv[1] ) * local2base;
  toViewcenter[MDY] = -atof( argv[2] ) * local2base;
  toViewcenter[MDZ] = -atof( argv[3] ) * local2base;
  new_mats();

  return TCL_OK;
}


int
mged_vrot(x, y, z)
double x, y, z;
{
  mat_t newrot;
  point_t model_pos;
  point_t new_pos;

#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(model_pos, view2model, edit_absolute_tran);
#endif
  mat_idn( newrot );
  buildHrot( newrot, x * degtorad, y * degtorad, z * degtorad);
  mat_mul2( newrot, Viewrot );

  {
    mat_t   newinv;

    mat_inv( newinv, newrot );
    wrt_view( ModelDelta, newinv, ModelDelta );
  }
  new_mats();
#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(edit_absolute_tran, model2view, model_pos);
#endif
  VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
  MAT4X3PNT(absolute_slew, model2view, new_pos);

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if (f_zap(clientData, interp, 1, av) == TCL_ERROR)
    return TCL_ERROR;

  if( dmp->dm_displaylist )  {
    /*
     * Force out the control list with NO solids being drawn,
     * then the display processor will not mind when we start
     * writing new subroutines out there...
     */
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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

	mat_idn( toViewcenter );
	MAT_DELTAS( toViewcenter, -center[X], -center[Y], -center[Z] );
	Viewscale = radial[X];
	V_MAX( Viewscale, radial[Y] );
	V_MAX( Viewscale, radial[Z] );

	i_Viewscale = Viewscale;
}

/*
 *			E D I T _ C O M
 *
 * B, e, and E commands uses this area as common
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

    save_dm_list = curr_dm_list;
    for( BU_LIST_FOR(p, dm_list, &head_dm_list.l) ){
      curr_dm_list = p;

      /* If we went from blank screen to non-blank, resize */
      if (mged_variables.autosize  && initial_blank_screen &&
	  BU_LIST_NON_EMPTY(&HeadSolid.l)) {
	size_reset();
	new_mats();

	MAT_DELTAS_GET(orig_pos, toViewcenter);
	absolute_zoom = 0.0;
	VSETALL( absolute_rotate, 0.0 );
	VSETALL( absolute_slew, 0.0 );
      }

      color_soltab();
#if 0
      dmp->dm_colorchange(dmp);
#endif
    }

    curr_dm_list = save_dm_list;
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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc == 1 )
		regdebug = !regdebug;	/* toggle */
	else
		regdebug = atoi( argv[1] );

	Tcl_AppendResult(interp, "regdebug=", argv[1], "\n", (char *)NULL);

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
 *			M G E D _ C O M B _ D E S C R I B E
 *
 *  Describe the non-tree portion of a combination node.
 */
void
mged_comb_describe( vls, comb )
struct bu_vls			*vls;
CONST struct rt_comb_internal	*comb;
{
	RT_CK_COMB(comb);

	if( comb->region_flag ) {
		bu_vls_printf( vls,
		       "REGION id=%d  (air=%d, los=%d, GIFTmater=%d) ",
			comb->region_id,
			comb->aircode,
			comb->los,
			comb->GIFTmater );
	}

	bu_vls_strcat( vls, "--\n" );
	if( bu_vls_strlen(&comb->shader) > 0 ) {
		bu_vls_printf( vls,
			"Shader '%s'\n",
			bu_vls_addr(&comb->shader) );
	}

	if( comb->rgb_valid ) {
		bu_vls_printf( vls,
			"Color %d %d %d\n",
			comb->rgb[0],
			comb->rgb[1],
			comb->rgb[2]);
	}

	if( bu_vls_strlen(&comb->shader) > 0 || comb->rgb_valid )  {
		if( comb->inherit ) {
			bu_vls_strcat( vls, 
	"(These material properties override all lower ones in the tree)\n");
		}
	}
}

#define STAT_ROT	1
#define STAT_XLATE	2
#define STAT_PERSP	4
#define STAT_SCALE	8
/*
 *			M A T _ C A T E G O R I Z E
 *
 *  Describe with a bit vector the effects this matrix will have.
 */
int
mat_categorize( matp )
CONST mat_t	matp;
{
	int	status = 0;

	if( !matp )  return 0;

	if( matp[0] != 1.0 || matp[5] != 1.0 || matp[10] != 1.0 )
		status |= STAT_ROT;

	if( matp[MDX] != 0.0 ||
	    matp[MDY] != 0.0 ||
	    matp[MDZ] != 0.0 )
		status |= STAT_XLATE;

	if( matp[12] != 0.0 ||
	    matp[13] != 0.0 ||
	    matp[14] != 0.0 )
		status |= STAT_PERSP;

	if( matp[15] != 1.0 )  status |= STAT_SCALE;

	return status;
}

/*
 *			M G E D _ T R E E _ D E S C R I B E
 */
void
mged_tree_describe( vls, tp, indented, lvl )
struct bu_vls		*vls;
CONST union tree	*tp;
int			indented;
int			lvl;
{
	int	status;

	BU_CK_VLS(vls);
	RT_CK_TREE(tp);
	switch( tp->tr_op )  {

	case OP_DB_LEAF:
		status = mat_categorize( tp->tr_l.tl_mat );

		/* One per line, out onto the vls */
		if( !indented )  bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, tp->tr_l.tl_name );
		if( status & STAT_ROT ) {
			fastf_t	az, el;
			ae_vec( &az, &el, tp->tr_l.tl_mat ?
				tp->tr_l.tl_mat : rt_identity );
			bu_vls_printf( vls, 
				" az=%g, el=%g, ",
				az, el );
		}
		if( status & STAT_XLATE ) {
			bu_vls_printf( vls, " [%g,%g,%g]",
				tp->tr_l.tl_mat[MDX]*base2local,
				tp->tr_l.tl_mat[MDY]*base2local,
				tp->tr_l.tl_mat[MDZ]*base2local);
		}
		if( status & STAT_SCALE ) {
			bu_vls_printf( vls, " scale %g",
				1.0/tp->tr_l.tl_mat[15] );
		}
		if( status & STAT_PERSP ) {
			bu_vls_printf( vls, 
				" Perspective=[%g,%g,%g]??",
				tp->tr_l.tl_mat[12],
				tp->tr_l.tl_mat[13],
				tp->tr_l.tl_mat[14] );
		}
		bu_vls_printf( vls, "\n" );
		return;

		/* This node is known to be a binary op */
	case OP_UNION:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "u " );
		goto bin;
	case OP_INTERSECT:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "+ " );
		goto bin;
	case OP_SUBTRACT:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "- " );
		goto bin;
	case OP_XOR:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "^ " );
bin:
		mged_tree_describe( vls, tp->tr_b.tb_left, 1, lvl+1 );
		mged_tree_describe( vls, tp->tr_b.tb_right, 0, lvl+1 );
		return;

		/* This node is known to be a unary op */
	case OP_NOT:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "! " );
		goto unary;
	case OP_GUARD:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "G " );
		goto unary;
	case OP_XNOP:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "X " );
unary:
		mged_tree_describe( vls, tp->tr_b.tb_left, 1, lvl+1 );
		return;

	case OP_NOP:
		if(!indented) bu_vls_spaces( vls, 2*lvl );
		bu_vls_strcat( vls, "NOP\n" );
		return;

	default:
		bu_log("mged_tree_describe: bad op %d\n", tp->tr_op);
		bu_bomb("mged_tree_describe\n");
	}
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
	register int	i;
	int			id;
	struct bu_external	ext;
	struct rt_db_internal	intern;
	mat_t			ident;
	struct bu_vls		str;

	bu_vls_init( &str );
	bu_vls_printf( outstrp, "%s:  ", dp->d_namep );
	RT_INIT_EXTERNAL(&ext);
	if( db_get_external( &ext, dp, dbip ) < 0 )  {
	  Tcl_AppendResult(interp, "db_get_external(", dp->d_namep,
			   ") failure\n", (char *)NULL);
	  return;
	}

	if( dp->d_flags & DIR_COMB )  {
		struct rt_comb_internal	*comb;

		/* Combination */
		bu_vls_printf( outstrp, "%s (len %d) ", dp->d_namep, 
			       dp->d_len-1 );

		if( rt_comb_v4_import( &intern, &ext, NULL ) < 0 ||
		    intern.idb_type != ID_COMBINATION )  {
			Tcl_AppendResult(interp, "rt_comb_v4_import(",
				dp->d_namep, ") failure\n", (char *)NULL);
			goto out;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);
		mged_comb_describe( outstrp, comb );
		if( verbose )  {
			mged_tree_describe( outstrp, comb->tree, 0, 1 );
		} else {
			rt_pr_tree_vls( outstrp, comb->tree );
		}
		rt_comb_ifree( &intern );
		goto out;
	}

	id = rt_id_solid( &ext );
	mat_idn( ident );
	if( rt_functab[id].ft_import( &intern, &ext, ident ) < 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": database import error\n", (char *)NULL);
	  goto out;
	}

	if( rt_functab[id].ft_describe( &str, &intern,
	    verbose, base2local ) < 0 )
	  Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_functab[id].ft_ifree( &intern );
	bu_vls_vlscat( outstrp, &str );

out:
	db_free_external( &ext );

	bu_vls_free( &str );
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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
  }

  Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  bu_vls_init(&vls);
  start_catching_output(&vls);
		   
  bu_log("STATE=%s, ", state_str[state] );
  bu_log("Viewscale=%f (%f mm)\n", Viewscale*base2local, Viewscale);
  bu_log("base2local=%f\n", base2local);
  mat_print("toViewcenter", toViewcenter);
  mat_print("Viewrot", Viewrot);
  mat_print("model2view", model2view);
  mat_print("view2model", view2model);
  if( state != ST_VIEW )  {
    mat_print("model2objview", model2objview);
    mat_print("objview2model", objview2model);
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
#if 0
  attach( dmp->dm_name );	/* reattach */
  dmaflag = 1;		/* causes refresh() */
  return CMD_OK;
#else
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  return reattach();
#endif
}

int
f_refresh(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	dmaflag = 1;		/* causes refresh() */
	return TCL_OK;
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
  int status = TCL_OK;
  fastf_t o_twist;
  fastf_t twist;
  char *av[5];
  struct bu_vls vls;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  /* Check for -i option */
  if(argv[1][0] == '-' && argv[1][1] == 'i'){
    iflag = 1;  /* treat arguments as incremental values */
    ++argv;
    --argc;
  }

  /* grab old twist angle before it's lost */
  o_twist = curr_dm_list->s_info->twist;

  /* set view using azimuth and elevation angles */
  if(iflag)
    setview( 270.0 + atof(argv[2]) + curr_dm_list->s_info->elevation,
	     0.0,
	     270.0 - atof(argv[1]) - curr_dm_list->s_info->azimuth);
  else
    setview( 270.0 + atof(argv[2]), 0.0, 270.0 - atof(argv[1]) );

  if(argc == 4){ /* twist angle supplied */
    twist = atof(argv[3]);
#if 0
    if(iflag)
      status = mged_vrot(0.0, 0.0, -o_twist - twist);
    else
      status = mged_vrot(0.0, 0.0, -twist);
#else
    bu_vls_init(&vls);
    if(iflag)
      bu_vls_printf(&vls, "knob -i az %f", -o_twist - twist);
    else
      bu_vls_printf(&vls, "knob -i az %f", -twist);

    status = Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
#endif
  }

  return status;
}

int
f_release(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if(argc == 2)
    return release(argv[1], 1);

  return release(NULL, 1);
}

/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all displays
 * which contain the specified object in their 'path'
 */
void
eraseobj( dp )
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
#if 0
			dmp->dm_viewchange( dmp, DM_CHGV_DEL, sp );
#endif
			rt_memfree( &(dmp->dm_map), sp->s_bytes, (unsigned long)sp->s_addr );
			BU_LIST_DEQUEUE(&sp->l);
			FREE_SOLID(sp, &FreeSolid.l);

			break;
		}
		sp = nsp;
	}
	if( dp->d_addr == RT_DIR_PHONY_ADDR )  {
		if( db_dirdelete( dbip, dp ) < 0 )  {
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
    bu_vls_printf(&vls, "  color=(%d,%d,%d) %d,%d,%d i=%d\n",
		  sp->s_basecolor[0],
		  sp->s_basecolor[1],
		  sp->s_basecolor[2],
		  sp->s_color[0],
		  sp->s_color[1],
		  sp->s_color[2],
		  sp->s_dmindex );

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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

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
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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
 
  dmaflag = 1;	/* values changed so update faceplate */
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
  char	*cmd;/* = argv[1];*/
  static int aslewflag = 0;
  int view_flag = 0;  /* force view interpretation */
  vect_t	aslew;
  int iknob = 0;
  int do_tran = 0;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  /* print the current values */
  if(argc == 1){
    struct bu_vls vls;

    bu_vls_init(&vls);

    if(mged_variables.rateknobs){
      bu_vls_printf(&vls, "x = %f\n", rate_rotate[X]);
      bu_vls_printf(&vls, "y = %f\n", rate_rotate[Y]);
      bu_vls_printf(&vls, "z = %f\n", rate_rotate[Z]);
      bu_vls_printf(&vls, "S = %f\n", rate_zoom);
      bu_vls_printf(&vls, "X = %f\n", rate_slew[X]);
      bu_vls_printf(&vls, "Y = %f\n", rate_slew[Y]);
      bu_vls_printf(&vls, "Z = %f\n", rate_slew[Z]);
    }else{
      bu_vls_printf(&vls, "ax = %f\n", absolute_rotate[X]);
      bu_vls_printf(&vls, "ay = %f\n", absolute_rotate[Y]);
      bu_vls_printf(&vls, "az = %f\n", absolute_rotate[Z]);
      bu_vls_printf(&vls, "aS = %f\n", absolute_zoom);
      bu_vls_printf(&vls, "aX = %f\n", absolute_slew[X]);
      bu_vls_printf(&vls, "aY = %f\n", absolute_slew[Y]);
      bu_vls_printf(&vls, "aZ = %f\n", absolute_slew[Z]);
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

/*XXX Should be returning TCL_OK */
    return TCL_ERROR;
  }

  /* Check for options */
  {
    int c;

    bu_optind = 1;
    while((c = bu_getopt(argc, argv, "iv")) != EOF){
      switch(c){
      case 'i':
	iknob = 1;
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
  

  for(--argc, ++argv; argc; --argc, ++argv){
    cmd = *argv;
    
    if( strcmp( cmd, "zap" ) == 0 || strcmp( cmd, "zero" ) == 0 )  {
      char *av[3];

      av[0] = "adc";
      av[1] = "reset";
      av[2] = (char *)NULL;

      VSETALL( rate_rotate, 0 );
      VSETALL( rate_slew, 0 );
      if(state != ST_VIEW){
	VSETALL( edit_rate_rotate, 0 );
	VSETALL( edit_rate_tran, 0 );
	edit_rate_scale = 0.0;
      }
      rate_zoom = 0;
		
      (void)f_adc( clientData, interp, 2, av );

      if(knob_hook)
	knob_hook();
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
      if( f < -1.0 )
	f = -1.0;
      else if( f > 1.0 )
	f = 1.0;

      switch( cmd[0] )  {
      case 'x':
	if(iknob){
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[X] += f;
	  else
	    rate_rotate[X] += f;
	}else{
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[X] = f;
	  else
	    rate_rotate[X] = f;
	}
	break;
      case 'y':
	if(iknob){
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[Y] += f;
	  else
	    rate_rotate[Y] += f;
	}else{
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[Y] = f;
	  else
	    rate_rotate[Y] = f;
	}
	break;
      case 'z':
	if(iknob){
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[Z] += f;
	  else
	    rate_rotate[Z] += f;
	}else{
	  if(EDIT_ROTATE && mged_variables.edit && !view_flag)
	    edit_rate_rotate[Z] = f;
	  else
	    rate_rotate[Z] = f;
	}
      break;
    case 'X':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[X] += f;
	else
	  rate_slew[X] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[X] = f;
	else
	  rate_slew[X] = f;
      }
      break;
    case 'Y':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[Y] += f;
	else
	  rate_slew[Y] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[Y] = f;
	else
	  rate_slew[Y] = f;
      }
      break;
    case 'Z':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[Z] += f;
	else
	  rate_slew[Z] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_rate_tran[Z] = f;
	else
	  rate_slew[Z] = f;
      }
      break;
    case 'S':
      if(iknob)
	rate_zoom += f;
      else
	rate_zoom = f;
      break;
    default:
      goto usage;
    }
  } else if( cmd[0] == 'a' && cmd[1] != '\0' && cmd[2] == '\0' ) {
    switch( cmd[1] ) {
    case 'x':
      if(iknob){
	if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	  edit_absolute_rotate[X] += f;
	  (void)irot(edit_absolute_rotate[X],
		     edit_absolute_rotate[Y],
		     edit_absolute_rotate[Z], 0);
	}else {
	  if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	    (void)irot(f, 0.0, 0.0, 1);
	    edit_absolute_rotate[X] += f;
	  }else{
	    (void)mged_vrot(f, 0.0, 0.0);
	    absolute_rotate[X] += f;
	  }
	}
      }else{
	  if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	    edit_absolute_rotate[X] = f;
	    (void)irot(edit_absolute_rotate[X],
		       edit_absolute_rotate[Y],
		       edit_absolute_rotate[Z], 0);
	  }else {
	    if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	      (void)irot(f - edit_absolute_rotate[X], 0.0, 0.0, 1);
	      edit_absolute_rotate[X] = f;
	    }else{
	      mged_vrot(f - absolute_rotate[X], 0.0, 0.0);
	      absolute_rotate[X] = f;
	    }
	    
	  }
      }
	  
	  /* wrap around */
      if(EDIT_ROTATE && mged_variables.edit && !view_flag){
	if(edit_absolute_rotate[X] < -180.0)
	  edit_absolute_rotate[X] = edit_absolute_rotate[X] + 360.0;
	else if(edit_absolute_rotate[X] > 180.0)
	  edit_absolute_rotate[X] = edit_absolute_rotate[X] - 360.0;
      }else{
	if(absolute_rotate[X] < -180.0)
	  absolute_rotate[X] = absolute_rotate[X] + 360.0;
	else if(absolute_rotate[X] > 180.0)
	  absolute_rotate[X] = absolute_rotate[X] - 360.0;
      }

      break;
    case 'y':
      if(iknob){
	if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	  edit_absolute_rotate[Y] += f;
	  (void)irot(edit_absolute_rotate[X],
		     edit_absolute_rotate[Y],
		     edit_absolute_rotate[Z], 0);
	}else {
	  if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	    (void)irot(0.0, f, 0.0, 1);
	    edit_absolute_rotate[Y] += f;
	  }else{
	    (void)mged_vrot(0.0, f, 0.0);
	    absolute_rotate[Y] += f;
	  }
	}
      }else{
	  if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	    edit_absolute_rotate[Y] = f;
	    (void)irot(edit_absolute_rotate[X],
		       edit_absolute_rotate[Y],
		       edit_absolute_rotate[Z], 0);
	  }else {
	    if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	      (void)irot(0.0, f - edit_absolute_rotate[Y], 0.0, 1);
	      edit_absolute_rotate[Y] = f;
	    }else{
	      mged_vrot(0.0, f - absolute_rotate[Y], 0.0);
	      absolute_rotate[Y] = f;
	    }
	    
	  }
      }
	  
	  /* wrap around */
      if(EDIT_ROTATE && mged_variables.edit && !view_flag){
	if(edit_absolute_rotate[Y] < -180.0)
	  edit_absolute_rotate[Y] = edit_absolute_rotate[Y] + 360.0;
	else if(edit_absolute_rotate[Y] > 180.0)
	  edit_absolute_rotate[Y] = edit_absolute_rotate[Y] - 360.0;
      }else{
	if(absolute_rotate[Y] < -180.0)
	  absolute_rotate[Y] = absolute_rotate[Y] + 360.0;
	else if(absolute_rotate[Y] > 180.0)
	  absolute_rotate[Y] = absolute_rotate[Y] - 360.0;
      }

      break;
    case 'z':
      if(iknob){
	if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	  edit_absolute_rotate[Z] += f;
	  (void)irot(edit_absolute_rotate[X],
		     edit_absolute_rotate[Y],
		     edit_absolute_rotate[Z], 0);
	}else {
	  if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	    (void)irot(0.0, 0.0, f, 1);
	    edit_absolute_rotate[Z] += f;
	  }else{
	    (void)mged_vrot(0.0, 0.0, f);
	    absolute_rotate[Z] += f;
	  }
	}
      }else{
	  if(SEDIT_ROTATE && mged_variables.edit && !view_flag){
	    edit_absolute_rotate[Z] = f;
	    (void)irot(edit_absolute_rotate[X],
		       edit_absolute_rotate[Y],
		       edit_absolute_rotate[Z], 0);
	  }else {
	    if(OEDIT_ROTATE && mged_variables.edit && !view_flag){
	      (void)irot(0.0, 0.0, f - edit_absolute_rotate[Z], 1);
	      edit_absolute_rotate[Z] = f;
	    }else{
	      mged_vrot(0.0, 0.0, f - absolute_rotate[Z]);
	      absolute_rotate[Z] = f;
	    }
	    
	  }
      }
	  
	  /* wrap around */
      if(EDIT_ROTATE && mged_variables.edit && !view_flag){
	if(edit_absolute_rotate[Z] < -180.0)
	  edit_absolute_rotate[Z] = edit_absolute_rotate[Z] + 360.0;
	else if(edit_absolute_rotate[Z] > 180.0)
	  edit_absolute_rotate[Z] = edit_absolute_rotate[Z] - 360.0;
      }else{
	if(absolute_rotate[Z] < -180.0)
	  absolute_rotate[Z] = absolute_rotate[Z] + 360.0;
	else if(absolute_rotate[Z] > 180.0)
	  absolute_rotate[Z] = absolute_rotate[Z] - 360.0;
      }

      break;
    case 'X':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[X] += f;
	else
	  absolute_slew[X] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[X] = f;
	else
	  absolute_slew[X] = f;
      }
      
      do_tran = 1;
      break;
    case 'Y':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[Y] += f;
	else
	  absolute_slew[Y] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[Y] = f;
	else
	  absolute_slew[Y] = f;
      }
      
      do_tran = 1;
      break;
    case 'Z':
      if(iknob){
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[Z] += f;
	else
	  absolute_slew[Z] += f;
      }else{
	if(EDIT_TRAN && mged_variables.edit && !view_flag)
	  edit_absolute_tran[Z] = f;
	else
	  absolute_slew[Z] = f;
      }
      
      do_tran = 1;
      break;
    case 'S':
      if(iknob){
	if(EDIT_SCALE)
	  edit_absolute_scale += f;
	else
	  absolute_zoom += f;
      }else{
	if(EDIT_SCALE)
	  edit_absolute_scale = f;
	else
	  absolute_zoom = f;
      }

      abs_zoom();
      break;
    default:
      goto usage;
    }
  } else if( strcmp( cmd, "xadc" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "x";
	  av[3] = NULL;

	  if(iknob)
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

	  if(iknob)
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

	  if(iknob)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%f", 45.0*(1.0-(double)i/2047.0));
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "ang2" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "a2";
	  av[3] = NULL;

	  if(iknob)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%f", 45.0*(1.0-(double)i/2047.0));
	  (void)f_adc(clientData, interp, 3, av);
	} else if( strcmp( cmd, "distadc" ) == 0 )  {
	  char *av[4];
	  char    sval[32];

	  av[1] = "dst";
	  av[3] = NULL;

	  if(iknob)
	    av[0] = "iadc";
	  else
	    av[0] = "adc";

	  av[2] = sval;
	  sprintf(sval, "%f", ((double)i/2047.0 + 1.0)*Viewscale * base2local * M_SQRT2_DIV2);
	  (void)f_adc(clientData, interp, 3, av);
	} else {
usage:
	  Tcl_AppendResult(interp,
		"knob: x,y,z for rotation, S for scale, X,Y,Z for slew (rates, range -1..+1)\n",
		"knob: ax,ay,az for absolute rotation, aS for absolute scale,\n",
		"knob: aX,aY,aZ for absolute slew.  calibrate to set current slew to 0\n",
		"knob: xadc, yadc, ang1, ang2, distadc (values, range -2048..+2047)\n",
		"knob: zero (cancel motion)\n", (char *)NULL);

	  return TCL_ERROR;
	}
      }
  }

  if(do_tran)
    (void)tran(view_flag);
 
  check_nonzero_rates();
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

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

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
	    sec = mged_nrm_tol * rt_radtodeg;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  mged_nrm_tol * rt_radtodeg, deg, min, sec );
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
			  acos(mged_tol.perp)*rt_radtodeg);
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
			mged_nrm_tol = f * rt_degtorad;
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
  char *av[3];

  av[0] = "zoom";
  av[1] = "1";
  av[2] = NULL;

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

  (void)f_zoom((ClientData)NULL, interp, 2, av);
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
  double	val;
  point_t new_pos;
  point_t old_pos;
  point_t diff;
  point_t model_pos;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;
#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(model_pos, view2model, absolute_slew);
#endif
  val = atof(argv[1]);
  if( val < SMALL_FASTF || val > INFINITY )  {
    Tcl_AppendResult(interp, "zoom: scale factor out of range\n", (char *)NULL);
    return TCL_ERROR;
  }

  if( Viewscale < SMALL_FASTF || Viewscale > INFINITY )
    return TCL_ERROR;

  Viewscale /= val;
  new_mats();

  absolute_zoom = 1.0 - Viewscale / i_Viewscale;
  if(absolute_zoom < 0.0)
    absolute_zoom /= 9.0;
#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(absolute_slew, model2view, model_pos);
#endif
  VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
  MAT4X3PNT(absolute_slew, model2view, new_pos);

  return TCL_OK;
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

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

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

    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

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
