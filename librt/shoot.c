/*
 *			S H O O T . C
 *
 *	Ray Tracing program shot coordinator.
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
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985,1991 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSshoot[] = "@(#)$Header$ (BRL)";
#endif

char rt_CopyRight_Notice[] = "@(#) Copyright (C) 1985,1991 by the United States Army";

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "./debug.h"

struct resource rt_uniresource;		/* Resources for uniprocessor */

extern void	rt_plot_cell();		/* at end of file */

#ifdef CRAY
#define AUTO register
#else
#define AUTO /*let the compiler decide*/
#endif

#define NUGRID_T_SETUP(_ax,_cval,_cno) \
	if( ssp->rstep[_ax] > 0 ) { \
		ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_epos - _cval) * \
					    ssp->inv_dir[_ax]; \
	} else if( ssp->rstep[_ax] < 0 ) { \
		ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_spos - _cval) * \
					    ssp->inv_dir[_ax]; \
	} else { \
		ssp->tv[_ax] = INFINITY; \
	}
#define NUGRID_T_ADV(_ax,_cno) \
	if( ssp->rstep[_ax] != 0 )  { \
		ssp->tv[_ax] += nu_axis[_ax][_cno].nu_width * \
			ssp->abs_inv_dir[_ax]; \
	}

#define BACKING_DIST	(-2.0)		/* mm to look behind start point */
#define OFFSET_DIST	0.01		/* mm to advance point into box */

struct shootray_status {
	fastf_t			dist_corr;	/* correction distance */
	fastf_t			odist_corr;
	fastf_t			box_start;
	fastf_t			obox_start; 
	fastf_t			box_end;
	fastf_t			obox_end;
	fastf_t			model_start;
	fastf_t			model_end;
	struct xray		newray;		/* closer ray start */
	struct application	*ap;
	struct resource		*resp;
	vect_t			inv_dir;      /* inverses of ap->a_ray.r_dir */
	vect_t			abs_inv_dir;  /* absolute values of inv_dir */
	int			rstep[3];     /* -/0/+ dir of ray in axis */
	CONST union cutter	*lastcut, *lastcell;
	CONST union cutter	*curcut;
	vect_t			curmin, curmax;
	int			igrid[3];     /* integer cell coordinates */
	vect_t			tv;	      /* next t intercept values */
	int			out_axis;     /* axis ray will leave through */
	struct shootray_status	*old_status;
};


/*
 *			R T _ F I N D _ N U G R I D
 *
 *  Along the given axis, find which NUgrid cell this value lies in.
 *  Use method of binary subdivision.
 */
int
rt_find_nugrid( nugnp, axis, val )
struct nugridnode	*nugnp;
int			 axis;
fastf_t			 val;
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

CONST union cutter *
rt_advance_to_next_cell( ssp )
register struct shootray_status	*ssp;
{
	register CONST union cutter		*cutp, *curcut = ssp->curcut;
	register CONST struct application	*ap = ssp->ap;
	register fastf_t			t0, px, py, pz;
	int					push_flag = 0;
	double					fraction;
	int					exponent;

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
 *  This version uses Gigante's non-uniform 3-D space grid/mesh discretization.
 */
			register int out_axis;
			register CONST struct nu_axis  **nu_axis =
			     (CONST struct nu_axis **)&curcut->nugn.nu_axis[0];
			register CONST int		*nu_stepsize =
			     &curcut->nugn.nu_stepsize[0];
			register CONST int	        *nu_cells_per_axis =
			     &curcut->nugn.nu_cells_per_axis[0];
			register CONST union cutter	*nu_grid =
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
			    cutp->bn.bn_len <= 0 ) {
				++ssp->resp->re_nempty_cells;
				goto again;
			}

			ssp->out_axis = out_axis;
			ssp->lastcell = cutp;
			
			if( rt_g.debug&DEBUG_ADVANCE )
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
 *  This version uses Muuss' non-uniform binary space partitioning tree.
 */
			t0 += OFFSET_DIST;
			cutp = curcut;
			break;
		default:
			rt_bomb(
		       "rt_advance_to_next_cell: unknown high-level cutnode" );
		}

test:		if( cutp==CUTTER_NULL ) {
			/* Move up out of the current node, or return if there
			   is nothing left to do. */
			register struct shootray_status *old = ssp->old_status;

			if( old == NULL ) return CUTTER_NULL;
			*ssp = *old;		/* struct copy -- XXX SLOW! */
			bu_free( old, "old shootray_status" );
			curcut = ssp->curcut;
			continue;
		}

		/* Compute position and bail if we're outside of the
		   current level. */
		px = ap->a_ray.r_pt[X] + t0*ap->a_ray.r_dir[X];
		py = ap->a_ray.r_pt[Y] + t0*ap->a_ray.r_dir[Y];
		pz = ap->a_ray.r_pt[Z] + t0*ap->a_ray.r_dir[Z];

		/* Optimization: when it's a boxnode in a nugrid,
		   just return. */
		if( cutp->cut_type == CUT_BOXNODE &&
		    curcut->cut_type == CUT_NUGRIDNODE ) {
			ssp->newray.r_pt[X] = px;
			ssp->newray.r_pt[Y] = py;
			ssp->newray.r_pt[Z] = pz;
			ssp->newray.r_min = 0;
			ssp->newray.r_max = ssp->tv[ssp->out_axis] - t0;
			goto done;
		}
		
		if( (ssp->rstep[X] <= 0 && px < ssp->curmin[X]) ||
		    (ssp->rstep[X] >= 0 && px > ssp->curmax[X]) ||
		    (ssp->rstep[Y] <= 0 && py < ssp->curmin[Y]) ||
		    (ssp->rstep[Y] >= 0 && py > ssp->curmax[Y]) ||
		    (ssp->rstep[Z] <= 0 && pz < ssp->curmin[Z]) ||
		    (ssp->rstep[Z] >= 0 && pz > ssp->curmax[Z]) ) {
			cutp = CUTTER_NULL;
			goto test;
		}
		
		if( rt_g.debug&DEBUG_ADVANCE ) {
			bu_log(
	           "rt_advance_to_next_cell() dist_corr=%g, pt=(%g, %g, %g)\n",
				t0 /*ssp->dist_corr*/, px, py, pz );
		}

		while( cutp->cut_type == CUT_CUTNODE ) {
			switch( cutp->cn.cn_axis )  {
			case X:
				if( px >= cutp->cn.cn_point )  {
					cutp=cutp->cn.cn_r;
				}  else  {
					cutp=cutp->cn.cn_l;
				}
				break;
			case Y:
				if( py >= cutp->cn.cn_point )  {
					cutp=cutp->cn.cn_r;
				}  else  {
					cutp=cutp->cn.cn_l;
				}
				break;
			case Z:
				if( pz >= cutp->cn.cn_point )  {
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
#if EXTRA_SAFETY
			if( (ssp->rstep[X] <= 0 && px < cutp->bn.bn_min[X]) ||
			    (ssp->rstep[X] >= 0 && px > cutp->bn.bn_max[X]) ||
			    (ssp->rstep[Y] <= 0 && py < cutp->bn.bn_min[Y]) ||
			    (ssp->rstep[Y] >= 0 && py > cutp->bn.bn_max[Y]) ||
			    (ssp->rstep[Z] <= 0 && pz < cutp->bn.bn_min[Z]) ||
			    (ssp->rstep[Z] >= 0 && pz > cutp->bn.bn_max[Z]) ) {
				/* This cell is old news. */
				bu_log(
		  "rt_advance_to_next_cell(): point not in cell, advancing\n");
				if( rt_g.debug & DEBUG_ADVANCE ) {
					bu_log( " pt (%.20e,%.20e,%.20e)\n",
						px, py, pz );
					bu_log(	"  min (%.20e,%.20e,%.20e)\n",
						V3ARGS(cutp->bn.bn_min) );
					bu_log( "  max (%.20e,%.20e,%.20e)\n",
						V3ARGS(cutp->bn.bn_max) );
					bu_log( "pt=(%g,%g,%g)\n",px, py, pz );
					rt_pr_cut( cutp, 0 );
				}

				/*
				 * Move newray point further into new box.
				 * Try again.
				 */
				t0 += OFFSET_DIST;
				goto top;
			}
#endif			
			/* Don't get stuck within the same box for
			   long */
			if( cutp==ssp->lastcut ) {
				fastf_t	delta;
push:				;	
				if( rt_g.debug & DEBUG_ADVANCE ) {
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

				if( rt_g.debug & DEBUG_ADVANCE ) {
					bu_log(
						"exp=%d, fraction=%.20e\n",
						exponent, fraction );
				}
				if( sizeof(fastf_t) <= 4 )
					fraction += 1.0e-5;
				else
					fraction += 1.0e-14;
				delta = ldexp( fraction, exponent );
#define MUCHO_DIAGS	1
#if MUCHO_DIAGS
				if( rt_g.debug & DEBUG_ADVANCE ) {
					bu_log(
						"ldexp: delta=%g, fract=%g, exp=%d\n",
						delta,
						fraction,
						exponent );
				}
#endif
				/* Never advance less than 1mm */
				if( delta < 1 ) delta = 1.0;
				ssp->box_start = ssp->box_end + delta;
				ssp->box_end = ssp->box_start + delta;
				
				if( rt_g.debug & DEBUG_ADVANCE ) {
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
					cutp = CUTTER_NULL;
					break;
				}
				t0 = ssp->box_start + OFFSET_DIST;
				goto top;
			}
			if( push_flag ) {
				push_flag = 0;
				if( rt_g.debug & DEBUG_ADVANCE ) {
					bu_log(
						"%d,%d Escaped %d. dist_corr=%g, box_start=%g, box_end=%g\n",
						ap->a_x, ap->a_y,
						push_flag,
						t0 /*ssp->dist_corr*/,
						ssp->box_start,
						ssp->box_end );
				}
			}
			if( rt_g.debug & DEBUG_ADVANCE ) {
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
				goto push;
			}

done:			ssp->lastcut = cutp;
#if EXTRA_SAFETY
			/* Diagnostic purposes only */
			ssp->odist_corr = ssp->dist_corr;
			ssp->obox_start = ssp->box_start;
			ssp->obox_end = ssp->box_end;
#endif			
			ssp->dist_corr = t0;
			ssp->box_start = t0 + ssp->newray.r_min;
			ssp->box_end = t0 + ssp->newray.r_max;
			if( rt_g.debug & DEBUG_ADVANCE )  {
				bu_log(
				"rt_advance_to_next_cell() box=(%g, %g)\n",
					ssp->box_start, ssp->box_end );
			}
			return cutp;
		case CUT_NUGRIDNODE: {
			struct shootray_status *old;

			BU_GETSTRUCT( old, shootray_status );
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

	rt_bomb("rt_advance_to_next_cell: escaped for(;;) loop: impossible!");
	return CUTTER_NULL; /* not reached */

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
rt_shootray( ap )
register struct application *ap;
{
	struct shootray_status	ss;
	struct seg		new_segs;	/* from solid intersections */
	struct seg		waiting_segs;	/* awaiting rt_boolweave() */
	struct seg		finished_segs;	/* processed by rt_boolweave() */
	AUTO fastf_t		last_bool_start;
	struct bu_bitv		*solidbits;	/* bits for all solids shot so far */
	struct bu_ptbl		*regionbits;	/* table of all involved regions */
	AUTO char		*status;
	auto struct partition	InitialPart;	/* Head of Initial Partitions */
	auto struct partition	FinalPart;	/* Head of Final Partitions */
	AUTO struct soltab	**stpp;
	register CONST union cutter *cutp;
	struct resource		*resp;
	AUTO struct rt_i	*rtip;
	CONST int		debug_shoot = rt_g.debug & DEBUG_SHOOT;

	RT_AP_CHECK(ap);
	if( ap->a_resource == RESOURCE_NULL )  {
		ap->a_resource = &rt_uniresource;
		rt_uniresource.re_magic = RESOURCE_MAGIC;
		if(rt_g.debug)bu_log("rt_shootray:  defaulting a_resource to &rt_uniresource\n");
	}
	ss.ap = ap;
	rtip = ap->a_rt_i;
	RT_CK_RTI( rtip );
	resp = ap->a_resource;
	RT_RESOURCE_CHECK(resp);
	ss.resp = resp;

	if(rt_g.debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION|DEBUG_ALLHITS)) {
		bu_log_indent_delta(2);
		bu_log("\n**********shootray cpu=%d  %d,%d lvl=%d (%s)\n",
			resp->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		bu_log("Pnt (%g, %g, %g) a_onehit=%d\n",
			V3ARGS(ap->a_ray.r_pt),
			ap->a_onehit );
		VPRINT("Dir", ap->a_ray.r_dir);
	}
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

	if( rtip->needprep )
		rt_prep(rtip);

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	InitialPart.pt_magic = PT_HD_MAGIC;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;
	FinalPart.pt_magic = PT_HD_MAGIC;

	BU_LIST_INIT( &new_segs.l );
	BU_LIST_INIT( &waiting_segs.l );
	BU_LIST_INIT( &finished_segs.l );

	if( BU_LIST_UNINITIALIZED( &resp->re_parthead ) )  {
		int i;

		BU_LIST_INIT( &resp->re_parthead );

		/* If one is, they all probably are.  Runs once per processor. */
		if( BU_LIST_UNINITIALIZED( &resp->re_solid_bitv ) )
			BU_LIST_INIT(  &resp->re_solid_bitv );
		if( BU_LIST_UNINITIALIZED( &resp->re_region_ptbl ) )
			BU_LIST_INIT(  &resp->re_region_ptbl );
		if( BU_LIST_UNINITIALIZED( &resp->re_nmgfree ) )
			BU_LIST_INIT(  &resp->re_nmgfree );

		for( i=0 ; i<RT_PM_NBUCKETS ; i++ )
		{
			resp->re_pmem.buckets[i].q_forw = &resp->re_pmem.buckets[i];
			resp->re_pmem.buckets[i].q_back = &resp->re_pmem.buckets[i];
		}
		resp->re_pmem.adjhead.q_forw = &resp->re_pmem.adjhead;
		resp->re_pmem.adjhead.q_back = &resp->re_pmem.adjhead;

		/*
		 *  Add this resource structure to the table.
		 *  This is how per-cpu resource structures are discovered.
		 */
		bu_semaphore_acquire(RT_SEM_MODEL);
		bu_ptbl_ins_unique( &rtip->rti_resources, (long *)resp );
		bu_semaphore_release(RT_SEM_MODEL);
	}
	if( BU_LIST_IS_EMPTY( &resp->re_solid_bitv ) )  {
		solidbits = bu_bitv_new( rtip->nsolids );
	} else {
		solidbits = BU_LIST_FIRST( bu_bitv, &resp->re_solid_bitv );
		BU_LIST_DEQUEUE( &solidbits->l );
		BU_CK_BITV(solidbits);
		BU_BITV_NBITS_CHECK( solidbits, rtip->nsolids );
	}
	bu_bitv_clear(solidbits);

	if( BU_LIST_IS_EMPTY( &resp->re_region_ptbl ) )  {
		BU_GETSTRUCT( regionbits, bu_ptbl );
		bu_ptbl_init( regionbits, 7, "rt_shootray() regionbits ptbl" );
	} else {
		regionbits = BU_LIST_FIRST( bu_ptbl, &resp->re_region_ptbl );
		BU_LIST_DEQUEUE( &regionbits->l );
		BU_CK_PTBL(regionbits);
	}

	/* Verify that direction vector has unit length */
	if(rt_g.debug) {
		FAST fastf_t f, diff;
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
	 *  If there are infinite solids in the model,
	 *  shoot at all of them, all of the time
	 *  (until per-solid classification is implemented)
	 *  before considering the model RPP.
	 *  This code is a streamlined version of the real version.
	 */
	if( rtip->rti_inf_box.bn.bn_len > 0 )  {
		/* Consider all objects within the box */
		cutp = &(rtip->rti_inf_box);
		stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
		for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
			register struct soltab *stp = *stpp;

			/* Shoot a ray */
			if(debug_shoot)bu_log("shooting %s\n", stp->st_name);
			BU_BITSET( solidbits, stp->st_bit );
			resp->re_shots++;
			if( rt_functab[stp->st_id].ft_shot( 
			    stp, &ap->a_ray, ap, &waiting_segs ) <= 0 )  {
				resp->re_shot_miss++;
				continue;	/* MISS */
			}
			resp->re_shot_hit++;
		}
	}

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  If ray ends exactly at the model RPP, trace it.
	 */
	if( !rt_in_rpp( &ap->a_ray, ss.inv_dir, rtip->mdl_min, rtip->mdl_max )  ||
	    ap->a_ray.r_max < 0.0 )  {
	    	/* XXX it would be better to do the infinite objects here,
	    	 * XXX and let rt_advance_to_next_cell mention them otherwise,
	    	 * XXX such as when rays escape from the model RPP.
	    	 */
	    	if( BU_LIST_NON_EMPTY( &waiting_segs.l ) )  {
	    		/* Go handle the infinite objects we hit */
	    		ss.model_end = INFINITY;
	    		goto weave;
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
	 */
	ss.box_start = ss.model_start = ap->a_ray.r_min;
	ss.box_end = ss.model_end = ap->a_ray.r_max;

	if( ss.box_start < BACKING_DIST )
		ss.box_start = BACKING_DIST; /* Only look a little bit behind */

	ss.lastcut = CUTTER_NULL;
	ss.old_status = (struct shootray_status *)NULL;
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

		if( cutp->bn.bn_len <= 0 )  {
			/* Push ray onwards to next box */
			ss.box_start = ss.box_end;
			resp->re_nempty_cells++;
			continue;
		}

		/* Consider all objects within the box */
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
			if( rt_functab[stp->st_id].ft_use_rpp )  {
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
			if( rt_functab[stp->st_id].ft_shot( 
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
					BU_LIST_INSERT( &(waiting_segs.l), &(s2->l) );
				}
			}
			resp->re_shot_hit++;
		}
		if( rt_g.debug & DEBUG_ADVANCE )
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
		if( ap->a_onehit != 0 && BU_LIST_NON_EMPTY( &(waiting_segs.l) ) )  {
			int	done;

			/* Weave these segments into partition list */
			rt_boolweave( &finished_segs, &waiting_segs, &InitialPart, ap );

			/* Evaluate regions upto box_end */
			done = rt_boolfinal( &InitialPart, &FinalPart,
				last_bool_start, ss.box_end, regionbits, ap, solidbits );
			last_bool_start = ss.box_end;

			/* See if enough partitions have been acquired */
			if( done > 0 )  goto hitit;
		}

		if( ap->a_ray_length > 0.0 && ss.box_end >= ap->a_ray_length )
			goto weave;

		/* Push ray onwards to next box */
		ss.box_start = ss.box_end;
	}

	/*
	 *  Ray has finally left known space --
	 *  Weave any remaining segments into the partition list.
	 */
weave:
	if( rt_g.debug&DEBUG_ADVANCE )
		bu_log( "rt_shootray: ray has left known space\n" );
	
	if( BU_LIST_NON_EMPTY( &(waiting_segs.l) ) )  {
		rt_boolweave( &finished_segs, &waiting_segs, &InitialPart, ap );
	}

	/* finished_segs chain now has all segments hit by this ray */
	if( BU_LIST_IS_EMPTY( &(finished_segs.l) ) )  {
		ap->a_return = ap->a_miss( ap );
		status = "MISS solids";
		goto out;
	}

	/*
	 *  All intersections of the ray with the model have
	 *  been computed.  Evaluate the boolean trees over each partition.
	 */
	(void)rt_boolfinal( &InitialPart, &FinalPart, BACKING_DIST,
		rtip->rti_inf_box.bn.bn_len > 0 ? INFINITY : ss.model_end,
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
	if(rt_g.debug&DEBUG_ALLHITS) rt_pr_partitions(rtip,&FinalPart,"Parition list passed to a_hit() routine");
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
	BU_CK_PTBL(regionbits);
	BU_LIST_APPEND( &resp->re_region_ptbl, &regionbits->l );

	/*
	 *  Record essential statistics in per-processor data structure.
	 */
	resp->re_nshootray++;

	/* Terminate any logging */
	if(rt_g.debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION|DEBUG_ALLHITS))  {
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
rt_in_rpp( rp, invdir, min, max )
struct xray		*rp;
register CONST fastf_t *invdir;	/* inverses of rp->r_dir[] */
register CONST fastf_t *min;
register CONST fastf_t *max;
{
	register CONST fastf_t	*pt = &rp->r_pt[0];
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
rt_DB_rpp( rp, invdir, min, max )
register struct xray *rp;
register CONST fastf_t *invdir;	/* inverses of rp->r_dir[] */
register CONST fastf_t *min;
register CONST fastf_t *max;
{
	register CONST fastf_t *pt = &rp->r_pt[0];
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

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/* Stub function which will "similate" a call to a vector shot routine */
void
rt_vstub( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap; /* pointer to an application */
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
rt_pr_library_version()
{
	bu_log("%s", rt_version);
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
rt_add_res_stats( rtip, resp )
register struct rt_i		*rtip;
register struct resource	*resp;
{
	RT_CK_RTI( rtip );

	if( resp == RESOURCE_NULL )  resp = &rt_uniresource;
	RT_CK_RESOURCE( resp );

	rtip->rti_nrays += resp->re_nshootray;
	rtip->nmiss_model += resp->re_nmiss_model;

	rtip->nshots += resp->re_shots;
	rtip->nhits += resp->re_shot_hit;
	rtip->nmiss += resp->re_shot_miss;

	rtip->nmiss_solid += resp->re_prune_solrpp;

	rtip->ndup += resp->re_ndup;
	rtip->nempty_cells += resp->re_nempty_cells;

	/* Zero out resource totals, so repeated calls are not harmful */
	resp->re_nshootray = 0;
	resp->re_nmiss_model = 0;

	resp->re_shots = 0;
	resp->re_shot_hit = 0;
	resp->re_shot_miss = 0;

	resp->re_prune_solrpp = 0;

	resp->re_ndup = 0;
	resp->re_nempty_cells = 0;
}

/*
 *  Routines for plotting the progress of one ray through the model.  -Mike
 */
void
rt_3move_raydist( fp, rayp, dist )
FILE		*fp;
struct xray	*rayp;
double		dist;
{
	point_t	p;

	VJOIN1( p, rayp->r_pt, dist, rayp->r_dir );
	pdv_3move( fp, p );
}

void
rt_3cont_raydist( fp, rayp, dist )
FILE		*fp;
struct xray	*rayp;
double		dist;
{
	point_t	p;

	VJOIN1( p, rayp->r_pt, dist, rayp->r_dir );
	pdv_3cont( fp, p );
}

/*
		rt_plot_cell( cutp, &ss, &(waiting_segs.l), rtip);
 */
void
rt_plot_cell( cutp, ssp, waiting_segs_hd, rtip )
union cutter		*cutp;
struct shootray_status	*ssp;
struct bu_list		*waiting_segs_hd;
struct rt_i		*rtip;
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

	/* Plot every solid listed in this cell */
	stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
	for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
		register struct soltab *stp = *stpp;

		rt_plot_solid( fp, rtip, stp );
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
