/*
 *			D O D R A W . C
 *
 * Functions -
 *	drawtrees	Add a set of tree hierarchies to the active set
 *	drawHsolid	Manage the drawing of a COMGEOM solid
 *	pathHmat	Find matrix across a given path
 *	replot_original_solid	Replot vector list for a solid
 *	replot_modified_solid	Replot solid, given matrix and db record.
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

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

#include "../librt/debug.h"	/* XXX */

extern void	(*nmg_plot_anim_upcall)();
extern void	(*nmg_vlblock_anim_upcall)();

int	no_memory;	/* flag indicating memory for drawing is used up */
long	nvectors;	/* number of vectors drawn so far */

static struct mater_info mged_no_mater = {
	/* RT default is white.  This is red, to stay clear of illuminate mode */
	1.0, 0, 0,		/* */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER		/* mater inherit */
};

/*
 *  This is just like the rt_initial_tree_state in librt/tree.c,
 *  except that the default color is red instead of white.
 *  This avoids confusion with illuminate mode.
 *  Red is a one-gun color, avoiding convergence problems too.
 */
static struct db_tree_state	mged_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0,		/* region, air, gmater */
	1.0, 0.0, 0.0,		/* color, RGB */
	0,			/* override */
	DB_INH_LOWER,		/* color inherit */
	DB_INH_LOWER,		/* mater inherit */
	"",			/* material name */
	"",			/* material params */
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

static int		mged_draw_wireframes;
static int		mged_draw_normals;
static struct model	*mged_nmg_model;

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
	char	buf[128];

	/* Overlay plot file */
	sprintf( buf, "overlay %s\n", file );
	cmdline( buf );

	event_check( 1 );	/* Take any device events */

	refresh();		/* Force screen update */

	/* Extra delay between screen updates, for more viewing time */
	if(us)
		(void)bsdselect( 0, 0, us );
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
mged_vlblock_anim_upcall_handler( vbp, us )
struct rt_vlblock	*vbp;
long		us;		/* microseconds of extra delay */
{

	cvt_vlblock_to_solids( vbp, "_PLOT_OVERLAY_" );

	event_check( 1 );	/* Take any device events */

	refresh();		/* Force screen update */

	/* Extra delay between screen updates, for more viewing time */
	if(us)
		(void)bsdselect( 0, 0, us );
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
	struct rt_list	vhead;

	RT_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("mged_wireframe_leaf(%s) path='%s'\n",
			rt_functab[id].ft_name, sofar );
		rt_free(sofar, "path string");
	}

	dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER) );

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, tsp->ts_mat ) < 0 )  {
		rt_log("%s:  solid import failure\n",
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	    	return(TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[id].ft_plot(
	    &vhead,
	    &intern,
	    mged_abs_tol, mged_rel_tol, mged_nrm_tol ) < 0 )  {
		printf("%s: plot failure\n",
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );
		rt_functab[id].ft_ifree( &intern );
	    	return(TREE_NULL);		/* ERROR */
	}
	rt_functab[id].ft_ifree( &intern );

	drawH_part2( dashflag, &vhead, pathp, tsp, SOLID_NULL );

	/* Indicate success by returning something other than TREE_NULL */
	GETUNION( curtree, tree );
	curtree->tr_op = OP_NOP;

	return( curtree );
}

/*
 *			M G E D _ N M G _ L E A F
 *
 *  Tessellate Solid into NMG
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *mged_nmg_leaf( tsp, pathp, ep, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	struct rt_db_internal	intern;
	struct nmgregion	*r1;
	union tree		*curtree;
	struct directory	*dp;

	dp = DB_FULL_PATH_CUR_DIR(pathp);

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, tsp->ts_mat ) < 0 )  {
		rt_log("%s:  solid import failure\n",
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	    	return(TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[id].ft_tessellate(
	    &r1, mged_nmg_model, &intern,
	    mged_abs_tol, mged_rel_tol, mged_nrm_tol ) < 0 )  {
		rt_log("%s: tessellation failure\n",
			dp->d_namep);
		rt_functab[id].ft_ifree( &intern );
	    	return(TREE_NULL);
	}
	rt_functab[id].ft_ifree( &intern );

	/* debug */
	NMG_CK_REGION( r1 );
	if( nmg_ck_closed_region( r1 ) != 0 )  {
#if 0
		nmg_kr( r1 );
		return(TREE_NULL);
#endif
	}

	GETUNION( curtree, tree );
	curtree->tr_op = OP_REGION;	/* tag for nmg */
	curtree->tr_a.tu_stp = (struct soltab *)r1;
	curtree->tr_a.tu_name = (char *)0;
	curtree->tr_a.tu_regionp = (struct region *)0;

	if(rt_g.debug&DEBUG_TREEWALK)
		rt_log("mged_nmg_leaf() %s\n", curtree->tr_a.tu_name );

	return(curtree);
}

/*
 *			M G E D _ N M G _ D O I T
 */
HIDDEN struct nmgregion *
mged_nmg_doit( tp )
register union tree	*tp;
{
	register struct nmgregion	*l;
	register struct nmgregion	*r;
	vect_t			diag;
	fastf_t			tol;
	fastf_t			rel;
	int			op;

	switch( tp->tr_op )  {
	case OP_NOP:
		return( 0 );

	case OP_REGION:
		r = (struct nmgregion *)tp->tr_a.tu_stp;
		tp->tr_a.tu_stp = SOLTAB_NULL;	/* Disconnect */
		return( r );

	case OP_UNION:
		op = NMG_BOOL_ADD;
		goto com;
	case OP_INTERSECT:
		op = NMG_BOOL_ISECT;
		goto com;
	case OP_SUBTRACT:
		op = NMG_BOOL_SUB;
		goto com;

	default:
		rt_log("mged_nmg_doit: bad op %d\n", tp->tr_op);
		return(0);
	}
com:
	l = mged_nmg_doit( tp->tr_b.tb_left );
	r = mged_nmg_doit( tp->tr_b.tb_right );
	if( l == 0 )  {
		if( r == 0 )
			return( 0 );
		return( r );
	}
	if( r == 0 )  {
		if( l == 0 )
			return(0);
		return( l );
	}
	/* debug */
	NMG_CK_REGION( r );
	NMG_CK_REGION( l );
	if( nmg_ck_closed_region( r ) != 0 ||
	    nmg_ck_closed_region( l ) != 0 )  {
	    	rt_log("mged_nmg_doit:  non-closed shell, skipped\n");
	    	nmg_kr( r );
	    	nmg_kr( l );
		return(0);
	}

	/*
	 *  Compute appropriate tolerance for the boolean routine.
	 *  This tolerance is an absolute distance metric.
	 *  The geometry is guaranteed to contain no errors larger than
	 *  this tolerance value.
	 */
	tol = mged_abs_tol;
	if( mged_rel_tol > 0.0 )  {
		if( l->ra_p )  {
			VSUB2( diag, l->ra_p->max_pt, l->ra_p->min_pt );
			rel = MAGNITUDE(diag) * mged_rel_tol;
			if( tol <= 0.0 || rel < tol )  tol = rel;
		}
		if( r->ra_p )  {
			VSUB2( diag, r->ra_p->max_pt, r->ra_p->min_pt );
			rel = MAGNITUDE(diag) * mged_rel_tol;
			if( tol <= 0.0 || rel < tol )  tol = rel;
		}
	}
	if( tol <= 0.0 )  tol = 0.1;		/* mm */

	/* input r1 and r2 are destroyed, output is new r1 */
	r = nmg_do_bool( l, r, op, tol );

	/* debug */
	NMG_CK_REGION( r );
	(void)nmg_ck_closed_region( r );
	return( r );
}

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
	struct rt_list		vhead;

	RT_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("mged_nmg_region_end() path='%s'\n",
			sofar);
		rt_free(sofar, "path string");
	}

	r = mged_nmg_doit( curtree );
	if( r != 0 )  {
		int	style;
		/* Convert NMG to vlist */
		NMG_CK_REGION(r);

		if( mged_draw_normals )  {
			style = 2;
			/* 0 = vectors, 1 = w/polygon markers, 2 = polys with normals */
		} else if( mged_draw_wireframes )  {
			/* Draw in vector form */
			style = 0;
		} else {
			/* Default -- draw polygons */
			style = 1;
		}
		nmg_r_to_vlist( &vhead, r, style );

		drawH_part2( 0, &vhead, pathp, tsp, SOLID_NULL );

		/* NMG region is no longer necessary, only vlist remains */
		nmg_kr( r );
	}

	/* Return original tree -- it needs to be freed (by caller) */
	return( curtree );
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

	RT_CHECK_DBI(dbip);

	if( argc <= 0 )  return(-1);	/* FAIL */

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;
	mged_draw_wireframes = 0;
	mged_draw_normals = 0;

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"wnP:")) != EOF )  {
		switch(c)  {
		case 'w':
			mged_draw_wireframes = 1;
			break;
		case 'n':
			mged_draw_normals = 1;
			break;
		case 'P':
			ncpu = atoi(optarg);
			break;
		default:
			printf("option '%c' unknown\n", c);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Establish upcall interfaces for use by bottom of NMG library */
	nmg_plot_anim_upcall = mged_plot_anim_upcall_handler;
	nmg_vlblock_anim_upcall = mged_vlblock_anim_upcall_handler;

	switch( kind )  {
	default:
		rt_log("ERROR, bad kind\n");
		return(-1);
	case 1:		/* Wireframes */
		i = db_walk_tree( dbip, argc, argv,
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
		rt_log("drawtrees:  can't do big-E here\n");
		return(-1);
#	    endif
	case 3:
	  {
		/* NMG */
	  	mged_nmg_model = nmg_mm();
		i = db_walk_tree( dbip, argc, argv,
			ncpu,
			&mged_initial_tree_state,
			0,			/* take all regions */
			mged_nmg_region_end,
			mged_nmg_leaf );

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
 */
mged_bound_solid( sp )
register struct solid *sp;
{
	register struct rt_vlist	*vp;
	vect_t				maxvalue;
	vect_t				minvalue;

	VSETALL( maxvalue, -INFINITY );
	VSETALL( minvalue,  INFINITY );
	sp->s_vlen = 0;
	for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	j;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( j = 0; j < nused; j++,cmd++,pt++ )  {
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
				/* Has normal vector, not location */
				break;
			case RT_VLIST_LINE_MOVE:
			case RT_VLIST_LINE_DRAW:
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
				VMINMAX( minvalue, maxvalue, *pt );
				break;
			default:
				(void)printf("unknown vlist op %d\n", *cmd);
			}
		}
		sp->s_vlen += nused;
	}

	VADD2SCALE( sp->s_center, minvalue, maxvalue, 0.5 );

	sp->s_size = maxvalue[X] - minvalue[X];
	MAX( sp->s_size, maxvalue[Y] - minvalue[Y] );
	MAX( sp->s_size, maxvalue[Z] - minvalue[Z] );
}

/*
 *			D R A W h _ P A R T 2
 *
 *  Once the vlist has been created, perform the common tasks
 *  in handling the drawn solid.
 *
 *  This routine must be prepared to run in parallel.
 */
drawH_part2( dashflag, vhead, pathp, tsp, existing_sp )
int			dashflag;
struct rt_list		*vhead;
struct db_full_path	*pathp;
struct db_tree_state	*tsp;
struct solid		*existing_sp;
{
	register struct solid *sp;
	register struct vlist *vp;
	register int	i;

	if( !existing_sp )  {
		/* Handling a new solid */
		GET_SOLID( sp );
	} else {
		/* Just updating an existing solid.
		 *  'tsp' and 'pathpos' will not be used
		 */
		sp = existing_sp;
	}


	/*
	 * Compute the min, max, and center points.
	 */
	RT_LIST_APPEND_LIST( &(sp->s_vlist), vhead );
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
		for( i=0; i<=sp->s_last; i++ )
			sp->s_path[i] = pathp->fp_names[i];
		sp->s_regionid = tsp->ts_regionid;
	}
	sp->s_addr = 0;
	sp->s_bytes = 0;

	/* Cvt to displaylist, determine displaylist memory requirement. */
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
			no_memory = 1;
			(void)printf("draw: out of Displaylist\n");
			sp->s_bytes = 0;	/* not drawn */
		} else {
			sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid is successfully drawn */
	if( !existing_sp )  {
		/* Add to linked list of solid structs */
		RES_ACQUIRE( &rt_g.res_model );
		APPEND_SOLID( sp, HeadSolid.s_back );
		RES_RELEASE( &rt_g.res_model );
		dmp->dmr_viewchange( DM_CHGV_ADD, sp );
	} else {
		/* replacing existing solid -- struct already linked in */
		sp->s_iflag = UP;
		dmp->dmr_viewchange( DM_CHGV_REPL, sp );
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
			printf("pathHmat:  %s is not a combination\n",
				parentp->d_namep);
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
		(void)printf("pathHmat: unable to follow %s/%s path\n",
			parentp->d_namep, kidp->d_namep );
		return;			/* ERROR */
next_level:
		rt_free( (char *)rp, "pathHmat recs");
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
	union record	*rp;
	mat_t		mat;

	if( sp->s_Eflag )  {
		(void)printf("replot_original_solid(%s): Unable to plot evaluated regions, skipping\n",
			sp->s_path[sp->s_last]->d_namep );
		return(-1);
	}
	pathHmat( sp, mat, sp->s_last-1 );
	rp = db_getmrec( dbip, sp->s_path[sp->s_last]);
	if( replot_modified_solid( sp, rp, mat ) < 0 )  {
		rt_free( (char *)rp, "original solid rec" );
		return(-1);
	}
	rt_free( (char *)rp, "original solid rec" );
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
replot_modified_solid( sp, recp, mat )
struct solid	*sp;
union record	*recp;
mat_t		mat;
{
	unsigned		addr, bytes;
	struct rt_list		vhead;
	int			id;
	struct rt_external	ext;
	struct rt_db_internal	intern;

	RT_LIST_INIT( &vhead );

	if( sp == SOLID_NULL )  {
		(void)printf("replot_modified_solid() sp==NULL?\n");
		return(-1);
	}

	/* Release existing vlist of this solid */
	RT_FREE_VLIST( &(sp->s_vlist) );

	RT_INIT_EXTERNAL( &ext );
	ext.ext_buf = (genptr_t)recp;
	ext.ext_nbytes = sizeof(union record);

	if( (id = rt_id_solid( &ext )) == ID_NULL )  {
		(void)printf("replot_modified_solid() unable to identify type of solid %s\n",
			sp->s_path[sp->s_last]->d_namep );
		return(-1);
	}

	/* Remember displaylist location of previous solid */
	addr = sp->s_addr;
	bytes = sp->s_bytes;

	/* Draw (plot) a normal solid */

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, &ext, mat ) < 0 )  {
		rt_log("%s:  solid import failure\n",
			sp->s_path[sp->s_last]->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	    	return(-1);		/* ERROR */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[id].ft_plot( &vhead, &intern,
	    mged_abs_tol, mged_rel_tol, mged_nrm_tol ) < 0 )  {
		(void)printf("%s: re-plot failure\n",
			sp->s_path[sp->s_last]->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	    	return(-1);
	}
	rt_functab[id].ft_ifree( &intern );

	/* Write new displaylist */
	drawH_part2( sp->s_soldash, &vhead,
		(struct db_full_path *)0,
		(struct db_tree_state *)0, sp );

	/* Release previous chunk of displaylist. */
	if( bytes > 0 )
		memfree( &(dmp->dmr_map), bytes, (unsigned long)addr );
	dmaflag = 1;
	return(0);
}

/*
 *			C V T _ V L B L O C K _ T O _ S O L I D S
 */
cvt_vlblock_to_solids( vbp, name )
struct rt_vlblock	*vbp;
char			*name;
{
	int		i;
	char		shortname[32];
	char		namebuf[64];
	char		cmd_buf[64];

	strncpy( shortname, name, 16-6 );
	shortname[16-6] = '\0';
	/* Remove any residue colors from a previous overlay w/same name */
	sprintf( cmd_buf, "kill %s*\n", shortname );
	cmdline(cmd_buf);

	for( i=0; i < vbp->nused; i++ )  {
		if( vbp->rgb[i] == 0 )  continue;
		if( RT_LIST_IS_EMPTY( &(vbp->head[i]) ) )  continue;
		if( i== 0 )  {
			invent_solid( name, &vbp->head[0], vbp->rgb[0] );
			continue;
		}
		sprintf( namebuf, "%s%x",
			shortname, vbp->rgb[i] );
		invent_solid( namebuf, &vbp->head[i], vbp->rgb[i] );
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
invent_solid( name, vhead, rgb )
char		*name;
struct rt_list	*vhead;
long		rgb;
{
	register struct directory	*dp;
	register struct solid		*sp;

#define PHONY_ADDR	(-1L)
	if( (dp = db_lookup( dbip,  name, LOOKUP_QUIET )) != DIR_NULL )  {
		if( dp->d_addr != PHONY_ADDR )  {
			printf("invent_solid(%s) would clobber existing database entry, ignored\n");
			return(-1);
		}
		/* Name exists from some other overlay,
		 * zap any associated solids
		 */
		eraseobj(dp);
	} else {
		/* Need to enter phony name in directory structure */
		dp = db_diradd( dbip,  name, PHONY_ADDR, 0, DIR_SOLID );
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
	GET_SOLID(sp);

	RT_LIST_APPEND_LIST( &(sp->s_vlist), vhead );
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
	if( !no_memory && (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
		/* Allocate displaylist storage for object */
		sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
		if( sp->s_addr == 0 )  {
			no_memory = 1;
			(void)printf("invent_solid: out of Displaylist\n");
			sp->s_bytes = 0;	/* not drawn */
		} else {
			sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes );
		}
	}

	/* Solid successfully drawn, add to linked list of solid structs */
	APPEND_SOLID( sp, HeadSolid.s_back );
	dmp->dmr_viewchange( DM_CHGV_ADD, sp );
	dmp->dmr_colorchange();
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
	struct nmgregion	*r;
	struct rt_list		vhead;

	RT_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		rt_log("mged_facetize_region_end() path='%s'\n",
			sofar);
		rt_free(sofar, "path string");
	}

	RES_ACQUIRE( &rt_g.res_model );
	if( mged_facetize_tree )  {
		union tree	*tr;
		tr = (union tree *)rt_calloc(1, sizeof(union tree), "union tree");
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
void
f_facetize( argc, argv )
int	argc;
char	**argv;
{
	int			i;
	register int		c;
	int			ncpu;
	char			*newname;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	union record		*rec;
	struct directory	*dp;
	struct nmgregion	*r;
	int			ngran;

	RT_CHECK_DBI(dbip);

	/* Initial vaues for options, must be reset each time */
	ncpu = 1;

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"P:")) != EOF )  {
		switch(c)  {
		case 'P':
			ncpu = atoi(optarg);
			break;
		default:
			printf("option '%c' unknown\n", c);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	newname = argv[0];
	argv++;
	argc--;

	if( db_lookup( dbip, newname, LOOKUP_QUIET ) != DIR_NULL )  {
		printf("error: solid '%s' already exists, aborting\n", newname);
		return;
	}

	rt_log("facetize:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
		mged_abs_tol, mged_rel_tol, mged_nrm_tol );
	mged_facetize_tree = (union tree *)0;
  	mged_nmg_model = nmg_mm();
	i = db_walk_tree( dbip, argc, argv,
		ncpu,
		&mged_initial_tree_state,
		0,			/* take all regions */
		mged_facetize_region_end,
		mged_nmg_leaf );

	if( i < 0 )  {
		rt_log("facetize: error in db_walk_tree()\n");
		/* Destroy NMG */
		nmg_km( mged_nmg_model );
		return;
	}

	/* Now, evaluate the boolean tree into ONE region */
	rt_log("facetize:  evaluating boolean expressions\n");
	r = mged_nmg_doit( mged_facetize_tree );
	if( r == 0 )  {
		rt_log("facetize:  no resulting region, aborting\n");
		nmg_km( mged_nmg_model );
		return;
	}
	/* New region remains part of this nmg "model" */
	NMG_CK_REGION( r );

	rt_log("facetize:  converting NMG to database format\n");

	/* Free boolean tree */
	db_free_tree( mged_facetize_tree );

	/* Export NMG as a new solid */
	RT_INIT_DB_INTERNAL(&intern);
	intern.idb_type = ID_NMG;
	intern.idb_ptr = (genptr_t)mged_nmg_model;
	RT_INIT_EXTERNAL( &ext );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ID_NMG].ft_export( &ext, &intern, 1.0 ) < 0 )  {
		rt_log("facetize(%s):  solid export failure\n",
			newname );
		if( intern.idb_ptr )  rt_functab[ID_NMG].ft_ifree( &intern );
		db_free_external( &ext );
		return;				/* FAIL */
	}
	rt_functab[ID_NMG].ft_ifree( &intern );

	/* Depends on solid names always being in the same place */
	rec = (union record *)ext.ext_buf;
	NAMEMOVE( newname, rec->s.s_name );

	ngran = (ext.ext_nbytes + sizeof(union record)-1)/sizeof(union record);
	if( (dp=db_diradd( dbip, newname, -1, ngran, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, ngran ) < 0 )  {
	    	ALLOC_ERR_return;
	}

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		db_free_external( &ext );
		ERROR_RECOVERY_SUGGESTION;
		WRITE_ERR_return;
	}
	rt_log("facetize:  wrote %.2f Kbytes to database\n",
		ext.ext_nbytes / 1024.0 );
	db_free_external( &ext );
	return;					/* OK */
}
