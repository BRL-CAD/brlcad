
/*
 *			E B M . C
 *
 *  Purpose -
 *	Intersect a ray with an Extruded Bitmap,
 *	where the bitmap is taken from a bw(5) file.
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
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSebm[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"
#include "./fixpt.h"

struct ebm_specific {
	char		*ebm_file;
	unsigned char	*ebm_map;
	int		ebm_dim[2];
	double		ebm_tallness;
	vect_t		ebm_xnorm;	/* ideal +X normal in final orientation */
	vect_t		ebm_ynorm;
	vect_t		ebm_znorm;
};
#define BM_NULL		((struct ebm_specific *)0)


struct seg		*ebm_shot();
struct ebm_specific	*ebm_readin();
struct seg		*ebm_dda();
struct seg		*rt_seg_planeclip();

/*
 *  Codes to represent surface normals.
 *  In a bitmap, there are only 4 possible normals.
 *  With this code, reverse the sign to reverse the direction.
 *  As always, the normal is expected to point outwards.
 */
#define NORM_ZPOS	3
#define NORM_YPOS	2
#define NORM_XPOS	1
#define NORM_XNEG	(-1)
#define NORM_YNEG	(-2)
#define NORM_ZNEG	(-3)

/*
 *  Normal addressing is used:  (0..W-1, 0..N-1),
 *  but the bitmap is stored with two cell of zeros all around,
 *  so permissible subscripts run (-2..W+1, -2..N+1).
 *  This eliminates special-case code for the boundary conditions.
 */
#define	BIT_XWIDEN	2
#define	BIT_YWIDEN	2
#define BIT(xx,yy)	ebmp->ebm_map[((yy)+BIT_YWIDEN)*(ebmp->ebm_dim[X] + \
				BIT_XWIDEN*2)+(xx)+BIT_XWIDEN]

/*
 *			R T _ S E G _ P L A N E C L I P
 *
 *  Take a segment chain, in sorted order (ascending hit_dist),
 *  and clip to the range (in, out) along the normal "out_norm".
 *  For the particular ray "rp", find the parametric distances:
 *	kmin is the minimum permissible parameter, "in" units away
 *	kmax is the maximum permissible parameter, "out" units away
 *
 *  Returns -
 *	segp		OK: trimmed segment chain, still in sorted order
 *	SEG_NULL	ERROR
 */
struct seg *
rt_seg_planeclip( segp, out_norm, in, out, rp, ap )
struct seg	*segp;
vect_t		out_norm;
fastf_t		in, out;
struct xray	*rp;
struct application *ap;
{
	fastf_t		norm_dist_min, norm_dist_max;
	fastf_t		slant_factor;
	fastf_t		kmin, kmax;
	vect_t		in_norm;
	register struct seg	*curr, *next, *outseg, *lastout;
	int		out_norm_code;

	norm_dist_min = in - VDOT( rp->r_pt, out_norm );
	slant_factor = VDOT( rp->r_dir, out_norm );	/* always abs < 1 */
	if( NEAR_ZERO( slant_factor, SQRT_SMALL_FASTF ) )  {
		if( norm_dist_min < 0.0 )  {
			rt_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
			/* Free segp chain */
			return(SEG_NULL);
		}
		kmin = -INFINITY;
	} else
		kmin =  norm_dist_min / slant_factor;

	VREVERSE( in_norm, out_norm );
	norm_dist_max = out - VDOT( rp->r_pt, in_norm );
	slant_factor = VDOT( rp->r_dir, in_norm );	/* always abs < 1 */
	if( NEAR_ZERO( slant_factor, SQRT_SMALL_FASTF ) )  {
		if( norm_dist_max < 0.0 )  {
			rt_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
			/* Free segp chain */
			return(SEG_NULL);
		}
		kmax = INFINITY;
	} else
		kmax =  norm_dist_max / slant_factor;

	if( kmin > kmax )  {
		/* Not sure why this happens;  for now, just exchange them */
		slant_factor = kmax;
		kmax = kmin;
		kmin = slant_factor;
		out_norm_code = NORM_ZPOS;
	} else {
		out_norm_code = NORM_ZNEG;
	}
	if(rt_g.debug&DEBUG_EBM)rt_log("kmin=%g, kmax=%g, out_norm_code=%d\n", kmin, kmax, out_norm_code );

	curr = segp;
	lastout = outseg = SEG_NULL;
	for( curr = segp; curr != SEG_NULL; curr = next )  {
		if(rt_g.debug&DEBUG_EBM)rt_log(" rt_seg_planeclip seg( %g, %g )\n", curr->seg_in.hit_dist, curr->seg_out.hit_dist );
		next = curr->seg_next;
		if( curr->seg_out.hit_dist <= kmin )  {
			if(rt_g.debug&DEBUG_EBM)rt_log("seg_out %g <= kmin %g, freeing\n", curr->seg_out.hit_dist, kmin );
			FREE_SEG(curr, ap->a_resource);
			continue;
		}
		if( curr->seg_in.hit_dist >= kmax )  {
			if(rt_g.debug&DEBUG_EBM)rt_log("seg_in  %g >= kmax %g, freeing\n", curr->seg_in.hit_dist, kmax );
			FREE_SEG(curr, ap->a_resource);
			continue;
		}
		if( curr->seg_in.hit_dist <= kmin )  {
			if(rt_g.debug&DEBUG_EBM)rt_log("seg_in = kmin %g\n", kmin );
			curr->seg_in.hit_dist = kmin;
			curr->seg_in.hit_private = (char *)out_norm_code;
		}
		if( curr->seg_out.hit_dist >= kmax )  {
			if(rt_g.debug&DEBUG_EBM)rt_log("seg_out= kmax %g\n", kmax );
			curr->seg_out.hit_dist = kmax;
			curr->seg_out.hit_private = (char *)(-out_norm_code);
		}
		curr->seg_next = SEG_NULL;
		if( lastout == SEG_NULL )  {
			outseg = curr;
			lastout = outseg;
		} else {
			lastout->seg_next = curr;
			lastout = curr;
		}
	}
	return( outseg );
}


/*
 *			E B M _ D D A
 *
 *  Solve the 2-D intersection by stepping through the bit array.
 *
 *  To solve an intercept, where point[axis] == 0:
 *  Using P + k * D formulation of line, solve
 *  P[axis] + k * D[axis] = 0;
 *  k = -P[axis]/D[axis].
 *
 *
 */
struct seg_head {
	struct seg	*sh_head;
	struct seg	*sh_tail;
	struct soltab	*sh_stp;
	struct application *sh_ap;
};

struct seg *
ebm_dda( rp, ebmp, stp, ap )
register struct xray	*rp;
register struct ebm_specific *ebmp;
struct soltab		*stp;
struct application	*ap;
{
	fastf_t		maj_entry_k, min_entry_k;
	fastf_t		maj_exit_k, min_exit_k;
	struct fixpt	maj_pos_deltas[2], min_pos_deltas[2];
	struct fixpt	maj_exit_pos[2], min_exit_pos[2];
	struct fixpt	maj_entry_pos[2], min_entry_pos[2];
	fastf_t		maj_k_delta, min_k_delta;
	int		maj_entry_norm, min_entry_norm;
	int		majdir, mindir;
	int		minor_has_deltas;
	int		inside = 0;
	int		do_minor_backup;
	struct seg_head	head;

	head.sh_head = head.sh_tail = SEG_NULL;
	head.sh_stp = stp;
	head.sh_ap = ap;

	/* Determine major and minor axes */
	if( fabs(rp->r_dir[X]) > fabs(rp->r_dir[Y]) )  {
		majdir = X;
		mindir = Y;
		if( rp->r_dir[X] > 0 )
			maj_entry_norm = NORM_XNEG;	/* points outwards */
		else	maj_entry_norm = NORM_XPOS;
		if( rp->r_dir[Y] > 0 )
			min_entry_norm = NORM_YNEG;
		else	min_entry_norm = NORM_YPOS;
	} else {
		majdir = Y;
		mindir = X;
		if( rp->r_dir[Y] > 0 )
			maj_entry_norm = NORM_YNEG;
		else	maj_entry_norm = NORM_YPOS;
		if( rp->r_dir[X] > 0 )
			min_entry_norm = NORM_XNEG;
		else	min_entry_norm = NORM_XPOS;
	}

	/*  First, find the natural starting point on the trimmed ray.
	 *  Use it as the start point for both major and minor entry_pos
	 */
	maj_entry_k = rp->r_min;
	maj_k_delta = fabs(1.0/rp->r_dir[majdir]);
	FIXPT_FLOAT( maj_entry_pos[X], rp->r_pt[X] + maj_entry_k * rp->r_dir[X] );
	FIXPT_FLOAT( maj_entry_pos[Y], rp->r_pt[Y] + maj_entry_k * rp->r_dir[Y] );
	min_entry_pos[X] = maj_entry_pos[X];	/* struct copy */
	min_entry_pos[Y] = maj_entry_pos[Y];	/* struct copy */
	if(rt_g.debug&DEBUG_EBM) PR_FIX2("original maj_entry_pos", maj_entry_pos);

	/*
	 * The major entry point MUST be a major axis intercept,
	 * force maj_entry_pos[majdir] == integer.
	 */
	if( maj_entry_pos[majdir].f != 0 )  {
		do_minor_backup = 1;
		if( rp->r_dir[majdir] < 0 )  {
			maj_entry_pos[majdir].i++;		/* back up */
		} else {
			if( maj_entry_pos[majdir].i <= 0 )
				maj_entry_pos[majdir].i--;	/* back up */
			/* else, just truncation to int will do the trick */
		}
		maj_entry_pos[majdir].f = 0;			/* truncate to int */

		maj_entry_k = (maj_entry_pos[majdir].i - rp->r_pt[majdir]) / rp->r_dir[majdir];
		FIXPT_FLOAT( maj_entry_pos[mindir],
			rp->r_pt[mindir] + maj_entry_k * rp->r_dir[mindir] );
		if(rt_g.debug&DEBUG_EBM) PR_FIX2("actual maj_entry_pos", maj_entry_pos);
	} else {
		do_minor_backup = 0;
	}

	maj_pos_deltas[majdir].i = rp->r_dir[majdir] < 0.0 ? -1 : 1;
	maj_pos_deltas[majdir].f = 0;
	FIXPT_FLOAT( maj_pos_deltas[mindir],
		rp->r_dir[mindir]/fabs(rp->r_dir[majdir]) );	/* should be <= 1 */
	if(rt_g.debug&DEBUG_EBM) PR_FIX2( "maj_pos_deltas", maj_pos_deltas );

	/*
	 *  Find the first minor axis intercept before the start position,
	 *  to "prime the pumps" in the minor direction.
	 */
	min_entry_k = rp->r_min;
	if( maj_pos_deltas[mindir].i != 0 ||
	    maj_pos_deltas[mindir].f != 0 )  {
	    	minor_has_deltas = 1;
		min_k_delta = fabs(1.0/rp->r_dir[mindir]);
		/*
		 *  The minor entry point MUST be on a minor axis intercept,
	    	 *  force min_entry_pos[mindir] == integer
	    	 */
	    	if( min_entry_pos[mindir].f != 0 || do_minor_backup)  {
			if( rp->r_dir[mindir] < 0 )  {
				min_entry_pos[mindir].i++;	/* back up */
			} else {
				if( min_entry_pos[mindir].i <= 0 )
					min_entry_pos[mindir].i--;	/* back up */
				/* else, just truncation to int will do the trick */
			}
			min_entry_pos[mindir].f = 0;		/* truncate to int */

			min_entry_k = (min_entry_pos[mindir].i - rp->r_pt[mindir]) / rp->r_dir[mindir];
			FIXPT_FLOAT( min_entry_pos[majdir],
				rp->r_pt[majdir] + min_entry_k * rp->r_dir[majdir] );
	    	}
		if(rt_g.debug&DEBUG_EBM) PR_FIX2( "min_entry_pos", min_entry_pos );

		min_pos_deltas[mindir].i = rp->r_dir[mindir] < 0.0 ? -1 : 1;
		min_pos_deltas[mindir].f = 0;
		FIXPT_FLOAT( min_pos_deltas[majdir],
			rp->r_dir[majdir]/fabs(rp->r_dir[mindir]) );	/* may be > 1 */
		if(rt_g.debug&DEBUG_EBM) PR_FIX2( "min_pos_deltas", min_pos_deltas );

		/* Take first minor step already, to have exit dist to check, below */
		FIXPT_ADD2( min_exit_pos[X], min_entry_pos[X], min_pos_deltas[X] );
		FIXPT_ADD2( min_exit_pos[Y], min_entry_pos[Y], min_pos_deltas[Y] );
	} else {
	    	minor_has_deltas = 0;
		min_k_delta = 0;
	}
	min_exit_k = min_entry_k + min_k_delta;
	if(rt_g.debug&DEBUG_EBM)  {
		rt_log("maj_k_delta=%g, min_k_delta=%g, major_axis=%s\n", maj_k_delta, min_k_delta, majdir==X ? "X" : "Y");
		rt_log("maj_entry_k = %g, min_entry_k = %g\n", maj_entry_k, min_entry_k );
	}

	/* Take first major step */
	FIXPT_ADD2( maj_exit_pos[X], maj_entry_pos[X], maj_pos_deltas[X] );
	FIXPT_ADD2( maj_exit_pos[Y], maj_entry_pos[Y], maj_pos_deltas[Y] );
	maj_exit_k = maj_entry_k + maj_k_delta;

	/*  See if a silent minor step is needed to get past
	 *  the starting K for the major axis.
	 */
	if( minor_has_deltas )  {
		while( min_exit_k < maj_entry_k )  {
			if(rt_g.debug&DEBUG_EBM) rt_log("catch-up minor step\n");
			/* Advance minor exit to some future cell */
			min_entry_pos[X] = min_exit_pos[X];		/* struct copy */
			min_entry_pos[Y] = min_exit_pos[Y];		/* struct copy */
			FIXPT_ADD2( min_exit_pos[X], min_entry_pos[X], min_pos_deltas[X] );
			FIXPT_ADD2( min_exit_pos[Y], min_entry_pos[Y], min_pos_deltas[Y] );
			min_entry_k += min_k_delta;
			min_exit_k += min_k_delta;
		}
	}

	while( inside >= 0 )  {
		if(rt_g.debug&DEBUG_EBM) rt_log("maj_exit_k = %g, min_exit_k = %g\n", maj_exit_k, min_exit_k );

		/*
		 *  First, do any minor direction exit step
		 *  which has become "due".
		 *  Minor direction steps usually happen once for
		 *  every several major direction steps (depending on slope).
		 */
		if( minor_has_deltas && min_exit_k < maj_exit_k )  {
		    	fastf_t	exit_k_diff;
			fastf_t	maj_dist;

			/*
			 * If either the entry or exit points (K values)
			 * are nearly equal, this is a shared
			 * major/minor vertex, don't bother doing
			 * a minor step partitioning in this cell,
			 * but DO push min_exit_pos along.
			 * All shared major+minor vertices should occur
			 * exactly on integer-valued grid coordinates;
			 * this is enforced.
			 */
#define VERT_TOL	(1.0e-10)
			exit_k_diff = maj_exit_k - min_exit_k;
			if(rt_g.debug&DEBUG_EBM)  {
				PR_FIX2("maj_entry_pos", maj_entry_pos);
				PR_FIX2("min_entry_pos", min_entry_pos);
				PR_FIX2("maj_exit_pos ", maj_exit_pos);
				PR_FIX2("min_exit_pos ", min_exit_pos);
				rt_log("exit_k_diff = %e\n", exit_k_diff);
			}
			if( exit_k_diff > -VERT_TOL &&
				   exit_k_diff <  VERT_TOL )  {
				/*  Exit Vertex overlap -
				 *  Do it as a regular major exit.
				 *  Force positions to exactly integer
				 *  values -- this is mandatory for
				 *  correct results.
				 */
if(rt_g.debug&DEBUG_EBM) rt_log("MINOR STEP SKIPPED -- exit vertex overlap\n");
				  
				FIXPT_ROUND( maj_exit_pos[X] );
				FIXPT_ROUND( maj_exit_pos[Y] );
				FIXPT_ROUND( min_exit_pos[X] );
				FIXPT_ROUND( min_exit_pos[Y] );
				inside = ebm_do_exit( maj_entry_k, maj_exit_k,
					maj_entry_pos, maj_exit_pos,
					rp, inside, ebmp,
					maj_entry_norm, &head );
			} else {
				if(rt_g.debug&DEBUG_EBM) rt_log("MINOR STEP\n");
				/*
			    	 *  Take a step in the minor direction.
				 *  Ensure that this step is not going
				 *  backwards along the major axis,
			    	 *  due to slight differences in the slopes
			    	 */
			    	maj_dist =
				    (FLOAT_FIXPT(min_exit_pos[majdir]) -
			    	    FLOAT_FIXPT(maj_entry_pos[majdir]) ) *
				    ( rp->r_dir[majdir] < 0.0 ? -1 : 1 );
				if(rt_g.debug&DEBUG_EBM) rt_log("maj_dist = %g\n", maj_dist);
#define DIFF_TOL	7.0/FIXPT_SCALE
			    	if( maj_dist < 0 )  {
			    		if( maj_dist < -DIFF_TOL )  {
rt_log("??? major element heading wrong way, maj_dist=%g (%d) ???\n", maj_dist, ((int)(maj_dist*FIXPT_SCALE)) );
			    		}
			    		/* Fix it up */
			    	    	min_exit_pos[majdir] = maj_entry_pos[majdir];	/* struct copy */
			    	}
				inside = ebm_do_exit( maj_entry_k, min_exit_k,
					maj_entry_pos, min_exit_pos,
					rp, inside, ebmp,
					maj_entry_norm, &head );
			    	if( inside < 0 )  break;
				inside = ebm_do_exit( min_exit_k, maj_exit_k,
					min_exit_pos, maj_exit_pos,
					rp, inside, ebmp,
					min_entry_norm, &head );
			}

			/* Advance minor exit to some future cell */
			min_entry_pos[X] = min_exit_pos[X];		/* struct copy */
			min_entry_pos[Y] = min_exit_pos[Y];		/* struct copy */
			FIXPT_ADD2( min_exit_pos[X], min_entry_pos[X], min_pos_deltas[X] );
			FIXPT_ADD2( min_exit_pos[Y], min_entry_pos[Y], min_pos_deltas[Y] );
			min_entry_k += min_k_delta;
			min_exit_k += min_k_delta;
		} else {
			/* Take regular major direction step */
			inside = ebm_do_exit( maj_entry_k, maj_exit_k,
				maj_entry_pos, maj_exit_pos,
				rp, inside, ebmp,
				maj_entry_norm, &head );
		}

		/* Loop bottom -- press on to next major direction cell */
		maj_entry_pos[X] = maj_exit_pos[X];		/* struct copy */
		maj_entry_pos[Y] = maj_exit_pos[Y];		/* struct copy */
		FIXPT_ADD2( maj_exit_pos[X], maj_entry_pos[X], maj_pos_deltas[X] );
		FIXPT_ADD2( maj_exit_pos[Y], maj_entry_pos[Y], maj_pos_deltas[Y] );
		maj_entry_k += maj_k_delta;
		maj_exit_k += maj_k_delta;
	}
	return(head.sh_head);
}

/*
 *			E B M _ D O _ E X I T
 */
int
ebm_do_exit( entry_k, exit_k, entry_pos, exit_pos, rp, inside, ebmp, entry_norm, headp )
double		entry_k, exit_k;
struct fixpt	*entry_pos, *exit_pos;
struct xray	*rp;
int		inside;
register struct ebm_specific *ebmp;
int		entry_norm;
struct seg_head	*headp;
{
	int	test[2];
	int	val;
	struct seg *segp = SEG_NULL;

	if(rt_g.debug&DEBUG_EBM)  {
		rt_log("ebm_do_exit(%g,%g) inside=%d\n", entry_k, exit_k, inside );
		PR_FIX2( " entry", entry_pos );
		PR_FIX2( "  exit", exit_pos );
	}

	/*  Determine proper cell index.
	 *  If the position is exactly a positive integer (no fractional part)
	 *  then apply the subscript correction.
	 *  If direction is negative, bitmap index needs correction,
	 *  because intercept point will be on top or right edge of cell,
	 *  resulting in round-up to next largest integer.
	 */
	if( entry_pos[X].f != 0 || entry_pos[X].i < 0 || rp->r_dir[X] >= 0 )
		test[X] = entry_pos[X].i;
	else	test[X] = entry_pos[X].i - 1;
	if( entry_pos[Y].f != 0 || entry_pos[Y].i < 0 || rp->r_dir[Y] >= 0 )
		test[Y] = entry_pos[Y].i;
	else	test[Y] = entry_pos[Y].i - 1;

	/*
	 *  Sitting on first ring of zero cells is OK,
	 *  but the outer ring of zeros is time to quit.
	 *  The inner ring should prevent it from ever happening.
	 */
	if( test[X] < 0 ||
	    test[X] >= ebmp->ebm_dim[X] ||
	    test[Y] < 0 ||
	    test[Y] >= ebmp->ebm_dim[Y] )  {

		if( test[X] < -2 ||
		    test[X] > ebmp->ebm_dim[X]+1 ||
		    test[Y] < -2 ||
		    test[Y] > ebmp->ebm_dim[Y]+1 )  {
		    	if( !inside )
		    		return( -1 );		/* OK, but time to stop */
		}

		/*
		 *  DDA ran off the edge of the bitmap,
		 *  perhaps while still "inside" a solid.
		 *  Just print up an empty (zerp valued) cell,
		 *  and return the -1 next time through.
		 *  This accomodates the initial intercepts shifting
		 *  the ray start positions outside the normal bounds.
		 */
		val = 0;
	} else {
		val = BIT( test[X], test[Y] );
	}

	/* Check cell value */
	if(rt_g.debug&DEBUG_EBM) rt_log(" .....TEST bit (%d, %d) val=%d\n",
		test[X], test[Y], val );

	if( val )  {
		/* New cell is solid */
		if( !inside )  {
			/* Handle the transition from vacuum to solid */
			inside = 1;

			GET_SEG(segp, headp->sh_ap->a_resource);
			segp->seg_stp = headp->sh_stp;
			segp->seg_in.hit_dist = entry_k;
			segp->seg_in.hit_private = (char *)entry_norm;

			if( headp->sh_head == SEG_NULL ||
			    headp->sh_tail == SEG_NULL )  {
				/* Install as beginning of list */
				headp->sh_tail = segp;
				segp->seg_next = headp->sh_head;
				headp->sh_head = segp;
			} else {
				/* Append to list */
				headp->sh_tail->seg_next = segp;
				headp->sh_tail = segp;
			}

			if(rt_g.debug&DEBUG_EBM) rt_log("START k=%g, n=%d\n", entry_k, entry_norm);
		}
	} else {
		/* New cell is empty */
		if( inside )  {
			/* Handle transition from solid to vacuum */
			inside = 0;

			headp->sh_tail->seg_out.hit_dist = entry_k;
			headp->sh_tail->seg_out.hit_private = (char *)(-entry_norm);
			if(rt_g.debug&DEBUG_EBM) rt_log("END k=%g, n=%d\n", entry_k, -entry_norm);
		}
	}
	return(inside);
}

/*
 *			E B M _ R E A D I N
 */
HIDDEN struct ebm_specific *
ebm_readin( rp )
union record	*rp;
{
	register struct ebm_specific *ebmp;
	FILE	*fp;
	int	nbytes;
	register int	y;

	/* tokenize the string in the solid record ?? */
	/* Perhaps just find the first WSp char */
	/* parse each assignment w/library routine */

	GETSTRUCT( ebmp, ebm_specific );
	ebmp->ebm_file = "bm.bw";
	ebmp->ebm_dim[X] = 6;
	ebmp->ebm_dim[Y] = 6;
	ebmp->ebm_tallness = 6.0;

	/* Get bit map from .bw(5) file */
	nbytes = (ebmp->ebm_dim[X]+BIT_XWIDEN*2)*(ebmp->ebm_dim[Y]+BIT_YWIDEN*2);
	ebmp->ebm_map = (unsigned char *)rt_malloc( nbytes, "ebm_readin bitmap" );
#ifdef SYSV
	memset( ebmp->ebm_map, 0, nbytes );
#else
	bzero( ebmp->ebm_map, nbytes );
#endif
	if( (fp = fopen(ebmp->ebm_file, "r")) == NULL )  {
		perror(ebmp->ebm_file);
		return( BM_NULL );
	}
	for( y=0; y < ebmp->ebm_dim[Y]; y++ )
		(void)fread( &BIT(0, y), ebmp->ebm_dim[X], 1, fp );
	fclose(fp);
	return( ebmp );
}

/*
 *			E B M _ P R E P
 *
 *  Returns -
 *	0	OK
 *	!0	Failure
 *
 *  Implicit return -
 *	A struct ebm_specific is created, and it's address is stored
 *	in stp->st_specific for use by ebm_shot().
 */
int
ebm_prep( stp, rp, rtip )
struct soltab	*stp;
union record	*rp;
struct rt_i	*rtip;
{
	register struct ebm_specific *ebmp;
	vect_t	norm;
	vect_t	radvec;

	if( (ebmp = ebm_readin( rp )) == BM_NULL )
		return(-1);	/* ERROR */

	/* build Xform matrix to ideal space */

	/* Pre-compute the necessary normals */
	VSET( norm, 1, 0 , 0 );
	MAT4X3VEC( ebmp->ebm_xnorm, stp->st_pathmat, norm );
	VSET( norm, 0, 1, 0 );
	MAT4X3VEC( ebmp->ebm_ynorm, stp->st_pathmat, norm );
	VSET( norm, 0, 0, 1 );
	MAT4X3VEC( ebmp->ebm_znorm, stp->st_pathmat, norm );

	stp->st_specific = (int *)ebmp;

	/* This needs to account for the rotations */
	VSETALL( stp->st_min, 0 );
	VSET( stp->st_max, ebmp->ebm_dim[X], ebmp->ebm_dim[Y], ebmp->ebm_tallness );

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( radvec, stp->st_min, stp->st_max, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

	return(0);		/* OK */
}

/*
 *			E B M _ P R I N T
 */
void
ebm_print( stp )
register struct soltab	*stp;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;

}

/*
 *			E B M _ S H O T
 *
 *  Intersect a ray with an extruded bitmap.
 *  If intersection occurs, a pointer to a sorted linked list of
 *  "struct seg"s will be returned.
 *
 *  Returns -
 *	0	MISS
 *	segp	HIT
 */
struct seg *
ebm_shot( stp, rp, ap )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;
	struct seg	*segp;
	vect_t		norm;

	segp = ebm_dda( rp, ebmp, stp, ap );

	VSET( norm, 0, 0, -1 );		/* letters grow in +z, which is "inside" the halfspace */
	segp = rt_seg_planeclip( segp, norm, 0.0, ebmp->ebm_tallness, rp, ap );
	return(segp);
}

/*
 *			E B M _ N O R M
 *
 *  Given one ray distance, return the normal and
 *  entry/exit point.
 *  This is mostly a matter of translating the stored
 *  code into the proper normal.
 */
void
ebm_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	switch( (int) hitp->hit_private )  {
	case NORM_XPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_xnorm );
		break;
	case NORM_XNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_xnorm );
		break;

	case NORM_YPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_ynorm );
		break;
	case NORM_YNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_ynorm );
		break;

	case NORM_ZPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_znorm );
		break;
	case NORM_ZNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_znorm );
		break;

	default:
		rt_log("ebm_norm(%s): norm code %d bad\n",
			stp->st_name, hitp->hit_private );
		VSETALL( hitp->hit_normal, 0 );
		break;
	}
}

/*
 *			E B M _ C U R V E
 *
 *  Everything has sharp edges.  This makes things easy.
 */
void
ebm_curve( cvp, hitp, stp )
register struct curvature	*cvp;
register struct hit		*hitp;
struct soltab			*stp;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;
}

/*
 *			E B M _ U V
 *
 *  Map the hit point in 2-D into the range 0..1
 *  untransformed X becomes U, and Y becomes V.
 */
void
ebm_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;
}

/*
 * 			E B M _ F R E E
 */
void
ebm_free( stp )
struct soltab	*stp;
{
	register struct ebm_specific *ebmp =
		(struct ebm_specific *)stp->st_specific;

	rt_free( (char *)ebmp->ebm_map, "ebm_map" );
	rt_free( (char *)ebmp, "ebm_specific" );
}

int
ebm_class()
{
	return(0);
}

/*
 *			E B M _ P L O T
 */
void
ebm_plot( rp, matp, vhead, dp )
union record	*rp;
register matp_t	matp;
struct vlhead	*vhead;
struct directory *dp;
{
	register struct ebm_specific *ebmp;
	register int	x,y;
	register int	following;
	register int	base;

	if( (ebmp = ebm_readin( rp )) == BM_NULL )
		return;

	/* Find vertical lines */
	base = 0;	/* lint */
	for( x=0; x <= ebmp->ebm_dim[X]; x++ )  {
		following = 0;
		for( y=0; y <= ebmp->ebm_dim[Y]; y++ )  {
			if( following )  {
				if( BIT( x-1, y ) != BIT( x, y ) )
					continue;
				ebm_plate( x, base, x, y, ebmp->ebm_tallness, vhead );
				following = 0;
			} else {
				if( BIT( x-1, y ) == BIT( x, y ) )
					continue;
				following = 1;
				base = y;
			}
		}
	}

	/* Find horizontal lines */
	for( y=0; y <= ebmp->ebm_dim[Y]; y++ )  {
		following = 0;
		for( x=0; x <= ebmp->ebm_dim[X]; x++ )  {
			if( following )  {
				if( BIT( x, y-1 ) != BIT( x, y ) )
					continue;
				ebm_plate( base, y, x, y, ebmp->ebm_tallness, vhead );
				following = 0;
			} else {
				if( BIT( x, y-1 ) == BIT( x, y ) )
					continue;
				following = 1;
				base = x;
			}
		}
	}
	rt_free( (char *)ebmp->ebm_map, "ebm_map" );
	rt_free( (char *)ebmp, "ebm_specific" );
}

/* either x1==x2, or y1==y2 */
ebm_plate( x1, y1, x2, y2, t, vhead )
double	t;
struct vlhead	*vhead;
{
	point_t	s, p;

	VSET( s, x1, y1, 0.0 );
	ADD_VL( vhead, s, 0 );

	VSET( p, x1, y1, t );
	ADD_VL( vhead, p, 1 );

	VSET( p, x2, y2, t );
	ADD_VL( vhead, p, 1 );

	p[Z] = 0;
	ADD_VL( vhead, p, 1 );

	ADD_VL( vhead, s, 1 );
}

/******** Test Driver *********/
#ifdef TEST_DRIVER

FILE			*plotfp;

struct soltab		Tsolid;
struct application	Tappl;
struct ebm_specific	*bmsp;
struct resource		resource;

main( argc, argv )
int	argc;
char	**argv;
{
	point_t	pt1;
	point_t	pt2;
	register int	x, y;
	fastf_t		xx, yy;
	mat_t		mat;
	register struct ebm_specific	*ebmp;

	if( argc > 1 )
		rt_g.debug |= DEBUG_EBM;

	plotfp = fopen( "ebm.pl", "w" );

	Tsolid.st_name = "Tsolid";
	Tappl.a_purpose = "testing";
	Tappl.a_resource = &resource;
	mat_idn( Tsolid.st_pathmat );

	if( ebm_prep( &Tsolid, 0, 0 ) != 0 )  {
		printf("prep failed\n");
		exit(1);
	}
	ebmp = bmsp = (struct ebm_specific *)Tsolid.st_specific;

	outline();

#if 0
	for( y=0; y<=ebmp->ebm_dim[Y]; y++ )  {
		for( x=0; x<=ebmp->ebm_dim[X]; x++ )  {
			VSET( pt1, 0, y, 2 );
			VSET( pt2, x, 0, 4 );
			trial( pt1, pt2 );
		}
	}
#endif
#if 0
	for( y=0; y<=ebmp->ebm_dim[Y]; y++ )  {
		for( x=0; x<=ebmp->ebm_dim[X]; x++ )  {
			VSET( pt1, ebmp->ebm_dim[X], y, 2 );
			VSET( pt2, x, ebmp->ebm_dim[Y], 4 );
			trial( pt1, pt2 );
		}
	}
#endif

#if 0
	for( yy=0; yy<=ebmp->ebm_dim[Y]; yy += 0.3 )  {
		for( xx=0; xx<=ebmp->ebm_dim[X]; xx += 0.3 )  {
			VSET( pt1, 0, yy, 2 );
			VSET( pt2, xx, 0, 4 );
			trial( pt1, pt2 );
		}
	}
#endif
#if 0
	for( yy=0; yy<=ebmp->ebm_dim[Y]; yy += 0.3 )  {
		for( xx=0; xx<=ebmp->ebm_dim[X]; xx += 0.3 )  {
			VSET( pt1, ebmp->ebm_dim[X], yy, 2 );
			VSET( pt2, xx, ebmp->ebm_dim[Y], 4 );
			trial( pt1, pt2 );
		}
	}
#endif

#if 0
	/* (6,5) (2,5) (3,4) (1.2,1.2)
	 * were especially troublesome */
	xx=6;
	yy=1.5;
	VSET( pt1, 0, yy, 2 );
	VSET( pt2, xx, 0, 4 );
	trial( pt1, pt2 );
#endif

#if 0
	/* (1,2) (3,2) (0,2) (0,0.3)
	 * were especially troublesome */
	xx=0;
	yy=0.3;
	VSET( pt1, ebmp->ebm_dim[X], yy, 2 );
	VSET( pt2, xx, ebmp->ebm_dim[Y], 4 );
	trial( pt1, pt2 );
#endif

#if 0
	VSET( pt1, 0, 1.5, 2 );
	VSET( pt2, 4.75, 6, 4 );
	trial( pt1, pt2 );

	VSET( pt1, 0, 2.5, 2 );
	VSET( pt2, 4.5, 0, 4 );
	trial( pt1, pt2 );
#endif


#if 0
/**	for( x=0; x<ebmp->ebm_dim[X]; x++ )  { **/
	VSET( pt1, 0.75, 1.1, -10 );
	{
		for( yy=0; yy<=ebmp->ebm_dim[Y]; yy += 0.3 )  {
			for( xx=0; xx<=ebmp->ebm_dim[X]; xx += 0.3 )  {
				VSET( pt2, xx, yy, 4 );
				trial( pt1, pt2 );
			}
		}
	}
#endif
#if 1
	for( x=0; x<ebmp->ebm_dim[X]; x++ )  {
		VSET( pt1, x+0.75, 1.1, -10 );
		VSET( pt2, x+0.75, 1.1, 4 );
		trial( pt1, pt2 );
	}
#endif

	exit(0);
}

trial(p1, p2)
vect_t	p1, p2;
{
	struct seg	*segp, *next;
	fastf_t		lastk;
	vect_t		minpt, maxpt;
	vect_t		inv_dir;
	struct xray	ray;
	register struct ebm_specific *ebmp = bmsp;

	VMOVE( ray.r_pt, p1 );
	VSUB2( ray.r_dir, p2, p1 );
	if( MAGNITUDE( ray.r_dir ) < 1.0e-10 )
		return;
	VUNITIZE( ray.r_dir );

	printf("------- (%g, %g, %g) to (%g, %g, %g), dir=(%g, %g, %g)\n",
		ray.r_pt[X], ray.r_pt[Y], ray.r_pt[Z],
		p2[X], p2[Y], p2[Z],
		ray.r_dir[X], ray.r_dir[Y], ray.r_dir[Z] );


	/*
	 *  Clip ray to bounding RPP of bitmap volume.
	 *  In real version, this would be done by rt_shootray().
	 */
	VSET( minpt, 0, 0, -INFINITY );
	VSET( maxpt, ebmp->ebm_dim[X], ebmp->ebm_dim[Y], INFINITY );
	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( ray.r_dir[X], SQRT_SMALL_FASTF ) )  {
		inv_dir[X]=1.0/ray.r_dir[X];
	} else {
		inv_dir[X] = INFINITY;
		ray.r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( ray.r_dir[Y], SQRT_SMALL_FASTF ) )  {
		inv_dir[Y]=1.0/ray.r_dir[Y];
	} else {
		inv_dir[Y] = INFINITY;
		ray.r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( ray.r_dir[Z], SQRT_SMALL_FASTF ) )  {
		inv_dir[Z]=1.0/ray.r_dir[Z];
	} else {
		inv_dir[Z] = INFINITY;
		ray.r_dir[Z] = 0.0;
	}
	if( rt_in_rpp( &ray, inv_dir, minpt, maxpt ) == 0 )
		return;

	segp = ebm_shot( &Tsolid, &ray, &Tappl );

	lastk = 0;
	while( segp != SEG_NULL )  {
		/* Draw 2-D segments */
		draw2seg( ray.r_pt, ray.r_dir, lastk, segp->seg_in.hit_dist, 0 );
		draw2seg( ray.r_pt, ray.r_dir, segp->seg_in.hit_dist, segp->seg_out.hit_dist, 1 );
		lastk = segp->seg_out.hit_dist;

		draw3seg( segp, ray.r_pt, ray.r_dir );

		next = segp->seg_next;
		FREE_SEG(segp, Tappl.a_resource);
		segp = next;
	}
}

outline()
{
	register struct ebm_specific *ebmp = bmsp;
	register struct vlist	*vp;
	struct vlhead	vhead;

	vhead.vh_first = vhead.vh_last = VL_NULL;

	pl_3space( plotfp, -BIT_XWIDEN,-BIT_YWIDEN,-BIT_XWIDEN,
		 ebmp->ebm_dim[X]+BIT_XWIDEN, ebmp->ebm_dim[Y]+BIT_YWIDEN, (int)(ebmp->ebm_tallness+1.99) );
	pl_3box( plotfp, -BIT_XWIDEN,-BIT_YWIDEN,-BIT_XWIDEN,
		 ebmp->ebm_dim[X]+BIT_XWIDEN, ebmp->ebm_dim[Y]+BIT_YWIDEN, (int)(ebmp->ebm_tallness+1.99) );

	/* call bit_plot(), then just draw the vlist */
	ebm_plot( 0, 0, &vhead, 0 );

	for( vp = vhead.vh_first; vp != VL_NULL; vp = vp->vl_forw )  {
		if( vp->vl_draw == 0 )
			pdv_3move( plotfp, vp->vl_pnt );
		else
			pdv_3cont( plotfp, vp->vl_pnt );
	}
	FREE_VL( vhead.vh_first );
}


draw2seg( pt, dir, k1, k2, inside )
vect_t	pt, dir;
fastf_t	k1, k2;
int	inside;
{
	vect_t	a, b;

#if 0
printf("draw2seg (%g %g) in=%d\n", k1, k2, inside);
#endif
	a[0] = pt[0] + k1 * dir[0];
	a[1] = pt[1] + k1 * dir[1];
	a[2] = 0;
	b[0] = pt[0] + k2 * dir[0];
	b[1] = pt[1] + k2 * dir[1];
	b[2] = 0;

	if( inside )
		pl_color( plotfp, 255, 0, 0 );	/* R */
	else
		pl_color( plotfp, 0, 255, 0 );	/* G */
	pdv_3line( plotfp, a, b );
}

draw3seg( segp, pt, dir )
register struct seg	*segp;
vect_t	pt, dir;
{
	vect_t	a, b;

	VJOIN1( a, pt, segp->seg_in.hit_dist, dir );
	VJOIN1( b, pt, segp->seg_out.hit_dist, dir );
	pl_color( plotfp, 0, 0, 255 );	/* B */
	pdv_3line( plotfp, a, b );
}
#endif /* test driver */
