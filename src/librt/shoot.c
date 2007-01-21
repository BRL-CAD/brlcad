/*                         S H O O T . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2007 United States Government as represented by
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
/** @addtogroup ray */
/** @{ */
/** @file shoot.c
 *
 *	Ray Tracing program shot coordinator.
 *
 *  This is the heart of LIBRT's ray-tracing capability.
 *
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  (building the finished_segs chain), and then call rt_boolregions()
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
 *  Authors -
 *	Michael John Muuss
 *	Glenn Durfee
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSshoot[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "plot3.h"
#include "./debug.h"

struct resource rt_uniresource;		/* Resources for uniprocessor */

extern void	rt_plot_cell(const union cutter *cutp, const struct rt_shootray_status *ssp, struct bu_list *waiting_segs_hd, struct rt_i *rtip);		/* at end of file */

#define V3PT_DEPARTING_RPP(_step, _lo, _hi, _pt ) \
		PT_DEPARTING_RPP(_step, _lo, _hi, (_pt)[X], (_pt)[Y], (_pt)[Z] )
#define PT_DEPARTING_RPP(_step, _lo, _hi, _px, _py, _pz ) \
		(   ((_step)[X] <= 0 && (_px) < (_lo)[X]) || \
		    ((_step)[X] >= 0 && (_px) > (_hi)[X]) || \
		    ((_step)[Y] <= 0 && (_py) < (_lo)[Y]) || \
		    ((_step)[Y] >= 0 && (_py) > (_hi)[Y]) || \
		    ((_step)[Z] <= 0 && (_pz) < (_lo)[Z]) || \
		    ((_step)[Z] >= 0 && (_pz) > (_hi)[Z])   )

/*
 *			R T _ R E S _ P I E C E S _ I N I T
 *
 *  Allocate the per-processor state variables needed
 *  to support rt_shootray()'s use of 'solid pieces'.
 */
void
rt_res_pieces_init(struct resource *resp, struct rt_i *rtip)
{
	struct rt_piecestate	*psptab;
	struct rt_piecestate	*psp;
	struct soltab		*stp;

	RT_CK_RESOURCE(resp);
	RT_CK_RTI(rtip);

	psptab = bu_calloc( sizeof(struct rt_piecestate),
		rtip->rti_nsolids_with_pieces, "re_pieces[]" );
	resp->re_pieces = psptab;

	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
		RT_CK_SOLTAB(stp);
		if( stp->st_npieces <= 1 )  continue;
		psp = &psptab[stp->st_piecestate_num];
		psp->magic = RT_PIECESTATE_MAGIC;
		psp->stp = stp;
		psp->shot = bu_bitv_new(stp->st_npieces);
		rt_htbl_init( &psp->htab, 8, "psp->htab" );
		psp->cutp = CUTTER_NULL;
	} RT_VISIT_ALL_SOLTABS_END
}

/*
 *			R T _ R E S _ P I E C E S _ C L E A N
 */
void
rt_res_pieces_clean(struct resource *resp, struct rt_i *rtip)
{
	struct rt_piecestate	*psp;
	int			i;

	RT_CK_RESOURCE(resp);
	RT_CK_RTI(rtip);

	if( !resp->re_pieces ) {
		rtip->rti_nsolids_with_pieces = 0;
		return;
	}
	for( i = rtip->rti_nsolids_with_pieces-1; i >= 0; i-- )  {
		psp = &resp->re_pieces[i];
		RT_CK_PIECESTATE(psp);
		rt_htbl_free(&psp->htab);
		bu_bitv_free(psp->shot);
		psp->shot = NULL;	/* sanity */
		psp->magic = 0;
	}
	bu_free( (char *)resp->re_pieces, "re_pieces[]" );
	resp->re_pieces = NULL;

	bu_ptbl_free( &resp->re_pieces_pending );

	rtip->rti_nsolids_with_pieces = 0;
}

/*
 *			R T _ F I N D _ N U G R I D
 *
 *  Along the given axis, find which NUgrid cell this value lies in.
 *  Use method of binary subdivision.
 */
int
rt_find_nugrid(const struct nugridnode *nugnp, int axis, fastf_t val)
{
	int	min;
	int	max;
	int	lim = nugnp->nu_cells_per_axis[axis]-1;
	int	cur;

	if( val < nugnp->nu_axis[axis][0].nu_spos ||
	    val > nugnp->nu_axis[axis][lim].nu_epos )
		return -1;

	min = 0;
	max = lim;
again:
	cur = (min + max) / 2;

	if( cur <= 0 )  return 0;
	if( cur >= lim )  return lim;

	if( val < nugnp->nu_axis[axis][cur].nu_spos )  {
		max = cur;
		goto again;
	}
	if( val > nugnp->nu_axis[axis][cur+1].nu_epos )  {
		min = cur+1;
		goto again;
	}
	if( val < nugnp->nu_axis[axis][cur].nu_epos )
		return cur;
	else
		return cur+1;
}


/*
 *			R T _ A D V A N C E _ T O _ N E X T _ C E L L
 */

const union cutter *
rt_advance_to_next_cell(register struct rt_shootray_status *ssp)
{
	register const union cutter		*cutp, *curcut = ssp->curcut;
	register const struct application	*ap = ssp->ap;
	register fastf_t			t0, px, py, pz;
	int					push_flag = 0;
	double					fraction;
	int					exponent;

	ssp->box_num++;

	if( curcut == &ssp->ap->a_rt_i->rti_inf_box )  {
		/* Last pass did the infinite solids, there is nothing more */
		ssp->curcut = CUTTER_NULL;
		return CUTTER_NULL;
	}

#if EXTRA_SAFETY
	if( curcut == CUTTER_NULL ) {
		bu_log(
		   "rt_advance_to_next_cell: warning: ssp->curcut not set\n" );
		ssp->curcut = curcut = &ap->a_rt_i->rti_CutHead;
	}
#endif

	for( ;; ) {
		/* Set cutp to CUTTER_NULL.  If it fails to become set in the
		   following switch statement, we know that we have exited the
		   subnode.  If this subnode is the highest-level node, then
		   we are done advancing the ray through the model. */
		cutp = CUTTER_NULL;
		push_flag = 0;

		/*
		 *  The point corresponding to the box_start distance
		 *  may not be in the "right" place,
		 *  due to the effects of floating point fuzz:
		 *  1)  The point might lie just outside
		 *	the model RPP, resulting in the point not
		 *	falling within the RPP of the indicated cell,
		 *	or
		 *  2)	The poing might lie just a little bit on the
		 *	wrong side of the cell wall, resulting in
		 *	the ray getting "stuck", and needing rescuing
		 *	all the time by the error recovery code below.
		 *  Therefore, "nudge" the point just slightly into the
		 *  next cell by adding OFFSET_DIST.
		 *  XXX At present, a cell is never less than 1mm wide.
		 *  XXX The value of OFFSET_DIST should be some
		 *	percentage of the cell's smallest dimension,
		 *	rather than an absolute distance in mm.
		 *	This will prevent doing microscopic models.
		 */
		t0 = ssp->box_start;
		/* NB: can't compute px,py,pz here since t0 may advance
		   in the following statement! */

top:		switch( curcut->cut_type ) {
		case CUT_NUGRIDNODE: {
			/*
			 ****************************************************************************************
			 *
			 *  This portion implements Gigante's non-uniform 3-D space grid/mesh discretization.
			 *
			 ****************************************************************************************
			 */
			register int out_axis;
			register const struct nu_axis  **nu_axis =
			     (const struct nu_axis **)&curcut->nugn.nu_axis[0];
			register const int		*nu_stepsize =
			     &curcut->nugn.nu_stepsize[0];
			register const int	        *nu_cells_per_axis =
			     &curcut->nugn.nu_cells_per_axis[0];
			register const union cutter	*nu_grid =
			     curcut->nugn.nu_grid;

			if( ssp->lastcell == CUTTER_NULL ) {
				/* We have just started into this NUgrid.  We
				   must find our location and set up the
				   NUgrid traversal state variables. */
				register int x, y, z;

				px = ap->a_ray.r_pt[X] + t0*ap->a_ray.r_dir[X];
				py = ap->a_ray.r_pt[Y] + t0*ap->a_ray.r_dir[Y];
				pz = ap->a_ray.r_pt[Z] + t0*ap->a_ray.r_dir[Z];

				/* Must find cell that contains newray.r_pt.
				   We do this by binary subdivision.
				   If any are out of bounds, we have left the
				   NUgrid and will pop a level off the stack
				   in the outer loop (if applicable).  */
				x = rt_find_nugrid( &curcut->nugn, X, px );
				if( x<0 ) break;
				y = rt_find_nugrid( &curcut->nugn, Y, py );
				if( y<0 ) break;
				z = rt_find_nugrid( &curcut->nugn, Z, pz );
				if( z<0 ) break;

				cutp = &nu_grid[z*nu_stepsize[Z] +
					        y*nu_stepsize[Y] +
					        x*nu_stepsize[X]];

				ssp->igrid[X] = x;
				ssp->igrid[Y] = y;
				ssp->igrid[Z] = z;

				NUGRID_T_SETUP( X, px, x );
				NUGRID_T_SETUP( Y, py, y );
				NUGRID_T_SETUP( Z, pz, z );
			} else {
				/* Advance from previous cell to next cell */
				/* Take next step, finding ray entry distance*/
				cutp = ssp->lastcell;
				out_axis = ssp->out_axis;

				/* We may be simply advancing to the next box
				   in the *same* NUgrid cell (if, for instance,
				   the NUgrid cell is a cutnode with
				   boxnode leaves).  So if t0 hasn't advanced
				   past the end of the box, advance
				   a tiny bit (less than rt_ct_optim makes
				   boxnodes) and be handled by tree-traversing
				   code below. */

				if( cutp->cut_type == CUT_CUTNODE &&
				    t0 + OFFSET_DIST < ssp->tv[out_axis] ) {
					t0 += OFFSET_DIST;
					break;
				}

				/* Advance to the next cell as appropriate,
				   bailing out with cutp=CUTTER_NULL
				   if we run past the end of the NUgrid
				   array. */

again:				t0 = ssp->tv[out_axis];
				if( ssp->rstep[out_axis] > 0 ) {
					if( ++(ssp->igrid[out_axis]) >=
					    nu_cells_per_axis[out_axis] ) {
						cutp = CUTTER_NULL;
						break;
					}
					cutp += nu_stepsize[out_axis];
				} else {
					if( --(ssp->igrid[out_axis]) < 0 ) {
						cutp = CUTTER_NULL;
						break;
					}
					cutp -= nu_stepsize[out_axis];
				}

				/* Update t advancement value for this axis */
				NUGRID_T_ADV( out_axis, ssp->igrid[out_axis] );
			}

			/* find minimum exit t value */
			if( ssp->tv[X] < ssp->tv[Y] )  {
				if( ssp->tv[Z] < ssp->tv[X] )  {
					out_axis = Z;
				} else {
					out_axis = X;
				}
			} else {
				if( ssp->tv[Z] < ssp->tv[Y] )  {
					out_axis = Z;
				} else {
					out_axis = Y;
				}
			}

			/* Zip through empty cells. */
			if( cutp->cut_type == CUT_BOXNODE &&
			    cutp->bn.bn_len <= 0 &&
			    cutp->bn.bn_piecelen <= 0 ) {
				++ssp->resp->re_nempty_cells;
				goto again;
			}

			ssp->out_axis = out_axis;
			ssp->lastcell = cutp;

			if( RT_G_DEBUG&DEBUG_ADVANCE )
				bu_log( "t0=%g found in cell (%d,%d,%d), out_axis=%c at %g; cell is %s\n",
					t0,
					ssp->igrid[X], ssp->igrid[Y],
					ssp->igrid[Z],
					"XYZ*"[ssp->out_axis],
					ssp->tv[out_axis],
					cutp->cut_type==CUT_CUTNODE?"cut":
					cutp->cut_type==CUT_BOXNODE?"box":
					cutp->cut_type==CUT_NUGRIDNODE?"nu":
					"?" );
			break; }
		case CUT_CUTNODE:
			/* fall through */
		case CUT_BOXNODE:
			/*
			 ****************************************************************************************
			 *
			 *  This portion implements Muuss' non-uniform binary space partitioning tree.
			 *
			 ****************************************************************************************
			 */
			t0 += OFFSET_DIST;
			cutp = curcut;
			break;
		default:
			rt_bomb(
		       "rt_advance_to_next_cell: unknown high-level cutnode" );
		}

		if( cutp==CUTTER_NULL ) {
pop_space_stack:
			/*
			 *  Pop the stack of nested space partitioning methods.
			 *  Move up out of the current node, or return if there
			 *  is nothing left to do.
			 */
			{
				register struct rt_shootray_status *old = ssp->old_status;

				if( old == NULL ) goto escaped_from_model;
				*ssp = *old;		/* struct copy */
				bu_free( old, "old rt_shootray_status" );
				curcut = ssp->curcut;
				cutp = CUTTER_NULL;
				continue;
			}
		}

		/* Compute position and bail if we're outside of the current level. */
		px = ap->a_ray.r_pt[X] + t0*ap->a_ray.r_dir[X];
		py = ap->a_ray.r_pt[Y] + t0*ap->a_ray.r_dir[Y];
		pz = ap->a_ray.r_pt[Z] + t0*ap->a_ray.r_dir[Z];

		/* Optimization: when it's a boxnode in a nugrid, just return. */
		if( cutp->cut_type == CUT_BOXNODE &&
		    curcut->cut_type == CUT_NUGRIDNODE ) {
			ssp->newray.r_pt[X] = px;
			ssp->newray.r_pt[Y] = py;
			ssp->newray.r_pt[Z] = pz;
			ssp->newray.r_min = 0;
			ssp->newray.r_max = ssp->tv[ssp->out_axis] - t0;
			goto done_return_cutp;
		}

		/*  Given direction of travel, see if point is outside bound.
		 *  This will be the model RPP for NUBSP.
		 */
		if( PT_DEPARTING_RPP( ssp->rstep, ssp->curmin, ssp->curmax, px, py, pz ) )
			goto pop_space_stack;

		if( RT_G_DEBUG&DEBUG_ADVANCE ) {
			bu_log(
	           "rt_advance_to_next_cell() dist_corr=%g, pt=(%g, %g, %g)\n",
				t0 /*ssp->dist_corr*/, px, py, pz );
		}

		while( cutp->cut_type == CUT_CUTNODE ) {
			switch( cutp->cn.cn_axis )  {
			case X:
				if( !(px < cutp->cn.cn_point) )  {
					cutp=cutp->cn.cn_r;
				}  else  {
					cutp=cutp->cn.cn_l;
				}
				break;
			case Y:
				if( !(py < cutp->cn.cn_point) )  {
					cutp=cutp->cn.cn_r;
				}  else  {
					cutp=cutp->cn.cn_l;
				}
				break;
			case Z:
				if( !(pz < cutp->cn.cn_point) )  {
					cutp=cutp->cn.cn_r;
				}  else  {
					cutp=cutp->cn.cn_l;
				}
				break;
			}
		}

		if( cutp == CUTTER_NULL )
			rt_bomb( "rt_advance_to_next_cell: leaf is NULL!" );

		switch( cutp->cut_type ) {
		case CUT_BOXNODE:
			if( RT_G_DEBUG&DEBUG_ADVANCE &&
			    PT_DEPARTING_RPP( ssp->rstep, ssp->curmin, ssp->curmax, px, py, pz )
			) {
				/* This cell is old news. */
				bu_log(
	  "rt_advance_to_next_cell(): point not in cell, advancing\n   pt (%.20e,%.20e,%.20e)\n",
					px, py, pz );
				bu_log(	"  min (%.20e,%.20e,%.20e)\n",
					V3ARGS(cutp->bn.bn_min) );
				bu_log( "  max (%.20e,%.20e,%.20e)\n",
					V3ARGS(cutp->bn.bn_max) );
				bu_log( "pt=(%g,%g,%g)\n",px, py, pz );
				rt_pr_cut( cutp, 0 );

				/*
				 * Move newray point further into new box.
				 * Try again.
				 */
				t0 += OFFSET_DIST;
				goto top;
			}
			/* Don't get stuck within the same box for long */
			if( cutp==ssp->lastcut ) {
				fastf_t	delta;
push_to_next_box:				;
				if( RT_G_DEBUG & DEBUG_ADVANCE ) {
					bu_log(
						"%d,%d box push odist_corr=%.20e n=%.20e model_end=%.20e\n",
						ap->a_x, ap->a_y,
						ssp->odist_corr,
						t0 /*ssp->dist_corr*/,
						ssp->model_end );
					bu_log(
						"box_start o=%.20e n=%.20e\nbox_end   o=%.20e n=%.20e\n",
						ssp->obox_start,
						ssp->box_start,
						ssp->obox_end,
						ssp->box_end );
					bu_log( "Point=(%g,%g,%g)\n",
						px, py, pz );
					VPRINT( "Dir",
						ssp->newray.r_dir );
					rt_pr_cut( cutp, 0 );
				}

				/* Advance 1mm, or smallest value that hardware
				 * floating point resolution will allow.
				 */
				fraction = frexp( ssp->box_end,
						  &exponent );

				if( RT_G_DEBUG & DEBUG_ADVANCE ) {
					bu_log(
						"exp=%d, fraction=%.20e\n",
						exponent, fraction );
				}
				if( sizeof(fastf_t) <= 4 )
					fraction += 1.0e-5;
				else
					fraction += 1.0e-14;
				delta = ldexp( fraction, exponent );
				if( RT_G_DEBUG & DEBUG_ADVANCE ) {
					bu_log(
						"ldexp: delta=%g, fract=%g, exp=%d\n",
						delta,
						fraction,
						exponent );
				}

				/* Never advance less than 1mm */
				if( delta < 1 ) delta = 1.0;
				ssp->box_start = ssp->box_end + delta;
				ssp->box_end = ssp->box_start + delta;

				if( RT_G_DEBUG & DEBUG_ADVANCE ) {
					bu_log(
						"push%d: was=%.20e, now=%.20e\n\n",
						push_flag,
						ssp->box_end,
						ssp->box_start );
				}
				push_flag++;
				if( push_flag > 3 ) {
					bu_log( "rt_advance_to_next_cell(): INTERNAL ERROR: infinite loop aborted, ray %d,%d truncated\n",
						ap->a_x, ap->a_y );
					goto pop_space_stack;
				}
				/* See if point marched outside model RPP */
				if( ssp->box_start > ssp->model_end )
					goto pop_space_stack;
				t0 = ssp->box_start + OFFSET_DIST;
				goto top;
			}
			if( push_flag ) {
				push_flag = 0;
				if( RT_G_DEBUG & DEBUG_ADVANCE ) {
					bu_log(
						"%d,%d Escaped %d. dist_corr=%g, box_start=%g, box_end=%g\n",
						ap->a_x, ap->a_y,
						push_flag,
						t0 /*ssp->dist_corr*/,
						ssp->box_start,
						ssp->box_end );
				}
			}
			if( RT_G_DEBUG & DEBUG_ADVANCE ) {
				bu_log(
					"rt_advance_to_next_cell()=x%x lastcut=x%x\n",
					cutp, ssp->lastcut);
			}

			ssp->newray.r_pt[X] = px;
			ssp->newray.r_pt[Y] = py;
			ssp->newray.r_pt[Z] = pz;
			if( !rt_in_rpp( &ssp->newray, ssp->inv_dir,
					cutp->bn.bn_min,
					cutp->bn.bn_max) )  {
				bu_log("rt_advance_to_next_cell():  MISSED BOX\nrmin,rmax(%.20e,%.20e) box(%.20e,%.20e)\n",
				       ssp->newray.r_min,
				       ssp->newray.r_max,
				       ssp->box_start, ssp->box_end );
				goto push_to_next_box;
			}

done_return_cutp:	ssp->lastcut = cutp;
#if EXTRA_SAFETY
			/* Diagnostic purposes only */
			ssp->odist_corr = ssp->dist_corr;
			ssp->obox_start = ssp->box_start;
			ssp->obox_end = ssp->box_end;
#endif
			ssp->dist_corr = t0;
			ssp->box_start = t0 + ssp->newray.r_min;
			ssp->box_end = t0 + ssp->newray.r_max;
			if( RT_G_DEBUG & DEBUG_ADVANCE )  {
				bu_log(
				"rt_advance_to_next_cell() box=(%g, %g)\n",
					ssp->box_start, ssp->box_end );
			}
			return cutp;
		case CUT_NUGRIDNODE: {
			struct rt_shootray_status *old;

			BU_GETSTRUCT( old, rt_shootray_status );
			*old = *ssp;	/* struct copy */

			/* Descend into node */
			ssp->old_status = old;
			ssp->lastcut = ssp->lastcell = CUTTER_NULL;
			curcut = ssp->curcut = cutp;
			VSET( ssp->curmin, cutp->nugn.nu_axis[X][0].nu_spos,
					   cutp->nugn.nu_axis[Y][0].nu_spos,
					   cutp->nugn.nu_axis[Z][0].nu_spos );
			VSET( ssp->curmax, ssp->curcut->nugn.nu_axis[X][ssp->curcut->nugn.nu_cells_per_axis[X]-1].nu_epos,
				 ssp->curcut->nugn.nu_axis[Y][ssp->curcut->nugn.nu_cells_per_axis[Y]-1].nu_epos,
				 ssp->curcut->nugn.nu_axis[Z][ssp->curcut->nugn.nu_cells_per_axis[Z]-1].nu_epos );
			break; }
		case CUT_CUTNODE:
			rt_bomb( "rt_advance_to_next_cell: impossible: cutnote as leaf!" );
			break;
		default:
			rt_bomb( "rt_advance_to_next_cell: unknown spt node" );
			break;
		}

		/* Continue with the current space partitioning algorithm. */
	}
	/* NOTREACHED */
	/*	bu_bomb("rt_advance_to_next_cell: escaped for(;;) loop: impossible!"); */

	/*
	 *  If ray has escaped from model RPP, and there are infinite solids
	 *  in the model, there is one more (special) BOXNODE for the
	 *  caller to process.
	 */
escaped_from_model:
	curcut = &ssp->ap->a_rt_i->rti_inf_box;
	if( curcut->bn.bn_len <= 0 && curcut->bn.bn_piecelen <= 0 )
		curcut = CUTTER_NULL;
	ssp->curcut = curcut;
	return curcut;
}

/*		R T _ F I N D _ B A C K I N G _ D I S T
 *
 *	This routine traces a ray from its start point to model exit through the space partitioning tree.
 *	The objective is to find all primitives that use "pieces" and also have a bounding box that
 *	extends behind the ray start point. The minimum (most negative) intersection with such a bounding
 *	box is returned. The "backbits" bit vector (provided by the caller) gets a bit set for every
 *	primitive that meets the above criteria.
 *	No primitive intersections are performed.
 *
 *	XXXX This routine does not work for NUGRID XXXX
 */
fastf_t
rt_find_backing_dist( struct rt_shootray_status *ss, struct bu_bitv *backbits ) {
	fastf_t min_backing_dist=BACKING_DIST;
	fastf_t cur_dist=0.0;
	point_t cur_pt;
	union cutter *cutp;
	struct bu_bitv *solidbits;
	struct xray ray;
	struct resource	*resp;
	struct rt_i *rtip;
	int i;

	resp = ss->ap->a_resource;
	rtip = ss->ap->a_rt_i;

	/* get a bit vector of our own to avoid duplicate bounding box intersection calculations */
	solidbits = get_solidbitv( rtip->nsolids, resp );
	bu_bitv_clear(solidbits);

	ray = ss->ap->a_ray;	/* struct copy, don't mess with the original */

	/* cur_dist keeps track of where we are along the ray */
	/* stop when cur_dist reaches far intersection of ray and model bounding box */
	while( cur_dist <= ss->ap->a_ray.r_max ) {
		/* calculate the current point along the ray */
		VJOIN1( cur_pt, ss->ap->a_ray.r_pt, cur_dist, ss->ap->a_ray.r_dir );

		/* descend into the space partitioning tree based on this point */
		cutp = &ss->ap->a_rt_i->rti_CutHead;
		while( cutp->cut_type == CUT_CUTNODE ) {
			if( cur_pt[cutp->cn.cn_axis] >= cutp->cn.cn_point )  {
				cutp=cutp->cn.cn_r;
			}  else  {
				cutp=cutp->cn.cn_l;
			}
		}

		/* we are now at the box node for the current point */
		/* check if the ray intersects this box */
		if( !rt_in_rpp( &ray, ss->inv_dir, cutp->bn.bn_min, cutp->bn.bn_max ) ) {
			/* ray does not intersect this cell
			 * one of two situations must exist:
			 *	1. ray starts outside model bounding box (no need for these calculations)
			 *	2. we have proceeded beyond end of model bounding box (we are done)
			 * in either case, we are finished
			 */
			goto done;
		} else {
			/* increment cur_dist into next cell for next execution of this loop */
			cur_dist = ray.r_max + OFFSET_DIST;
		}

		/* process this box node (look at all the pieces) */
		for( i=0 ; i<cutp->bn.bn_piecelen ; i++ ) {
			struct rt_piecelist *plp=&cutp->bn.bn_piecelist[i];

			if( BU_BITTEST( solidbits, plp->stp->st_bit ) == 0 ) {
				/* we haven't looked at this primitive before */
				if( rt_in_rpp( &ray, ss->inv_dir, plp->stp->st_min, plp->stp->st_max ) ) {
					/* ray intersects this primitive bounding box */

					if( ray.r_min < BACKING_DIST ) {
						if( ray.r_min < min_backing_dist ) {
							/* move our backing distance back to catch this one */
							min_backing_dist = ray.r_min;
						}

						/* add this one to our list of primitives to check */
						BU_BITSET( backbits, plp->stp->st_bit );
					}
				}
				/* set bit so we don't repeat this calculation */
				BU_BITSET( solidbits, plp->stp->st_bit );
			}
		}
	}
 done:
	/* put our bit vector on the resource list */
	BU_CK_BITV(solidbits);
	BU_LIST_APPEND( &resp->re_solid_bitv, &solidbits->l );

	/* return our minimum backing distance */
	return min_backing_dist;
}

/*
 *			R T _ S H O O T R A Y
 *
 *  Note that the direction vector r_dir
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
 *  Formal Return: whatever the application function returns (an int).
 *
 *  NOTE:  The appliction functions may call rt_shootray() recursively.
 *	Thus, none of the local variables may be static.
 *
 *  To prevent having to lock the statistics variables in a PARALLEL
 *  environment, all the statistics variables have been moved into
 *  the 'resource' structure, which is allocated per-CPU.
 */

int
rt_shootray(register struct application *ap)
{
	struct rt_shootray_status	ss;
	struct seg		new_segs;	/* from solid intersections */
	struct seg		waiting_segs;	/* awaiting rt_boolweave() */
	struct seg		finished_segs;	/* processed by rt_boolweave() */
	fastf_t			last_bool_start;
	struct bu_bitv		*solidbits;	/* bits for all solids shot so far */
	struct bu_bitv		*backbits=NULL;	/* bits for all solids using pieces that need to be intersected behind
						   the the ray start point */
	struct bu_ptbl		*regionbits;	/* table of all involved regions */
	char			*status;
	auto struct partition	InitialPart;	/* Head of Initial Partitions */
	auto struct partition	FinalPart;	/* Head of Final Partitions */
	struct soltab		**stpp;
	register const union cutter *cutp;
	struct resource		*resp;
	struct rt_i		*rtip;
	const int		debug_shoot = RT_G_DEBUG & DEBUG_SHOOT;
	fastf_t			pending_hit = 0; /* dist of closest odd hit pending */

	RT_AP_CHECK(ap);
	if( ap->a_magic )  {
		RT_CK_AP(ap);
	} else {
		ap->a_magic = RT_AP_MAGIC;
	}
	if( ap->a_ray.magic )  {
		RT_CK_RAY(&(ap->a_ray));
	} else {
		ap->a_ray.magic = RT_RAY_MAGIC;
	}
	if( ap->a_resource == RESOURCE_NULL )  {
		ap->a_resource = &rt_uniresource;
		if(RT_G_DEBUG)bu_log("rt_shootray:  defaulting a_resource to &rt_uniresource\n");
		if( rt_uniresource.re_magic == 0 )
			rt_init_resource( &rt_uniresource, 0, ap->a_rt_i );
	}
	ss.ap = ap;
	rtip = ap->a_rt_i;
	RT_CK_RTI( rtip );
	resp = ap->a_resource;
	RT_CK_RESOURCE(resp);
	ss.resp = resp;

	if(RT_G_DEBUG) {
	    /* only test extensively if something in run-time debug is enabled */
	    if (RT_G_DEBUG & (DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION|DEBUG_ALLHITS)) {
		bu_log_indent_delta(2);
		bu_log("\n**********shootray cpu=%d  %d,%d lvl=%d a_onehit=%d (%s)\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_onehit,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		bu_log("Pnt (%.20G, %.20G, %.20G)\nDir (%.20G, %.20G, %.20G)\n",
			V3ARGS(ap->a_ray.r_pt),
			V3ARGS(ap->a_ray.r_dir) );
	    }
	}
#ifndef NO_BADRAY_CHECKING
	if(RT_BADVEC(ap->a_ray.r_pt)||RT_BADVEC(ap->a_ray.r_dir))  {
		bu_log("\n**********shootray cpu=%d  %d,%d lvl=%d (%s)\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		VPRINT(" r_pt", ap->a_ray.r_pt);
		VPRINT("r_dir", ap->a_ray.r_dir);
		rt_bomb("rt_shootray() bad ray\n");
	}
#endif

	if( rtip->needprep )
		rt_prep_parallel(rtip, 1);	/* Stay on our CPU */

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	InitialPart.pt_magic = PT_HD_MAGIC;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;
	FinalPart.pt_magic = PT_HD_MAGIC;
	ap->a_Final_Part_hdp = &FinalPart;

	BU_LIST_INIT( &new_segs.l );
	BU_LIST_INIT( &waiting_segs.l );
	BU_LIST_INIT( &finished_segs.l );
	ap->a_finished_segs_hdp = &finished_segs;

	if( BU_LIST_UNINITIALIZED( &resp->re_parthead ) )  {
		/* XXX This shouldn't happen any more */
		bu_log("rt_shootray() resp=x%x uninitialized, fixing it\n", resp);
		/*
		 *  We've been handed a mostly un-initialized resource struct,
		 *  with only a magic number and a cpu number filled in.
		 *  Init it and add it to the table.
		 *  This is how application-provided resource structures
		 *  are remembered for later cleanup by the library.
		 */
		rt_init_resource( resp, resp->re_cpu, rtip );
	}
	/* Ensure that this CPU's resource structure is registered */
	if( resp != &rt_uniresource )
		BU_ASSERT_PTR( BU_PTBL_GET(&rtip->rti_resources, resp->re_cpu), !=, NULL );

	solidbits = get_solidbitv( rtip->nsolids, resp );
	bu_bitv_clear(solidbits);

	if( BU_LIST_IS_EMPTY( &resp->re_region_ptbl ) )  {
		BU_GETSTRUCT( regionbits, bu_ptbl );
		bu_ptbl_init( regionbits, 7, "rt_shootray() regionbits ptbl" );
	} else {
		regionbits = BU_LIST_FIRST( bu_ptbl, &resp->re_region_ptbl );
		BU_LIST_DEQUEUE( &regionbits->l );
		BU_CK_PTBL(regionbits);
	}

	if( !resp->re_pieces && rtip->rti_nsolids_with_pieces > 0 )  {
		/* Initialize this processors 'solid pieces' state */
		rt_res_pieces_init( resp, rtip );
	}
	if( BU_LIST_MAGIC_WRONG( &resp->re_pieces_pending.l, BU_PTBL_MAGIC ) )
		bu_ptbl_init( &resp->re_pieces_pending, 100, "re_pieces_pending" );
	bu_ptbl_reset( &resp->re_pieces_pending );

	/* Verify that direction vector has unit length */
	if(RT_G_DEBUG) {
		FAST fastf_t f, diff;
		/* Fancy version of BN_VEC_NON_UNIT_LEN() */
		f = MAGSQ(ap->a_ray.r_dir);
		if( NEAR_ZERO(f, 0.0001) )  {
			rt_bomb("rt_shootray:  zero length dir vector\n");
			return(0);
		}
		diff = f - 1;
		if( !NEAR_ZERO( diff, 0.0001 ) )  {
			bu_log("rt_shootray: non-unit dir vect (x%d y%d lvl%d)\n",
				ap->a_x, ap->a_y, ap->a_level );
			f = 1/f;
			VSCALE( ap->a_ray.r_dir, ap->a_ray.r_dir, f );
		}
	}

	/*
	 *  Record essential statistics in per-processor data structure.
	 */
	resp->re_nshootray++;

	/* Compute the inverse of the direction cosines */
	if( ap->a_ray.r_dir[X] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[X] = -(ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
		ss.rstep[X] = -1;
	} else if( ap->a_ray.r_dir[X] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[X] =  (ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
		ss.rstep[X] = 1;
	} else {
		ap->a_ray.r_dir[X] = 0.0;
		ss.abs_inv_dir[X] = ss.inv_dir[X] = INFINITY;
		ss.rstep[X] = 0;
	}
	if( ap->a_ray.r_dir[Y] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Y] = -(ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
		ss.rstep[Y] = -1;
	} else if( ap->a_ray.r_dir[Y] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Y] =  (ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
		ss.rstep[Y] = 1;
	} else {
		ap->a_ray.r_dir[Y] = 0.0;
		ss.abs_inv_dir[Y] = ss.inv_dir[Y] = INFINITY;
		ss.rstep[Y] = 0;
	}
	if( ap->a_ray.r_dir[Z] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Z] = -(ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
		ss.rstep[Z] = -1;
	} else if( ap->a_ray.r_dir[Z] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Z] =  (ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
		ss.rstep[Z] = 1;
	} else {
		ap->a_ray.r_dir[Z] = 0.0;
		ss.abs_inv_dir[Z] = ss.inv_dir[Z] = INFINITY;
		ss.rstep[Z] = 0;
	}
	VMOVE( ap->a_inv_dir, ss.inv_dir );

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  If ray ends exactly at the model RPP, trace it.
	 */
	if( !rt_in_rpp( &ap->a_ray, ss.inv_dir, rtip->mdl_min, rtip->mdl_max )  ||
	    ap->a_ray.r_max < 0.0 )  {
		cutp = &ap->a_rt_i->rti_inf_box;
	    	if( cutp->bn.bn_len > 0 )  {
	    		/* Model has infinite solids, need to fire at them. */
			ss.box_start = BACKING_DIST;
			ss.model_start = 0;
			ss.box_end = ss.model_end = INFINITY;
			ss.lastcut = CUTTER_NULL;
			ss.old_status = (struct rt_shootray_status *)NULL;
			ss.curcut = cutp;
	    		ss.lastcell = ss.curcut;
			VMOVE( ss.curmin, rtip->mdl_min );
			VMOVE( ss.curmax, rtip->mdl_max );
			last_bool_start = BACKING_DIST;
			ss.newray = ap->a_ray;		/* struct copy */
			ss.odist_corr = ss.obox_start = ss.obox_end = -99;
	    		ss.dist_corr = 0.0;
	    		goto start_cell;
	    	}
		resp->re_nmiss_model++;
		ap->a_return = ap->a_miss( ap );
		status = "MISS model";
		goto out;
	}

	/*
	 *  The interesting part of the ray starts at distance 0.
	 *  If the ray enters the model at a negative distance,
	 *  (ie, the ray starts within the model RPP),
	 *  we only look at little bit behind (BACKING_DIST) to see if we are
	 *  just coming out of something, but never further back than
	 *  the intersection with the model RPP.
	 *  If the ray enters the model at a positive distance,
	 *  we always start there.
	 *  It is vital that we never pick a start point outside the
	 *  model RPP, or the space partitioning tree will pick the
	 *  wrong box and the ray will miss it.
	 *
	 *  BACKING_DIST should probably be determined by floating point
	 *  noise factor due to model RPP size -vs- number of bits of
	 *  floating point mantissa significance, rather than a constant,
	 *  but that is too hideous to think about here.
	 *  Also note that applications that really depend on knowing
	 *  what region they are leaving from should probably back their
	 *  own start-point up, rather than depending on it here, but
	 *  it isn't much trouble here.
	 *
	 *  Modification by JRA for pieces methodology:
	 *	The original algorithm here assumed that if we encountered any primitive
	 *	along the positive direction of the ray, ALL its intersections would be calculated.
	 *	With pieces, we may see only an exit hit if the entrance piece is in a space partition cell
	 *	that is more than "BACKING_DIST" behind the ray start point (leading to incorrect results).
	 *	I have modified the setting of "ss.box_start" (when pieces are present and the ray start point is
	 *	inside the model bounding box) as follows (see rt_find_backing_dist()):
	 *		The ray is traced through the space partitioning tree
	 *		The ray is intersected with the bounding box of each primitive using pieces in each cell
	 *		The minimum of all these intersections is set as the initial "ss.box_start".
	 *		The "backbits" bit vector has a bit set for each of the primitives using pieces that have
	 *			bounding boxes that extend behind the ray start point
	 *	Further below (in the "pieces" loop), I have added code to ignore primitives that do not have a bit
	 *	set in the backbits vector when we are behind the ray start point.
	 */

	/* these two values set the point where the ray tracing actually begins and ends */
	ss.box_start = ss.model_start = ap->a_ray.r_min;
	ss.box_end = ss.model_end = ap->a_ray.r_max;

	if( ap->a_rt_i->rti_nsolids_with_pieces > 0 ) {
		/* pieces are present */
		if( ss.box_start < BACKING_DIST ) {
			/* the first ray intersection with the model bounding box is more than BACKING_DIST
			 * behind the ray start point
			 */

			/* get a bit vector to keep track of which primitives need to be intersected
			 * behind the ray start point (those having bounding boxes extending behind the
			 * ray start point and using pieces)
			 */
			backbits = get_solidbitv( rtip->nsolids, resp );
			bu_bitv_clear(backbits);

			/* call "rt_find_backing_dist()" to calculate the required
			 * start point for calculation, and to fill in the "backbits" bit vector
			 */
			ss.box_start = rt_find_backing_dist( &ss, backbits );
		}
	} else {
		/* no pieces present, use the old scheme */
		if( ss.box_start < BACKING_DIST )
			ss.box_start = BACKING_DIST; /* Only look a little bit behind */
	}

	ss.lastcut = CUTTER_NULL;
	ss.old_status = (struct rt_shootray_status *)NULL;
	ss.curcut = &ap->a_rt_i->rti_CutHead;
	if( ss.curcut->cut_type == CUT_NUGRIDNODE ) {
		ss.lastcell = CUTTER_NULL;
		VSET( ss.curmin, ss.curcut->nugn.nu_axis[X][0].nu_spos,
				 ss.curcut->nugn.nu_axis[Y][0].nu_spos,
				 ss.curcut->nugn.nu_axis[Z][0].nu_spos );
		VSET( ss.curmax, ss.curcut->nugn.nu_axis[X][ss.curcut->nugn.nu_cells_per_axis[X]-1].nu_epos,
				 ss.curcut->nugn.nu_axis[Y][ss.curcut->nugn.nu_cells_per_axis[Y]-1].nu_epos,
				 ss.curcut->nugn.nu_axis[Z][ss.curcut->nugn.nu_cells_per_axis[Z]-1].nu_epos );
	} else if( ss.curcut->cut_type == CUT_CUTNODE ||
		   ss.curcut->cut_type == CUT_BOXNODE ) {
		ss.lastcell = ss.curcut;
		VMOVE( ss.curmin, rtip->mdl_min );
		VMOVE( ss.curmax, rtip->mdl_max );
	}

	last_bool_start = BACKING_DIST;
	ss.newray = ap->a_ray;		/* struct copy */
	ss.odist_corr = ss.obox_start = ss.obox_end = -99;
	ss.dist_corr = 0.0;
	ss.box_num = 0;

	/*
	 *  While the ray remains inside model space,
	 *  push from box to box until ray emerges from
	 *  model space again (or first hit is found, if user is impatient).
	 *  It is vitally important to always stay within the model RPP, or
	 *  the space partitoning tree will pick wrong boxes & miss them.
	 */
	while( (cutp = rt_advance_to_next_cell( &ss )) != CUTTER_NULL )  {
start_cell:
		if(debug_shoot) {
			bu_log("BOX #%d interval is %g..%g\n", ss.box_num, ss.box_start, ss.box_end);
			rt_pr_cut( cutp, 0 );
		}

		if( cutp->bn.bn_len <= 0 && cutp->bn.bn_piecelen <= 0 )  {
			/* Push ray onwards to next box */
			ss.box_start = ss.box_end;
			resp->re_nempty_cells++;
			continue;
		}

		/* Consider all "pieces" of all solids within the box */
		pending_hit = ss.box_end;
		if( cutp->bn.bn_piecelen > 0 )  {
			register struct rt_piecelist *plp;

			plp = &(cutp->bn.bn_piecelist[cutp->bn.bn_piecelen-1]);
			for( ; plp >= cutp->bn.bn_piecelist; plp-- )  {
				struct rt_piecestate *psp;
				struct soltab	*stp;
				int ret;
				int had_hits_before;

				RT_CK_PIECELIST(plp);

				/* Consider all pieces of this one solid in this cell */
				stp = plp->stp;
				RT_CK_SOLTAB(stp);

				if( backbits && ss.box_end < BACKING_DIST && BU_BITTEST( backbits, stp->st_bit ) == 0 ) {
					/* we are behind the ray start point and this primitive is not one
					 * that we need to intersect back here
					 */
					continue;
				}

				psp = &(resp->re_pieces[stp->st_piecestate_num]);
				RT_CK_PIECESTATE(psp);
				if( psp->ray_seqno != resp->re_nshootray )  {
					/* state is from an earlier ray, scrub */
					BU_BITV_ZEROALL(psp->shot);
					psp->ray_seqno = resp->re_nshootray;
					rt_htbl_reset( &psp->htab );

					/* Compute ray entry and exit to entire solid's bounding box */
					if( !rt_in_rpp( &ss.newray, ss.inv_dir,
					    stp->st_min, stp->st_max ) )  {
						if(debug_shoot)bu_log("rpp miss %s (all pieces)\n", stp->st_name);
						resp->re_prune_solrpp++;
						BU_BITSET( solidbits, stp->st_bit );
						continue;	/* MISS */
					}
					psp->mindist = ss.newray.r_min + ss.dist_corr;
					psp->maxdist = ss.newray.r_max + ss.dist_corr;
					if(debug_shoot) bu_log("%s mindist=%g, maxdist=%g\n", stp->st_name, psp->mindist, psp->maxdist);
					had_hits_before = 0;
				} else {
					if( BU_BITTEST( solidbits, stp->st_bit ) )  {
						/* we missed the solid RPP in an earlier cell */
						resp->re_ndup++;
						continue;	/* already shot */
					}
					had_hits_before = psp->htab.end;
				}

				/*
				 *  Allow this solid to shoot at all of its
				 *  'pieces' in this cell, all at once.
				 *  'newray' has been transformed to be near
				 *  to this cell, and
				 *  'dist_corr' is the additive correction
				 *  factor that ft_piece_shot() must apply
				 *  to hits calculated using 'newray'.
				 */
				resp->re_piece_shots++;
				psp->cutp = cutp;
				if( (ret = stp->st_meth->ft_piece_shot(
				    psp, plp, ss.dist_corr, &ss.newray, ap, &waiting_segs )) <= 0 )  {
				    	/* No hits at all */
					resp->re_piece_shot_miss++;
				} else {
					resp->re_piece_shot_hit++;
				}
				if(debug_shoot)bu_log("shooting %s pieces, nhit=%d\n", stp->st_name, ret);

				/*  See if this solid has been fully processed yet.
				 *  If ray has passed through bounding volume, we're done.
				 *  ft_piece_hitsegs() will only be called once per ray.
				 */
				if( ss.box_end > psp->maxdist && psp->htab.end > 0 ) {
					/* Convert hits into segs */
					if(debug_shoot)bu_log("shooting %s pieces complete, making segs\n", stp->st_name);
					/* Distance correction was handled in ft_piece_shot */
					stp->st_meth->ft_piece_hitsegs( psp, &waiting_segs, ap );
					rt_htbl_reset( &psp->htab );
					BU_BITSET( solidbits, stp->st_bit );

					if( had_hits_before )
    						bu_ptbl_rm( &resp->re_pieces_pending, (long *)psp );
				} else {
					if( !had_hits_before )
						bu_ptbl_ins_unique( &resp->re_pieces_pending, (long *)psp );
				}
			}
		}

		/* Consider all solids within the box */
		if( cutp->bn.bn_len > 0 && ss.box_end >= BACKING_DIST )  {
			stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
			for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
				register struct soltab *stp = *stpp;

				if( BU_BITTEST( solidbits, stp->st_bit ) )  {
					resp->re_ndup++;
					continue;	/* already shot */
				}

				/* Shoot a ray */
				BU_BITSET( solidbits, stp->st_bit );

				/* Check against bounding RPP, if desired by solid */
				if( stp->st_meth->ft_use_rpp )  {
					if( !rt_in_rpp( &ss.newray, ss.inv_dir,
					    stp->st_min, stp->st_max ) )  {
						if(debug_shoot)bu_log("rpp miss %s\n", stp->st_name);
						resp->re_prune_solrpp++;
						continue;	/* MISS */
					}
					if( ss.dist_corr + ss.newray.r_max < BACKING_DIST )  {
						if(debug_shoot)bu_log("rpp skip %s, dist_corr=%g, r_max=%g\n", stp->st_name, ss.dist_corr, ss.newray.r_max);
						resp->re_prune_solrpp++;
						continue;	/* MISS */
					}
				}

				if(debug_shoot)bu_log("shooting %s\n", stp->st_name);
				resp->re_shots++;
				BU_LIST_INIT( &(new_segs.l) );
				if( stp->st_meth->ft_shot(
				    stp, &ss.newray, ap, &new_segs ) <= 0 )  {
					resp->re_shot_miss++;
					continue;	/* MISS */
				}

				/* Add seg chain to list awaiting rt_boolweave() */
				{
					register struct seg *s2;
					while(BU_LIST_WHILE(s2,seg,&(new_segs.l)))  {
						BU_LIST_DEQUEUE( &(s2->l) );
						/* Restore to original distance */
						s2->seg_in.hit_dist += ss.dist_corr;
						s2->seg_out.hit_dist += ss.dist_corr;
						s2->seg_in.hit_rayp = s2->seg_out.hit_rayp = &ap->a_ray;
						BU_LIST_INSERT( &(waiting_segs.l), &(s2->l) );
					}
				}
				resp->re_shot_hit++;
			}
		}
		if( RT_G_DEBUG & DEBUG_ADVANCE )
			rt_plot_cell( cutp, &ss, &(waiting_segs.l), rtip);

		/*
		 *  If a_onehit == 0 and a_ray_length <= 0, then the ray
		 *  is traced to +infinity.
		 *
		 *  If a_onehit != 0, then it indicates how many hit points
		 *  (which are greater than the ray start point of 0.0)
		 *  the application requires, ie, partitions with inhit >= 0.
		 *  (If negative, indicates number of non-air hits needed).
		 *  If this box yielded additional segments,
		 *  immediately weave them into the partition list,
		 *  and perform final boolean evaluation.
		 *  If this results in the required number of final
		 *  partitions, then cease ray-tracing and hand the
		 *  partitions over to the application.
		 *  All partitions will have valid in and out distances.
		 *  a_ray_length is treated similarly to a_onehit.
		 */
		if( BU_LIST_NON_EMPTY( &(waiting_segs.l) ) )  {
			if(debug_shoot)  {
				struct seg *segp;
				bu_log("Waiting segs:\n");
				for( BU_LIST_FOR( segp, seg, &(waiting_segs.l) ) )  {
					rt_pr_seg(segp);
				}
			}
			if( ap->a_onehit != 0 )  {
				int	done;

				/* Weave these segments into partition list */
				rt_boolweave( &finished_segs, &waiting_segs, &InitialPart, ap );

				if( BU_PTBL_LEN( &resp->re_pieces_pending ) > 0 )  {
					/* Find the lowest pending mindist, that's as far as boolfinal can progress to */
					struct rt_piecestate **psp;
					for( BU_PTBL_FOR( psp, (struct rt_piecestate **), &resp->re_pieces_pending ) )  {
						FAST fastf_t dist;

						dist = (*psp)->mindist;
						BU_ASSERT_DOUBLE( dist, <, INFINITY );
						if( dist < pending_hit )  {
							pending_hit = dist;
							if(debug_shoot) bu_log("pending_hit lowered to %g by %s\n", pending_hit, (*psp)->stp->st_name);
						}
					}
				}

				/* Evaluate regions upto end of good segs */
				if( ss.box_end < pending_hit )  pending_hit = ss.box_end;
				done = rt_boolfinal( &InitialPart, &FinalPart,
					last_bool_start, pending_hit, regionbits, ap, solidbits );
				last_bool_start = pending_hit;

				/* See if enough partitions have been acquired */
				if( done > 0 )  goto hitit;
			}
		}

		if( ap->a_ray_length > 0.0 &&
		    ss.box_end >= ap->a_ray_length &&
		    ap->a_ray_length < pending_hit )
			goto weave;

		/* Push ray onwards to next box */
		ss.box_start = ss.box_end;
	}

	/*
	 *  Ray has finally left known space --
	 *  Weave any remaining segments into the partition list.
	 */
weave:
	if( RT_G_DEBUG&DEBUG_ADVANCE )
		bu_log( "rt_shootray: ray has left known space\n" );

	/* Process any pending hits into segs */
	if( BU_PTBL_LEN( &resp->re_pieces_pending ) > 0 )  {
		struct rt_piecestate **psp;
		for( BU_PTBL_FOR( psp, (struct rt_piecestate **), &resp->re_pieces_pending ) )  {
			if( (*psp)->htab.end > 0 )  {
				/* Convert any pending hits into segs */
				/* Distance correction was handled in ft_piece_shot */
				(*psp)->stp->st_meth->ft_piece_hitsegs( *psp, &waiting_segs, ap );
				rt_htbl_reset( &(*psp)->htab );
			}
			*psp = NULL;
		}
		bu_ptbl_reset( &resp->re_pieces_pending );
	}

	if( BU_LIST_NON_EMPTY( &(waiting_segs.l) ) )  {
		rt_boolweave( &finished_segs, &waiting_segs, &InitialPart, ap );
	}

	/* finished_segs chain now has all segments hit by this ray */
	if( BU_LIST_IS_EMPTY( &(finished_segs.l) ) )  {
		ap->a_return = ap->a_miss( ap );
		status = "MISS prims";
		goto out;
	}

	/*
	 *  All intersections of the ray with the model have
	 *  been computed.  Evaluate the boolean trees over each partition.
	 */
	(void)rt_boolfinal( &InitialPart, &FinalPart, BACKING_DIST,
		INFINITY,
		regionbits, ap, solidbits);

	if( FinalPart.pt_forw == &FinalPart )  {
		ap->a_return = ap->a_miss( ap );
		status = "MISS bool";
		RT_FREE_PT_LIST( &InitialPart, resp );
		RT_FREE_SEG_LIST( &finished_segs, resp );
		goto out;
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
	if(debug_shoot)  rt_pr_partitions(rtip,&FinalPart,"a_hit()");

	/*
	 *  Before recursing, release storage for unused Initial partitions.
	 *  finished_segs can not be released yet, because FinalPart
	 *  partitions will point to hits in those segments.
	 */
	RT_FREE_PT_LIST( &InitialPart, resp );

	/*
	 *  finished_segs is only used by special hit routines
	 *  which don't follow the traditional solid modeling paradigm.
	 */
	if(RT_G_DEBUG&DEBUG_ALLHITS) rt_pr_partitions(rtip,&FinalPart,"Partition list passed to a_hit() routine");
	ap->a_return = ap->a_hit( ap, &FinalPart, &finished_segs );
	status = "HIT";

	RT_FREE_SEG_LIST( &finished_segs, resp );
	RT_FREE_PT_LIST( &FinalPart, resp );

	/*
	 * Processing of this ray is complete.
	 */
out:
	/*  Return dynamic resources to their freelists.  */
	BU_CK_BITV(solidbits);
	BU_LIST_APPEND( &resp->re_solid_bitv, &solidbits->l );
	if( backbits ) {
		BU_CK_BITV(backbits);
		BU_LIST_APPEND( &resp->re_solid_bitv, &backbits->l );
	}
	BU_CK_PTBL(regionbits);
	BU_LIST_APPEND( &resp->re_region_ptbl, &regionbits->l );

	/* Clean up any pending hits */
	if( BU_PTBL_LEN( &resp->re_pieces_pending ) > 0 )  {
		struct rt_piecestate **psp;
		for( BU_PTBL_FOR( psp, (struct rt_piecestate **), &resp->re_pieces_pending ) )  {
			if( (*psp)->htab.end > 0 )
				rt_htbl_reset( &(*psp)->htab );
		}
		bu_ptbl_reset( &resp->re_pieces_pending );
	}

	/* Terminate any logging */
	if(RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION|DEBUG_ALLHITS))  {
		bu_log_indent_delta(-2);
		bu_log("----------shootray cpu=%d  %d,%d lvl=%d (%s) %s ret=%d\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
			status, ap->a_return);
	}
	return( ap->a_return );
}

/*
 *			R T _ C E L L _ N _ O N _ R A Y
 *
 *  Return pointer to cell 'n' along a given ray.
 *  Used for debugging of how space partitioning interacts with shootray.
 *  Intended to mirror the operation of rt_shootray().
 *  The first cell is 0.
 */
const union cutter *
rt_cell_n_on_ray(register struct application *ap, int n)

   	  		/* First cell is #0 */
{
	struct rt_shootray_status	ss;
	register const union cutter *cutp;
	struct resource		*resp;
	struct rt_i		*rtip;
	const int		debug_shoot = RT_G_DEBUG & DEBUG_SHOOT;

	RT_AP_CHECK(ap);
	if( ap->a_magic )  {
		RT_CK_AP(ap);
	} else {
		ap->a_magic = RT_AP_MAGIC;
	}
	if( ap->a_ray.magic )  {
		RT_CK_RAY(&(ap->a_ray));
	} else {
		ap->a_ray.magic = RT_RAY_MAGIC;
	}
	if( ap->a_resource == RESOURCE_NULL )  {
		ap->a_resource = &rt_uniresource;
		if(RT_G_DEBUG)bu_log("rt_cell_n_on_ray:  defaulting a_resource to &rt_uniresource\n");
		if( rt_uniresource.re_magic == 0 )
			rt_init_resource( &rt_uniresource, 0, ap->a_rt_i );
	}
	ss.ap = ap;
	rtip = ap->a_rt_i;
	RT_CK_RTI( rtip );
	resp = ap->a_resource;
	RT_CK_RESOURCE(resp);
	ss.resp = resp;

	if(RT_G_DEBUG&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION|DEBUG_ALLHITS)) {
		bu_log_indent_delta(2);
		bu_log("\n**********cell_n_on_ray cpu=%d  %d,%d lvl=%d (%s), n=%d\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?", n );
		bu_log("Pnt (%g, %g, %g) a_onehit=%d\n",
			V3ARGS(ap->a_ray.r_pt),
			ap->a_onehit );
		VPRINT("Dir", ap->a_ray.r_dir);
	}
#ifndef NO_BADRAY_CHECKING
	if(RT_BADVEC(ap->a_ray.r_pt)||RT_BADVEC(ap->a_ray.r_dir))  {
		bu_log("\n**********cell_n_on_ray cpu=%d  %d,%d lvl=%d (%s)\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		VPRINT(" r_pt", ap->a_ray.r_pt);
		VPRINT("r_dir", ap->a_ray.r_dir);
		rt_bomb("rt_cell_n_on_ray() bad ray\n");
	}
#endif

	if( rtip->needprep )
		rt_prep_parallel(rtip, 1);	/* Stay on our CPU */

	if( BU_LIST_UNINITIALIZED( &resp->re_parthead ) )  {
		/* XXX This shouldn't happen any more */
		bu_log("rt_cell_n_on_ray() resp=x%x uninitialized, fixing it\n", resp);
		/*
		 *  We've been handed a mostly un-initialized resource struct,
		 *  with only a magic number and a cpu number filled in.
		 *  Init it and add it to the table.
		 *  This is how application-provided resource structures
		 *  are remembered for later cleanup by the library.
		 */
		rt_init_resource( resp, resp->re_cpu, rtip );
	}
	/* Ensure that this CPU's resource structure is registered */
	BU_ASSERT_PTR( BU_PTBL_GET(&rtip->rti_resources, resp->re_cpu), !=, NULL );

	/* Verify that direction vector has unit length */
	if(RT_G_DEBUG) {
		FAST fastf_t f, diff;
		/* Fancy version of BN_VEC_NON_UNIT_LEN() */
		f = MAGSQ(ap->a_ray.r_dir);
		if( NEAR_ZERO(f, 0.0001) )  {
			rt_bomb("rt_cell_n_on_ray:  zero length dir vector\n");
			return CUTTER_NULL;
		}
		diff = f - 1;
		if( !NEAR_ZERO( diff, 0.0001 ) )  {
			bu_log("rt_cell_n_on_ray: non-unit dir vect (x%d y%d lvl%d)\n",
				ap->a_x, ap->a_y, ap->a_level );
			f = 1/f;
			VSCALE( ap->a_ray.r_dir, ap->a_ray.r_dir, f );
		}
	}

	/* Compute the inverse of the direction cosines */
	if( ap->a_ray.r_dir[X] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[X] = -(ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
		ss.rstep[X] = -1;
	} else if( ap->a_ray.r_dir[X] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[X] =  (ss.inv_dir[X]=1.0/ap->a_ray.r_dir[X]);
		ss.rstep[X] = 1;
	} else {
		ap->a_ray.r_dir[X] = 0.0;
		ss.abs_inv_dir[X] = ss.inv_dir[X] = INFINITY;
		ss.rstep[X] = 0;
	}
	if( ap->a_ray.r_dir[Y] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Y] = -(ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
		ss.rstep[Y] = -1;
	} else if( ap->a_ray.r_dir[Y] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Y] =  (ss.inv_dir[Y]=1.0/ap->a_ray.r_dir[Y]);
		ss.rstep[Y] = 1;
	} else {
		ap->a_ray.r_dir[Y] = 0.0;
		ss.abs_inv_dir[Y] = ss.inv_dir[Y] = INFINITY;
		ss.rstep[Y] = 0;
	}
	if( ap->a_ray.r_dir[Z] < -SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Z] = -(ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
		ss.rstep[Z] = -1;
	} else if( ap->a_ray.r_dir[Z] > SQRT_SMALL_FASTF )  {
		ss.abs_inv_dir[Z] =  (ss.inv_dir[Z]=1.0/ap->a_ray.r_dir[Z]);
		ss.rstep[Z] = 1;
	} else {
		ap->a_ray.r_dir[Z] = 0.0;
		ss.abs_inv_dir[Z] = ss.inv_dir[Z] = INFINITY;
		ss.rstep[Z] = 0;
	}

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  If ray ends exactly at the model RPP, trace it.
	 */
	if( !rt_in_rpp( &ap->a_ray, ss.inv_dir, rtip->mdl_min, rtip->mdl_max )  ||
	    ap->a_ray.r_max < 0.0 )  {
		cutp = &ap->a_rt_i->rti_inf_box;
	    	if( cutp->bn.bn_len > 0 )  {
	    		if( n == 0 )  return cutp;
	    	}
    		return CUTTER_NULL;
	}

	/*
	 *  The interesting part of the ray starts at distance 0.
	 *  If the ray enters the model at a negative distance,
	 *  (ie, the ray starts within the model RPP),
	 *  we only look at little bit behind (BACKING_DIST) to see if we are
	 *  just coming out of something, but never further back than
	 *  the intersection with the model RPP.
	 *  If the ray enters the model at a positive distance,
	 *  we always start there.
	 *  It is vital that we never pick a start point outside the
	 *  model RPP, or the space partitioning tree will pick the
	 *  wrong box and the ray will miss it.
	 *
	 *  BACKING_DIST should probably be determined by floating point
	 *  noise factor due to model RPP size -vs- number of bits of
	 *  floating point mantissa significance, rather than a constant,
	 *  but that is too hideous to think about here.
	 *  Also note that applications that really depend on knowing
	 *  what region they are leaving from should probably back their
	 *  own start-point up, rather than depending on it here, but
	 *  it isn't much trouble here.
	 */
	ss.box_start = ss.model_start = ap->a_ray.r_min;
	ss.box_end = ss.model_end = ap->a_ray.r_max;

	if( ss.box_start < BACKING_DIST )
		ss.box_start = BACKING_DIST; /* Only look a little bit behind */

	ss.lastcut = CUTTER_NULL;
	ss.old_status = (struct rt_shootray_status *)NULL;
	ss.curcut = &ap->a_rt_i->rti_CutHead;
	if( ss.curcut->cut_type == CUT_NUGRIDNODE ) {
		ss.lastcell = CUTTER_NULL;
		VSET( ss.curmin, ss.curcut->nugn.nu_axis[X][0].nu_spos,
				 ss.curcut->nugn.nu_axis[Y][0].nu_spos,
				 ss.curcut->nugn.nu_axis[Z][0].nu_spos );
		VSET( ss.curmax, ss.curcut->nugn.nu_axis[X][ss.curcut->nugn.nu_cells_per_axis[X]-1].nu_epos,
				 ss.curcut->nugn.nu_axis[Y][ss.curcut->nugn.nu_cells_per_axis[Y]-1].nu_epos,
				 ss.curcut->nugn.nu_axis[Z][ss.curcut->nugn.nu_cells_per_axis[Z]-1].nu_epos );
	} else if( ss.curcut->cut_type == CUT_CUTNODE ||
		   ss.curcut->cut_type == CUT_BOXNODE ) {
		ss.lastcell = ss.curcut;
		VMOVE( ss.curmin, rtip->mdl_min );
		VMOVE( ss.curmax, rtip->mdl_max );
	}

	ss.newray = ap->a_ray;		/* struct copy */
	ss.odist_corr = ss.obox_start = ss.obox_end = -99;
	ss.dist_corr = 0.0;

	/*
	 *  While the ray remains inside model space,
	 *  push from box to box until ray emerges from
	 *  model space again (or first hit is found, if user is impatient).
	 *  It is vitally important to always stay within the model RPP, or
	 *  the space partitoning tree will pick wrong boxes & miss them.
	 */
	while( (cutp = rt_advance_to_next_cell( &ss )) != CUTTER_NULL )  {
		if(debug_shoot) {
			rt_pr_cut( cutp, 0 );
		}
		if( --n <= 0 )  return cutp;

		/* Push ray onwards to next box */
		ss.box_start = ss.box_end;
	}
	return CUTTER_NULL;
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
 * Note -
 *  A hit is returned if the intersect is behind the start point.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit return -
 *	rp->r_min = dist from start of ray to point at which ray ENTERS solid
 *	rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
int
rt_in_rpp(struct xray		*rp,
	  register const fastf_t *invdir,	/* inverses of rp->r_dir[] */
	  register const fastf_t *min,
	  register const fastf_t *max)
{
	register const fastf_t	*pt = &rp->r_pt[0];
	register fastf_t	sv;
#define st sv			/* reuse the register */
	register fastf_t	rmin = -INFINITY;
	register fastf_t	rmax =  INFINITY;

	/* Start with infinite ray, and trim it down */

	/* X axis */
	if( *invdir < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		if(rmax > (sv = (*min - *pt) * *invdir) )
			rmax = sv;
		if( rmin < (st = (*max - *pt) * *invdir) )
			rmin = st;
	}  else if( *invdir > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		if(rmax > (st = (*max - *pt) * *invdir) )
			rmax = st;
		if( rmin < ((sv = (*min - *pt) * *invdir)) )
			rmin = sv;
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
	if( *invdir < 0.0 )  {
		if(rmax > (sv = (*min - *pt) * *invdir) )
			rmax = sv;
		if( rmin < (st = (*max - *pt) * *invdir) )
			rmin = st;
	}  else if( *invdir > 0.0 )  {
		if(rmax > (st = (*max - *pt) * *invdir) )
			rmax = st;
		if( rmin < ((sv = (*min - *pt) * *invdir)) )
			rmin = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
	if( *invdir < 0.0 )  {
		if(rmax > (sv = (*min - *pt) * *invdir) )
			rmax = sv;
		if( rmin < (st = (*max - *pt) * *invdir) )
			rmin = st;
	}  else if( *invdir > 0.0 )  {
		if(rmax > (st = (*max - *pt) * *invdir) )
			rmax = st;
		if( rmin < ((sv = (*min - *pt) * *invdir)) )
			rmin = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* If equal, RPP is actually a plane */
	if( rmin > rmax )
		return(0);	/* MISS */

	/* HIT.  Only now do rp->r_min and rp->r_max have to be written */
	rp->r_min = rmin;
	rp->r_max = rmax;
	return(1);		/* HIT */
}

/* For debugging */
int
rt_DB_rpp(register struct xray *rp, register const fastf_t *invdir, register const fastf_t *min, register const fastf_t *max)

                               	/* inverses of rp->r_dir[] */


{
	register const fastf_t *pt = &rp->r_pt[0];
	FAST fastf_t sv;

	/* Start with infinite ray, and trim it down */
	rp->r_min = -INFINITY;
	rp->r_max = INFINITY;

VPRINT("r_pt ", pt);
VPRINT("r_dir", rp->r_dir);
VPRINT("min  ", min);
VPRINT("max  ", max);
	/* X axis */
	if( rp->r_dir[X] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[X] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_max=%g\n", st, rp->r_max);
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		/*
		 *  Direction cosines along this axis is NEAR 0,
		 *  which implies that the ray is perpendicular to the axis,
		 *  so merely check position against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* Y axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Y] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[Y] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_max=%g\n", st, rp->r_max);
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Z] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[Z] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
bu_log("st=%g, r_max=%g\n", st, rp->r_max);
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
bu_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
bu_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* If equal, RPP is actually a plane */
	if( rp->r_min > rp->r_max )
		goto miss;
	bu_log("HIT:  %g..%g\n", rp->r_min, rp->r_max );
	return(1);		/* HIT */
miss:
	bu_log("MISS\n");
	return(0);		/* MISS */
}

#undef st


#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;

/* Stub function which will "similate" a call to a vector shot routine */
void
rt_vstub(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */
                  	     /* pointer to an application */
{
	register int    i;
	register struct seg *tmp_seg;
	struct seg	seghead;

	BU_LIST_INIT( &(seghead.l) );

	/* go through each ray/solid pair and call a scalar function */
	for (i = 0; i < n; i++) {
		if (stp[i] != 0){ /* skip call if solid table pointer is NULL */
			/* do scalar call, place results in segp array */
			if( rt_functab[stp[i]->st_id].ft_shot(stp[i], rp[i], ap, &seghead) <= 0 )  {
				SEG_MISS(segp[i]);
			} else {
				tmp_seg = BU_LIST_FIRST(seg, &(seghead.l) );
				BU_LIST_DEQUEUE( &(tmp_seg->l) );
				segp[i] = *tmp_seg; /* structure copy */
				RT_FREE_SEG(tmp_seg, ap->a_resource);
			}
		}
	}
}

/*
 *			R T _ P R _ L I B R A R Y _ V E R S I O N
 *
 *  In case anyone actually cares, print out library's compilation version.
 */
void
rt_pr_library_version(void)
{
	bu_log("%s", rt_version);
}

void
rt_zero_res_stats( struct resource *resp )
{
	RT_CK_RESOURCE( resp );

	resp->re_nshootray = 0;
	resp->re_nmiss_model = 0;

	resp->re_shots = 0;
	resp->re_shot_hit = 0;
	resp->re_shot_miss = 0;

	resp->re_prune_solrpp = 0;

	resp->re_ndup = 0;
	resp->re_nempty_cells = 0;

	resp->re_piece_shots = 0;
	resp->re_piece_shot_hit = 0;
	resp->re_piece_shot_miss = 0;
	resp->re_piece_ndup = 0;
}

/*
 *			R T _ A D D _ R E S _ S T A T S
 *
 *  To be called only in non-parallel mode, to tally up the statistics
 *  from the resource structure(s) into the rt instance structure.
 *
 *  Non-parallel programs should call
 *	rt_add_res_stats( rtip, RESOURCE_NULL );
 *  to have the default resource results tallied in.
 */
void
rt_add_res_stats(register struct rt_i *rtip, register struct resource *resp)
{
	RT_CK_RTI( rtip );

	if( resp == RESOURCE_NULL )  resp = &rt_uniresource;
	RT_CK_RESOURCE( resp );

	rtip->rti_nrays += resp->re_nshootray;
	rtip->nmiss_model += resp->re_nmiss_model;

	rtip->nshots += resp->re_shots + resp->re_piece_shots;
	rtip->nhits += resp->re_shot_hit + resp->re_piece_shot_hit;
	rtip->nmiss += resp->re_shot_miss + resp->re_piece_shot_miss;

	rtip->nmiss_solid += resp->re_prune_solrpp;

	rtip->ndup += resp->re_ndup + resp->re_piece_ndup;
	rtip->nempty_cells += resp->re_nempty_cells;

	/* Zero out resource totals, so repeated calls are not harmful */
	rt_zero_res_stats( resp );
}

/*
 *  Routines for plotting the progress of one ray through the model.  -Mike
 */
void
rt_3move_raydist(FILE *fp, struct xray *rayp, double dist)
{
	point_t	p;

	VJOIN1( p, rayp->r_pt, dist, rayp->r_dir );
	pdv_3move( fp, p );
}

void
rt_3cont_raydist(FILE *fp, struct xray *rayp, double dist)
{
	point_t	p;

	VJOIN1( p, rayp->r_pt, dist, rayp->r_dir );
	pdv_3cont( fp, p );
}

/*
		rt_plot_cell( cutp, &ss, &(waiting_segs.l), rtip);
 */
void
rt_plot_cell(const union cutter *cutp, const struct rt_shootray_status *ssp, struct bu_list *waiting_segs_hd, struct rt_i *rtip)
{
	char		buf[128];
	static int	fnum = 0;
	FILE		*fp;
	struct soltab	**stpp;
	struct application	*ap;

	RT_CK_RTI(rtip);
	RT_AP_CHECK(ssp->ap);
	RT_CK_RTI(ssp->ap->a_rt_i);
	ap = ssp->ap;

	sprintf( buf, "cell%d.pl", fnum++ );
	if( (fp = fopen( buf, "w" )) == NULL )  {
		perror(buf);
	}

	pl_color( fp, 0, 100, 0 );		/* green box for model RPP */

	/* Plot the model RPP, to provide some context */
	pdv_3space( fp, rtip->rti_pmin, rtip->rti_pmax );
	pdv_3box( fp, rtip->rti_pmin, rtip->rti_pmax );

	/* Plot the outline of this one cell */
	pl_color( fp, 80, 80, 250 );
	switch( cutp->cut_type )  {
	case CUT_BOXNODE:
		pdv_3box( fp, cutp->bn.bn_min, cutp->bn.bn_max );
		break;
	default:
		bu_log("cut_type = %d\n", cutp->cut_type );
		bu_bomb("Unknown cut_type\n");
	}

	if( cutp->bn.bn_len > 0 ) {
		/* Plot every solid listed in this cell */
		stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
		for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
			register struct soltab *stp = *stpp;

			rt_plot_solid( fp, rtip, stp, ap->a_resource );
		}
	}

	/* Plot interval of ray in box, in green */
	pl_color( fp, 100, 255, 200 );
	rt_3move_raydist( fp, &ap->a_ray, ssp->box_start );
	rt_3cont_raydist( fp, &ap->a_ray, ssp->box_end );

	if( bu_list_len( waiting_segs_hd ) <= 0 )  {
		/* No segments, just plot the whole ray */
		pl_color( fp, 255, 255, 0 );	/* yellow -- no segs */
		rt_3move_raydist( fp, &ap->a_ray, ssp->model_start );
		rt_3cont_raydist( fp, &ap->a_ray, ssp->box_start );
		rt_3move_raydist( fp, &ap->a_ray, ssp->box_end );
		rt_3cont_raydist( fp, &ap->a_ray, ssp->model_end );
	} else {
		/* Plot the segments awaiting boolweave. */
		struct seg	*segp;

		for( BU_LIST_FOR( segp, seg, waiting_segs_hd ) )  {
			RT_CK_SEG(segp);
			pl_color( fp, 255, 0, 0 );	/* red */
			rt_3move_raydist( fp, &ap->a_ray, segp->seg_in.hit_dist );
			rt_3cont_raydist( fp, &ap->a_ray, segp->seg_out.hit_dist );
		}
	}

	fclose(fp);
	bu_log("wrote %s\n", buf);
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
