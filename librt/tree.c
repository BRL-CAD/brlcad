/*
 *			T R E E
 *
 *  Ray Tracing library database tree walker.
 *  Collect and prepare regions and solids for subsequent ray-tracing.
 *
 *  PARALLEL lock usage note -
 *	res_model	used for interlocking rti_headsolid list (PARALLEL)
 *	res_results	used for interlocking HeadRegion list (PARALLEL)
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
static char RCStree[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

int rt_pure_boolean_expressions = 0;

int		rt_bound_tree();	/* used by rt/sh_light.c */
extern char	*rt_basename();
HIDDEN struct region *rt_getregion();
HIDDEN void	rt_tree_region_assign();


CONST struct db_tree_state	rt_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0, 0,		/* region, air, gmater, LOS */
	1.0, 1.0, 1.0,		/* color, RGB */
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

static struct rt_i	*rt_tree_rtip;

/*
 *			R T _ G E T T R E E _ R E G I O N _ S T A R T
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN int rt_gettree_region_start( tsp, pathp )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
{

	/* Ignore "air" regions unless wanted */
	if( rt_tree_rtip->useair == 0 &&  tsp->ts_aircode != 0 )  {
		rt_tree_rtip->rti_air_discards++;
		return(-1);	/* drop this region */
	}
	return(0);
}

/*
 *			R T _ G E T T R E E _ R E G I O N _ E N D
 *
 *  This routine will be called by db_walk_tree() once all the
 *  solids in this region have been visited.
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *rt_gettree_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	struct region		*rp;
	struct directory	*dp;
	vect_t			region_min, region_max;

	if( curtree->tr_op == OP_NOP )  {
		/* Ignore empty regions */
		return  curtree;
	}

	GETSTRUCT( rp, region );
	rp->reg_forw = REGION_NULL;
	rp->reg_regionid = tsp->ts_regionid;
	rp->reg_aircode = tsp->ts_aircode;
	rp->reg_gmater = tsp->ts_gmater;
	rp->reg_los = tsp->ts_los;
	rp->reg_mater = tsp->ts_mater;		/* struct copy */
	rp->reg_name = db_path_to_string( pathp );

	dp = DB_FULL_PATH_CUR_DIR(pathp);

	if(rt_g.debug&DEBUG_TREEWALK)  {
		rt_log("rt_gettree_region_end() %s\n", rp->reg_name );
		rt_pr_tree( curtree, 0 );
	}

	/*
	 *  Find region RPP, and update the model maxima and minima
	 *
	 *  Don't update min & max for halfspaces;  instead, add them
	 *  to the list of infinite solids, for special handling.
	 */
	if( rt_bound_tree( curtree, region_min, region_max ) < 0 )  {
		rt_bomb("rt_gettree_region_end(): rt_bound_tree() fail\n");
	}
	if( region_max[X] >= INFINITY )  {
		/* skip infinite region */
	} else {
		VMINMAX( rt_tree_rtip->mdl_min, rt_tree_rtip->mdl_max, region_min );
		VMINMAX( rt_tree_rtip->mdl_min, rt_tree_rtip->mdl_max, region_max );
	}

	/*
	 *  Add a region and it's boolean tree to all the appropriate places.
	 *  The	region and treetop are cross-linked, and the region is added
	 *  to the linked list of regions.
	 *  Positions in the region bit vector are established at this time.
	 */
	/* Cross-ref: Mark all solids & nodes as belonging to this region */
	rt_tree_region_assign( curtree, rp );
	rp->reg_treetop = curtree;

	/* Determine material properties */
	rp->reg_mfuncs = (char *)0;
	rp->reg_udata = (char *)0;
	if( rp->reg_mater.ma_override == 0 )
		rt_region_color_map(rp);

	RES_ACQUIRE( &rt_g.res_results );	/* enter critical section */
	rp->reg_instnum = dp->d_uses++;

	/* Add to linked list */
	rp->reg_forw = rt_tree_rtip->HeadRegion;
	rt_tree_rtip->HeadRegion = rp;

	rp->reg_bit = rt_tree_rtip->nregions++;	/* Assign bit vector pos. */
	RES_RELEASE( &rt_g.res_results );	/* leave critical section */

	if( rt_g.debug & DEBUG_REGIONS )  {
		rt_log("Add Region %s instnum %d\n",
			rp->reg_name, rp->reg_instnum);
	}

	/* Indicate that we have swiped 'curtree' */
	return(TREE_NULL);
}

/*
 *			R T _ G E T T R E E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *rt_gettree_leaf( tsp, pathp, ep, id )
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	register fastf_t	f;
	register struct soltab	*stp;
	union tree		*curtree;
	struct directory	*dp;
	struct rt_db_internal	intern;
	register int		i;
	register matp_t		mat;

	RT_CK_EXTERNAL(ep);
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	/* Determine if this matrix is an identity matrix */
	for( i=0; i<16; i++ )  {
		f = tsp->ts_mat[i] - rt_identity[i];
		if( !NEAR_ZERO(f, 0.0001) )
			break;
	}
	if( i < 16 )  {
		/* Not identity matrix */
		mat = tsp->ts_mat;
	} else {
		/* Identity matrix */
		mat = (matp_t)0;
	}

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 *  XXX There is a race on reading rti_headsolid here, prob. not harmful.
	 */
	for( RT_LIST( stp, soltab, &(rt_tree_rtip->rti_headsolid) ) )  {

		RT_CHECK_SOLTAB(stp);				/* debug */

		/* Leaf solids must be the same */
		if( dp != stp->st_dp )  continue;

		if( mat == (matp_t)0 )  {
			if( stp->st_matp == (matp_t)0 )  {
				if( rt_g.debug & DEBUG_SOLIDS )
					rt_log("rt_gettree_leaf:  %s re-referenced (ident)\n",
						dp->d_namep );
				goto found_it;
			}
			goto next_one;
		}
		if( stp->st_matp == (matp_t)0 )  goto next_one;
		for( i=0; i<16; i++ )  {
			f = mat[i] - stp->st_matp[i];
			if( !NEAR_ZERO(f, 0.0001) )
				goto next_one;
		}
		/* Success, we have a match! */
		if( rt_g.debug & DEBUG_SOLIDS )
			rt_log("rt_gettree_leaf:  %s re-referenced\n",
				dp->d_namep );
		goto found_it;
next_one: ;
	}

	GETSTRUCT(stp, soltab);
	stp->l.magic = RT_SOLTAB_MAGIC;
	stp->st_id = id;
	stp->st_dp = dp;
	if( mat )  {
		stp->st_matp = (matp_t)rt_malloc( sizeof(mat_t), "st_matp" );
		mat_copy( stp->st_matp, mat );
	} else {
		stp->st_matp = mat;
	}
	stp->st_specific = (genptr_t)0;

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, stp->st_matp ? stp->st_matp : rt_identity ) < 0 )  {
		rt_log("rt_gettree_leaf(%s):  solid import failure\n", dp->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		if( stp->st_matp )  rt_free( (char *)stp->st_matp, "st_matp");
		rt_free( (char *)stp, "struct soltab");
		return( TREE_NULL );		/* BAD */
	}
	RT_CK_DB_INTERNAL( &intern );

	if(rt_g.debug&DEBUG_SOLIDS)  {
		struct rt_vls	str;
		rt_vls_init( &str );
		/* verbose=1, mm2local=1.0 */
		if( rt_functab[id].ft_describe( &str, &intern, 1, 1.0 ) < 0 )  {
			rt_log("rt_gettree_leaf(%s):  solid describe failure\n",
				dp->d_namep );
		}
		rt_log( "%s:  %s", dp->d_namep, rt_vls_addr( &str ) );
		rt_vls_free( &str );
	}

    	/*
    	 *  If the ft_prep routine wants to keep the internal structure,
    	 *  that is OK, as long as idb_ptr is set to null.
    	 */
	if( rt_functab[id].ft_prep( stp, &intern, rt_tree_rtip ) )  {
		/* Error, solid no good */
		rt_log("rt_gettree_leaf(%s):  prep failure\n", dp->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		if( stp->st_matp )  rt_free( (char *)stp->st_matp, "st_matp");
		rt_free( (char *)stp, "struct soltab");
		return( TREE_NULL );		/* BAD */
	}
    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	id = stp->st_id;	/* type may have changed in prep */

	/* For now, just link them all onto the same list */
	RES_ACQUIRE( &rt_g.res_model );	/* enter critical section */
	RT_LIST_INSERT( &(rt_tree_rtip->rti_headsolid), &(stp->l) );
	stp->st_bit = rt_tree_rtip->nsolids++;
	RES_RELEASE( &rt_g.res_model );	/* leave critical section */

	if(rt_g.debug&DEBUG_SOLIDS)  rt_pr_soltab( stp );

found_it:
	GETUNION( curtree, tree );
	curtree->tr_op = OP_SOLID;
	curtree->tr_a.tu_stp = stp;
	curtree->tr_a.tu_name = db_path_to_string( pathp );
	/* regionp will be filled in later by rt_tree_region_assign() */
	curtree->tr_a.tu_regionp = (struct region *)0;

	if(rt_g.debug&DEBUG_TREEWALK)
		rt_log("rt_gettree_leaf() %s\n", curtree->tr_a.tu_name );

	return(curtree);
}

/*
 *  			R T _ G E T T R E E
 *
 *  User-called function to add a tree hierarchy to the displayed set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettree( rtip, node )
struct rt_i	*rtip;
char		*node;
{
	return( rt_gettrees( rtip, 1, &node, 1 ) );
}

/*
 *  			R T _ G E T T R E E S
 *
 *  User-called function to add a set of tree hierarchies to the active set.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettrees( rtip, argc, argv, ncpus )
struct rt_i	*rtip;
int		argc;
char		**argv;
int		ncpus;
{
	int			prev_sol_count;
	int			i;

	RT_CHECK_RTI(rtip);

	if(!rtip->needprep)
		rt_bomb("rt_gettree called again after rt_prep!\n");

	if( argc <= 0 )  return(-1);	/* FAIL */

	prev_sol_count = rtip->nsolids;
	rt_tree_rtip = rtip;

	i = db_walk_tree( rtip->rti_dbip, argc, argv, ncpus,
		&rt_initial_tree_state,
		rt_gettree_region_start,
		rt_gettree_region_end,
		rt_gettree_leaf );

	rt_tree_rtip = (struct rt_i *)0;	/* sanity */
	if( i < 0 )  return(-1);

	if( rtip->nsolids <= prev_sol_count )
		rt_log("rt_gettrees(%s) warning:  no solids found\n", argv[0]);
	return(0);	/* OK */
}

#if 0	/* XXX rt_plookup replaced by db_follow_path_for_state */
/*
 *			R T _ P L O O K U P
 * 
 *  Look up a path where the elements are separates by slashes.
 *  If the whole path is valid,
 *  set caller's pointer to point at path array.
 *
 *  Returns -
 *	# path elements on success
 *	-1	ERROR
 */
int
rt_plookup( rtip, dirp, cp, noisy )
struct rt_i	*rtip;
struct directory ***dirp;
register char	*cp;
int		noisy;
{
	struct db_tree_state	ts;
	struct db_full_path	path;

	bzero( (char *)&ts, sizeof(ts) );
	ts.ts_dbip = rtip->rti_dbip;
	mat_idn( ts.ts_mat );
	path.fp_len = path.fp_maxlen = 0;
	path.fp_names = (struct directory **)0;

	if( db_follow_path_for_state( &ts, &path, cp, noisy ) < 0 )
		return(-1);		/* ERROR */

	*dirp = path.fp_names;
	return(path.fp_len);
}
#endif

/*
 *			R T _ B O U N D _ T R E E
 *
 *  Calculate the bounding RPP of the region whose boolean tree is 'tp'.
 *  The bounding RPP is returned in tree_min and tree_max, which need
 *  not have been initialized first.
 *
 *  Returns -
 *	0	success
 *	-1	failure (tree_min and tree_max may have been altered)
 */
int
rt_bound_tree( tp, tree_min, tree_max )
register union tree	*tp;
vect_t			tree_min;
vect_t			tree_max;
{	
	vect_t	r_min, r_max;		/* rpp for right side of tree */

	if( tp == TREE_NULL )  {
		rt_log( "rt_bound_tree(): NULL tree pointer.\n" );
		return(-1);
	}

	switch( tp->tr_op )  {

	case OP_SOLID:
		{
			register struct soltab	*stp;

			stp = tp->tr_a.tu_stp;

			if( stp->st_aradius >= INFINITY )  {
				VSETALL( tree_min, -INFINITY );
				VSETALL( tree_max,  INFINITY );
				return(0);
			}
			VMOVE( tree_min, stp->st_min );
			VMOVE( tree_max, stp->st_max );
			return(0);
		}

	default:
		rt_log( "rt_bound_tree(x%x): unknown op=x%x\n",
			tp, tp->tr_op );
		return(-1);

	case OP_XOR:
	case OP_UNION:
		/* BINARY type -- expand to contain both */
		if( rt_bound_tree( tp->tr_b.tb_left, tree_min, tree_max ) < 0 ||
		    rt_bound_tree( tp->tr_b.tb_right, r_min, r_max ) < 0 )
			return(-1);
		VMIN( tree_min, r_min );
		VMAX( tree_max, r_max );
		break;
	case OP_INTERSECT:
		/* BINARY type -- find common area only */
		if( rt_bound_tree( tp->tr_b.tb_left, tree_min, tree_max ) < 0 ||
		    rt_bound_tree( tp->tr_b.tb_right, r_min, r_max ) < 0 )
			return(-1);
		/* min = largest min, max = smallest max */
		VMAX( tree_min, r_min );
		VMIN( tree_max, r_max );
		break;
	case OP_SUBTRACT:
		/* BINARY type -- just use left tree */
		if( rt_bound_tree( tp->tr_b.tb_left, tree_min, tree_max ) < 0 ||
		    rt_bound_tree( tp->tr_b.tb_right, r_min, r_max ) < 0 )
			return(-1);
		/* Discard right rpp */
		break;
	}
	return(0);
}

/*
 *			R T _ B A S E N A M E
 *
 *  Given a string containing slashes such as a pathname, return a
 *  pointer to the first character after the last slash.
 */
char *
rt_basename( str )
register char	*str;
{	
	register char	*p = str;
	while( *p != '\0' )
		if( *p++ == '/' )
			str = p;
	return	str;
}

/*
 *			R T _ G E T R E G I O N
 *
 *  Return a pointer to the corresponding region structure of the given
 *  region's name (reg_name), or REGION_NULL if it does not exist.
 *
 *  If the full path of a region is specified, then that one is
 *  returned.  However, if only the database node name of the
 *  region is specified and that region has been referenced multiple
 *  time in the tree, then this routine will simply return the first one.
 */
HIDDEN struct region *
rt_getregion( rtip, reg_name )
struct rt_i	*rtip;
register char	*reg_name;
{	
	register struct region	*regp = rtip->HeadRegion;
	register char *reg_base = rt_basename(reg_name);

	for( ; regp != REGION_NULL; regp = regp->reg_forw )  {	
		register char	*cp;
		/* First, check for a match of the full path */
		if( *reg_base == regp->reg_name[0] &&
		    strcmp( reg_base, regp->reg_name ) == 0 )
			return(regp);
		/* Second, check for a match of the database node name */
		cp = rt_basename( regp->reg_name );
		if( *cp == *reg_name && strcmp( cp, reg_name ) == 0 )
			return(regp);
	}
	return(REGION_NULL);
}

/*
 *			R T _ R P P _ R E G I O N
 *
 *  Calculate the bounding RPP for a region given the name of
 *  the region node in the database.  See remarks in rt_getregion()
 *  above for name conventions.
 *  Returns 0 for failure (and prints a diagnostic), or 1 for success.
 */
int
rt_rpp_region( rtip, reg_name, min_rpp, max_rpp )
struct rt_i	*rtip;
char		*reg_name;
fastf_t		*min_rpp, *max_rpp;
{	
	register struct region	*regp;

	RT_CHECK_RTI(rtip);

	regp = rt_getregion( rtip, reg_name );
	if( regp == REGION_NULL )  return(0);
	if( rt_bound_tree( regp->reg_treetop, min_rpp, max_rpp ) < 0)
		return(0);
	return(1);
}

/*
 *			R T _ F A S T F _ F L O A T
 *
 *  Convert TO fastf_t FROM 3xfloats (for database) 
 */
void
rt_fastf_float( ff, fp, n )
register fastf_t *ff;
register dbfloat_t *fp;
register int n;
{
	while( n-- )  {
		*ff++ = *fp++;
		*ff++ = *fp++;
		*ff++ = *fp++;
		ff += ELEMENTS_PER_VECT-3;
	}
}

/*
 *			R T _ M A T _ D B M A T
 *
 *  Convert TO fastf_t matrix FROM dbfloats (for database) 
 */
void
rt_mat_dbmat( ff, dbp )
register fastf_t *ff;
register dbfloat_t *dbp;
{

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
}

/*
 *			R T _ D B M A T _ M A T
 *
 *  Convert FROM fastf_t matrix TO dbfloats (for updating database) 
 */
void
rt_dbmat_mat( dbp, ff )
register dbfloat_t *dbp;
register fastf_t *ff;
{

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;

	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
	*dbp++ = *ff++;
}

/*
 *  			R T _ F I N D _ S O L I D
 *  
 *  Given the (leaf) name of a solid, find the first occurance of it
 *  in the solid list.  Used mostly to find the light source.
 *  Returns soltab pointer, or RT_SOLTAB_NULL.
 */
struct soltab *
rt_find_solid( rtip, name )
struct rt_i *rtip;
register char *name;
{
	register struct soltab	*stp;
	register char		*cp;

	RT_CHECK_RTI(rtip);

	for( RT_LIST( stp, soltab, &(rtip->rti_headsolid) ) )  {
		if( *(cp = stp->st_name) == *name  &&
		    strcmp( cp, name ) == 0 )  {
			return(stp);
		}
	}
	return(RT_SOLTAB_NULL);
}


/*
 *			R T _ O P T I M _ T R E E
 */
HIDDEN void
rt_optim_tree( tp, resp )
register union tree	*tp;
struct resource		*resp;
{
	register union tree	**sp;
	register union tree	*low;
	register union tree	**stackend;

	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
	*sp++ = TREE_NULL;
	*sp++ = tp;
	while( (tp = *--sp) != TREE_NULL ) {
		switch( tp->tr_op )  {
		case OP_SOLID:
			break;
		case OP_SUBTRACT:
			while( (low=tp->tr_b.tb_left)->tr_op == OP_SUBTRACT )  {
				/* Rewrite X - A - B as X - ( A union B ) */
				tp->tr_b.tb_left = low->tr_b.tb_left;
				low->tr_op = OP_UNION;
				low->tr_b.tb_left = low->tr_b.tb_right;
				low->tr_b.tb_right = tp->tr_b.tb_right;
				tp->tr_b.tb_right = low;
			}
			/* push both nodes - search left first */
			*sp++ = tp->tr_b.tb_right;
			*sp++ = tp->tr_b.tb_left;
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_XOR:
			/* Need to look at 3-level optimizations here, both sides */
			/* push both nodes - search left first */
			*sp++ = tp->tr_b.tb_right;
			*sp++ = tp->tr_b.tb_left;
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		default:
			rt_log("rt_optim_tree: bad op x%x\n", tp->tr_op);
			break;
		}
	}
}

/*
 *			R T _ T R E E _ R E G I O N _ A S S I G N
 */
HIDDEN void
rt_tree_region_assign( tp, regionp )
register union tree	*tp;
register struct region	*regionp;
{
	switch( tp->tr_op )  {
	case OP_SOLID:
		tp->tr_a.tu_regionp = regionp;
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		tp->tr_b.tb_regionp = regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		tp->tr_b.tb_regionp = regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		rt_tree_region_assign( tp->tr_b.tb_right, regionp );
		return;

	default:
		rt_bomb("rt_tree_region_assign: bad op\n");
	}
}
