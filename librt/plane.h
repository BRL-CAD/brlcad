/*
 *  			P L A N E . H
 *
 *  This header file describes the plane_specific structure, which
 *  is common to ARB8 and ARS processing.
 *  
 * U. S. Army Ballistic Research Laboratory
 * May 1, 1984
 *
 * $Revision$
 */

#define MAXPTS	4			/* All we need are 4 points */
#define pl_A	pl_points[0]		/* Synonym for A point */

struct plane_specific  {
	int	pl_npts;		/* number of points on plane */
	point_t	pl_points[MAXPTS];	/* Actual points on plane */
	vect_t	pl_Xbasis;		/* X (B-A) vector (for 2d coords) */
	vect_t	pl_Ybasis;		/* Y (C-A) vector (for 2d coords) */
	vect_t	pl_N;			/* Unit-length Normal (outward) */
	fastf_t	pl_NdotA;		/* Normal dot A */
	fastf_t	pl_2d_x[MAXPTS];	/* X 2d-projection of points */
	fastf_t	pl_2d_y[MAXPTS];	/* Y 2d-projection of points */
	fastf_t	pl_2d_com[MAXPTS];	/* pre-computed common-term */
	struct plane_specific *pl_forw;	/* Forward link */
	char	pl_code[MAXPTS+1];	/* Face code string.  Decorative. */
};
