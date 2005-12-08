/*                        V S H O O T . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup librt */
/*@{*/

/** @file vshoot.c
 *  EXPERIMENTAL vector version of the
 * Ray Tracing program shot coordinator.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCSshoot[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

struct resource rt_uniresource;		/* Resources for uniprocessor */

/*
 *			R T _ S H O O T R A Y
 *
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  (building the HeadSeg chain), and then call rt_boolregions()
 *  to build and evaluate the partition chain.
 *  If the ray actually hit anything, call the application's
 *  a_hit() routine with a pointer to the partition chain,
 *  otherwise, call the application's a_miss() routine.
 *
 *  It is important to note that rays extend infinitely only in the
 *  positive direction.  The ray is composed of all points P, where
 *
 *	P = r_pt + K * r_dir
 *
 *  for K ranging from 0 to +infinity.  There is no looking backwards.
 *
 *  It is also important to note that the direction vector r_dir
 *  must have unit length;  this is mandatory, and is not ordinarily checked,
 *  in the name of efficiency.
 *
 *  Input:  Pointer to an application structure, with these mandatory fields:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 *  Calls user's a_miss() or a_hit() routine as appropriate.
 *  Passes a_hit() routine list of partitions, with only hit_dist
 *  fields valid.  Normal computation deferred to user code,
 *  to avoid needless computation here.
 *
 *  Returns: whatever the application function returns (an int).
 *
 *  NOTE:  The appliction functions may call rt_shootray() recursively.
 *	Thus, none of the local variables may be static.
 *
 *  An open issue for execution in a PARALLEL environment is locking
 *  of the statistics variables.
 */
int
rt_shootray( struct application *ap )
{
	struct seg		*HeadSeg;
	int		ret;
	auto vect_t		inv_dir;	/* inverses of ap->a_ray.r_dir */
	union bitv_elem	*solidbits;	/* bits for all solids shot so far */
	bitv_t		*regionbits;	/* bits for all involved regions */
	char		*status;
	auto struct partition	InitialPart;	/* Head of Initial Partitions */
	auto struct partition	FinalPart;	/* Head of Final Partitions */
	int			nrays = 1;	/* for now */
	int			vlen;
	int			id;
	int			i;
	struct soltab		**ary_stp;	/* array of pointers */
	struct xray		**ary_rp;	/* array of pointers */
	struct seg		*ary_seg;	/* array of structures */
	struct rt_i		*rtip;

#define BACKING_DIST	(-2.0)		/* mm to look behind start point */
	rtip = ap->a_rt_i;
	RT_AP_CHECK(ap);
	if( ap->a_resource == RESOURCE_NULL )  {
		ap->a_resource = &rt_uniresource;
		rt_uniresource.re_magic = RESOURCE_MAGIC;
	}
	RT_RESOURCE_CHECK(ap->a_resource);

	if(RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
		rt_g.rtg_logindent += 2;
		bu_log("\n**********mshootray cpu=%d  %d,%d lvl=%d (%s)\n",
			ap->a_resource->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		VPRINT("Pnt", ap->a_ray.r_pt);
		VPRINT("Dir", ap->a_ray.r_dir);
	}

	rtip->rti_nrays++;
	if( rtip->needprep )
		rt_prep(rtip);

	/* Allocate dynamic memory */
	vlen = nrays * rtip->rti_maxsol_by_type;
	ary_stp = (struct soltab **)bu_calloc( vlen, sizeof(struct soltab *),
		"*ary_stp[]" );
	ary_rp = (struct xray **)bu_calloc( vlen, sizeof(struct xray *),
		"*ary_rp[]" );
	ary_seg = (struct seg *)bu_calloc( vlen, sizeof(struct seg),
		"ary_seg[]" );

	/**** for each ray, do this ****/

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;

	HeadSeg = SEG_NULL;

	GET_BITV( rtip, solidbits, ap->a_resource );	/* see rt_get_bitv() for details */
	bzero( (char *)solidbits, rtip->rti_bv_bytes );
	regionbits = &solidbits->be_v[
		2+RT_BITV_BITS2WORDS(ap->a_rt_i->nsolids)];

	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( ap->a_ray.r_dir[X], SQRT_SMALL_FASTF ) )  {
		inv_dir[X]=1.0/ap->a_ray.r_dir[X];
	} else {
		inv_dir[X] = INFINITY;
		ap->a_ray.r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Y], SQRT_SMALL_FASTF ) )  {
		inv_dir[Y]=1.0/ap->a_ray.r_dir[Y];
	} else {
		inv_dir[Y] = INFINITY;
		ap->a_ray.r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Z], SQRT_SMALL_FASTF ) )  {
		inv_dir[Z]=1.0/ap->a_ray.r_dir[Z];
	} else {
		inv_dir[Z] = INFINITY;
		ap->a_ray.r_dir[Z] = 0.0;
	}

	/*
	 *  XXX handle infinite solids here, later.
	 */

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  If ray ends exactly at the model RPP, trace it.
	 */
	if( !rt_in_rpp( &ap->a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max )  ||
	    ap->a_ray.r_max < 0.0 )  {
		rtip->nmiss_model++;
		ret = ap->a_miss( ap );
		status = "MISS model";
		goto out;
	}

	/* For each type of solid to be shot at, assemble the vectors */
	for( id = 1; id <= ID_MAX_SOLID; id++ )  {
		register int	nsol;

		if( (nsol = rtip->rti_nsol_by_type[id]) <= 0 )  continue;

		/* For each instance of this solid type */
		for( i = nsol-1; i >= 0; i-- )  {
			ary_stp[i] = rtip->rti_sol_by_type[id][i];
			ary_rp[i] = &(ap->a_ray);	/* XXX, sb [ray] */
			ary_seg[i].seg_stp = SOLTAB_NULL;
			ary_seg[i].seg_next = SEG_NULL;
		}
		/* bounding box check */
		/* bit vector per ray check */
		/* mark elements to be skipped with ary_stp[] = SOLTAB_NULL */
		ap->a_rt_i->nshots += nsol;	/* later: skipped ones */
		rt_functab[id].ft_vshot(
			ary_stp, ary_rp, ary_seg,
			nsol, ap->a_resource );

		/* set bits for all solids shot at for each ray */

		/* append resulting seg list to input for boolweave */
		for( i = nsol-1; i >= 0; i-- )  {
			register struct seg	*seg2;

			if( ary_seg[i].seg_stp == SOLTAB_NULL )  {
				/* MISS */
				ap->a_rt_i->nmiss++;
				continue;
			}
			ap->a_rt_i->nhits++;

			/* For now, do it the slow way.  sb [ray] */
			/* MUST dup it -- all segs have to live till after a_hit() */
			GET_SEG( seg2, ap->a_resource );
			*seg2 = ary_seg[i];	/* struct copy */
			rt_boolweave( seg2, &InitialPart, ap );

			/* Add seg chain to list of used segs awaiting reclaim */
			{
				register struct seg *seg3 = seg2;
				while( seg3->seg_next != SEG_NULL )
					seg3 = seg3->seg_next;
				seg3->seg_next = HeadSeg;
				HeadSeg = seg2;
			}
		}

		/* OR in regionbits */
		for( i = nsol-1; i >= 0; i-- )  {
			register int words;
			register bitv_t *in = ary_stp[i]->st_regions;
			register bitv_t *out = regionbits;	/* XXX sb [ray] */

			words = RT_BITV_BITS2WORDS(ary_stp[i]->st_maxreg);
#			include "noalias.h"
			for( --words; words >= 0; words-- )
				regionbits[words] |= in[words];
		}
	}

	/*
	 *  Ray has finally left known space.
	 */
	if( InitialPart.pt_forw == &InitialPart )  {
		ret = ap->a_miss( ap );
		status = "MISSed all primitives";
		goto freeup;
	}

	/*
	 *  All intersections of the ray with the model have
	 *  been computed.  Evaluate the boolean trees over each partition.
	 */
	rt_boolfinal( &InitialPart, &FinalPart, BACKING_DIST,
		INFINITY,
		regionbits, ap);

	if( FinalPart.pt_forw == &FinalPart )  {
		ret = ap->a_miss( ap );
		status = "MISS bool";
		goto freeup;
	}

	/*
	 *  Ray/model intersections exist.  Pass the list to the
	 *  user's a_hit() routine.  Note that only the hit_dist
	 *  elements of pt_inhit and pt_outhit have been computed yet.
	 *  To compute both hit_point and hit_normal, use the
	 *
	 *  	RT_HIT_NORM( hitp, stp, rayp )
	 *
	 *  macro.  To compute just hit_point, use
	 *
	 *  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	 */
hitit:
	if(RT_G_DEBUG&DEBUG_SHOOT)  rt_pr_partitions(rtip,&FinalPart,"a_hit()");

	ret = ap->a_hit( ap, &FinalPart );
	status = "HIT";

	/*
	 * Processing of this ray is complete.  Free dynamic resources.
	 */
freeup:
	{
		register struct partition *pp;

		/* Free up initial partition list */
		for( pp = InitialPart.pt_forw; pp != &InitialPart;  )  {
			register struct partition *newpp;
			newpp = pp;
			pp = pp->pt_forw;
			FREE_PT(newpp, ap->a_resource);
		}
		/* Free up final partition list */
		for( pp = FinalPart.pt_forw; pp != &FinalPart;  )  {
			register struct partition *newpp;
			newpp = pp;
			pp = pp->pt_forw;
			FREE_PT(newpp, ap->a_resource);
		}
	}
	/* Segs can't be freed until after a_hit() has returned */
	{
		register struct seg *segp;

		while( HeadSeg != SEG_NULL )  {
			segp = HeadSeg->seg_next;
			FREE_SEG( HeadSeg, ap->a_resource );
			HeadSeg = segp;
		}
	}

out:
	bu_free( (char *)ary_stp, "*ary_stp[]" );
	bu_free( (char *)ary_rp, "*ary_rp[]" );
	bu_free( (char *)ary_seg, "ary_seg[]" );

	if( solidbits != BITV_NULL)  {
		FREE_BITV( solidbits, ap->a_resource );
	}
	if(RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION))  {
		if( rt_g.rtg_logindent > 0 )
			rt_g.rtg_logindent -= 2;
		else
			rt_g.rtg_logindent = 0;
		bu_log("----------mshootray cpu=%d  %d,%d lvl=%d (%s) %s ret=%d\n",
			ap->a_resource->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
			status, ret);
	}
	return( ret );
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;

/* Stub function which will "similate" a call to a vector shot routine */
/*void*/
rt_vstub(struct soltab *stp[],	/* An array of solid pointers */
	 struct xray *rp[],	/* An array of ray pointers */
	 struct seg segp[],	/* array of segs (results returned) */
	 int n, 		/* Number of ray/object pairs */
	 struct resource *resp)
{
	register int    i;
	register struct seg *tmp_seg;

	/* go through each ray/solid pair and call a scalar function */
	for (i = 0; i < n; i++) {
		if (stp[i] != 0){ /* skip call if solid table pointer is NULL */
			/* do scalar call */
			tmp_seg = rt_functab[stp[i]->st_id].ft_shot(
				stp[i], rp[i], resp);

			/* place results in segp array */
			if ( tmp_seg == 0) {
				SEG_MISS(segp[i]);
			}
			else {
				segp[i] = *tmp_seg; /* structure copy */
				FREE_SEG(tmp_seg, resp);
			}
		}
	}
}

/*
 *			R T _ I N _ R P P
 *
 *  Compute the intersections of a ray with a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes
 *
 *  The algorithm here was developed by Gary Kuehl for GIFT.
 *  A good description of the approach used can be found in
 *  "??" by XYZZY and Barsky,
 *  ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note -
 *  The computation of entry and exit distance is mandatory, as the final
 *  test catches the majority of misses.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  invdir is the inverses of rp->r_dir[]
 *
 *  Implicit return -
 *	rp->r_min = dist from start of ray to point at which ray ENTERS solid
 *	rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
rt_in_rpp( struct xray *rp, fastf_t *invdir, fastf_t *min, fastf_t *max )
{
	register fastf_t *pt = &rp->r_pt[0];
	FAST fastf_t sv;
#define st sv			/* reuse the register */

	/* Start with infinite ray, and trim it down */
	rp->r_min = -INFINITY;
	rp->r_max = INFINITY;

	/* X axis */
	if( rp->r_dir[X] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[X] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		/*
		 *  Direction cosines along this axis is NEAR 0,
		 *  which implies that the ray is perpendicular to the axis,
		 *  so merely check position against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* Y axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Y] < 0.0 )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Y] > 0.0 )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Z] < 0.0 )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Z] > 0.0 )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* If equal, RPP is actually a plane */
	if( rp->r_min > rp->r_max )
		return(0);	/* MISS */
	return(1);		/* HIT */
}

/*
 *			R T _ B I T V _ O R
 */
void
rt_bitv_or( bitv_t *out, bitv_t *in, int nbits )
{
	register int words;

	words = RT_BITV_BITS2WORDS(nbits);
#ifdef VECTORIZE
#	include "noalias.h"
	for( --words; words >= 0; words-- )
		out[words] |= in[words];
#else
	while( words-- > 0 )
		*out++ |= *in++;
#endif
}

/*
 *  			R T _ G E T _ B I T V
 *
 *  This routine is called by the GET_BITV macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the bitv resource must already be locked.
 *  malloc() locking is done in bu_malloc.
 *
 *  Also note that there is a bit of trickery going on here:
 *  the *real* size of be_v[] array is determined at runtime, here.
 */
void
rt_get_bitv(struct rt_i *rtip, struct resource *res)
{
	register char *cp;
	register int bytes;
	register int size;		/* size of structure to really get */

	size = rtip->rti_bv_bytes;
	size = (size+sizeof(long)-1) & ~(sizeof(long)-1);
	bytes = bu_malloc_len_roundup(16*size);
	if( (cp = bu_malloc(bytes, "rt_get_bitv")) == (char *)0 )  {
		bu_log("rt_get_bitv: malloc failure\n");
		exit(17);
	}
	while( bytes >= size )  {
		((union bitv_elem *)cp)->be_next = res->re_bitv;
		res->re_bitv = (union bitv_elem *)cp;
		res->re_bitvlen++;
		cp += size;
		bytes -= size;
	}
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
