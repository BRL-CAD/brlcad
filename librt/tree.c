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
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

RT_EXTERN(void		rt_tree_kill_dead_solid_refs, (union tree *tp));
RT_EXTERN(CONST char *	rt_basename, (CONST char *str));
int		rt_bound_tree();	/* used by rt/sh_light.c */

HIDDEN struct region *rt_getregion();
HIDDEN void	rt_tree_region_assign();


/*
 *  Also used by converters in conv/ directory.
 *  Don't forget to initialize ts_dbip before use.
 */
CONST struct db_tree_state	rt_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0, 0, 0, 0,		/* region, air, gmater, LOS */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
		1.0, 1.0, 1.0,		/* color, RGB */
		0,			/* override */
		DB_INH_LOWER,		/* color inherit */
		DB_INH_LOWER,		/* mater inherit */
		(char *)0		/* shader */
#if __STDC__
	}
#endif
	,
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
/* ARGSUSED */
HIDDEN int rt_gettree_region_start( tsp, pathp )
CONST struct db_tree_state	*tsp;
CONST struct db_full_path	*pathp;
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
 *  As a result, note that the details of the solids pointed to
 *  by the soltab pointers in the tree may not be filled in
 *  when this routine is called (due to the way multiple instances of
 *  solids are handled).
 *  Therefore, everything which referred to the tree has been moved
 *  out into the serial section.  (rt_tree_region_assign, rt_bound_tree)
 */
HIDDEN union tree *rt_gettree_region_end( tsp, pathp, curtree )
register CONST struct db_tree_state	*tsp;
CONST struct db_full_path	*pathp;
union tree			*curtree;
{
	struct region		*rp;
	struct directory	*dp;
	int			shader_len=0;

	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);
	RT_CK_TREE(curtree);

	if( curtree->tr_op == OP_NOP )  {
		/* Ignore empty regions */
		return  curtree;
	}

	BU_GETSTRUCT( rp, region );
	rp->reg_magic = RT_REGION_MAGIC;
	rp->reg_forw = REGION_NULL;
	rp->reg_regionid = tsp->ts_regionid;
	rp->reg_aircode = tsp->ts_aircode;
	rp->reg_gmater = tsp->ts_gmater;
	rp->reg_los = tsp->ts_los;
	rp->reg_mater = tsp->ts_mater;		/* struct copy */
	if( tsp->ts_mater.ma_shader )
		shader_len = strlen( tsp->ts_mater.ma_shader );
	if( shader_len )
	{
		rp->reg_mater.ma_shader = (char *)bu_malloc( shader_len+1, "rt_gettree_region_end: ma_shader" );
		strcpy( rp->reg_mater.ma_shader, tsp->ts_mater.ma_shader );
	}
	else
		rp->reg_mater.ma_shader = (char *)NULL;

	rp->reg_name = db_path_to_string( pathp );

	dp = (struct directory *)DB_FULL_PATH_CUR_DIR(pathp);

	if(rt_g.debug&DEBUG_TREEWALK)  {
		bu_log("rt_gettree_region_end() %s\n", rp->reg_name );
		rt_pr_tree( curtree, 0 );
	}
	
	rp->reg_treetop = curtree;

	/* Determine material properties */
	rp->reg_mfuncs = (char *)0;
	rp->reg_udata = (char *)0;
	if( rp->reg_mater.ma_override == 0 )
		rt_region_color_map(rp);

	bu_semaphore_acquire( RT_SEM_RESULTS );	/* enter critical section */

	rp->reg_instnum = dp->d_uses++;

	/*
	 *  Add the region to the linked list of regions.
	 *  Positions in the region bit vector are established at this time.
	 */
	rp->reg_forw = rt_tree_rtip->HeadRegion;
	rt_tree_rtip->HeadRegion = rp;

	rp->reg_bit = rt_tree_rtip->nregions++;	/* Assign bit vector pos. */
	bu_semaphore_release( RT_SEM_RESULTS );	/* leave critical section */

	if( rt_g.debug & DEBUG_REGIONS )  {
		bu_log("Add Region %s instnum %d\n",
			rp->reg_name, rp->reg_instnum);
	}

	/* Indicate that we have swiped 'curtree' */
	return(TREE_NULL);
}

/*
 *			R T _ F I N D _ I D E N T I C A L _ S O L I D
 *
 *  See if solid "dp" as transformed by "mat" already exists in the
 *  soltab list.  If it does, return the matching stp, otherwise,
 *  create a new soltab structure, enrole it in the list, and return
 *  a pointer to that.
 *
 *  "mat" will be a null pointer when an identity matrix is signified.
 *  This greatly speeds the comparison process.
 *
 *  The two cases can be distinguished by the fact that stp->st_id will
 *  be 0 for a new soltab structure, and non-zero for an existing one.
 *
 *  This routine will run in parallel.
 *
 *  In order to avoid a race between searching the soltab list and
 *  adding new solids to it, the new solid to be added *must* be
 *  enrolled in the list before exiting the critical section.
 *
 *  To limit the size of the list to be searched, there are many lists.
 *  The selection of which list is determined by the hash value
 *  computed from the solid's name.
 *  This is the same optimization used in searching the directory lists.
 *
 *  This subroutine is the critical bottleneck in parallel tree walking.
 *
 *  It is safe, and much faster, to use several different
 *  critical sections when searching different lists.
 *
 *  There are only 4 dedicated semaphores defined, TREE0 through TREE3.
 *  This unfortunately limits the code to having only 4 CPUs doing list
 *  searching at any one time.  Hopefully, this is enough parallelism
 *  to keep the rest of the CPUs doing I/O and actual solid prepping.
 *
 *  Since the algorithm has been reduced from an O((nsolid/128)**2)
 *  search on the entire rti_solidheads[hash] list to an O(ninstance)
 *  search on the dp->d_use_head list for this one solid, the critical
 *  section should be relatively short-lived.  Having the 3-way split
 *  should provide ample opportunity for parallelism through here,
 *  while still ensuring that the necessary variables are protected.
 *
 *  There are two critical variables which *both* need to be protected:
 *  the specific rti_solidhead[hash] list head, and the specific
 *  dp->d_use_hd list head.  Fortunately, since the selection of
 *  critical section is based upon db_dirhash(dp->d_namep), any other
 *  processor that wants to search this same 'dp' will get the same
 *  hash as the current thread, and will thus wait for the appropriate
 *  semaphore to be released.  Similarly, any other thread that
 *  wants to search the same rti_solidhead[hash] list as the current
 *  thread will be using the same hash, and will thus wait for the
 *  proper semaphore.
 */
HIDDEN struct soltab *rt_find_identical_solid( mat, dp, rtip )
register CONST matp_t		mat;
register struct directory	*dp;
struct rt_i			*rtip;
{
	register struct soltab	*stp = RT_SOLTAB_NULL;
	int			have_match;
	int			hash;

	RT_CK_DIR(dp);
	RT_CK_RTI(rtip);

	have_match = 0;
	hash = db_dirhash( dp->d_namep );

	/* Enter the appropriate dual critical-section */
	switch( hash&3 )  {
	case 0:
		bu_semaphore_acquire( RT_SEM_TREE0 );
		break;
	case 1:
		bu_semaphore_acquire( RT_SEM_TREE1 );
		break;
	case 2:
		bu_semaphore_acquire( RT_SEM_TREE2 );
		break;
	default:
		bu_semaphore_acquire( RT_SEM_TREE3 );
		break;
	}

	/* If solid has not been referenced yet, the search can be skipped */
	if( dp->d_uses > 0 )  {
		struct bu_list	*mid;

		/* Search dp->d_use_hd list for other instances */
		for( BU_LIST_FOR( mid, rt_list, &dp->d_use_hd ) )  {

			stp = BU_LIST_MAIN_PTR( soltab, mid, l2 );
			RT_CK_SOLTAB(stp);

			/* Don't instance this solid in some other model instance */
			if( stp->st_rtip != rtip )  continue;

			if( mat == (matp_t)0 )  {
				if( stp->st_matp == (matp_t)0 )  {
					have_match = 1;
					break;
				}
				continue;
			}
			if( stp->st_matp == (matp_t)0 )  continue;

			if (rt_mat_is_equal(mat, stp->st_matp, &rtip->rti_tol)) {
				have_match = 1;
				break;
			}
		}
	}

	if( have_match )  {
		/*
		 *  stp now points to re-referenced solid
		 */
		RT_CK_SOLTAB(stp);		/* sanity */
		stp->st_uses++;
		/* dp->d_uses is NOT incremented, because number of soltab's using it has not gone up. */
		if( rt_g.debug & DEBUG_SOLIDS )  {
			bu_log( mat ?
			    "rt_find_identical_solid:  %s re-referenced %d\n" :
			    "rt_find_identical_solid:  %s re-referenced %d (identity mat)\n",
				dp->d_namep, stp->st_uses );
		}
	} else {
		/*
		 *  Create and link a new solid into the list.
		 *  Ensure the search keys "dp" and "mat" are stored now.
		 */
		BU_GETSTRUCT(stp, soltab);
		stp->l.magic = RT_SOLTAB_MAGIC;
		stp->l2.magic = RT_SOLTAB2_MAGIC;
		stp->st_rtip = rtip;
		stp->st_uses = 1;
		stp->st_dp = dp;
		dp->d_uses++;
		stp->st_bit = rtip->nsolids++;
		/* stp->st_id is intentionally left zero here, as a flag */

		if( mat )  {
			stp->st_matp = (matp_t)rt_malloc( sizeof(mat_t), "st_matp" );
			mat_copy( stp->st_matp, mat );
		} else {
			stp->st_matp = (matp_t)0;
		}
		/* Add to the appropriate soltab list head */
		/* PARALLEL NOTE:  Uses critical section on rt_solidheads element */
		BU_LIST_INSERT( &(rtip->rti_solidheads[hash]), &(stp->l) );

		/* Also add to the directory structure list head */
		/* PARALLEL NOTE:  Uses critical section on this 'dp' */
		BU_LIST_INSERT( &dp->d_use_hd, &(stp->l2) );

		/* Tables of regions using this solid.  Usually small. */
		bu_ptbl_init( &stp->st_regions, 7, "st_regions ptbl" );
	}

	/* Leave the appropriate dual critical-section */
	switch( hash&3 )  {
	case 0:
		bu_semaphore_release( RT_SEM_TREE0 );
		break;
	case 1:
		bu_semaphore_release( RT_SEM_TREE1 );
		break;
	case 2:
		bu_semaphore_release( RT_SEM_TREE2 );
		break;
	default:
		bu_semaphore_release( RT_SEM_TREE3 );
		break;
	}
	return stp;
}


/*
 *			R T _ G E T T R E E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *rt_gettree_leaf( tsp, pathp, ep, id )
CONST struct db_tree_state	*tsp;
struct db_full_path		*pathp;
CONST struct bu_external	*ep;
int				id;
{
	register struct soltab	*stp;
	union tree		*curtree;
	struct directory	*dp;
	struct rt_db_internal	intern;
	register matp_t		mat;
	int			i;

	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_FULL_PATH(pathp);
	BU_CK_EXTERNAL(ep);
	RT_CK_RTI(rt_tree_rtip);
	dp = DB_FULL_PATH_CUR_DIR(pathp);

	/* Determine if this matrix is an identity matrix */

	if( !rt_mat_is_equal(tsp->ts_mat, bn_mat_identity, &rt_tree_rtip->rti_tol)) {
		/* Not identity matrix */
		mat = (matp_t)tsp->ts_mat;
	} else {
		/* Identity matrix */
		mat = (matp_t)0;
	}

	/*
	 *  Check to see if this exact solid has already been processed.
	 *  Match on leaf name and matrix.
	 *  Note that there is a race here between having st_id filled
	 *  in a few lines below (which is necessary for calling ft_prep),
	 *  and ft_prep filling in st_aradius.  Fortunately, st_aradius
	 *  starts out as zero, and will never go down to -1 unless this
	 *  soltab structure has become a dead solid, so by testing against
	 *  -1 (instead of <= 0, like before, oops), it isn't a problem.
	 */
	stp = rt_find_identical_solid( mat, dp, rt_tree_rtip );
	if( stp->st_id != 0 )  {
		if( stp->st_aradius <= -1 )  {
			stp->st_uses--;
			return( TREE_NULL );	/* BAD: instance of dead solid */
		}
		goto found_it;
	}

	stp->st_id = id;
	if( mat )  {
		mat = stp->st_matp;
	} else {
		mat = (matp_t)bn_mat_identity;
	}

	/*
	 *  Import geometry from on-disk (external) format to internal.
	 */
    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, ep, mat ) < 0 )  {
		bu_log("rt_gettree_leaf(%s):  solid import failure\n", dp->d_namep );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		/* Too late to delete soltab entry; mark it as "dead" */
		stp->st_aradius = -1;
		stp->st_uses--;
		return( TREE_NULL );		/* BAD */
	}
	RT_CK_DB_INTERNAL( &intern );

	/* init solid's maxima and minima */
	VSETALL( stp->st_max, -INFINITY );
	VSETALL( stp->st_min,  INFINITY );

    	/*
    	 *  If the ft_prep routine wants to keep the internal structure,
    	 *  that is OK, as long as idb_ptr is set to null.
	 *  Note that the prep routine may have changed st_id.
    	 */
	if( rt_functab[id].ft_prep( stp, &intern, rt_tree_rtip ) )  {
		/* Error, solid no good */
		bu_log("rt_gettree_leaf(%s):  prep failure\n", dp->d_namep );
	    	if( intern.idb_ptr )  rt_functab[stp->st_id].ft_ifree( &intern );
		/* Too late to delete soltab entry; mark it as "dead" */
		stp->st_aradius = -1;
		stp->st_uses--;
		return( TREE_NULL );		/* BAD */
	}

	/*
	 *  If there is more than just a direct reference to this leaf
	 *  from it's containing region, copy that below-region path
	 *  into st_path.  Otherwise, leave st_path's magic number 0.
	 */
	i = pathp->fp_len-1;
	if( i > 0 && !(pathp->fp_names[i-1]->d_flags & DIR_REGION) )  {
		/* Search backwards for region.  If no region, use whole path */
		for( --i; i > 0; i-- )  {
			if( pathp->fp_names[i-1]->d_flags & DIR_REGION ) break;
		}
		if( i < 0 )  i = 0;
		db_full_path_init( &stp->st_path );
		db_dup_path_tail( &stp->st_path, pathp, i );
		if(rt_g.debug&DEBUG_TREEWALK)  {
			char	*sofar = db_path_to_string(&stp->st_path);
			bu_log("rt_gettree_leaf() st_path=%s\n", sofar );
			rt_free(sofar, "path string");
		}
	}

#if 0
	if(rt_g.debug&DEBUG_SOLIDS)  {
		struct bu_vls	str;
		bu_log("\n---Solid %d: %s\n", stp->st_bit, dp->d_namep);
		bu_vls_init( &str );
		/* verbose=1, mm2local=1.0 */
		if( rt_functab[stp->st_id].ft_describe( &str, &intern, 1, 1.0 ) < 0 )  {
			bu_log("rt_gettree_leaf(%s):  solid describe failure\n",
				dp->d_namep );
		}
		bu_log( "%s:  %s", dp->d_namep, bu_vls_addr( &str ) );
		bu_vls_free( &str );
	}
#endif

	/* Release internal version */
    	if( intern.idb_ptr )  rt_functab[stp->st_id].ft_ifree( &intern );

found_it:
	BU_GETUNION( curtree, tree );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_SOLID;
	curtree->tr_a.tu_stp = stp;
	/* regionp will be filled in later by rt_tree_region_assign() */
	curtree->tr_a.tu_regionp = (struct region *)0;

	if(rt_g.debug&DEBUG_TREEWALK)  {
		char	*sofar = db_path_to_string(pathp);
		bu_log("rt_gettree_leaf() %s\n", sofar );
		rt_free(sofar, "path string");
	}

	return(curtree);
}

/*
 *			R T _ F R E E _ S O L T A B
 *
 *  Decrement use count on soltab structure.  If no longer needed,
 *  release associated storage, and free the structure.
 *
 *  This routine can not be used in PARALLEL, hence the st_aradius hack.
 *
 *  This routine will semaphore protect against other copies of itself
 *  running in parallel.  However, there is no protection against
 *  other routines (such as rt_find_identical_solid()) which might
 *  be modifying the linked list heads while locked on other semaphores
 *  (resources).  This is the strongest argument I can think of for
 *  removing the 3-way semaphore stuff in rt_find_identical_solid().
 *
 *  Called by -
 *	db_free_tree()
 *	rt_clean()
 *	rt_gettrees()
 *	rt_kill_deal_solid_refs()
 */
void
rt_free_soltab( stp )
struct soltab	*stp;
{
	RT_CK_SOLTAB(stp);
	if( stp->st_id < 0 || stp->st_id >= rt_nfunctab )
		rt_bomb("rt_free_soltab:  bad st_id");

	bu_semaphore_acquire( RT_SEM_MODEL );
	if( --(stp->st_uses) > 0 )  {
		bu_semaphore_release( RT_SEM_MODEL );
		return;
	}

	/* NON-PARALLEL, on d_use_hd (may be locked on other semaphore) */
	BU_LIST_DEQUEUE( &(stp->l2) );	/* remove from st_dp->d_use_hd list */

	BU_LIST_DEQUEUE( &(stp->l) );	/* NON-PARALLEL on rti_solidheads[] */

	bu_semaphore_release( RT_SEM_MODEL );

	if( stp->st_aradius > 0 )  {
		rt_functab[stp->st_id].ft_free( stp );
		stp->st_aradius = 0;
	}
	if( stp->st_matp )  rt_free( (char *)stp->st_matp, "st_matp");
	stp->st_matp = (matp_t)0;	/* Sanity */

	bu_ptbl_free(&stp->st_regions);

	stp->st_dp = DIR_NULL;		/* Sanity */
	rt_free( (char *)stp, "struct soltab" );

	if( stp->st_path.magic )  {
		RT_CK_FULL_PATH( &stp->st_path );
		db_free_full_path( &stp->st_path );
	}
}

/*
 *  			R T _ G E T T R E E
 *
 *  User-called function to add a tree hierarchy to the displayed set.
 *
 *  This function is not multiply re-entrant.
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettree( rtip, node )
struct rt_i	*rtip;
CONST char	*node;
{
	return( rt_gettrees( rtip, 1, &node, 1 ) );
}

/*
 *  			R T _ G E T T R E E S
 *
 *  User-called function to add a set of tree hierarchies to the active set.
 *
 *  This function may run in parallel, but is not multiply re-entrant itself,
 *  because db_walk_tree() isn't multiply re-entrant.
 *
 *  Semaphores used for critical sections in parallel mode:
 *	RT_SEM_MODEL	protects rti_solidheads[] lists
 *	RT_SEM_RESULTS	protects HeadRegion, mdl_min/max, d_uses, nregions
 *	res_worker	(db_walk_dispatcher, from db_walk_tree)
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
int
rt_gettrees( rtip, argc, argv, ncpus )
struct rt_i	*rtip;
int		argc;
CONST char	**argv;
int		ncpus;
{
	register struct soltab	*stp;
	register struct region	*regp;
	struct db_tree_state	tree_state;
	int			prev_sol_count;
	int			i;
	point_t			region_min, region_max;

	RT_CHECK_RTI(rtip);
	RT_CK_DBI(rtip->rti_dbip);

	if(!rtip->needprep)  {
		bu_log("ERROR: rt_gettree() called again after rt_prep!\n");
		return(-1);		/* FAIL */
	}

	if( argc <= 0 )  return(-1);	/* FAIL */

	prev_sol_count = rtip->nsolids;
	rt_tree_rtip = rtip;

	tree_state = rt_initial_tree_state;	/* struct copy */
	tree_state.ts_dbip = rtip->rti_dbip;

	i = db_walk_tree( rtip->rti_dbip, argc, argv, ncpus,
		&rt_initial_tree_state,
		rt_gettree_region_start,
		rt_gettree_region_end,
		rt_gettree_leaf );

	rt_tree_rtip = (struct rt_i *)0;	/* sanity */

	/* DEBUG:  Ensure that all region trees are valid */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		RT_CK_REGION(regp);
		db_ck_tree(regp->reg_treetop);
	}

	/*
	 *  Eliminate any "dead" solids that parallel code couldn't change.
	 *  First remove any references from the region tree,
	 *  then remove actual soltab structs from the soltab list.
	 */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		RT_CK_REGION(regp);
		rt_tree_kill_dead_solid_refs( regp->reg_treetop );
		(void)rt_tree_elim_nops( regp->reg_treetop );
	}
again:
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		RT_CK_SOLTAB(stp);
		if( stp->st_aradius <= 0 )  {
			bu_log("rt_gettrees() cleaning up dead solid '%s'\n",
				stp->st_dp->d_namep );
			rt_free_soltab(stp);
			/* Can't do rtip->nsolids--, that doubles as max bit number! */
			/* The macro makes it hard to regain place, punt */
			goto again;
		}
		if(rt_g.debug&DEBUG_SOLIDS)
			rt_pr_soltab( stp );
	} RT_VISIT_ALL_SOLTABS_END

	/* Handle finishing touches on the trees that needed soltab structs
	 * that the parallel code couldn't look at yet.
	 */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		RT_CK_REGION(regp);

		/* The region and the entire tree are cross-referenced */
		rt_tree_region_assign( regp->reg_treetop, regp );

		/*
		 *  Find region RPP, and update the model maxima and minima
		 *
		 *  Don't update min & max for halfspaces;  instead, add them
		 *  to the list of infinite solids, for special handling.
		 */
		if( rt_bound_tree( regp->reg_treetop, region_min, region_max ) < 0 )  {
			bu_log("rt_gettrees() %s\n", regp->reg_name );
			rt_bomb("rt_gettrees(): rt_bound_tree() fail\n");
		}
		if( region_max[X] < INFINITY )  {
			/* infinite regions are exempted from this */
			VMINMAX( rtip->mdl_min, rtip->mdl_max, region_min );
			VMINMAX( rtip->mdl_min, rtip->mdl_max, region_max );
		}
	}

	/* DEBUG:  Ensure that all region trees are valid */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		RT_CK_REGION(regp);
		db_ck_tree(regp->reg_treetop);
	}

	if( i < 0 )  return(-1);

	if( rtip->nsolids <= prev_sol_count )
		bu_log("rt_gettrees(%s) warning:  no solids found\n", argv[0]);
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
register CONST union tree	*tp;
vect_t				tree_min;
vect_t				tree_max;
{	
	vect_t	r_min, r_max;		/* rpp for right side of tree */

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_SOLID:
		{
			register CONST struct soltab	*stp;

			stp = tp->tr_a.tu_stp;
			RT_CK_SOLTAB(stp);
			if( stp->st_aradius <= 0 )  {
				bu_log("rt_bound_tree: encountered dead solid '%s'\n",
					stp->st_dp->d_namep);
				return -1;	/* ERROR */
			}

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
		bu_log( "rt_bound_tree(x%x): unknown op=x%x\n",
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
	case OP_NOP:
		/* Implies that this tree has nothing in it */
		break;
	}
	return(0);
}

/*
 *			R T _ T R E E _ K I L L _ D E A D _ S O L I D _ R E F S
 *
 *  Convert any references to "dead" solids into NOP nodes.
 */
void
rt_tree_kill_dead_solid_refs( tp )
register union tree	*tp;
{	

	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_SOLID:
		{
			register struct soltab	*stp;

			stp = tp->tr_a.tu_stp;
			RT_CK_SOLTAB(stp);
			if( stp->st_aradius <= 0 )  {
				if(rt_g.debug&DEBUG_TREEWALK)bu_log("rt_tree_kill_dead_solid_refs: encountered dead solid '%s' stp=x%x, tp=x%x\n",
					stp->st_dp->d_namep, stp, tp);
				rt_free_soltab(stp);
				tp->tr_a.tu_stp = SOLTAB_NULL;
				tp->tr_op = OP_NOP;	/* Convert to NOP */
			}
			return;
		}

	default:
		bu_log( "rt_tree_kill_dead_solid_refs(x%x): unknown op=x%x\n",
			tp, tp->tr_op );
		return;

	case OP_XOR:
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
		/* BINARY */
		rt_tree_kill_dead_solid_refs( tp->tr_b.tb_left );
		rt_tree_kill_dead_solid_refs( tp->tr_b.tb_right );
		break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* UNARY tree -- for completeness only, should never be seen */
		rt_tree_kill_dead_solid_refs( tp->tr_b.tb_left );
		break;
	case OP_NOP:
		/* This sub-tree has nothing further in it */
		return;
	}
	return;
}

/*
 *			R T _ T R E E _ E L I M _ N O P S
 *
 *  Eliminate any references to NOP nodes from the tree.
 *  It is safe to use db_free_tree() here, because there will not
 *  be any dead solids.  They will all have been converted to OP_NOP
 *  nodes by rt_tree_kill_dead_solid_refs(), previously, so there
 *  is no need to worry about multiple db_free_tree()'s repeatedly
 *  trying to free one solid that has been instanced multiple times.
 *
 *  Returns -
 *	0	this node is OK.
 *	-1	request caller to kill this node
 */
int
rt_tree_elim_nops( tp )
register union tree	*tp;
{	
	union tree	*left, *right;
top:
	RT_CK_TREE(tp);

	switch( tp->tr_op )  {

	case OP_SOLID:
		return(0);		/* Retain */

	default:
		bu_log( "rt_tree_elim_nops(x%x): unknown op=x%x\n",
			tp, tp->tr_op );
		return(-1);

	case OP_XOR:
	case OP_UNION:
		/* BINARY type -- rewrite tp as surviving side */
		left = tp->tr_b.tb_left;
		right = tp->tr_b.tb_right;
		if( rt_tree_elim_nops( left ) < 0 )  {
			*tp = *right;	/* struct copy */
			rt_free( (char *)left, "rt_tree_elim_nops union tree");
			rt_free( (char *)right, "rt_tree_elim_nops union tree");
			goto top;
		}
		if( rt_tree_elim_nops( right ) < 0 )  {
			*tp = *left;	/* struct copy */
			rt_free( (char *)left, "rt_tree_elim_nops union tree");
			rt_free( (char *)right, "rt_tree_elim_nops union tree");
			goto top;
		}
		break;
	case OP_INTERSECT:
		/* BINARY type -- if either side fails, nuke subtree */
		left = tp->tr_b.tb_left;
		right = tp->tr_b.tb_right;
		if( rt_tree_elim_nops( left ) < 0 ||
		    rt_tree_elim_nops( right ) < 0 )  {
		    	db_free_tree( left );
		    	db_free_tree( right );
		    	tp->tr_op = OP_NOP;
		    	return(-1);	/* eliminate reference to tp */
		}
		break;
	case OP_SUBTRACT:
		/* BINARY type -- if right fails, rewrite (X - 0 = X).
		 *  If left fails, nuke entire subtree (0 - X = 0).
		 */
		left = tp->tr_b.tb_left;
		right = tp->tr_b.tb_right;
		if( rt_tree_elim_nops( left ) < 0 )  {
		    	db_free_tree( left );
		    	db_free_tree( right );
		    	tp->tr_op = OP_NOP;
		    	return(-1);	/* eliminate reference to tp */
		}
		if( rt_tree_elim_nops( right ) < 0 )  {
			*tp = *left;	/* struct copy */
			rt_free( (char *)left, "rt_tree_elim_nops union tree");
			rt_free( (char *)right, "rt_tree_elim_nops union tree");
			goto top;
		}
		break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		/* UNARY tree -- for completeness only, should never be seen */
		left = tp->tr_b.tb_left;
		if( rt_tree_elim_nops( left ) < 0 )  {
			rt_free( (char *)left, "rt_tree_elim_nops union tree");
			tp->tr_op = OP_NOP;
			return(-1);	/* Kill ref to unary op, too */
		}
		break;
	case OP_NOP:
		/* Implies that this tree has nothing in it */
		return(-1);		/* Kill ref to this NOP */
	}
	return(0);
}

/*
 *			R T _ B A S E N A M E
 *
 *  Given a string containing slashes such as a pathname, return a
 *  pointer to the first character after the last slash.
 */
CONST char *
rt_basename( str )
register CONST char	*str;
{	
	register CONST char	*p = str;
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
struct rt_i		*rtip;
register CONST char	*reg_name;
{	
	register struct region	*regp = rtip->HeadRegion;
	register CONST char *reg_base = rt_basename(reg_name);

	RT_CK_RTI(rtip);
	for( ; regp != REGION_NULL; regp = regp->reg_forw )  {	
		register CONST char	*cp;
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
CONST char	*reg_name;
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
register CONST dbfloat_t *fp;
register int n;
{
#	include "noalias.h"
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
register CONST dbfloat_t *dbp;
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
register CONST fastf_t *ff;
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
CONST struct rt_i	*rtip;
register CONST char	*name;
{
	register struct soltab	*stp;
	struct directory	*dp;

	RT_CHECK_RTI(rtip);
	if( (dp = db_lookup( (struct db_i *)rtip->rti_dbip, (char *)name,
	    LOOKUP_QUIET )) == DIR_NULL )
		return(RT_SOLTAB_NULL);

	RT_VISIT_ALL_SOLTABS_START( stp, (struct rt_i *)rtip )  {
		if( stp->st_dp == dp )
			return(stp);
	} RT_VISIT_ALL_SOLTABS_END
	return(RT_SOLTAB_NULL);
}


/*
 *			R T _ O P T I M _ T R E E
 */
void
rt_optim_tree( tp, resp )
register union tree	*tp;
struct resource		*resp;
{
	register union tree	**sp;
	register union tree	*low;
	register union tree	**stackend;

	RT_CK_TREE(tp);
	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
	*sp++ = TREE_NULL;
	*sp++ = tp;
	while( (tp = *--sp) != TREE_NULL ) {
		switch( tp->tr_op )  {
		case OP_NOP:
			/* XXX Consider eliminating nodes of form (A op NOP) */
			/* XXX Needs to be detected in previous iteration */
			break;
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
			bu_log("rt_optim_tree: bad op x%x\n", tp->tr_op);
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
register CONST struct region	*regionp;
{
	RT_CK_TREE(tp);
	RT_CK_REGION(regionp);
	switch( tp->tr_op )  {
	case OP_NOP:
		return;

	case OP_SOLID:
		tp->tr_a.tu_regionp = (struct region *)regionp;
		return;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
		tp->tr_b.tb_regionp = (struct region *)regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		return;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		tp->tr_b.tb_regionp = (struct region *)regionp;
		rt_tree_region_assign( tp->tr_b.tb_left, regionp );
		rt_tree_region_assign( tp->tr_b.tb_right, regionp );
		return;

	default:
		rt_bomb("rt_tree_region_assign: bad op\n");
	}
}
