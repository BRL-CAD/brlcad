/*
 *			D O D R A W . C
 *
 * Functions -
 *	drawtrees	Add a set of tree hierarchies to the active set
 *	drawHsolid	Manage the drawing of a COMGEOM solid
 *	pathHmat	Find matrix across a given path
 *	replot_original_solid	Replot vector list for a solid
 *	replot_modified_solid	Replot solid, given matrix and db record.
 *	invent_solid		Turn list of vectors into phony solid
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

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

#include "../librt/debug.h"	/* XXX */

extern void color_soltab();

void		cvt_vlblock_to_solids();
void		drawH_part2();
extern void	(*nmg_plot_anim_upcall)();
extern void	(*nmg_vlblock_anim_upcall)();
extern void	(*nmg_mged_debug_display_hack)();
extern double       frametime;
int	no_memory;	/* flag indicating memory for drawing is used up */
long	nvectors;	/* number of vectors drawn so far */

/*
 *  This is just like the rt_initial_tree_state in librt/tree.c,
 *  except that the default color is red instead of white.
 *  This avoids confusion with illuminate mode.
 *  Red is a one-gun color, avoiding convergence problems too.
 */
struct db_tree_state	mged_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0,		/* region, air, gmater */
	100,			/* GIFT los */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
		1.0, 0.0, 0.0,		/* color, RGB */
		0,			/* override */
		DB_INH_LOWER,		/* color inherit */
		DB_INH_LOWER,		/* mater inherit */
		"",			/* material name */
		""			/* material params */
#if __STDC__
	}
#endif
	,
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

static int		mged_draw_nmg_only;
static int		mged_nmg_triangulate;
static int		mged_draw_wireframes;
static int		mged_draw_normals;
static int		mged_draw_solid_lines_only=0;
static int		mged_draw_no_surfaces = 0;
static int		mged_shade_per_vertex_normals=0;
static struct model	*mged_nmg_model;
struct rt_tess_tol	mged_ttol;	/* XXX needs to replace mged_abs_tol, et.al. */

extern struct rt_tol		mged_tol;	/* from ged.c */

/*
 *		M G E D _ P L O T _ A N I M _ U P C A L L _ H A N D L E R
 *
 *  Used via upcall by routines deep inside LIBRT, to have a UNIX-plot
 *  file dyanmicly overlaid on the screen.
 *  This can be used to provide a very easy to program diagnostic
 *  animation capability.
 *  Alas, no wextern keyword to make this a little less indirect.
 */
void
mged_plot_anim_upcall_handler( file, us )
char	*file;
long	us;		/* microseconds of extra delay */
{
	char *av[3];

	/* Overlay plot file */
	av[0] = "overlay";
	av[1] = file;
	av[2] = NULL;
	(void)f_overlay((ClientData)NULL, interp, 2, av);

	do {
		event_check( 1 );	/* Take any device events */
		refresh();		/* Force screen update */
		us -= frametime * 1000000;
	} while (us > 0);

#if 0
	/* Extra delay between screen updates, for more viewing time */
	/* Use /dev/tty to select on, because stdin may be a file */
	if(us)  {
		int	fd;

		if( (fd = open("/dev/tty", 2)) < 0 )  {
			perror("/dev/tty");
		} else {
			struct timeval tv;
			fd_set readfds;

			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);
			tv.tv_sec = 0L;
			tv.tv_usec = us;

			select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &tv );
			close(fd);
		}
	}
#endif
}

/*
 *		M G E D _ V L B L O C K _ A N I M _ U P C A L L _ H A N D L E R
 *
 *  Used via upcall by routines deep inside LIBRT, to have a UNIX-plot
 *  file dyanmicly overlaid on the screen.
 *  This can be used to provide a very easy to program diagnostic
 *  animation capability.
 *  Alas, no wextern keyword to make this a little less indirect.
 */
void
mged_vlblock_anim_upcall_handler( vbp, us, copy )
struct rt_vlblock	*vbp;
long		us;		/* microseconds of extra delay */
int		copy;
{

	cvt_vlblock_to_solids( vbp, "_PLOT_OVERLAY_", copy );


	do  {
		event_check( 1 );	/* Take any device events */
		refresh();		/* Force screen update */
		us -= frametime * 1000000;
	} while (us > 0);
#if 0
	/* Extra delay between screen updates, for more viewing time */
	/* Use /dev/tty to select on, because stdin may be a file */
	if(us)  {
		int	fd;
		if( (fd = open("/dev/tty", 2)) < 0 )  {
			perror("/dev/tty");
		} else {
			struct timeval tv;
			fd_set readfds;

			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);
			tv.tv_sec = 0L;
			tv.tv_usec = us;

			select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &tv );
			close(fd);
		}
	}
#endif
}
static void
hack_for_lee()
{
	event_check( 1 );	/* Take any device events */

	refresh();		/* Force screen update */
}

/*
 *			M G E D _ W I R E F R A M E _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *mged_wireframe_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	return( curtree );
}

/*
 *			M G E D _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *mged_wireframe_leaf( tsp, pathp, ep, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	struct rt_db_internal	intern;
	union tree	*curtree;
	int		dashflag;		/* draw with dashed lines */
	struct bu_list	vhead;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);

	BU_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_wireframe_leaf(", rt_functab[id].ft_name,
			   ") path='", sofar, "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( mged_draw_solid_lines_only )
		dashflag = 0;
	else
		dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER) );

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, tsp->ts_mat ) < 0 )  {
	  Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
			   ":  solid import failure\n", (char *)NULL);

	  if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	  return(TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[id].ft_plot(
	    &vhead,
	    &intern,
	    tsp->ts_ttol, tsp->ts_tol ) < 0 )  {
	  Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
			   ": plot failure\n", (char *)NULL);
	  rt_functab[id].ft_ifree( &intern );
	  return(TREE_NULL);		/* ERROR */
	}

	/*
	 * XXX HACK CTJ - drawH_part2 sets the default color of a
	 * solid by looking in tps->ts_mater.ma_color, for pseudo
	 * solids, this needs to be something different and drawH
	 * has no idea or need to know what type of solid this is.
	 */
	if (intern.idb_type == ID_GRIP) {
		int r,g,b;
		r= tsp->ts_mater.ma_color[0];
		g= tsp->ts_mater.ma_color[1];
		b= tsp->ts_mater.ma_color[2];
		tsp->ts_mater.ma_color[0] = 0;
		tsp->ts_mater.ma_color[1] = 128;
		tsp->ts_mater.ma_color[2] = 128;
		drawH_part2( dashflag, &vhead, pathp, tsp, SOLID_NULL );
		tsp->ts_mater.ma_color[0] = r;
		tsp->ts_mater.ma_color[1] = g;
		tsp->ts_mater.ma_color[2] = b;
	} else {
		drawH_part2( dashflag, &vhead, pathp, tsp, SOLID_NULL );
	}
	rt_functab[id].ft_ifree( &intern );

	/* Indicate success by returning something other than TREE_NULL */
	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;

	return( curtree );
}
/* XXX Grotesque, shameless hack */
static int mged_do_not_draw_nmg_solids_during_debugging = 0;
static int mged_draw_edge_uses=0;
static struct rt_vlblock	*mged_draw_edge_uses_vbp;

/*
 *			M G E D _ N M G _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *mged_nmg_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	int			failed;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BU_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_nmg_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	if ( ! mged_draw_nmg_only ) {
		failed = nmg_boolean( curtree, *tsp->ts_m, tsp->ts_tol );
		if( failed )  {
			db_free_tree( curtree );
			return (union tree *)NULL;
		}
	}
	else if( curtree->tr_op != OP_NMG_TESS )
	{
	  Tcl_AppendResult(interp, "Cannot use '-d' option when Boolean evaluation is required\n", (char *)NULL);
	  db_free_tree( curtree );
	  return (union tree *)NULL;
	}
	r = curtree->tr_d.td_r;
	NMG_CK_REGION(r);

	if( mged_do_not_draw_nmg_solids_during_debugging && r )  {
		db_free_tree( curtree );
		return (union tree *)NULL;
	}

	if (mged_nmg_triangulate) {
		nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
	}

	if( r != 0 )  {
		int	style;
		/* Convert NMG to vlist */
		NMG_CK_REGION(r);

		if( mged_draw_wireframes )  {
			/* Draw in vector form */
			style = NMG_VLIST_STYLE_VECTOR;
		} else {
			/* Default -- draw polygons */
			style = NMG_VLIST_STYLE_POLYGON;
		}
		if( mged_draw_normals )  {
			style |= NMG_VLIST_STYLE_VISUALIZE_NORMALS;
		}
		if( mged_shade_per_vertex_normals )  {
			style |= NMG_VLIST_STYLE_USE_VU_NORMALS;
		}
		if( mged_draw_no_surfaces )  {
			style |= NMG_VLIST_STYLE_NO_SURFACES;
		}
		nmg_r_to_vlist( &vhead, r, style );

		drawH_part2( 0, &vhead, pathp, tsp, SOLID_NULL );

		if( mged_draw_edge_uses )  {
			nmg_vlblock_r(mged_draw_edge_uses_vbp, r, 1);
		}
		/* NMG region is no longer necessary, only vlist remains */
		db_free_tree( curtree );
		return (union tree *)NULL;
	}

	/* Return tree -- it needs to be freed (by caller) */
	return curtree;
}

/*
 *			D R A W T R E E S
 *
 *  This routine is MGED's analog of rt_gettrees().
 *  Add a set of tree hierarchies to the active set.
 *  Note that argv[0] should be ignored, it has the command name in it.
 *
 *  Kind =
 *	1	regular wireframes
 *	2	big-E
 *	3	NMG polygons
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
drawtrees( argc, argv, kind )
int	argc;
char	**argv;
int	kind;
{
	int		i;
	register int	c;
	int		ncpu;
	int		mged_nmg_use_tnurbs = 0;

	RT_CHECK_DBI(dbip);

	if( argc <= 0 )  return(-1);	/* FAIL */

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;
	mged_draw_nmg_only = 0;	/* no booleans */
	mged_nmg_triangulate = 1;
	mged_draw_wireframes = 0;
	mged_draw_normals = 0;
	mged_draw_edge_uses = 0;
	mged_draw_solid_lines_only = 0;
	mged_shade_per_vertex_normals = 0;
	mged_draw_no_surfaces = 0;

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"dnqstuvwSTP:")) != EOF )  {
		switch(c)  {
		case 'u':
			mged_draw_edge_uses = 1;
			break;
		case 's':
			mged_draw_solid_lines_only = 1;
			break;
		case 't':
			mged_nmg_use_tnurbs = 1;
			break;
		case 'v':
			mged_shade_per_vertex_normals = 1;
			break;
		case 'w':
			mged_draw_wireframes = 1;
			break;
		case 'S':
			mged_draw_no_surfaces = 1;
			break;
		case 'T':
			mged_nmg_triangulate = 0;
			break;
		case 'n':
			mged_draw_normals = 1;
			break;
		case 'P':
			ncpu = atoi(optarg);
			break;
		case 'q':
			mged_do_not_draw_nmg_solids_during_debugging = 1;
			break;
		case 'd':
			mged_draw_nmg_only = 1;
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "option '%c' unknown\n", c);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  Tcl_AppendResult(interp, "Usage: ev [-dnqstuvwST] [-P ncpu] object(s)\n\
	-d draw nmg without performing boolean operations\n\
	-w draw wireframes (rather than polygons)\n\
	-n draw surface normals as little 'hairs'\n\
	-s draw solid lines only (no dot-dash for subtract and intersect)\n\
	-t Perform CSG-to-tNURBS conversion\n\
	-v shade using per-vertex normals, when present.\n\
	-u debug: draw edgeuses\n\
	-S draw tNURBs with trimming curves only, no surfaces.\n\
	-T debug: disable triangulator\n", (char *)NULL);
		break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Establish upcall interfaces for use by bottom of NMG library */
	nmg_plot_anim_upcall = mged_plot_anim_upcall_handler;
	nmg_vlblock_anim_upcall = mged_vlblock_anim_upcall_handler;
	nmg_mged_debug_display_hack = hack_for_lee;

	/* Establish tolerances */
	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	switch( kind )  {
	default:
	  Tcl_AppendResult(interp, "ERROR, bad kind\n", (char *)NULL);
	  return(-1);
	case 1:		/* Wireframes */
		i = db_walk_tree( dbip, argc, (CONST char **)argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_wireframe_region_end,
			mged_wireframe_leaf );
		if( i < 0 )  return(-1);
		break;
	case 2:		/* Big-E */
#	    if 0
		i = db_walk_tree( dbip, argc, argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_bigE_region_end,
			mged_bigE_leaf );
		if( i < 0 )  return(-1);
		break;
#	    else
		Tcl_AppendResult(interp, "drawtrees:  can't do big-E here\n", (char *)NULL);
		return(-1);
#	    endif
	case 3:
	  {
		/* NMG */
	    Tcl_AppendResult(interp, "\
Please note that the NMG library used by this command is experimental.\n\
A production implementation will exist in the maintenance release.\n", (char *)NULL);
	  	mged_nmg_model = nmg_mm();
		mged_initial_tree_state.ts_m = &mged_nmg_model;
	  	if (mged_draw_edge_uses) {
		  Tcl_AppendResult(interp, "Doing the edgeuse thang (-u)\n", (char *)NULL);
		  mged_draw_edge_uses_vbp = rt_vlblock_init();
	  	}

		i = db_walk_tree( dbip, argc, (CONST char **)argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_nmg_region_end,
	  		mged_nmg_use_tnurbs ?
	  			nmg_booltree_leaf_tnurb :
				nmg_booltree_leaf_tess
			);

	  	if (mged_draw_edge_uses) {
	  		cvt_vlblock_to_solids(mged_draw_edge_uses_vbp, "_EDGEUSES_", 0);
	  		rt_vlblock_free(mged_draw_edge_uses_vbp);
			mged_draw_edge_uses_vbp = (struct rt_vlblock *)NULL;
 	  	}

		/* Destroy NMG */
		nmg_km( mged_nmg_model );

		if( i < 0 )  return(-1);
	  	break;
	  }
	}
	return(0);	/* OK */
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate rt_vlist_rpp() routine, for librt/vlist.c
 */
void
mged_bound_solid( sp )
register struct solid *sp;
{
	register struct rt_vlist	*vp;
	register double			xmax, ymax, zmax;
	register double			xmin, ymin, zmin;

	xmax = ymax = zmax = -INFINITY;
	xmin = ymin = zmin =  INFINITY;
	sp->s_vlen = 0;
	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	j;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( j = 0; j < nused; j++,cmd++,pt++ )  {
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
			case RT_VLIST_POLY_VERTNORM:
				/* Has normal vector, not location */
				break;
			case RT_VLIST_LINE_MOVE:
			case RT_VLIST_LINE_DRAW:
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
				V_MIN( xmin, (*pt)[X] );
				V_MAX( xmax, (*pt)[X] );
				V_MIN( ymin, (*pt)[Y] );
				V_MAX( ymax, (*pt)[Y] );
				V_MIN( zmin, (*pt)[Z] );
				V_MAX( zmax, (*pt)[Z] );
				break;
			default:
			  {
			    struct bu_vls tmp_vls;

			    bu_vls_init(&tmp_vls);
			    bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", *cmd);
			    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			    bu_vls_free(&tmp_vls);
			  }
			}
		}
		sp->s_vlen += nused;
	}

	sp->s_center[X] = (xmin + xmax) * 0.5;
	sp->s_center[Y] = (ymin + ymax) * 0.5;
	sp->s_center[Z] = (zmin + zmax) * 0.5;

	sp->s_size = xmax - xmin;
	V_MAX( sp->s_size, ymax - ymin );
	V_MAX( sp->s_size, zmax - zmin );
}

/*
 *			D R A W h _ P A R T 2
 *
 *  Once the vlist has been created, perform the common tasks
 *  in handling the drawn solid.
 *
 *  This routine must be prepared to run in parallel.
 */
void
drawH_part2( dashflag, vhead, pathp, tsp, existing_sp )
int			dashflag;
struct bu_list		*vhead;
struct db_full_path	*pathp;
struct db_tree_state	*tsp;
struct solid		*existing_sp;
{
	register struct solid *sp;
	register int	i;

	if( !existing_sp )  {
		if (pathp->fp_len > MAX_PATH) {
		  char *cp = db_path_to_string(pathp);

		  Tcl_AppendResult(interp, "dodraw: path too long, solid ignored.\n\t",
				   cp, "\n", (char *)NULL);
		  bu_free((genptr_t)cp, "Path string");
		  return;
		}
		/* Handling a new solid */
		GET_SOLID(sp, &FreeSolid.l);
	} else {
		/* Just updating an existing solid.
		 *  'tsp' and 'pathpos' will not be used
		 */
		sp = existing_sp;
	}


	/*
	 * Compute the min, max, and center points.
	 */
	BU_LIST_APPEND_LIST( &(sp->s_vlist), vhead );
	mged_bound_solid( sp );
	nvectors += sp->s_vlen;

	/*
	 *  If this solid is new, fill in it's information.
	 *  Otherwise, don't touch what is already there.
	 */
	if( !existing_sp )  {
		/* Take note of the base color */
		if( tsp )  {
			sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
			sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
			sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
		}
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;
		sp->s_Eflag = 0;	/* This is a solid */
		sp->s_last = pathp->fp_len-1;

		/* Copy path information */
		for( i=0; i<=sp->s_last; i++ ) {
			sp->s_path[i] = pathp->fp_names[i];
		}
		sp->s_regionid = tsp->ts_regionid;
	}
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( dmp, sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
		  no_memory = 1;
		  Tcl_AppendResult(interp, "draw: out of Displaylist\n" ,(char *)NULL);
		  sp->s_bytes = 0;	/* not drawn */
		} else {
		  sp->s_bytes = dmp->dmr_load(dmp, sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid is successfully drawn */
	if( !existing_sp )  {
		/* Add to linked list of solid structs */
		RES_ACQUIRE( &rt_g.res_model );
		BU_LIST_APPEND(HeadSolid.l.back, &sp->l);
		RES_RELEASE( &rt_g.res_model );
		dmp->dmr_viewchange( dmp, DM_CHGV_ADD, sp );
	} else {
		/* replacing existing solid -- struct already linked in */
		sp->s_iflag = UP;
		dmp->dmr_viewchange( dmp, DM_CHGV_REPL, sp );
	}
}

/*
 *  			P A T H h M A T
 *  
 *  Find the transformation matrix obtained when traversing
 *  the arc indicated in sp->s_path[] to the indicated depth.
 *  Be sure to omit s_path[sp->s_last] -- it's a solid.
 */
void
pathHmat( sp, matp, depth )
register struct solid *sp;
matp_t matp;
{
	register union record	*rp;
	register struct directory *parentp;
	register struct directory *kidp;
	register int		j;
	auto mat_t		tmat;
	register int		i;

	mat_idn( matp );
	for( i=0; i <= depth; i++ )  {
		parentp = sp->s_path[i];
		kidp = sp->s_path[i+1];
		if( !(parentp->d_flags & DIR_COMB) )  {
		  Tcl_AppendResult(interp, "pathHmat:  ", parentp->d_namep,
				   " is not a combination\n", (char *)NULL);
		  return;		/* ERROR */
		}
		if( (rp = db_getmrec( dbip, parentp )) == (union record *)0 )
			return;		/* ERROR */
		for( j=1; j < parentp->d_len; j++ )  {
			static mat_t xmat;	/* temporary fastf_t matrix */

			/* Examine Member records */
			if( strcmp( kidp->d_namep, rp[j].M.m_instname ) != 0 )
				continue;

			/* convert matrix to fastf_t from disk format */
			rt_mat_dbmat( xmat, rp[j].M.m_mat );
			mat_mul( tmat, matp, xmat );
			mat_copy( matp, tmat );
			goto next_level;
		}
		Tcl_AppendResult(interp, "pathHmat: unable to follow ", parentp->d_namep,
				 "/", kidp->d_namep, "\n", (char *)NULL);
		return;			/* ERROR */
next_level:
		bu_free( (genptr_t)rp, "pathHmat recs");
	}
}

/*
 *			R E P L O T _ O R I G I N A L _ S O L I D
 *
 *  Given an existing solid structure that may have been subjected to
 *  solid editing, recompute the vector list, etc, to make the solid
 *  the same as it originally was.
 *
 *  Returns -
 *	-1	error
 *	 0	OK
 */
int
replot_original_solid( sp )
struct solid	*sp;
{
	struct rt_external	ext;
	struct rt_db_internal	intern;
	struct directory	*dp;
	mat_t			mat;
	int			id;

	dp = sp->s_path[sp->s_last];
	if( sp->s_Eflag )  {
	  Tcl_AppendResult(interp, "replot_original_solid(", dp->d_namep,
			   "): Unable to plot evaluated regions, skipping\n", (char *)NULL);
	  return(-1);
	}
	pathHmat( sp, mat, sp->s_last-1 );

	RT_INIT_EXTERNAL( &ext );
	if( db_get_external( &ext, dp, dbip ) < 0 )  return(-1);

	if( (id = rt_id_solid( &ext )) == ID_NULL )  {
	  Tcl_AppendResult(interp, "replot_original_solid() unable to identify type of solid ",
			   dp->d_namep, "\n", (char *)NULL);
	  db_free_external( &ext );
	  return(-1);
	}

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, &ext, mat ) < 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ":  solid import failure\n", (char *)NULL);
	  if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	  db_free_external( &ext );
	  return(-1);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( replot_modified_solid( sp, &intern, rt_identity ) < 0 )  {
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		db_free_external( &ext );
		return(-1);
	}
	if( intern.idb_type > ID_NULL && intern.idb_ptr )
		rt_functab[id].ft_ifree( &intern );
	db_free_external( &ext );
	return(0);
}

/*
 *  			R E P L O T _ M O D I F I E D _ S O L I D
 *
 *  Given the solid structure of a solid that has already been drawn,
 *  and a new database record and transform matrix,
 *  create a new vector list for that solid, and substitute.
 *  Used for solid editing mode.
 *
 *  Returns -
 *	-1	error
 *	 0	OK
 */
int
replot_modified_solid( sp, ip, mat )
struct solid			*sp;
struct rt_db_internal		*ip;
CONST mat_t			mat;
{
	struct rt_db_internal	intern;
	unsigned		addr, bytes;
	struct bu_list		vhead;

	BU_LIST_INIT( &vhead );

	if( sp == SOLID_NULL )  {
	  Tcl_AppendResult(interp, "replot_modified_solid() sp==NULL?\n", (char *)NULL);
	  return(-1);
	}

	/* Release existing vlist of this solid */
	RT_FREE_VLIST( &(sp->s_vlist) );

	/* Remember displaylist location of previous solid */
	addr = sp->s_addr;
	bytes = sp->s_bytes;

	/* Draw (plot) a normal solid */
	RT_CK_DB_INTERNAL( ip );

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	transform_editing_solid( &intern, mat, ip, 0 );

	if( rt_functab[ip->idb_type].ft_plot( &vhead, &intern, &mged_ttol, &mged_tol ) < 0 )  {
	  Tcl_AppendResult(interp, sp->s_path[sp->s_last]->d_namep,
			   ": re-plot failure\n", (char *)NULL);
	  return(-1);
	}
    	if( intern.idb_ptr )  rt_functab[ip->idb_type].ft_ifree( &intern );

	/* Write new displaylist */
	drawH_part2( sp->s_soldash, &vhead,
		(struct db_full_path *)0,
		(struct db_tree_state *)0, sp );

	/* Release previous chunk of displaylist. */
	if( bytes > 0 )
		rt_memfree( &(dmp->dmr_map), bytes, (unsigned long)addr );
	dmaflag = 1;
	return(0);
}

/*
 *			C V T _ V L B L O C K _ T O _ S O L I D S
 */
void
cvt_vlblock_to_solids( vbp, name, copy )
struct rt_vlblock	*vbp;
char			*name;
int			copy;
{
	int		i;
	char		shortname[32];
	char		namebuf[64];
	char		*av[4];

	strncpy( shortname, name, 16-6 );
	shortname[16-6] = '\0';
	/* Remove any residue colors from a previous overlay w/same name */
	av[0] = "kill";
	av[1] = "-f";
	av[2] = shortname;
	av[3] = NULL;
	(void)f_kill((ClientData)NULL, interp, 3, av);

	for( i=0; i < vbp->nused; i++ )  {
		if( vbp->rgb[i] == 0 )  continue;
		if( BU_LIST_IS_EMPTY( &(vbp->head[i]) ) )  continue;
		if( i== 0 )  {
			invent_solid( name, &vbp->head[0], vbp->rgb[0], copy );
			continue;
		}
		sprintf( namebuf, "%s%x",
			shortname, vbp->rgb[i] );
		invent_solid( namebuf, &vbp->head[i], vbp->rgb[i], copy );
	}
}

/*
 *			I N V E N T _ S O L I D
 *
 *  Invent a solid by adding a fake entry in the database table,
 *  adding an entry to the solid table, and populating it with
 *  the given vector list.
 *
 *  This parallels much of the code in dodraw.c
 */
int
invent_solid( name, vhead, rgb, copy )
char		*name;
struct bu_list	*vhead;
long		rgb;
int		copy;
{
	register struct directory	*dp;
	register struct solid		*sp;

	if( (dp = db_lookup( dbip,  name, LOOKUP_QUIET )) != DIR_NULL )  {
	  if( dp->d_addr != RT_DIR_PHONY_ADDR )  {
	    Tcl_AppendResult(interp, "invent_solid(", name,
			     ") would clobber existing database entry, ignored\n", (char *)NULL);
	    return(-1);
	  }
	  /* Name exists from some other overlay,
	   * zap any associated solids
	   */
	  eraseobj(dp);
	} else {
	  /* Need to enter phony name in directory structure */
	  dp = db_diradd( dbip,  name, RT_DIR_PHONY_ADDR, 0, DIR_SOLID );
	}

#if 0
	/* XXX need to get this going. */
	path.fp_names[0] = dp;
	state.ts_mater.ma_color[0] = ((rgb>>16) & 0xFF) / 255.0
	state.ts_mater.ma_color[1] = ((rgb>> 8) & 0xFF) / 255.0
	state.ts_mater.ma_color[2] = ((rgb    ) & 0xFF) / 255.0
	drawH_part2( 0, vhead, path, &state, SOLID_NULL );
#else

	/* Obtain a fresh solid structure, and fill it in */
	GET_SOLID(sp,&FreeSolid.l);

	if( copy )  {
		BU_LIST_INIT( &(sp->s_vlist) );
		rt_vlist_copy( &(sp->s_vlist), vhead );
	} else {
		/* For efficiency, just swipe the vlist */
		BU_LIST_APPEND_LIST( &(sp->s_vlist), vhead );
		BU_LIST_INIT(vhead);
	}
	mged_bound_solid( sp );
	nvectors += sp->s_vlen;

	/* set path information -- this is a top level node */
	sp->s_last = 0;
	sp->s_path[0] = dp;

	sp->s_iflag = DOWN;
	sp->s_soldash = 0;
	sp->s_Eflag = 1;		/* Can't be solid edited! */
	sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
	sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
	sp->s_color[2] = sp->s_basecolor[2] = (rgb    ) & 0xFF;
	sp->s_regionid = 0;
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( dmp, sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = rt_memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
		  no_memory = 1;
		  Tcl_AppendResult(interp, "invent_solid: out of Displaylist\n", (char *)NULL);
		  sp->s_bytes = 0;	/* not drawn */
		} else {
		  sp->s_bytes = dmp->dmr_load(dmp, sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid successfully drawn, add to linked list of solid structs */
	BU_LIST_APPEND(HeadSolid.l.back, &sp->l);
	dmp->dmr_viewchange( dmp, DM_CHGV_ADD, sp );
	dmp->dmr_colorchange(dmp);
#endif
	return(0);		/* OK */
}

static union tree	*mged_facetize_tree;

/*
 *			M G E D _ F A C E T I Z E _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *mged_facetize_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	struct bu_list		vhead;

	BU_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_facetize_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	RES_ACQUIRE( &rt_g.res_model );
	if( mged_facetize_tree )  {
		union tree	*tr;
		tr = (union tree *)bu_calloc(1, sizeof(union tree), "union tree");
		tr->magic = RT_TREE_MAGIC;
		tr->tr_op = OP_UNION;
		tr->tr_b.tb_regionp = REGION_NULL;
		tr->tr_b.tb_left = mged_facetize_tree;
		tr->tr_b.tb_right = curtree;
		mged_facetize_tree = tr;
	} else {
		mged_facetize_tree = curtree;
	}
	RES_RELEASE( &rt_g.res_model );

	/* Tree has been saved, and will be freed later */
	return( TREE_NULL );
}

/* facetize [opts] new_obj old_obj(s) */
int
f_facetize(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int			i;
	register int		c;
	int			ncpu;
	int			triangulate;
	char			*newname;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	struct directory	*dp;
	int			ngran;
	int			failed;
	int			mged_nmg_use_tnurbs = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	RT_CHECK_DBI(dbip);

	Tcl_AppendResult(interp, "Please note that the NMG library used by ",
			 "this command is experimental.\n", "A production implementation ",
			 "will exist in the maintenance release.\n", (char *)NULL);

	/* Establish tolerances */
	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;
	triangulate = 0;

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"tTP:")) != EOF )  {
		switch(c)  {
		case 'P':
			ncpu = atoi(optarg);
			break;
		case 'T':
			triangulate = 1;
			break;
		case 't':
			mged_nmg_use_tnurbs = 1;
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "option '%c' unknown\n", c);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls),
				     "Usage: facetize [-tT] [-P ncpu] object(s)\n",
				     "\t-t Perform CSG-to-tNURBS conversion\n",
				     "\t-T enable triangulator\n", (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }
		  break;
		}
	}
	argc -= optind;
	argv += optind;
	if( argc < 0 ){
	  Tcl_AppendResult(interp, "facetize: missing argument\n", (char *)NULL);
	  return TCL_ERROR;
	}

	newname = argv[0];
	argv++;
	argc--;
	if( argc < 0 ){
	  Tcl_AppendResult(interp, "facetize: missing argument\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( db_lookup( dbip, newname, LOOKUP_QUIET ) != DIR_NULL )  {
	  Tcl_AppendResult(interp, "error: solid '", newname,
			   "' already exists, aborting\n", (char *)NULL);
	  return TCL_ERROR;
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls,
			"facetize:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
			mged_abs_tol, mged_rel_tol, mged_nrm_tol );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}
	mged_facetize_tree = (union tree *)0;
  	mged_nmg_model = nmg_mm();
	mged_initial_tree_state.ts_m = &mged_nmg_model;

	i = db_walk_tree( dbip, argc, (CONST char **)argv,
		ncpu,
		&mged_initial_tree_state,
		0,			/* take all regions */
		mged_facetize_region_end,
  		mged_nmg_use_tnurbs ?
  			nmg_booltree_leaf_tnurb :
			nmg_booltree_leaf_tess
		);


	if( i < 0 )  {
	  Tcl_AppendResult(interp, "facetize: error in db_walk_tree()\n", (char *)NULL);
	  /* Destroy NMG */
	  nmg_km( mged_nmg_model );
	  return TCL_ERROR;
	}

	if( mged_facetize_tree )
	{
		/* Now, evaluate the boolean tree into ONE region */
		Tcl_AppendResult(interp, "facetize:  evaluating boolean expressions\n", (char *)NULL);

		failed = nmg_boolean( mged_facetize_tree, mged_nmg_model, &mged_tol );
	}
	else
		failed = 1;

	if( failed )  {
	  Tcl_AppendResult(interp, "facetize:  no resulting region, aborting\n", (char *)NULL);
	  if( mged_facetize_tree )
		db_free_tree( mged_facetize_tree );
	  mged_facetize_tree = (union tree *)NULL;
	  nmg_km( mged_nmg_model );
	  mged_nmg_model = (struct model *)NULL;
	  return TCL_ERROR;
	}
	/* New region remains part of this nmg "model" */
	NMG_CK_REGION( mged_facetize_tree->tr_d.td_r );
	Tcl_AppendResult(interp, "facetize:  ", mged_facetize_tree->tr_d.td_name,
			 "\n", (char *)NULL);

	/* Triangulate model, if requested */
	if( triangulate )
	{
	  Tcl_AppendResult(interp, "facetize:  triangulating resulting object\n", (char *)NULL);
	  nmg_triangulate_model( mged_nmg_model , &mged_tol );
	}

	Tcl_AppendResult(interp, "facetize:  converting NMG to database format\n", (char *)NULL);

	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_type = ID_NMG;
	intern.idb_ptr = (genptr_t)mged_nmg_model;
	mged_nmg_model = (struct model *)NULL;

	RT_INIT_EXTERNAL( &ext );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
	  Tcl_AppendResult(interp, "facetize(", newname, "):  solid export failure\n",
			   (char *)NULL);
	  if( intern.idb_ptr )  rt_functab[ID_NMG].ft_ifree( &intern );
	  db_free_external( &ext );
	  return TCL_ERROR;				/* FAIL */
	}
	rt_functab[ID_NMG].ft_ifree( &intern );
	mged_facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;

	ngran = (ext.ext_nbytes + sizeof(union record)-1)/sizeof(union record);
	if( (dp=db_diradd( dbip, newname, -1, ngran, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, ngran ) < 0 )  {
	    	TCL_ALLOC_ERR_return;
	}

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		db_free_external( &ext );
		TCL_WRITE_ERR_return;
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "facetize:  wrote %.2f Kbytes to database\n",
			ext.ext_nbytes / 1024.0 );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	db_free_external( &ext );

	/* Free boolean tree, and the regions in it */
	db_free_tree( mged_facetize_tree );
    	mged_facetize_tree = (union tree *)NULL;

	return TCL_OK;					/* OK */
}

/* bev [opts] new_obj obj1 op obj2 op obj3 ...
 *
 *	tesselates each operand object, then performs
 *	the Boolean evaluation, storing result in
 *	new_obj
 */
int
f_bev(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int			i;
	register int		c;
	int			ncpu;
	int			triangulate;
	char			*newname;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	struct directory	*dp;
	union tree		*tmp_tree;
	char			op;
	int			ngran;
	int			failed;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	RT_CHECK_DBI( dbip );

	Tcl_AppendResult(interp, "Please note that the NMG library used by this command",
			 "is experimental.\n A production implementation will exist",
			 "in the maintenance release.\n", (char *)NULL);

	/* Establish tolerances */
	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;
	triangulate = 0;

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"tP:")) != EOF )  {
		switch(c)  {
		case 'P':
			ncpu = atoi(optarg);
			break;
		case 't':
			triangulate = 1;
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "option '%c' unknown\n", c);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  break;
		}
	}
	argc -= optind;
	argv += optind;

	newname = argv[0];
	argv++;
	argc--;

	if( db_lookup( dbip, newname, LOOKUP_QUIET ) != DIR_NULL )  {
	  Tcl_AppendResult(interp, "error: solid '", newname,
			   "' already exists, aborting\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( argc < 1 )
	{
	  Tcl_AppendResult(interp, "Nothing to evaluate!!!\n", (char *)NULL);
	  return TCL_ERROR;
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls,
			"bev:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
			mged_abs_tol, mged_rel_tol, mged_nrm_tol);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	mged_facetize_tree = (union tree *)0;
  	mged_nmg_model = nmg_mm();
	mged_initial_tree_state.ts_m = &mged_nmg_model;

	op = ' ';
	tmp_tree = (union tree *)NULL;

	while( argc )
	{
		i = db_walk_tree( dbip, 1, (CONST char **)argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_facetize_region_end,
			nmg_booltree_leaf_tess );

		if( i < 0 )  {
		  Tcl_AppendResult(interp, "bev: error in db_walk_tree()\n", (char *)NULL);
		  /* Destroy NMG */
		  nmg_km( mged_nmg_model );
		  return TCL_ERROR;
		}
		argc--;
		argv++;

		if( tmp_tree && op != ' ' )
		{
			union tree *new_tree;

			BU_GETUNION( new_tree, tree );

			new_tree->magic = RT_TREE_MAGIC;
			new_tree->tr_b.tb_regionp = REGION_NULL;
			new_tree->tr_b.tb_left = tmp_tree;
			new_tree->tr_b.tb_right = mged_facetize_tree;

			switch( op )
			{
				case 'u':
				case 'U':
					new_tree->tr_op = OP_UNION;
					break;
				case '-':
					new_tree->tr_op = OP_SUBTRACT;
					break;
				case '+':
					new_tree->tr_op = OP_INTERSECT;
					break;
				default:
				  {
				    struct bu_vls tmp_vls;

				    bu_vls_init(&tmp_vls);
				    bu_vls_printf(&tmp_vls, "Unrecognized operator: (%c)\n" , op );
				    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls),
						     "Aborting\n", (char *)NULL);
				    bu_vls_free(&tmp_vls);
				    db_free_tree( mged_facetize_tree );
				    nmg_km( mged_nmg_model );
				    return TCL_ERROR;
				  }
			}

			tmp_tree = new_tree;
			mged_facetize_tree = (union tree *)NULL;
		}
		else if( !tmp_tree && op == ' ' )
		{
			/* just starting out */
			tmp_tree = mged_facetize_tree;
			mged_facetize_tree = (union tree *)NULL;
		}

		if( argc )
		{
			op = *argv[0];
			argc--;
			argv++;
		}
		else
			op = ' ';

	}

	if( tmp_tree )
	{
		/* Now, evaluate the boolean tree into ONE region */
		Tcl_AppendResult(interp, "bev:  evaluating boolean expressions\n", (char *)NULL);

		failed = nmg_boolean( tmp_tree, mged_nmg_model, &mged_tol );
	}
	else
		failed = 1;

	if( failed )  {
	  Tcl_AppendResult(interp, "bev:  no resulting region, aborting\n", (char *)NULL);
	  if( tmp_tree )
		db_free_tree( tmp_tree );
	  tmp_tree = (union tree *)NULL;
	  nmg_km( mged_nmg_model );
	  mged_nmg_model = (struct model *)NULL;
	  return TCL_ERROR;
	}
	/* New region remains part of this nmg "model" */
	NMG_CK_REGION( tmp_tree->tr_d.td_r );
	Tcl_AppendResult(interp, "facetize:  ", tmp_tree->tr_d.td_name, "\n", (char *)NULL);

	nmg_vmodel( mged_nmg_model );

	/* Triangulate model, if requested */
	if( triangulate )
	{
	  Tcl_AppendResult(interp, "bev:  triangulating resulting object\n", (char *)NULL);
	  nmg_triangulate_model( mged_nmg_model , &mged_tol );
	}

	Tcl_AppendResult(interp, "bev:  converting NMG to database format\n", (char *)NULL);

	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_type = ID_NMG;
	intern.idb_ptr = (genptr_t)mged_nmg_model;
	mged_nmg_model = (struct model *)NULL;

	RT_INIT_EXTERNAL( &ext );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
	  Tcl_AppendResult(interp, "bev(", newname, "):  solid export failure\n", (char *)NULL);
	  if( intern.idb_ptr )  rt_functab[ID_NMG].ft_ifree( &intern );
	  db_free_external( &ext );
	  return TCL_ERROR;
	}
	rt_functab[ID_NMG].ft_ifree( &intern );
	tmp_tree->tr_d.td_r = (struct nmgregion *)NULL;

	ngran = (ext.ext_nbytes + sizeof(union record)-1)/sizeof(union record);
	if( (dp=db_diradd( dbip, newname, -1, ngran, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, ngran ) < 0 )  {
	  TCL_ALLOC_ERR_return;
	}

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		db_free_external( &ext );
		TCL_WRITE_ERR_return;
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "bev:  wrote %.2f Kbytes to database\n",
			ext.ext_nbytes / 1024.0 );
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	db_free_external( &ext );

	/* Free boolean tree, and the regions in it. */
	db_free_tree( tmp_tree );


	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = newname;
	  av[2] = NULL;

	  /* draw the new solid */
	  return f_edit( clientData, interp, 2, av );
	}
}
