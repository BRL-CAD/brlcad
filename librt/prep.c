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
static char RCSprep[] = "@(#)$Header$ (BRL)";
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
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

HIDDEN void	rt_solid_bitfinder();

extern struct resource	rt_uniresource;		/* from shoot.c */

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
	vect_t			diag;

	RT_CK_RTI(rtip);

	RES_ACQUIRE(&rt_g.res_results);	/* start critical section */
	if(!rtip->needprep)  {
		rt_log("WARNING: rt_prep() invoked a second time, ignored");
		RES_RELEASE(&rt_g.res_results);
		return;
	}

	/* This table is used for discovering the per-cpu resource structures */
	bu_ptbl_init( &rtip->rti_resources, MAX_PSW, "rti_resources ptbl" );

	rt_uniresource.re_magic = RESOURCE_MAGIC;

	if( rtip->nsolids <= 0 )  {
		if( rtip->rti_air_discards > 0 )
			rt_log("rt_prep: %d solids discarded due to air regions\n", rtip->rti_air_discards );
		rt_bomb("rt_prep:  no solids left to prep");
	}

	/* In case everything is a halfspace, set a minimum space */
	if( rtip->mdl_min[X] >= INFINITY )  {
		rt_log("All solids are halspaces, setting minimum\n");
		VSETALL( rtip->mdl_min, -1 );
	}
	if( rtip->mdl_max[X] <= -INFINITY )  {
		rt_log("All solids are halspaces, setting maximum\n");
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

	/*  Build array of region pointers indexed by reg_bit.
	 *  Optimize each region's expression tree.
	 *  Set this region's bit in the bit vector of every solid
	 *  contained in the subtree.
	 */
	rtip->Regions = (struct region **)rt_malloc(
		rtip->nregions * sizeof(struct region *),
		"rtip->Regions[]" );
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		rtip->Regions[regp->reg_bit] = regp;
		rt_optim_tree( regp->reg_treetop, &rt_uniresource );
		rt_solid_bitfinder( regp->reg_treetop, regp,
			&rt_uniresource );
		if(rt_g.debug&DEBUG_REGIONS)  {
			rt_pr_region( regp );
		}
	}
	if(rt_g.debug&DEBUG_REGIONS)  {
		RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
			rt_log("solid %s ", stp->st_name);
			bu_pr_ptbl( "st_regions", &stp->st_regions, 1 );
		} RT_VISIT_ALL_SOLTABS_END
	}

	/*  Space for array of soltab pointers indexed by solid bit number.
	 *  Include enough extra space for an extra bitv_t's worth of bits,
	 *  to handle round-up.
	 */
	rtip->rti_Solids = (struct soltab **)rt_calloc(
		rtip->nsolids + (1<<BITV_SHIFT), sizeof(struct soltab *),
		"rtip->rti_Solids[]" );
	/*
	 *  Build array of solid table pointers indexed by solid ID.
	 *  Last element for each kind will be found in
	 *	rti_sol_by_type[id][rti_nsol_by_type[id]-1]
	 */
	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		rtip->rti_Solids[stp->st_bit] = stp;
		rtip->rti_nsol_by_type[stp->st_id]++;
	} RT_VISIT_ALL_SOLTABS_END

	/* Find solid type with maximum length (for rt_shootray) */
	rtip->rti_maxsol_by_type = 0;
	for( i=0; i <= ID_MAXIMUM; i++ )  {
		if( rtip->rti_nsol_by_type[i] > rtip->rti_maxsol_by_type )
			rtip->rti_maxsol_by_type = rtip->rti_nsol_by_type[i];
	}
	/* Malloc the storage and zero the counts */
	for( i=0; i <= ID_MAXIMUM; i++ )  {
		if( rtip->rti_nsol_by_type[i] <= 0 )  continue;
		rtip->rti_sol_by_type[i] = (struct soltab **)rt_calloc(
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
	if( rt_g.debug & DEBUG_DB )  {
		for( i=1; i <= ID_MAXIMUM; i++ )  {
			rt_log("%5d %s (%d)\n",
				rtip->rti_nsol_by_type[i],
				rt_functab[i].ft_name,
				i );
		}
	}

	/* If region-id expression file exists, process it */
	rt_regionfix(rtip);

	/* Partition space */
	/* This is the only part which uses multiple CPUs */
	rt_cut_it(rtip, ncpu);

	/* Plot bounding RPPs */
	if( (rt_g.debug&DEBUG_PLOTBOX) )  {
		FILE	*plotfp;

		if( (plotfp=fopen("rtrpp.plot", "w"))!=NULL) {
			/* Plot solid bounding boxes, in white */
			pl_color( plotfp, 255, 255, 255 );
			rt_plot_all_bboxes( plotfp, rtip );
			(void)fclose(plotfp);
		}
	}

	/* Plot solid outlines */
	if( (rt_g.debug&DEBUG_PLOTBOX) )  {
		FILE		*plotfp;

		if( (plotfp=fopen("rtsolids.pl", "w")) != NULL)  {
			rt_plot_all_solids( plotfp, rtip );
			(void)fclose(plotfp);
		}
	}
	rtip->needprep = 0;		/* prep is done */
	RES_RELEASE(&rt_g.res_results);	/* end critical section */
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
rt_plot_all_solids( fp, rtip )
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

		(void)rt_plot_solid( fp, rtip, stp );
	} RT_VISIT_ALL_SOLTABS_END
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
rt_plot_solid( fp, rtip, stp )
register FILE		*fp;
struct rt_i		*rtip;
struct soltab		*stp;
{
	struct rt_list			vhead;
	struct region			*regp;
	struct rt_external		ext;
	struct rt_db_internal		intern;
	int				id = stp->st_id;
	int				rnum;
	struct rt_tess_tol		ttol;
	struct rt_tol			tol;
	matp_t				mat;

	RT_LIST_INIT( &vhead );

	if( db_get_external( &ext, stp->st_dp, rtip->rti_dbip ) < 0 )  {
		rt_log("rt_plot_solid(%s): db_get_external() failure\n",
			stp->st_name);
		return(-1);			/* FAIL */
	}

	if( !(mat = stp->st_matp) )
		mat = (matp_t)rt_identity;
    	RT_INIT_DB_INTERNAL(&intern);
	if( rt_functab[id].ft_import( &intern, &ext, mat ) < 0 )  {
		rt_log("rt_plot_solid(%s):  solid import failure\n",
			stp->st_name );
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		db_free_external( &ext );
		return(-1);			/* FAIL */
	}
	RT_CK_DB_INTERNAL( &intern );

	ttol.magic = RT_TESS_TOL_MAGIC;
	ttol.abs = 0.0;
	ttol.rel = 0.01;
	ttol.norm = 0;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	if( rt_functab[id].ft_plot(
		&vhead,
		&intern,
		&ttol,
		&tol
	    ) < 0 )  {
		rt_log("rt_plot_solid(%s): ft_plot() failure\n",
			stp->st_name);
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		db_free_external( &ext );
	    	return(-2);
	}
	rt_functab[id].ft_ifree( &intern );
	db_free_external( &ext );

	/* Take color from one region */
	if( (regp = (struct region *)BU_PTBL_GET(&stp->st_regions,0)) != REGION_NULL )  {
		pl_color( fp,
			(int)(255*regp->reg_mater.ma_color[0]),
			(int)(255*regp->reg_mater.ma_color[1]),
			(int)(255*regp->reg_mater.ma_color[2]) );
	}

	if( RT_LIST_IS_EMPTY( &vhead ) )  {
		rt_log("rt_plot_solid(%s): no vectors to plot?\n",
			stp->st_name);
		return(-3);		/* FAIL */
	}

	rt_vlist_to_uplot( fp, &vhead );
	RT_FREE_VLIST( &vhead );
	return(0);			/* OK */
}

/*
 *			R T _ F R E E _ R E S O U R C E
 *
 *  Deallocate the per-cpu "private" memory resources.
 *	segment freelist
 *	partition freelist
 *	solid_bitv freelist
 *	region_ptbl freelist
 *	re_boolstack
 *
 *  Some care is required, as rt_uniresource may not be fully initialized
 *  before it gets freed.
 */
void
rt_free_resource( rtip, resp )
struct rt_i	*rtip;
struct resource	*resp;
{
	RT_CK_RTI(rtip);
	RT_CK_RESOURCE(resp);

	/* The 'struct seg' guys are malloc()ed in blocks, not individually */
	RT_LIST_INIT( &resp->re_seg );	/* abandon the list of individuals */
	if( !RT_LIST_UNINITIALIZED( &resp->re_solid_bitv ) )  {
		struct seg **spp;
		BU_CK_PTBL( &resp->re_seg_blocks );
		for( BU_PTBL_FOR( spp, (struct seg **), &resp->re_seg_blocks ) )  {
			RT_CK_SEG(*spp);	/* Head of block will be a valid seg */
			bu_free( (genptr_t)(*spp), "struct seg" );
		}
		bu_ptbl_free( &resp->re_seg_blocks );
	}

	/* The 'struct partition' guys are individually malloc()ed */
	if( !RT_LIST_UNINITIALIZED( &resp->re_parthead ) )  {
		struct partition *pp;
		while( RT_LIST_WHILE( pp, partition, &resp->re_parthead ) )  {
			RT_CK_PT(pp);
			RT_LIST_DEQUEUE( (struct rt_list *)pp );
			bu_ptbl_free( &pp->pt_solids_hit );
			bu_free( (genptr_t)pp, "struct partition" );
		}
	}

	/* The 'struct bu_bitv' guys on re_solid_bitv are individually malloc()ed */
	if( !RT_LIST_UNINITIALIZED( &resp->re_solid_bitv ) )  {
		struct bu_bitv	*bvp;
		while( RT_LIST_WHILE( bvp, bu_bitv, &resp->re_solid_bitv ) )  {
			BU_CK_BITV( bvp );
			RT_LIST_DEQUEUE( &bvp->l );
			bu_free( (genptr_t)bvp, "struct bu_bitv" );
		}
	}

	/* The 'struct bu_ptbl' guys on re_region_ptbl are individually malloc()ed */
	if( !RT_LIST_UNINITIALIZED( &resp->re_region_ptbl ) )  {
		struct bu_ptbl	*tabp;
		while( RT_LIST_WHILE( tabp, bu_ptbl, &resp->re_region_ptbl ) )  {
			BU_CK_PTBL(tabp);
			RT_LIST_DEQUEUE( &tabp->l );
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
}

/*
 *			R T _ C L E A N
 *
 *  Release all the dynamic storage associated with a particular rt_i
 *  structure, except for the database instance information (dir, etc).
 *
 *  Note that an animation script can invoke a "clean" operation before
 *  anything has been prepped.
 */
void
rt_clean( rtip )
register struct rt_i *rtip;
{
	register struct region *regp;
	register struct rt_list	*head;
	register struct soltab *stp;
	int	i;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_clean:  bad rtip\n");

	/* DEBUG: Ensure that all region trees are valid */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		RT_CK_REGION(regp);
		db_ck_tree( regp->reg_treetop );
	}
	/*  
	 *  Clear out the region table
	 *  db_free_tree() will delete most soltab structures.
	 */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; )  {
		register struct region *nextregp = regp->reg_forw;

		RT_CK_REGION(regp);
		db_free_tree( regp->reg_treetop );
		rt_free( (char *)regp->reg_name, "region name str");
		regp->reg_name = (char *)0;
		rt_free( (char *)regp, "struct region");
		regp = nextregp;
	}
	rtip->HeadRegion = REGION_NULL;
	rtip->nregions = 0;

	/*
	 *  Clear out the solid table, AFTER doing the region table.
	 */
	head = &(rtip->rti_solidheads[0]);
	for( ; head < &(rtip->rti_solidheads[RT_DBNHASH]); head++ )  {
		while( RT_LIST_WHILE( stp, soltab, head ) )  {
			RT_CHECK_SOLTAB(stp);
			rt_free_soltab(stp);
		}
	}
	rtip->nsolids = 0;

	/* Clean out the array of pointers to regions, if any */
	if( rtip->Regions )  {
		rt_free( (char *)rtip->Regions, "rtip->Regions[]" );
		rtip->Regions = (struct region **)0;

		/* Free space partitions */
		rt_fr_cut( rtip, &(rtip->rti_CutHead) );
		bzero( (char *)&(rtip->rti_CutHead), sizeof(union cutter) );
		rt_fr_cut( rtip, &(rtip->rti_inf_box) );
		bzero( (char *)&(rtip->rti_inf_box), sizeof(union cutter) );
	}
	rt_cut_clean(rtip);

	/* Reset instancing counters in database directory */
	for( i=0; i < RT_DBNHASH; i++ )  {
		register struct directory	*dp;

		dp = rtip->rti_dbip->dbi_Head[i];
		for( ; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_uses = 0;
	}

	/* Free animation structures */
	db_free_anim(rtip->rti_dbip);

	/* Free array of solid table pointers indexed by solid ID */
	for( i=0; i <= ID_MAXIMUM; i++ )  {
		if( rtip->rti_nsol_by_type[i] <= 0 )  continue;
		rt_free( (char *)rtip->rti_sol_by_type[i], "sol_by_type" );
		rtip->rti_sol_by_type[i] = (struct soltab **)0;
	}
	if( rtip->rti_Solids )  {
		rt_free( (char *)rtip->rti_Solids, "rtip->rti_Solids[]" );
		rtip->rti_Solids = (struct soltab **)0;
	}

	/*
	 *  Clean out every cpu's "struct resource".
	 *  These are provided by the caller's application (or are
	 *  defaulted to rt_uniresource) and can't themselves be freed.
	 *  rt_shootray() saved a table of them for us to use here.
 	 */
	if( !BU_LIST_UNINITIALIZED( &rtip->rti_resources.l ) )  {
		struct resource	**rpp;
		BU_CK_PTBL( &rtip->rti_resources );
		for( BU_PTBL_FOR( rpp, (struct resource **), &rtip->rti_resources ) )  {
			RT_CK_RESOURCE(*rpp);
			rt_free_resource(rtip, *rpp);
		}
		bu_ptbl_free( &rtip->rti_resources );
	}
	if( rt_uniresource.re_magic )  {
		rt_free_resource(rtip, &rt_uniresource );/* Used for rt_optim_tree() */
	}

	/*
	 *  Re-initialize everything important.
	 *  This duplicates the code in rt_dirbuild().
	 */

	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;
	VMOVE( rtip->rti_inf_box.bn.bn_min, rtip->mdl_min );
	VMOVE( rtip->rti_inf_box.bn.bn_max, rtip->mdl_max );
	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );

	rt_hist_free( &rtip->rti_hist_cellsize );
	rt_hist_free( &rtip->rti_hist_cutdepth );

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
 *	-1	if unable to find indicated region
 *	 0	success
 */
int
rt_del_regtree( rtip, delregp )
struct rt_i *rtip;
register struct region *delregp;
{
	register struct region *regp;
	register struct region *nextregp;

	regp = rtip->HeadRegion;
	if( rt_g.debug & DEBUG_REGIONS )
		rt_log("Del Region %s\n", delregp->reg_name);

	if( regp == delregp )  {
		rtip->HeadRegion = regp->reg_forw;
		goto zot;
	}

	for( ; regp != REGION_NULL; regp=nextregp )  {
		nextregp=regp->reg_forw;
		if( nextregp == delregp )  {
			regp->reg_forw = nextregp->reg_forw;	/* unlink */
			goto zot;
		}
	}
	rt_log("rt_del_region:  unable to find %s\n", delregp->reg_name);
	return(-1);
zot:
	db_free_tree( delregp->reg_treetop );
	delregp->reg_treetop = TREE_NULL;
	rt_free( (char *)delregp->reg_name, "region name str");
	delregp->reg_name = (char *)0;
	rt_free( (char *)delregp, "struct region");
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
	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen-1]);
	*sp++ = TREE_NULL;
	*sp++ = treep;
	while( (treep = *--sp) != TREE_NULL ) {
		switch( treep->tr_op )  {
		case OP_NOP:
			break;
		case OP_SOLID:
			stp = treep->tr_a.tu_stp;
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
			rt_log("rt_solid_bitfinder:  op=x%x\n", treep->tr_op);
			break;
		}
	}
}
