/*                        D O D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dodraw.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"		/* for ID_POLY special support */
#include "db.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./cmd.h"

#include "../librt/debug.h"	/* XXX */

void		cvt_vlblock_to_solids(struct bn_vlblock *vbp, const char *name, int copy);
void		drawH_part2(int dashflag, struct bu_list *vhead, struct db_full_path *pathp, struct db_tree_state *tsp, struct solid *existing_sp);
long	nvectors;	/* number of vectors drawn so far */

unsigned char geometry_default_color[] = { 255, 0, 0 };

/*
 *  This is just like the rt_initial_tree_state in librt/tree.c,
 *  except that the default color is red instead of white.
 *  This avoids confusion with illuminate mode.
 *  Red is a one-gun color, avoiding convergence problems too.
 */
struct db_tree_state	mged_initial_tree_state = {
	RT_DBTS_MAGIC,		/* magic */
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0,		/* region, air, gmater */
	100,			/* GIFT los */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
#if __STDC__
		{
#endif
		    1.0, 0.0, 0.0
#if __STDC__
		}
#endif
		,		/* color, RGB */
		-1.0,			/* Temperature */
		0,			/* ma_color_valid=0 --> use default */
		0,			/* color inherit */
		0,			/* mater inherit */
		(char *)NULL		/* shader */
#if __STDC__
	}
#endif
	,
#if __STDC__
	{
#endif
	    1.0, 0.0, 0.0, 0.0,
	    0.0, 1.0, 0.0, 0.0,
	    0.0, 0.0, 1.0, 0.0,
	    0.0, 0.0, 0.0, 1.0
#if __STDC__
	}
#endif
	,
	REGION_NON_FASTGEN,		/* ts_is_fastgen */
#if __STDC__
	{
#endif
		/* attribute value set */
		BU_AVS_MAGIC,
		0,
		0,
		NULL,
		NULL,
		NULL
#if __STDC__
	}
#endif
	,
	0,				/* ts_stop_at_regions */
	NULL,				/* ts_region_start_func */
	NULL,				/* ts_region_end_func */
	NULL,				/* ts_leaf_func */
	NULL,				/* ts_ttol */
	NULL,				/* ts_tol */
	NULL,				/* ts_m */
	NULL,				/* ts_rtip */
	NULL				/* ts_resp */
};

static int		mged_draw_nmg_only;
static int		mged_nmg_triangulate;
static int		mged_draw_wireframes;
static int		mged_draw_normals;
static int		mged_draw_solid_lines_only=0;
static int		mged_draw_no_surfaces = 0;
static int		mged_shade_per_vertex_normals=0;
int			mged_wireframe_color_override;
int			mged_wireframe_color[3];
static struct model	*mged_nmg_model;

extern struct bn_tol		mged_tol;	/* from ged.c */
extern struct rt_tess_tol	mged_ttol;	/* from ged.c */

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
mged_plot_anim_upcall_handler(char *file, long int us)

    	   		/* microseconds of extra delay */
{
	char *av[3];

	/* Overlay plot file */
	av[0] = "overlay";
	av[1] = file;
	av[2] = NULL;
	(void)cmd_overlay((ClientData)NULL, interp, 2, av);

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
mged_vlblock_anim_upcall_handler(struct bn_vlblock *vbp, long int us, int copy)

    		   		/* microseconds of extra delay */

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
hack_for_lee(void)
{
	event_check( 1 );	/* Take any device events */

	refresh();		/* Force screen update */
}

/*
 *			M G E D _ W I R E F R A M E _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
mged_wireframe_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	return( curtree );
}

/*
 *			M G E D _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
mged_wireframe_leaf(struct db_tree_state *tsp, struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
	union tree	*curtree;
	int		dashflag;		/* draw with dashed lines */
	struct bu_list	vhead;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	RT_CK_DB_INTERNAL(ip);

	BU_LIST_INIT( &vhead );

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_wireframe_leaf(",
			   ip->idb_meth->ft_name,
			   ") path='", sofar, "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( mged_draw_solid_lines_only )
		dashflag = 0;
	else
		dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER) );

	if( ip->idb_meth->ft_plot(
	    &vhead, ip,
	    tsp->ts_ttol, tsp->ts_tol ) < 0 )  {
	  Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
			   ": plot failure\n", (char *)NULL);
	  return(TREE_NULL);		/* ERROR */
	}

	/*
	 * XXX HACK CTJ - drawH_part2 sets the default color of a
	 * solid by looking in tps->ts_mater.ma_color, for pseudo
	 * solids, this needs to be something different and drawH
	 * has no idea or need to know what type of solid this is.
	 */
	if (ip->idb_type == ID_GRIP) {
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

	/* Indicate success by returning something other than TREE_NULL */
	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;

	return( curtree );
}
/* XXX Grotesque, shameless hack */
static int mged_do_not_draw_nmg_solids_during_debugging = 0;
static int mged_draw_edge_uses=0;
static int mged_enable_fastpath = 0;
static int mged_fastpath_count=0;	/* statistics */
static struct bn_vlblock	*mged_draw_edge_uses_vbp;

/*
 *			M G E D _ N M G _ R E G I O N _ S T A R T
 *
 *  When performing "ev" on a region, consider whether to process
 *  the whole subtree recursively.
 *  Normally, say "yes" to all regions by returning 0.
 *
 *  Check for special case:  a region of one solid, which can be
 *  directly drawn as polygons without going through NMGs.
 *  If we draw it here, then return -1 to signal caller to ignore
 *  further processing of this region.
 *  A hack to view polygonal models (converted from FASTGEN) more rapidly.
 */
int
mged_nmg_region_start(struct db_tree_state *tsp, struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
	union tree		*tp;
	struct directory	*dp;
	struct rt_db_internal	intern;
	mat_t			xform;
	matp_t			matp;
	struct bu_list		vhead;

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("mged_nmg_region_start(%s)\n", sofar);
		bu_free((genptr_t)sofar, "path string");
		rt_pr_tree( combp->tree, 1 );
		db_pr_tree_state(tsp);
	}

	BU_LIST_INIT( &vhead );

	RT_CK_COMB(combp);
	tp = combp->tree;
	if( !tp )
		return( -1 );
	RT_CK_TREE(tp);
	if( tp->tr_l.tl_op != OP_DB_LEAF )
		return 0;	/* proceed as usual */

	/* The subtree is a single node.  It may be a combination, though */

	/* Fetch by name, check to see if it's an easy type */
	dp = db_lookup( tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_NOISY );
	if( !dp )
		return 0;	/* proceed as usual */
	if( tsp->ts_mat )  {
		if( tp->tr_l.tl_mat )  {
			matp = xform;
			bn_mat_mul( xform, tsp->ts_mat, tp->tr_l.tl_mat );
		} else {
			matp = tsp->ts_mat;
		}
	} else {
		if( tp->tr_l.tl_mat )  {
			matp = tp->tr_l.tl_mat;
		} else {
			matp = (matp_t)NULL;
		}
	}
	if( rt_db_get_internal(&intern, dp, tsp->ts_dbip, matp, &rt_uniresource ) < 0 )
		return 0;	/* proceed as usual */

	switch( intern.idb_type )  {
	case ID_POLY:
		{
			if(RT_G_DEBUG&DEBUG_TREEWALK)  {
				bu_log("fastpath draw ID_POLY %s\n", dp->d_namep);
			}
			if( mged_draw_wireframes )  {
				(void)rt_pg_plot( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			} else {
				(void)rt_pg_plot_poly( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			}
		}
		goto out;
	case ID_BOT:
		{
			if (RT_G_DEBUG&DEBUG_TREEWALK) {
				bu_log("fastpath draw ID_BOT %s\n", dp->d_namep);
			}
			if( mged_draw_wireframes )  {
				(void)rt_bot_plot( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			} else {
				(void)rt_bot_plot_poly( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			}
		}
		goto out;
	case ID_COMBINATION:
	default:
		break;
	}
	rt_db_free_internal(&intern, &rt_uniresource);
	return 0;

out:
	/* Successful fastpath drawing of this solid */
	db_add_node_to_full_path( pathp, dp );
	drawH_part2( 0, &vhead, pathp, tsp, SOLID_NULL );
	DB_FULL_PATH_POP(pathp);
	rt_db_free_internal(&intern, &rt_uniresource);
	mged_fastpath_count++;
	return -1;	/* SKIP THIS REGION */
}

/*
 *			M G E D _ N M G _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
mged_nmg_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	int			failed;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BU_LIST_INIT( &vhead );

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_nmg_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	if ( !mged_draw_nmg_only ) {
		if( BU_SETJUMP )
		{
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(interp, "WARNING: Boolean evaluation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree, &rt_uniresource );
			return (union tree *)NULL;
		}
		failed = nmg_boolean( curtree, *tsp->ts_m, tsp->ts_tol, &rt_uniresource );
		BU_UNSETJUMP;
		if( failed )  {
			db_free_tree( curtree, &rt_uniresource );
			return (union tree *)NULL;
		}
	}
	else if( curtree->tr_op != OP_NMG_TESS )
	{
	  Tcl_AppendResult(interp, "Cannot use '-d' option when Boolean evaluation is required\n", (char *)NULL);
	  db_free_tree( curtree, &rt_uniresource );
	  return (union tree *)NULL;
	}
	r = curtree->tr_d.td_r;
	NMG_CK_REGION(r);

	if( mged_do_not_draw_nmg_solids_during_debugging && r )  {
		db_free_tree( curtree, &rt_uniresource );
		return (union tree *)NULL;
	}

	if (mged_nmg_triangulate) {
		if( BU_SETJUMP )
		{
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(interp, "WARNING: Triangulation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree, &rt_uniresource );
			return (union tree *)NULL;
		}
		nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
		BU_UNSETJUMP;
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
		db_free_tree( curtree, &rt_uniresource );
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
drawtrees(
	int	argc,
	char	**argv,
	int	kind)
{
	int		ret = 0;
	register int	c;
	int		ncpu;
	int		mged_nmg_use_tnurbs = 0;

	if(dbip == DBI_NULL)
	  return 0;

	RT_CHECK_DBI(dbip);

	if( argc <= 0 )  return(-1);	/* FAIL */

	/* Initial values for options, must be reset each time */
	ncpu = 1;
	mged_draw_nmg_only = 0;	/* no booleans */
	mged_nmg_triangulate = 1;
	mged_draw_wireframes = 0;
	mged_draw_normals = 0;
	mged_draw_edge_uses = 0;
	mged_draw_solid_lines_only = 0;
	mged_shade_per_vertex_normals = 0;
	mged_draw_no_surfaces = 0;
	mged_wireframe_color_override = 0;
	mged_fastpath_count = 0;
	mged_enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 1;		/* re-init bu_getopt() */
	while( (c=bu_getopt(argc,argv,"dfnqrstuvwSTP:C:")) != EOF )  {
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
			ncpu = atoi(bu_optarg);
			break;
		case 'q':
			mged_do_not_draw_nmg_solids_during_debugging = 1;
			break;
		case 'd':
			mged_draw_nmg_only = 1;
			break;
		case 'f':
			mged_enable_fastpath = 1;
			break;
		case 'C':
			{
				int		r,g,b;
				register char	*cp = bu_optarg;

				r = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				g = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				b = atoi(cp);

				if( r < 0 || r > 255 )  r = 255;
				if( g < 0 || g > 255 )  g = 255;
				if( b < 0 || b > 255 )  b = 255;

				mged_wireframe_color_override = 1;
				mged_wireframe_color[0] = r;
				mged_wireframe_color[1] = g;
				mged_wireframe_color[2] = b;
			}
			break;
		case 'r':
			/* Draw in all-red, as in Release 3 and earlier */
			/* Useful for spotting regions colored black */
			mged_wireframe_color_override = 1;
			mged_wireframe_color[0] = 255;
			mged_wireframe_color[1] = 0;
			mged_wireframe_color[2] = 0;
			break;
		default:
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "help %s", argv[0]);
				Tcl_Eval(interp, bu_vls_addr(&vls));
				bu_vls_free(&vls);

				return TCL_ERROR;
			}
#if 0
		  Tcl_AppendResult(interp, "Usage: ev [-dfnqstuvwST] [-P ncpu] object(s)\n\
	-d draw nmg without performing boolean operations\n\
	-f enable polysolid fastpath\n\
	-w draw wireframes (rather than polygons)\n\
	-n draw surface normals as little 'hairs'\n\
	-s draw solid lines only (no dot-dash for subtract and intersect)\n\
	-t Perform CSG-to-tNURBS conversion\n\
	-v shade using per-vertex normals, when present.\n\
	-u debug: draw edgeuses\n\
	-S draw tNURBs with trimming curves only, no surfaces.\n\
	-T debug: disable triangulator\n", (char *)NULL);
			break;
#endif
		}
	}
	argc -= bu_optind;
	argv += bu_optind;

	/* Establish upcall interfaces for use by bottom of NMG library */
	nmg_plot_anim_upcall = mged_plot_anim_upcall_handler;
	nmg_vlblock_anim_upcall = mged_vlblock_anim_upcall_handler;
	nmg_mged_debug_display_hack = hack_for_lee;

	/* Establish tolerances */
	mged_initial_tree_state.ts_ttol = &mged_ttol;
	mged_initial_tree_state.ts_tol = &mged_tol;

#if 0
	/* set default wireframe color */
	VMOVE(mged_initial_tree_state.ts_mater.ma_color, default_wireframe_color);
#endif

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	switch( kind )  {
	default:
	  Tcl_AppendResult(interp, "ERROR, bad kind\n", (char *)NULL);
	  return(-1);
	case 1:		/* Wireframes */
		ret = db_walk_tree( dbip, argc, (const char **)argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_wireframe_region_end,
			mged_wireframe_leaf, (genptr_t)NULL );
		break;
	case 2:		/* Big-E */
#	    if 0
		ret = db_walk_tree( dbip, argc, argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_bigE_region_end,
			mged_bigE_leaf, (genptr_t)NULL );
		break;
#	    else
		Tcl_AppendResult(interp, "drawtrees:  can't do big-E here\n", (char *)NULL);
		return(-1);
#	    endif
	case 3:
	  {
		/* NMG */
#if 0
	    Tcl_AppendResult(interp, "\
Please note that the NMG library used by this command is experimental.\n\
A production implementation will exist in the maintenance release.\n", (char *)NULL);
#endif
	  	mged_nmg_model = nmg_mm();
		mged_initial_tree_state.ts_m = &mged_nmg_model;
	  	if (mged_draw_edge_uses) {
		  Tcl_AppendResult(interp, "Doing the edgeuse thang (-u)\n", (char *)NULL);
		  mged_draw_edge_uses_vbp = rt_vlblock_init();
	  	}

		ret = db_walk_tree( dbip, argc, (const char **)argv,
			ncpu,
			&mged_initial_tree_state,
			mged_enable_fastpath ? mged_nmg_region_start : 0,
			mged_nmg_region_end,
	  		mged_nmg_use_tnurbs ?
	  			nmg_booltree_leaf_tnurb :
				nmg_booltree_leaf_tess,
			(genptr_t)NULL
			);

	  	if (mged_draw_edge_uses) {
	  		cvt_vlblock_to_solids(mged_draw_edge_uses_vbp, "_EDGEUSES_", 0);
	  		rt_vlblock_free(mged_draw_edge_uses_vbp);
			mged_draw_edge_uses_vbp = (struct bn_vlblock *)NULL;
 	  	}

		/* Destroy NMG */
		nmg_km( mged_nmg_model );
	  	break;
	  }
	}
	if(mged_fastpath_count)  {
		bu_log("%d region%s rendered through polygon fastpath\n",
			mged_fastpath_count, mged_fastpath_count==1?"":"s");
	}
	if( ret < 0 )  return(-1);
	return(0);	/* OK */
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate bn_vlist_rpp() routine, for librt/vlist.c
 */
void
mged_bound_solid(register struct solid *sp)
{
	register struct bn_vlist	*vp;
	register double			xmax, ymax, zmax;
	register double			xmin, ymin, zmin;

	xmax = ymax = zmax = -INFINITY;
	xmin = ymin = zmin =  INFINITY;
	sp->s_vlen = 0;
	for( BU_LIST_FOR( vp, bn_vlist, &(sp->s_vlist) ) )  {
		register int	j;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( j = 0; j < nused; j++,cmd++,pt++ )  {
			switch( *cmd )  {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
				/* Has normal vector, not location */
				break;
			case BN_VLIST_LINE_MOVE:
			case BN_VLIST_LINE_DRAW:
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
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
drawH_part2(
	int			dashflag,
	struct bu_list		*vhead,
	struct db_full_path	*pathp,
	struct db_tree_state	*tsp,
	struct solid		*existing_sp)
{
	register struct solid *sp;

	if( !existing_sp )  {
		/* Handling a new solid */
		GET_SOLID(sp, &FreeSolid.l);
		/* NOTICE:  The structure is dirty & not initialized for you! */

		sp->s_dlist = BU_LIST_LAST(solid, &dgop->dgo_headSolid)->s_dlist + 1;
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
		if( mged_wireframe_color_override ) {
		        /* a user specified the color, so arrange to use it */
			sp->s_uflag = 1;
			sp->s_dflag = 0;
			sp->s_basecolor[0] = mged_wireframe_color[0];
			sp->s_basecolor[1] = mged_wireframe_color[1];
			sp->s_basecolor[2] = mged_wireframe_color[2];
		} else {
			sp->s_uflag = 0;
			if (tsp) {
			  if (tsp->ts_mater.ma_color_valid) {
			    sp->s_dflag = 0;	/* color specified in db */
			  } else {
			    sp->s_dflag = 1;	/* default color */
			  }
			  /* Copy into basecolor anyway, to prevent black */
			  sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
			  sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
			  sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
			}
		}
		sp->s_cflag = 0;
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;
		sp->s_Eflag = 0;	/* This is a solid */
		db_dup_full_path( &sp->s_fullpath, pathp );
		sp->s_regionid = tsp->ts_regionid;
	}

#ifdef DO_DISPLAY_LISTS
	createDListALL(sp);
#endif

	/* Solid is successfully drawn */
	if( !existing_sp )  {
		/* Add to linked list of solid structs */
		bu_semaphore_acquire( RT_SEM_MODEL );
		BU_LIST_APPEND(dgop->dgo_headSolid.back, &sp->l);
		bu_semaphore_release( RT_SEM_MODEL );
	} else {
		/* replacing existing solid -- struct already linked in */
		sp->s_iflag = UP;
	}
}

HIDDEN void
Do_getmat(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3)
{
	matp_t	xmat;
	char	*kid_name;
	int	*found;

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	kid_name = (char *)user_ptr2;

	if( strncmp( comb_leaf->tr_l.tl_name, kid_name, NAMESIZE ) )
		return;

	xmat = (matp_t)user_ptr1;
	found = (int *)user_ptr3;

	(*found) = 1;
	if( comb_leaf->tr_l.tl_mat ) {
		MAT_COPY( xmat, comb_leaf->tr_l.tl_mat );
	}
	else {
		MAT_IDN( xmat );
	}
}

/*
 *  			P A T H h M A T
 *
 *  Find the transformation matrix obtained when traversing
 *  the arc indicated in sp->s_path[] to the indicated depth.
 *
 *  Returns -
 *	matp is filled with values (never read first).
 *	sp may have fields updated.
 */
void
pathHmat(
	register struct solid *sp,
	matp_t matp,
	int depth)
{
	struct db_tree_state	ts;
	struct db_full_path	null_path;

	RT_CHECK_DBI(dbip);

	db_full_path_init( &null_path );
	ts = mged_initial_tree_state;		/* struct copy */
	ts.ts_dbip = dbip;
	ts.ts_resp = &rt_uniresource;

	(void)db_follow_path( &ts, &null_path, &sp->s_fullpath, LOOKUP_NOISY, depth+1 );
	db_free_full_path( &null_path );

#if 0
	/*
	 *  Copy color out to solid structure, in case it changed.
	 *  This is an odd place to do this, but...
	 */
#if 0
	sp->s_color[0] = sp->s_basecolor[0] = ts.ts_mater.ma_color[0] * 255.;
	sp->s_color[1] = sp->s_basecolor[1] = ts.ts_mater.ma_color[1] * 255.;
	sp->s_color[2] = sp->s_basecolor[2] = ts.ts_mater.ma_color[2] * 255.;
#else
	if(!sp->s_uflag){
	  /* the user did not specify a color */
	  sp->s_basecolor[0] = ts.ts_mater.ma_color[0] * 255.;
	  sp->s_basecolor[1] = ts.ts_mater.ma_color[1] * 255.;
	  sp->s_basecolor[2] = ts.ts_mater.ma_color[2] * 255.;
	}
#endif
#endif

	MAT_COPY( matp, ts.ts_mat );	/* implicit return */

	db_free_db_tree_state( &ts );
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
replot_original_solid( struct solid *sp )
{
	struct rt_db_internal	intern;
	struct directory	*dp;
	mat_t			mat;

	if(dbip == DBI_NULL)
	  return 0;

	dp = LAST_SOLID(sp);
	if( sp->s_Eflag )  {
	  Tcl_AppendResult(interp, "replot_original_solid(", dp->d_namep,
			   "): Unable to plot evaluated regions, skipping\n", (char *)NULL);
	  return(-1);
	}
	pathHmat( sp, mat, sp->s_fullpath.fp_len-2 );

	if( rt_db_get_internal( &intern, dp, dbip, mat, &rt_uniresource ) < 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ":  solid import failure\n", (char *)NULL);
	  return(-1);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( replot_modified_solid( sp, &intern, bn_mat_identity ) < 0 )  {
		rt_db_free_internal( &intern, &rt_uniresource );
		return(-1);
	}
	rt_db_free_internal( &intern, &rt_uniresource );
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
replot_modified_solid(
	struct solid			*sp,
	struct rt_db_internal		*ip,
	const mat_t			mat)
{
	struct rt_db_internal	intern;
	struct bu_list		vhead;

	BU_LIST_INIT( &vhead );

	if( sp == SOLID_NULL )  {
	  Tcl_AppendResult(interp, "replot_modified_solid() sp==NULL?\n", (char *)NULL);
	  return(-1);
	}

	/* Release existing vlist of this solid */
	RT_FREE_VLIST( &(sp->s_vlist) );

	/* Draw (plot) a normal solid */
	RT_CK_DB_INTERNAL( ip );

	mged_ttol.magic = RT_TESS_TOL_MAGIC;
	mged_ttol.abs = mged_abs_tol;
	mged_ttol.rel = mged_rel_tol;
	mged_ttol.norm = mged_nrm_tol;

	transform_editing_solid( &intern, mat, ip, 0 );

	if( rt_functab[ip->idb_type].ft_plot( &vhead, &intern, &mged_ttol, &mged_tol ) < 0 )  {
	  Tcl_AppendResult(interp, LAST_SOLID(sp)->d_namep,
			   ": re-plot failure\n", (char *)NULL);
	  return(-1);
	}
	rt_db_free_internal( &intern, &rt_uniresource );

	/* Write new displaylist */
	drawH_part2( sp->s_soldash, &vhead,
		(struct db_full_path *)0,
		(struct db_tree_state *)0, sp );

#if 0
	/* Release previous chunk of displaylist. */
	if( bytes > 0 )
		rt_memfree( &(dmp->dm_map), bytes, (unsigned long)addr );
#endif

	view_state->vs_flag = 1;
	return(0);
}

/*
 *			C V T _ V L B L O C K _ T O _ S O L I D S
 */
void
cvt_vlblock_to_solids(
	struct bn_vlblock	*vbp,
	const char		*name,
	int			copy)
{
	int		i;
	char		shortname[32];
	char		namebuf[64];
	char		*av[4];

	strncpy( shortname, name, 16-6 );
	shortname[16-6] = '\0';
	/* Remove any residue colors from a previous overlay w/same name */
	if( dbip->dbi_read_only )  {
		av[0] = "d";
		av[1] = shortname;
		av[2] = NULL;
		(void)cmd_erase((ClientData)NULL, interp, 2, av);
	} else {
		av[0] = "kill";
		av[1] = "-f";
		av[2] = shortname;
		av[3] = NULL;
		(void)cmd_kill((ClientData)NULL, interp, 3, av);
	}

	for( i=0; i < vbp->nused; i++ )  {
#if 0
		if( vbp->rgb[i] == 0 )  continue;
#endif
		if( BU_LIST_IS_EMPTY( &(vbp->head[i]) ) )  continue;

		sprintf( namebuf, "%s%lx",
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
invent_solid(
	const char	*name,
	struct bu_list	*vhead,
	long		rgb,
	int		copy)
{
	struct directory	*dp;
	struct directory	*dpp[2] = {DIR_NULL, DIR_NULL};
	register struct solid	*sp;
	int type = 0;

	if(dbip == DBI_NULL)
	  return 0;

	if( (dp = db_lookup( dbip,  name, LOOKUP_QUIET )) != DIR_NULL )  {
	  if( dp->d_addr != RT_DIR_PHONY_ADDR )  {
	    Tcl_AppendResult(interp, "invent_solid(", name,
			     ") would clobber existing database entry, ignored\n", (char *)NULL);
	    return(-1);
	  }
	  /* Name exists from some other overlay,
	   * zap any associated solids
	   */
	  dpp[0] = dp;
	  eraseobjall(dpp);
	}
	/* Need to enter phony name in directory structure */
	dp = db_diradd( dbip,  name, RT_DIR_PHONY_ADDR, 0, DIR_SOLID, &type );

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
		BU_LIST_INIT(&(sp->s_vlist));
		BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
	}
	mged_bound_solid( sp );
	nvectors += sp->s_vlen;

	/* set path information -- this is a top level node */
	db_add_node_to_full_path( &sp->s_fullpath, dp );

	sp->s_iflag = DOWN;
	sp->s_soldash = 0;
	sp->s_Eflag = 1;		/* Can't be solid edited! */
	sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
	sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
	sp->s_color[2] = sp->s_basecolor[2] = (rgb    ) & 0xFF;
	sp->s_regionid = 0;
	sp->s_dlist = BU_LIST_LAST(solid, &dgop->dgo_headSolid)->s_dlist + 1;

	/* Solid successfully drawn, add to linked list of solid structs */
	BU_LIST_APPEND(dgop->dgo_headSolid.back, &sp->l);

#ifdef DO_DISPLAY_LISTS
	createDListALL(sp);
#endif
#endif
	return(0);		/* OK */
}

static union tree	*mged_facetize_tree;

/*
 *			M G E D _ F A C E T I Z E _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
mged_facetize_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	struct bu_list		vhead;

	BU_LIST_INIT( &vhead );

	if(RT_G_DEBUG&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(interp, "mged_facetize_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	bu_semaphore_acquire( RT_SEM_MODEL );
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
	bu_semaphore_release( RT_SEM_MODEL );

	/* Tree has been saved, and will be freed later */
	return( TREE_NULL );
}

/* facetize [opts] new_obj old_obj(s) */
int
f_facetize(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int			i;
	register int		c;
	int			ncpu;
	int			triangulate;
	char			*newname;
	struct rt_db_internal	intern;
	struct directory	*dp;
	int			failed;
	int			mged_nmg_use_tnurbs = 0;
	int			make_bot;

	CHECK_DBI_NULL;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help facetize");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	RT_CHECK_DBI(dbip);

	/* Establish tolerances */
	mged_initial_tree_state.ts_ttol = &wdbp->wdb_ttol;
	mged_initial_tree_state.ts_tol = &wdbp->wdb_tol;

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;
	triangulate = 0;

	/* Parse options. */
	make_bot = 1;
	bu_optind = 1;		/* re-init bu_getopt() */
	while( (c=bu_getopt(argc,argv,"ntTP:")) != EOF )  {
		switch(c)  {
		case 'n':
			make_bot = 0;
			break;
		case 'P':
			ncpu = atoi(bu_optarg);
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
	argc -= bu_optind;
	argv += bu_optind;
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

	i = db_walk_tree( dbip, argc, (const char **)argv,
		ncpu,
		&mged_initial_tree_state,
		0,			/* take all regions */
		mged_facetize_region_end,
  		mged_nmg_use_tnurbs ?
  			nmg_booltree_leaf_tnurb :
			nmg_booltree_leaf_tess,
		(genptr_t)NULL
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

		if( BU_SETJUMP )
		{
			BU_UNSETJUMP;
			Tcl_AppendResult(interp, "WARNING: facetization failed!!!\n", (char *)NULL );
			if( mged_facetize_tree )
				db_free_tree( mged_facetize_tree, &rt_uniresource );
			mged_facetize_tree = (union tree *)NULL;
			nmg_km( mged_nmg_model );
			mged_nmg_model = (struct model *)NULL;
			return TCL_ERROR;
		}

		failed = nmg_boolean( mged_facetize_tree, mged_nmg_model, &mged_tol, &rt_uniresource );
		BU_UNSETJUMP;
	}
	else
		failed = 1;

	if( failed )  {
	  Tcl_AppendResult(interp, "facetize:  no resulting region, aborting\n", (char *)NULL);
	  if( mged_facetize_tree )
		db_free_tree( mged_facetize_tree, &rt_uniresource );
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
	if( triangulate && !make_bot )
	{
		Tcl_AppendResult(interp, "facetize:  triangulating resulting object\n", (char *)NULL);
		if( BU_SETJUMP )
		{
			BU_UNSETJUMP;
			Tcl_AppendResult(interp, "WARNING: triangulation failed!!!\n", (char *)NULL );
			if( mged_facetize_tree )
				db_free_tree( mged_facetize_tree, &rt_uniresource );
			mged_facetize_tree = (union tree *)NULL;
			nmg_km( mged_nmg_model );
			mged_nmg_model = (struct model *)NULL;
			return TCL_ERROR;
		}
		nmg_triangulate_model( mged_nmg_model , &mged_tol );
		BU_UNSETJUMP;
	}

	if( make_bot )
	{
		struct rt_bot_internal *bot;
		struct nmgregion *r;
		struct shell *s;

		Tcl_AppendResult(interp, "facetize:  converting to BOT format\n", (char *)NULL);

		r = BU_LIST_FIRST( nmgregion, &mged_nmg_model->r_hd );
		s = BU_LIST_FIRST( shell, &r->s_hd );
		bot = (struct rt_bot_internal *)nmg_bot( s, &mged_tol );
		nmg_km( mged_nmg_model );
		mged_nmg_model = (struct model *)NULL;

		/* Export BOT as a new solid */
		RT_INIT_DB_INTERNAL(&intern);
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_BOT;
		intern.idb_meth = &rt_functab[ID_BOT];
		intern.idb_ptr = (genptr_t) bot;
	}
	else
	{

		Tcl_AppendResult(interp, "facetize:  converting NMG to database format\n", (char *)NULL);

		/* Export NMG as a new solid */
		RT_INIT_DB_INTERNAL(&intern);
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_NMG;
		intern.idb_meth = &rt_functab[ID_NMG];
		intern.idb_ptr = (genptr_t)mged_nmg_model;
		mged_nmg_model = (struct model *)NULL;
	}

	if( (dp=db_diradd( dbip, newname, -1L, 0, DIR_SOLID, (genptr_t)&intern.idb_type)) == DIR_NULL )
	{
		Tcl_AppendResult(interp, "Cannot add ", newname, " to directory\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		TCL_WRITE_ERR_return;
	}

	mged_facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;

	/* Free boolean tree, and the regions in it */
	db_free_tree( mged_facetize_tree, &rt_uniresource );
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
f_bev(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int			i;
	register int		c;
	int			ncpu;
	int			triangulate;
	char			*newname;
	struct rt_db_internal	intern;
	struct directory	*dp;
	union tree		*tmp_tree;
	char			op;
	int			failed;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help bev");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	RT_CHECK_DBI( dbip );

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
	bu_optind = 1;		/* re-init bu_getopt() */
	while( (c=bu_getopt(argc,argv,"tP:")) != EOF )  {
		switch(c)  {
		case 'P':
#if 0
			/* not yet supported */
			ncpu = atoi(bu_optarg);
#endif
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
	argc -= bu_optind;
	argv += bu_optind;

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
		i = db_walk_tree( dbip, 1, (const char **)argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_facetize_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)NULL );

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
				    db_free_tree( mged_facetize_tree, &rt_uniresource );
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

		if( BU_SETJUMP )
		{
			BU_UNSETJUMP;

			Tcl_AppendResult(interp, "WARNING: Boolean evaluation failed!!!\n", (char *)NULL );
			if( tmp_tree )
				db_free_tree( tmp_tree, &rt_uniresource );
			tmp_tree = (union tree *)NULL;
			nmg_km( mged_nmg_model );
			mged_nmg_model = (struct model *)NULL;
			return TCL_ERROR;
		}

		failed = nmg_boolean( tmp_tree, mged_nmg_model, &mged_tol, &rt_uniresource );
		BU_UNSETJUMP;
	}
	else
		failed = 1;

	if( failed )  {
	  Tcl_AppendResult(interp, "bev:  no resulting region, aborting\n", (char *)NULL);
	  if( tmp_tree )
		db_free_tree( tmp_tree, &rt_uniresource );
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
		if( BU_SETJUMP )
		{
			BU_UNSETJUMP;
			Tcl_AppendResult(interp, "WARNING: Triangulation failed!!!\n", (char *)NULL );
			if( tmp_tree )
				db_free_tree( tmp_tree, &rt_uniresource );
			tmp_tree = (union tree *)NULL;
			nmg_km( mged_nmg_model );
			mged_nmg_model = (struct model *)NULL;
			return TCL_ERROR;
		}
		nmg_triangulate_model( mged_nmg_model , &mged_tol );
		BU_UNSETJUMP;
	}

	Tcl_AppendResult(interp, "bev:  converting NMG to database format\n", (char *)NULL);

	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_NMG;
	intern.idb_meth = &rt_functab[ID_NMG];
	intern.idb_ptr = (genptr_t)mged_nmg_model;
	mged_nmg_model = (struct model *)NULL;

	if( (dp=db_diradd( dbip, newname, -1L, 0, DIR_SOLID, (genptr_t)&intern.idb_type)) == DIR_NULL ) {
		Tcl_AppendResult(interp, "Cannot add ", newname, " to directory\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		TCL_WRITE_ERR_return;
	}

	tmp_tree->tr_d.td_r = (struct nmgregion *)NULL;

	/* Free boolean tree, and the regions in it. */
	db_free_tree( tmp_tree, &rt_uniresource );


	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = newname;
	  av[2] = NULL;

	  /* draw the new solid */
	  return cmd_draw( clientData, interp, 2, av );
	}
}

/*
 *			A D D _ S O L I D _ P A T H _ T O _ R E S U L T
 */
void
add_solid_path_to_result(
	Tcl_Interp *interp,
	struct solid *sp)
{
	struct bu_vls str;

	bu_vls_init(&str);
	db_path_to_vls(&str, &sp->s_fullpath );
	Tcl_AppendResult( interp, bu_vls_addr(&str), " ", NULL );
	bu_vls_free(&str);
}

/*
 *			R E D R A W _ V L I S T
 *
 *  Given the name(s) of database objects, re-generate the vlist
 *  associated with every solid in view which references the
 *  named object(s), either solids or regions.
 *  Particularly useful with outboard .inmem database modifications.
 */
int
cmd_redraw_vlist(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct directory	*dp;
	int		i;

	CHECK_DBI_NULL;

	if( argc < 2 )  {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help redraw_vlist");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	for( i = 1; i < argc; i++ )  {
		register struct solid	*sp;

		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == NULL )
			continue;

		FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
			if( db_full_path_search( &sp->s_fullpath, dp ) )  {
#if 0
				add_solid_path_to_result(interp, sp);
#endif
				(void)replot_original_solid( sp );
				sp->s_iflag = DOWN;	/* It won't be drawn otherwise */
			}
		}
	}


	update_views = 1;
	return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
