/*
 *			P R E P
 *
 *  Manage one-time preparations to be done before actual
 *  ray-tracing can commence.
 *
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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSprep[] = "@(#)$Header$ (BRL)";
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
#include "raytrace.h"
#include "plot3.h"
#include "./debug.h"

BU_EXTERN(void		rt_ck, (struct rt_i	*rtip));

HIDDEN void	rt_solid_bitfinder();

extern struct resource	rt_uniresource;		/* from shoot.c */

/* XXX Need rt_init_rtg(), rt_clean_rtg() */

/*
 *			R T _ N E W _ R T I
 *
 *  Given a db_i database instance, create an rt_i instance.
 *  If caller just called db_open, they need to do a db_close(),
 *  because we have cloned our own instance of the db_i.
 */
struct rt_i *
rt_new_rti( dbip )
struct db_i	*dbip;
{
	register struct rt_i	*rtip;
	register int		i;

	RT_CK_DBI( dbip );

	/* XXX Move to rt_global_init() ? */
	if( BU_LIST_FIRST( bu_list, &rt_g.rtg_vlfree ) == 0 )  {
		BU_LIST_INIT( &rt_g.rtg_vlfree );
	}

	BU_GETSTRUCT( rtip, rt_i );
	rtip->rti_magic = RTI_MAGIC;
	for( i=0; i < RT_DBNHASH; i++ )  {
		BU_LIST_INIT( &(rtip->rti_solidheads[i]) );
	}
	rtip->rti_dbip = db_clone_dbi( dbip, (long *)rtip );
	rtip->needprep = 1;

	BU_LIST_INIT( &rtip->HeadRegion );

	/* This table is used for discovering the per-cpu resource structures */
	bu_ptbl_init( &rtip->rti_resources, MAX_PSW+1, "rti_resources ptbl" );
	BU_PTBL_END(&rtip->rti_resources) = MAX_PSW+1;	/* Make 'em all available */

	rt_uniresource.re_magic = RESOURCE_MAGIC;

	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );
	VSETALL( rtip->rti_inf_box.bn.bn_min, -0.1 );
	VSETALL( rtip->rti_inf_box.bn.bn_max,  0.1 );
	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;

	/* XXX These defaults need to be improved */
	rtip->rti_tol.magic = BN_TOL_MAGIC;
	rtip->rti_tol.dist = 0.0005;
	rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
	rtip->rti_tol.perp = 1e-6;
	rtip->rti_tol.para = 1 - rtip->rti_tol.perp;

	rtip->rti_ttol.magic = RT_TESS_TOL_MAGIC;
	rtip->rti_ttol.abs = 0.0;
	rtip->rti_ttol.rel = 0.01;
	rtip->rti_ttol.norm = 0;

	rtip->rti_max_beam_radius = 175.0/2;	/* Largest Army bullet */

	rtip->rti_space_partition = RT_PART_NUBSPT;
	rtip->rti_nugrid_dimlimit = 0;
	rtip->rti_nu_gfactor = RT_NU_GFACTOR_DEFAULT;

	/*
	 *  Zero the solid instancing counters in dbip database instance.
	 *  Done here because the same dbip could be used by multiple
	 *  rti's, and rt_gettrees() can be called multiple times on
	 *  this one rtip.
	 *  There is a race (collision!) here on d_uses if rt_gettrees()
	 *  is called on another rtip of the same dbip
	 *  before this rtip is done
	 *  with all it's treewalking.
	 */
	for( i=0; i < RT_DBNHASH; i++ )  {
		register struct directory	*dp;

		dp = rtip->rti_dbip->dbi_Head[i];
		for( ; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_uses = 0;
	}

	return rtip;
}

/*
 *			R T _ F R E E _ R T I
 *
 *  Release all the dynamic storage acquired by rt_dirbuild() and
 *  any subsequent ray-tracing operations.
 *
 *  Any PARALLEL resource structures are freed by rt_clean().
 *  Note that the rt_g structure needs to be cleaned separately.
 */
void
rt_free_rti( rtip )
struct rt_i	*rtip;
{
	RT_CK_RTI(rtip);

	rt_clean( rtip );

#if 0
	/* XXX These can't be freed here either, because we allocated
	 * them all on rt_uniresource, which doesn't discriminate
	 * on which db_i they go with.
	 */

	/* The 'struct directory' guys are malloc()ed in big blocks */
	resp->re_directory_hd = NULL;	/* abandon list */
	if( BU_LIST_IS_INITIALIZED( &resp->re_directory_blocks.l ) )  {
		struct directory **dpp;
		BU_CK_PTBL( &resp->re_directory_blocks );
		for( BU_PTBL_FOR( dpp, (struct directory **), &resp->re_directory_blocks ) )  {
			RT_CK_DIR(*dpp);	/* Head of block will be a valid seg */
			bu_free( (genptr_t)(*dpp), "struct directory block" );
		}
		bu_ptbl_free( &resp->re_directory_blocks );
	}
#endif

	db_close_client( rtip->rti_dbip, (long *)rtip );
	rtip->rti_dbip = (struct db_i *)NULL;

	/* Freeing the actual resource structures's memory is the app's job */
	bu_ptbl_free( &rtip->rti_resources );

	bu_free( (char *)rtip, "struct rt_i" );
}

/*
 *  			R T _ P R E P _ P A R A L L E L
 *  
 *  This routine should be called just before the first call to rt_shootray().
 *  It should only be called ONCE per execution, unless rt_clean() is
 *  called inbetween.
 *
 *  Because this can be called from rt_shootray(), it may potentially be
 *  called ncpu times, hence the critical section.
 */
void
rt_prep_parallel(rtip, ncpu)
register struct rt_i	*rtip;
int			ncpu;
{
	register struct region *regp;
	register struct soltab *stp;
	register int		i;
	struct resource		*resp;
	vect_t			diag;

	RT_CK_RTI(rtip);

	if(RT_G_DEBUG&DEBUG_REGIONS)  bu_log("rt_prep_parallel(%s,%d,ncpu=%d) START\n",
			rtip->rti_dbip->dbi_filename,
			rtip->rti_dbip->dbi_uses, ncpu);

	bu_semaphore_acquire(RT_SEM_RESULTS);	/* start critical section */
	if(!rtip->needprep)  {
		bu_log("WARNING: rt_prep_parallel(%s,%d) invoked a second time, ignored",
			rtip->rti_dbip->dbi_filename,
			rtip->rti_dbip->dbi_uses);
		bu_semaphore_release(RT_SEM_RESULTS);
		return;
	}

	if( rtip->nsolids <= 0 )  {
		if( rtip->rti_air_discards > 0 )
			bu_log("rt_prep_parallel(%s,%d): %d primitives discarded due to air regions\n",
				rtip->rti_dbip->dbi_filename,
				rtip->rti_dbip->dbi_uses,
				rtip->rti_air_discards );
		rt_bomb("rt_prep_parallel:  no primitives left to prep");
	}

	/* In case everything is a halfspace, set a minimum space */
	if( rtip->mdl_min[X] >= INFINITY )  {
		bu_log("All primitives are halfspaces, setting minimum\n");
		VSETALL( rtip->mdl_min, -1 );
	}
	if( rtip->mdl_max[X] <= -INFINITY )  {
		bu_log("All primitives are halfspaces, setting maximum\n");
		VSETALL( rtip->mdl_max, 1 );
	}

	/*
	 *  Enlarge the model RPP just slightly, to avoid nasty
	 *  effects with a solid's face being exactly on the edge
	 */
	rtip->mdl_min[X] = floor( rtip->mdl_min[X] );
	rtip->mdl_min[Y] = floor( rtip->mdl_min[Y] );
	rtip->mdl_min[Z] = floor( rtip->mdl_min[Z] );
	rtip->mdl_max[X] = ceil( rtip->mdl_max[X] );
	rtip->mdl_max[Y] = ceil( rtip->mdl_max[Y] );
	rtip->mdl_max[Z] = ceil( rtip->mdl_max[Z] );

	/* Compute radius of a model bounding sphere */
	VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
	rtip->rti_radius = 0.5 * MAGNITUDE(diag);

	/*  If a resource structure has been provided for us, use it. */
	resp = (struct resource *)BU_PTBL_GET(&rtip->rti_resources, 0);
	if( !resp )  resp = &rt_uniresource;
	RT_CK_RESOURCE(resp);

	/*  Build array of region pointers indexed by reg_bit.
	 *  Optimize each region's expression tree.
	 *  Set this region's bit in the bit vector of every solid
	 *  contained in the subtree.
	 */
	rtip->Regions = (struct region **)bu_calloc(
		rtip->nregions, sizeof(struct region *),
		"rtip->Regions[]" );
	if(RT_G_DEBUG&DEBUG_REGIONS)  bu_log("rt_prep_parallel(%s,%d) about to optimize regions\n",
			rtip->rti_dbip->dbi_filename,
			rtip->rti_dbip->dbi_uses);
	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		/* Ensure bit numbers are unique */
		BU_ASSERT_PTR(rtip->Regions[regp->reg_bit], ==, REGION_NULL);
		rtip->Regions[regp->reg_bit] = regp;
		rt_optim_tree( regp->reg_treetop, resp );
		rt_solid_bitfinder( regp->reg_treetop, regp, resp );
		if(RT_G_DEBUG&DEBUG_REGIONS)  {
			db_ck_tree( regp->reg_treetop);
			rt_pr_region( regp );
		}
	}
	if(RT_G_DEBUG&DEBUG_REGIONS)  {
		bu_log("rt_prep_parallel() printing primitives' region pointers\n");
		RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
			bu_log("solid %s ", stp->st_name);
			bu_pr_ptbl( "st_regions", &stp->st_regions, 1 );
		} RT_VISIT_ALL_SOLTABS_END
	}

	/*  Space for array of soltab pointers indexed by solid bit number.
	 *  Include enough extra space for an extra bitv_t's worth of bits,
	 *  to handle round-up.
	 */
	rtip->rti_Solids = (struct soltab **)bu_calloc(
		rtip->nsolids + (1<<BITV_SHIFT), sizeof(struct soltab *),
		"rtip->rti_Solids[]" );
	/*
	 *  Build array of solid table pointers indexed by solid ID.
	 *  Last element for each kind will be found in
	 *	rti_sol_by_type[id][rti_nsol_by_type[id]-1]
	 */
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		/* Ensure bit numbers are unique */
		register struct soltab **ssp = &rtip->rti_Solids[stp->st_bit];
		if( *ssp != SOLTAB_NULL )  {
			bu_log("rti_Solids[%d] is non-empty! rtip=x%x\n", stp->st_bit, rtip);
			bu_log("Existing entry is (st_rtip=x%x):\n", (*ssp)->st_rtip);
			rt_pr_soltab(*ssp);
			bu_log("2nd soltab also claiming that bit is (st_rtip=x%x):\n", stp->st_rtip);
			rt_pr_soltab(stp);
		}
		BU_ASSERT_PTR(*ssp, ==, SOLTAB_NULL);
		*ssp = stp;
		rtip->rti_nsol_by_type[stp->st_id]++;
	} RT_VISIT_ALL_SOLTABS_END

	/* Find solid type with maximum length (for rt_shootray) */
	rtip->rti_maxsol_by_type = 0;
	for( i=0; i <= ID_MAX_SOLID; i++ )  {
		if( rtip->rti_nsol_by_type[i] > rtip->rti_maxsol_by_type )
			rtip->rti_maxsol_by_type = rtip->rti_nsol_by_type[i];
	}
	/* Malloc the storage and zero the counts */
	for( i=0; i <= ID_MAX_SOLID; i++ )  {
		if( rtip->rti_nsol_by_type[i] <= 0 )  continue;
		rtip->rti_sol_by_type[i] = (struct soltab **)bu_calloc(
			rtip->rti_nsol_by_type[i],
			sizeof(struct soltab *),
			"rti_sol_by_type[]" );
		rtip->rti_nsol_by_type[i] = 0;
	}
	/* Fill in the array and rebuild the count (aka index) */
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		register int	id;
		id = stp->st_id;
		rtip->rti_sol_by_type[id][rtip->rti_nsol_by_type[id]++] = stp;
	} RT_VISIT_ALL_SOLTABS_END
	if( RT_G_DEBUG & (DEBUG_DB|DEBUG_SOLIDS) )  {
		bu_log("rt_prep_parallel(%s,%d) printing number of prims by type\n",
			rtip->rti_dbip->dbi_filename,
			rtip->rti_dbip->dbi_uses);
		for( i=1; i <= ID_MAX_SOLID; i++ )  {
			bu_log("%5d %s (%d)\n",
				rtip->rti_nsol_by_type[i],
				rt_functab[i].ft_name,
				i );
		}
	}

	/* If region-id expression file exists, process it */
	rt_regionfix(rtip);
	
	/* For plotting, compute a slight enlargement of the model RPP,
	 * to allow room for rays clipped to the model RPP to be depicted.
	 * Always do this, because application debugging may use it too.
	 */
	{
		register fastf_t f, diff;

		diff = (rtip->mdl_max[X] - rtip->mdl_min[X]);
		f = (rtip->mdl_max[Y] - rtip->mdl_min[Y]);
		if( f > diff )  diff = f;
		f = (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
		if( f > diff )  diff = f;
		diff *= 0.1;	/* 10% expansion of box */
		rtip->rti_pmin[0] = rtip->mdl_min[0] - diff;
		rtip->rti_pmin[1] = rtip->mdl_min[1] - diff;
		rtip->rti_pmin[2] = rtip->mdl_min[2] - diff;
		rtip->rti_pmax[0] = rtip->mdl_max[0] + diff;
		rtip->rti_pmax[1] = rtip->mdl_max[1] + diff;
		rtip->rti_pmax[2] = rtip->mdl_max[2] + diff;
	}

	/*
	 *	Partition space
	 *
	 *  Multiple CPUs can be used here.
	 */
	for( i=1; i<=CUT_MAXIMUM; i++ ) rtip->rti_ncut_by_type[i] = 0;
	rt_cut_it(rtip, ncpu);

	/* Release storage used for bounding RPPs of solid "pieces" */
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		if( stp->st_piece_rpps )  {
			bu_free( (char *)stp->st_piece_rpps, "st_piece_rpps[]" );
			stp->st_piece_rpps = NULL;
		}
	} RT_VISIT_ALL_SOLTABS_END

	/* Plot bounding RPPs */
	if( (RT_G_DEBUG&DEBUG_PLOTBOX) )  {
		FILE	*plotfp;

		if( (plotfp=fopen("rtrpp.plot", "w"))!=NULL) {
			/* Plot solid bounding boxes, in white */
			pl_color( plotfp, 255, 255, 255 );
			rt_plot_all_bboxes( plotfp, rtip );
			(void)fclose(plotfp);
		}
	}

	/* Plot solid outlines */
	if( (RT_G_DEBUG&DEBUG_PLOTSOLIDS) )  {
		FILE		*plotfp;

		if( (plotfp=fopen("rtsolids.pl", "w")) != NULL)  {
			rt_plot_all_solids( plotfp, rtip, resp );
			(void)fclose(plotfp);
		}
	}
	rtip->needprep = 0;		/* prep is done */
	bu_semaphore_release(RT_SEM_RESULTS);	/* end critical section */

	if(RT_G_DEBUG&DEBUG_REGIONS)  bu_log("rt_prep_parallel(%s,%d,ncpu=%d) FINISH\n",
			rtip->rti_dbip->dbi_filename,
			rtip->rti_dbip->dbi_uses, ncpu);
}

/*
 *			R T _ P R E P
 *
 *  Compatability stub.  Only uses 1 CPU.
 */
void
rt_prep(rtip)
register struct rt_i *rtip;
{
	RT_CK_RTI(rtip);
	rt_prep_parallel(rtip, 1);
}

/*
 *			R T _ P L O T _ A L L _ B B O X E S
 *
 *  Plot the bounding boxes of all the active solids.
 *  Color may be set in advance by the caller.
 */
void
rt_plot_all_bboxes( fp, rtip )
FILE		*fp;
struct rt_i	*rtip;
{
	register struct soltab	*stp;

	RT_CK_RTI(rtip);
	pdv_3space( fp, rtip->rti_pmin, rtip->rti_pmax );
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		/* Ignore "dead" solids in the list.  (They failed prep) */
		if( stp->st_aradius <= 0 )  continue;
		/* Don't draw infinite solids */
		if( stp->st_aradius >= INFINITY )
			continue;
		pdv_3box( fp, stp->st_min, stp->st_max );
	} RT_VISIT_ALL_SOLTABS_END
}

/*
 *			R T _ P L O T _ A L L _ S O L I D S
 */
void
rt_plot_all_solids(
	FILE		*fp,
	struct rt_i	*rtip,
	struct resource	*resp)
{
	register struct soltab	*stp;

	RT_CK_RTI(rtip);

	pdv_3space( fp, rtip->rti_pmin, rtip->rti_pmax );

	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		/* Ignore "dead" solids in the list.  (They failed prep) */
		if( stp->st_aradius <= 0 )  continue;

		/* Don't draw infinite solids */
		if( stp->st_aradius >= INFINITY )
			continue;

		(void)rt_plot_solid( fp, rtip, stp, resp );
	} RT_VISIT_ALL_SOLTABS_END
}

/*
 *			R T _ V L I S T _ S O L I D
 *
 *  "Draw" a solid with the same kind of wireframes that MGED would display,
 *  appending vectors to an already initialized vlist head.
 *
 *  Returns -
 *	<0	failure
 *	 0	OK
 */
int
rt_vlist_solid(
	struct bu_list		*vhead,
	struct rt_i		*rtip,
	const struct soltab	*stp,
	struct resource		*resp)
{
	struct rt_db_internal		intern;

	if( rt_db_get_internal( &intern, stp->st_dp, rtip->rti_dbip, stp->st_matp, resp ) < 0 )  {
		bu_log("rt_vlist_solid(%s): rt_db_get_internal() failed\n",
			stp->st_name);
		return(-1);			/* FAIL */
	}
	RT_CK_DB_INTERNAL( &intern );

	if( rt_functab[intern.idb_type].ft_plot(
		vhead,
		&intern,
		&rtip->rti_ttol,
		&rtip->rti_tol
	    ) < 0 )  {
		bu_log("rt_vlist_solid(%s): ft_plot() failure\n",
			stp->st_name);
		rt_db_free_internal( &intern, resp );
	    	return(-2);
	}
	rt_db_free_internal( &intern, resp );
	return 0;
}

/*
 *			R T _ P L O T _ S O L I D
 *
 *  Plot a solid with the same kind of wireframes that MGED would display,
 *  in UNIX-plot form, on the indicated file descriptor.
 *  The caller is responsible for calling pdv_3space().
 *
 *  Returns -
 *	<0	failure
 *	 0	OK
 */
int
rt_plot_solid(
	register FILE		*fp,
	struct rt_i		*rtip,
	const struct soltab	*stp,
	struct resource		*resp)
{
	struct bu_list			vhead;
	struct region			*regp;

	RT_CK_RTI(rtip);
	RT_CK_SOLTAB(stp);

	BU_LIST_INIT( &vhead );

	if( rt_vlist_solid( &vhead, rtip, stp, resp ) < 0 )  {
		bu_log("rt_plot_solid(%s): rt_vlist_solid() failed\n",
			stp->st_name);
		return(-1);			/* FAIL */
	}

	if( BU_LIST_IS_EMPTY( &vhead ) )  {
		bu_log("rt_plot_solid(%s): no vectors to plot?\n",
			stp->st_name);
		return(-3);		/* FAIL */
	}

	/* Take color from one region */
	if( (regp = (struct region *)BU_PTBL_GET(&stp->st_regions,0)) != REGION_NULL )  {
		pl_color( fp,
			(int)(255*regp->reg_mater.ma_color[0]),
			(int)(255*regp->reg_mater.ma_color[1]),
			(int)(255*regp->reg_mater.ma_color[2]) );
	}

	rt_vlist_to_uplot( fp, &vhead );

	RT_FREE_VLIST( &vhead );
	return(0);			/* OK */
}

/*			R T _ I N I T _ R E S O U R C E
 *
 *  initialize memory resources.
 *	This routine should initialize all the same resources
 *	that rt_clean_resource() releases.
 *	It shouldn't allocate any dynamic memory, just init pointers & lists.
 *
 *  if( BU_LIST_UNINITIALIZED( &resp->re_parthead ) )
 *  indicates that this initialization is needed.
 *
 *  Note that this routine is also called as part of rt_clean_resource().
 *
 *  Special case, resp == rt_uniresource, rtip may be NULL (but give it if you have it).
 */
void
rt_init_resource(
	struct resource *resp,
	int		cpu_num,
	struct rt_i	*rtip)
{

	if( resp == &rt_uniresource )  {
		cpu_num = MAX_PSW;		/* array is [MAX_PSW+1] just for this */
		if(rtip)  RT_CK_RTI(rtip);	/* check it if provided */
	} else {
		BU_ASSERT_PTR( resp, !=, NULL );
		BU_ASSERT_LONG( cpu_num, >=, 0 );
		BU_ASSERT_LONG( cpu_num, <, MAX_PSW );
		RT_CK_RTI(rtip);		/* mandatory */
	}

	resp->re_magic = RESOURCE_MAGIC;
	resp->re_cpu = cpu_num;

	/* XXX resp->re_randptr is an "application" (rt) level field. For now. */

	if( BU_LIST_UNINITIALIZED( &resp->re_seg ) )
		BU_LIST_INIT( &resp->re_seg )

	if( BU_LIST_UNINITIALIZED( &resp->re_seg_blocks.l ) )
		bu_ptbl_init( &resp->re_seg_blocks, 64, "re_seg_blocks ptbl" );

	if( BU_LIST_UNINITIALIZED( &resp->re_directory_blocks.l ) )
		bu_ptbl_init( &resp->re_directory_blocks, 64, "re_directory_blocks ptbl" );

	if( BU_LIST_UNINITIALIZED( &resp->re_parthead ) )
		BU_LIST_INIT( &resp->re_parthead )

	if( BU_LIST_UNINITIALIZED( &resp->re_solid_bitv ) )
		BU_LIST_INIT( &resp->re_solid_bitv )

	if( BU_LIST_UNINITIALIZED( &resp->re_region_ptbl ) )
		BU_LIST_INIT( &resp->re_region_ptbl )

	if( BU_LIST_UNINITIALIZED( &resp->re_nmgfree ) )
		BU_LIST_INIT( &resp->re_nmgfree )

	resp->re_boolstack = NULL;
	resp->re_boolslen = 0;

	if( rtip == NULL )  return;	/* only in rt_uniresource case */

	/* Ensure that this CPU's resource structure is registered in rt_i */
	/* It may already be there when we're called from rt_clean_resource */
	{
		struct resource	*ores = (struct resource *)
			BU_PTBL_GET(&rtip->rti_resources, cpu_num);
		if( ores != NULL && ores != resp )  {
			bu_log("rt_init_resource(cpu=%d) re-registering resource, had x%x, new=x%x\n",
				cpu_num,
				ores,
				resp );
			bu_bomb("rt_init_resource() re-registration\n");
		}
		BU_PTBL_SET(&rtip->rti_resources, cpu_num, resp);
	}
}

/*
 *			R T _ C L E A N _ R E S O U R C E
 *
 *  Deallocate the per-cpu "private" memory resources.
 *	segment freelist
 *	hitmiss freelist for NMG raytracer
 *	partition freelist
 *	solid_bitv freelist
 *	region_ptbl freelist
 *	re_boolstack
 *
 *  Some care is required, as rt_uniresource may not be fully initialized
 *  before it gets freed.
 *
 *  Note that the resource struct's storage is not freed (it may be static
 *  or otherwise allocated by a LIBRT application) but any dynamic
 *  memory pointed to by it is freed.
 *
 *  One exception to this is that the re_directory_hd and re_directory_blocks
 *  are not touched, because the "directory" structures (which are really
 *  part of the db_i) continue to be in use.
 */
void
rt_clean_resource( rtip, resp )
struct rt_i	*rtip;
struct resource	*resp;
{
	RT_CK_RTI(rtip);
	RT_CK_RESOURCE(resp);

	/*  The 'struct seg' guys are malloc()ed in blocks, not individually,
	 *  so they're kept track of two different ways.
	 */
	BU_LIST_INIT( &resp->re_seg );	/* abandon the list of individuals */
	if( BU_LIST_IS_INITIALIZED( &resp->re_seg_blocks.l ) )  {
		struct seg **spp;
		BU_CK_PTBL( &resp->re_seg_blocks );
		for( BU_PTBL_FOR( spp, (struct seg **), &resp->re_seg_blocks ) )  {
			RT_CK_SEG(*spp);	/* Head of block will be a valid seg */
			bu_free( (genptr_t)(*spp), "struct seg block" );
		}
		bu_ptbl_free( &resp->re_seg_blocks );
	}

	/*
	 *  The 'struct directory' guys are malloc()ed in big blocks,
	 *  but CAN'T BE FREED HERE!  We are not done with the db_i yet.
	 */

	/* The "struct hitmiss' guys are individually malloc()ed */
	if( BU_LIST_IS_INITIALIZED( &resp->re_nmgfree ) )  {
		struct hitmiss *hitp;
		while( BU_LIST_WHILE( hitp, hitmiss, &resp->re_nmgfree ) )  {
			NMG_CK_HITMISS(hitp);
			BU_LIST_DEQUEUE( (struct bu_list *)hitp );
			bu_free( (genptr_t)hitp, "struct hitmiss" );
		}
	}

	/* The 'struct partition' guys are individually malloc()ed */
	if( BU_LIST_IS_INITIALIZED( &resp->re_parthead ) )  {
		struct partition *pp;
		while( BU_LIST_WHILE( pp, partition, &resp->re_parthead ) )  {
			RT_CK_PT(pp);
			BU_LIST_DEQUEUE( (struct bu_list *)pp );
			bu_ptbl_free( &pp->pt_seglist );
			bu_free( (genptr_t)pp, "struct partition" );
		}
	}

	/* The 'struct bu_bitv' guys on re_solid_bitv are individually malloc()ed */
	if( BU_LIST_IS_INITIALIZED( &resp->re_solid_bitv ) )  {
		struct bu_bitv	*bvp;
		while( BU_LIST_WHILE( bvp, bu_bitv, &resp->re_solid_bitv ) )  {
			BU_CK_BITV( bvp );
			BU_LIST_DEQUEUE( &bvp->l );
			bvp->nbits = 0;		/* sanity */
			bu_free( (genptr_t)bvp, "struct bu_bitv" );
		}
	}

	/* The 'struct bu_ptbl' guys on re_region_ptbl are individually malloc()ed */
	if( BU_LIST_IS_INITIALIZED( &resp->re_region_ptbl ) )  {
		struct bu_ptbl	*tabp;
		while( BU_LIST_WHILE( tabp, bu_ptbl, &resp->re_region_ptbl ) )  {
			BU_CK_PTBL(tabp);
			BU_LIST_DEQUEUE( &tabp->l );
			bu_ptbl_free( tabp );
			bu_free( (genptr_t)tabp, "struct bu_ptbl" );
		}
	}

	/* 're_boolstack' is a simple pointer */
	if( resp->re_boolstack )  {
		bu_free( (genptr_t)resp->re_boolstack, "boolstack" );
		resp->re_boolstack = NULL;
		resp->re_boolslen = 0;
	}

/* Release the state variables for 'solid pieces' */
	rt_res_pieces_clean( resp, rtip );

	/* Reinitialize pointers, to be tidy.  No storage is allocated. */
	rt_init_resource( resp, resp->re_cpu, rtip );
}

/*
 *			R T _ C L E A N
 *
 *  Release all the dynamic storage associated with a particular rt_i
 *  structure, except for the database instance information (dir, etc)
 *  and the rti_resources ptbl.
 *
 *  Note that an animation script can invoke a "clean" operation before
 *  anything has been prepped.
 */
void
rt_clean( rtip )
register struct rt_i *rtip;
{
	register struct region *regp;
	register struct bu_list	*head;
	register struct soltab *stp;
	int	i;

	RT_CK_RTI(rtip);

	/* DEBUG: Ensure that all region trees are valid */
	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		RT_CK_REGION(regp);
		db_ck_tree( regp->reg_treetop );
	}
	/*  
	 *  Clear out the region table
	 *  db_free_tree() will delete most soltab structures.
	 */
	while( BU_LIST_WHILE( regp, region, &rtip->HeadRegion ) )  {
		RT_CK_REGION(regp);
		BU_LIST_DEQUEUE(&(regp->l));
		db_free_tree( regp->reg_treetop, &rt_uniresource );
		bu_free( (genptr_t)regp->reg_name, "region name str");
		regp->reg_name = (char *)0;
		if( regp->reg_mater.ma_shader )
		{
			bu_free( (genptr_t)regp->reg_mater.ma_shader, "ma_shader" );
			regp->reg_mater.ma_shader = (char *)NULL;
		}
		if( regp->attr_values ) {
			i = 0;
			while( regp->attr_values[i] ) {
				bu_mro_free( regp->attr_values[i] );
				i++;
			}
			bu_free( (char *)regp->attr_values, "regp->attr_values" );
		}
		bu_free( (genptr_t)regp, "struct region");
	}
	rtip->nregions = 0;

	/*
	 *  Clear out the solid table, AFTER doing the region table.
	 *  Can't use RT_VISIT_ALL_SOLTABS_START here
	 */
	head = &(rtip->rti_solidheads[0]);
	for( ; head < &(rtip->rti_solidheads[RT_DBNHASH]); head++ )  {
		while( BU_LIST_WHILE( stp, soltab, head ) )  {
			RT_CHECK_SOLTAB(stp);
			rt_free_soltab(stp);
		}
	}
	rtip->nsolids = 0;

	/* Clean out the array of pointers to regions, if any */
	if( rtip->Regions )  {
		bu_free( (char *)rtip->Regions, "rtip->Regions[]" );
		rtip->Regions = (struct region **)0;

		/* Free space partitions */
		rt_fr_cut( rtip, &(rtip->rti_CutHead) );
		bzero( (char *)&(rtip->rti_CutHead), sizeof(union cutter) );
		rt_fr_cut( rtip, &(rtip->rti_inf_box) );
		bzero( (char *)&(rtip->rti_inf_box), sizeof(union cutter) );
	}
	rt_cut_clean(rtip);

	/* Free animation structures */
/* XXX modify to only free those from this rtip */
	db_free_anim(rtip->rti_dbip);

	/* Free array of solid table pointers indexed by solid ID */
	for( i=0; i <= ID_MAX_SOLID; i++ )  {
		if( rtip->rti_nsol_by_type[i] <= 0 )  continue;
		bu_free( (char *)rtip->rti_sol_by_type[i], "sol_by_type" );
		rtip->rti_sol_by_type[i] = (struct soltab **)0;
	}
	if( rtip->rti_Solids )  {
		bu_free( (char *)rtip->rti_Solids, "rtip->rti_Solids[]" );
		rtip->rti_Solids = (struct soltab **)0;
	}

	/*
	 *  Clean out every cpu's "struct resource".
	 *  These are provided by the caller's application (or are
	 *  defaulted to rt_uniresource) and can't themselves be freed.
	 *  rt_shootray() saved a table of them for us to use here.
	 *  rt_uniresource may or may not be in this table.
 	 */
	if( BU_LIST_MAGIC_OK(&rtip->rti_resources.l, BU_PTBL_MAGIC ) )  {
		struct resource	**rpp;
		BU_CK_PTBL( &rtip->rti_resources );
		for( BU_PTBL_FOR( rpp, (struct resource **), &rtip->rti_resources ) )  {
			/* After using a submodel, some entries may be NULL
			 * while others are not NULL
			 */
			if( *rpp == NULL )  continue;
			RT_CK_RESOURCE(*rpp);
			/* Clean but do not free the resource struct */
			rt_clean_resource(rtip, *rpp);
#if 0
/* XXX Can't do this, or 'clean' command in RT animation script will dump core. */
/* rt expects them to stay inited and remembered, forever. */
/* Submodels will clean up after themselves */
			/* Forget remembered ptr, but keep ptbl allocated */
			*rpp = NULL;
#endif
		}
	}
	if( rt_uniresource.re_magic )  {
		rt_clean_resource(rtip, &rt_uniresource );/* Used for rt_optim_tree() */
	}

	/*
	 *  Re-initialize everything important.
	 *  This duplicates the code in rt_new_rti().
	 */

	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;
	VMOVE( rtip->rti_inf_box.bn.bn_min, rtip->mdl_min );
	VMOVE( rtip->rti_inf_box.bn.bn_max, rtip->mdl_max );
	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );

	bu_hist_free( &rtip->rti_hist_cellsize );
	bu_hist_free( &rtip->rti_hist_cutdepth );

	/*
	 *  Zero the solid instancing counters in dbip database instance.
	 *  Done here because the same dbip could be used by multiple
	 *  rti's, and rt_gettrees() can be called multiple times on
	 *  this one rtip.
	 *  There is a race (collision!) here on d_uses if rt_gettrees()
	 *  is called on another rtip of the same dbip
	 *  before this rtip is done
	 *  with all it's treewalking.
	 *
	 *  This must be done for each 'clean' to keep
	 *  rt_find_identical_solid() working properly as d_uses goes up.
	 */
	for( i=0; i < RT_DBNHASH; i++ )  {
		register struct directory	*dp;

		dp = rtip->rti_dbip->dbi_Head[i];
		for( ; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_uses = 0;
	}

	rtip->rti_magic = RTI_MAGIC;
	rtip->needprep = 1;
}

/*
 *			R T _ D E L _ R E G T R E E
 *
 *  Remove a region from the linked list.  Used to remove a particular
 *  region from the active database, presumably after some useful
 *  information has been extracted (eg, a light being converted to
 *  implicit type), or for special effects.
 *
 *  Returns -
 *	 0	success
 */
int
rt_del_regtree( struct rt_i *rtip, register struct region *delregp, struct resource *resp )
{
	RT_CK_RESOURCE(resp);

	if( RT_G_DEBUG & DEBUG_REGIONS )
		bu_log("rt_del_regtree(%s): region deleted\n", delregp->reg_name);

	BU_LIST_DEQUEUE(&(delregp->l));

	db_free_tree( delregp->reg_treetop, resp );
	delregp->reg_treetop = TREE_NULL;
	bu_free( (char *)delregp->reg_name, "region name str");
	delregp->reg_name = (char *)0;
	if( delregp->attr_values ) {
		int i=0;
		while( delregp->attr_values[i] ) {
			bu_mro_free( delregp->attr_values[i] );
			i++;
		}
		bu_free( (char *)delregp->attr_values, "delregp->attr_values" );
	}
	bu_free( (char *)delregp, "struct region");
	return(0);
}

/*
 *  			S O L I D _ B I T F I N D E R
 *  
 *  Used to walk the boolean tree, setting bits for all the solids in the tree
 *  to the provided bit vector.  Should be called AFTER the region bits
 *  have been assigned.
 */
HIDDEN void
rt_solid_bitfinder( treep, regp, resp )
register union tree	*treep;
struct region		*regp;
struct resource		*resp;
{
	register union tree	**sp;
	register struct soltab	*stp;
	register union tree	**stackend;

	RT_CK_REGION(regp);
	RT_CK_RESOURCE(resp);

	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);

	*sp++ = TREE_NULL;
	*sp++ = treep;
	while( (treep = *--sp) != TREE_NULL ) {
		RT_CK_TREE(treep);
		switch( treep->tr_op )  {
		case OP_NOP:
			break;
		case OP_SOLID:
			stp = treep->tr_a.tu_stp;
			RT_CK_SOLTAB(stp);
			bu_ptbl_ins( &stp->st_regions, (long *)regp );
			break;
		case OP_UNION:
		case OP_INTERSECT:
		case OP_SUBTRACT:
			/* BINARY type */
			/* push both nodes - search left first */
			*sp++ = treep->tr_b.tb_right;
			*sp++ = treep->tr_b.tb_left;
			if( sp >= stackend )  {
				register int off = sp - resp->re_boolstack;
				rt_grow_boolstack( resp );
				sp = &(resp->re_boolstack[off]);
				stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
			}
			break;
		default:
			bu_log("rt_solid_bitfinder:  op=x%x\n", treep->tr_op);
			break;
		}
	}
}

/*
 *			R T _ C K
 *
 *  Check as many of the in-memory data structures as is practical.
 *  Useful for detecting memory corruption, and inappropriate sharing
 *  between different LIBRT instances.
 */
void
rt_ck( rtip )
register struct rt_i	*rtip;
{
	struct region		*regp;
	struct soltab		*stp;

	RT_CK_RTI(rtip);
	RT_CK_DBI(rtip->rti_dbip);

	db_ck_directory(rtip->rti_dbip);

	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		RT_CK_SOLTAB(stp);
	} RT_VISIT_ALL_SOLTABS_END

	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		RT_CK_REGION(regp);
		db_ck_tree(regp->reg_treetop);
	}
	/* rti_CutHead */

}

/*
 *		R T _ L O A D _ A T T R S
 *
 *	Loads a new set of attribute values (corresponding to the provided list of attribute
 *	names) for each region structure in the provided rtip.
 *
 *	RETURNS
 *		The number of region structures affected
 */

int rt_load_attrs( struct rt_i *rtip, char **attrs )
{
	struct region *regp;
	struct bu_attribute_value_set avs;
	struct directory *dp;
	const char *reg_name;
	const char *attr;
	int attr_count=0;
	int mro_count;
	int did_set;
	int region_count=0;
	int i;

	RT_CHECK_RTI(rtip);
	RT_CK_DBI(rtip->rti_dbip);

	if( rtip->rti_dbip->dbi_version < 5 )
		return 0;

	while( attrs[attr_count] )
		attr_count++;

	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		RT_CK_REGION(regp);

		did_set = 0;
		mro_count = 0;
		while( regp->attr_values[mro_count] )
			mro_count++;

		if( mro_count < attr_count ) {
			/* don't have enough to store all the values */
			for( i=0 ; i<mro_count ; i++ ) {
				bu_mro_free( regp->attr_values[i] );
				bu_free( (char *)regp->attr_values[i], "regp->attr_values[i]" );
			}
			if( mro_count )
				bu_free( (char *)regp->attr_values, "regp->attr_values" );
			regp->attr_values = (struct bu_mro **)bu_calloc( attr_count + 1,
						 sizeof( struct bu_mro *), "regp->attr_values" );
			for( i=0 ; i<attr_count ; i++ ) {
				regp->attr_values[i] = bu_malloc( sizeof( struct bu_mro ),
							"regpp->attr_values[i]" );
				bu_mro_init( regp->attr_values[i] );
			}
		} else if ( mro_count > attr_count ) {
			/* just reuse what we have */
			for( i=attr_count ; i<mro_count ; i++ ) {
				bu_mro_free( regp->attr_values[i] );
				bu_free( (char *)regp->attr_values[i], "regp->attr_values[i]" );
			}
			regp->attr_values[attr_count] = (struct bu_mro *)NULL;
		}

		if( (reg_name=strrchr( regp->reg_name, '/' ) ) == NULL )
			reg_name = regp->reg_name;
		else
			reg_name++;

		if( (dp=db_lookup( rtip->rti_dbip, reg_name, LOOKUP_NOISY ) ) == DIR_NULL )
			continue;

		if( db5_get_attributes( rtip->rti_dbip, &avs, dp ) ) {
			bu_log( "rt_load_attrs: Failed to get attributes for region %s\n", reg_name );
			continue;
		}

		for( i=0 ; i<attr_count ; i++ ) {
			if( (attr = bu_avs_get( &avs, attrs[i] ) ) == NULL )
				continue;

			bu_mro_set( regp->attr_values[i], attr );
			did_set = 1;
		}

		if( did_set )
			region_count++;

		bu_avs_free( &avs );
	}

	return( region_count );
}
