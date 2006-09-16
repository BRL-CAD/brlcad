/** @addtogroup nurb */
/*@{*/
/** @file nurb_solve.c
 * Decompose a matrix into its LU decomposition using pivoting.
 *
 * Author:	Paul R. Stay
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Mar 23 1983
 * Copyright (c) 1983, University of Utah
 *
 */
/*@}*/

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* These Procedures take a set of matrices of the form Ax = b and
 * alows one to solve the system by various means. The rt_nurb_doolittle
 * routine takes the system and creates a lu decomposition using
 * pivoting  to get the system in a desired form. Forward and backward
 * substitution are then used to solve the system.  All work is done
 * in place.
 */

/* Solve must be passed two matrices that are of the form of pointer
 * to double arrays. mat_1 is the n by n matrix that is to be
 * decomposed and mat_2 is the matrix that contains the left side of
 * the equation.  The variable solution which is double is also to be
 * created by the user and consisting of n elements of which the
 * solution set is passed back.
 *
 *  Arguments mat_1 and mat_2 are modified by this call.
 *  The solution is written into the solution[] array.
 */
void
rt_nurb_solve(fastf_t *mat_1, fastf_t *mat_2, fastf_t *solution, int dim, int coords)
       	       		/* A and b array of the system Ax= b*/


   	    		/* dimension of the matrix */
   	       		/* Number of coordsinates for mat_2 and solution */
{
	register int i, k;
	fastf_t *y;
	fastf_t * b;
	fastf_t * s;

	y = (fastf_t *) bu_malloc(sizeof (fastf_t) * dim,
	    "rt_nurb_solve: y");/* Create temp array */

	b = (fastf_t *) bu_malloc(sizeof (fastf_t) * dim,
	    "rt_nurb_solve: b");/* Create temp array */

	s = (fastf_t *) bu_malloc(sizeof (fastf_t) * dim,
	    "rt_nurb_solve: s");/* Create temp array */

	rt_nurb_doolittle (mat_1,mat_2, dim, coords);/* Create LU decomosition */

	for( k =0; k < coords; k++)
	{
		fastf_t * ptr;

		ptr = mat_2 + k;

		for( i = 0; i < dim; i++)
		{
			b[i] = *ptr;
			ptr += coords;
		}

		/* Solve the system Ly =b */
		rt_nurb_forw_solve (mat_1, b, y, dim);

		/* Solve the system Ux = y */
		rt_nurb_back_solve (mat_1, y, s, dim);


		ptr = solution + k;
		for( i=0; i < dim; i++)
		{
			*ptr = s[i];
			ptr += coords;
		}
	}

	bu_free ((char *)y,"rt_nurb_solve: y");			/* Free up storage */
	bu_free ((char *)b,"rt_nurb_solve: b");			/* Free up storage */
	bu_free ((char *)s,"rt_nurb_solve: s");			/* Free up storage */
}

/*
 *  Create LU decomosition.
 *  Modifies both mat_1 and mat_2 values.
 */
void
rt_nurb_doolittle(fastf_t *mat_1, fastf_t *mat_2, int row, int coords)
{
	register int i;
	register int j;
	register int k;
	register int x;
	int m;
	register fastf_t *d;		/* Scaling factors */
	register fastf_t *s;		/* vector for swapping if needed */
	register fastf_t *ds;		/* See if swapping is needed */
	fastf_t  maxd;
	fastf_t tmp;

	int     max_pivot;

	d = (fastf_t * ) bu_malloc( sizeof (fastf_t) * row,
	    "rt_nurb_doolittle:d");	/* scale factor */
	s = (fastf_t * ) bu_malloc( sizeof (fastf_t) * row * row,
	    "rt_nurb_doolittle:s");	/* vector to check */
	ds = (fastf_t *) bu_malloc( sizeof (fastf_t) * row,
	    "rt_nurb_doolittle:ds");	/* if rows need to be swaped */

	for ( i = 0; i < row; i++)		/* calculate the scaling factors */
	{
		maxd = 0.0;
		for( j = 0; j < row; j++)
		{
			if( maxd < fabs(mat_1[i * row + j]) )
				maxd = fabs(mat_1[i * row + j]);
		}
		d[i] = 1.0 / maxd;
	}

	for ( k = 0 ; k < row; k++)
	{
		for( i = k; i < row; i++)
		{
			tmp = 0.0;
			for( j = 0; j <= k -1; j ++)
				tmp += mat_1[i * row + j ] * mat_1[j * row + k];
			s[i * row + k] = mat_1[i * row + k] - tmp;
		}

		max_pivot = k;

		for (i = k; i < row; i ++)	/* check to see if rows need */
		{				/* to be swaped */
			ds[i] = d[i] * s[ i * row + k];
			if (ds[max_pivot] < ds[i])
				max_pivot = i;
		}

		if (max_pivot != k )		/* yes swap row k with row max_pivot */
		{
			for( m = 0; m < row; m++)
			{
				tmp = mat_1[k * row + m];
				mat_1[k * row + m] = mat_1[max_pivot * row + m];
				mat_1[max_pivot * row + m] = tmp;
			}

			for( x = 0; x < coords; x++)
			{
				tmp = mat_2[k*coords + x];		/* b matrix also */
				mat_2[k*coords+x] = mat_2[max_pivot*coords+x];
				mat_2[max_pivot*coords+x] = tmp;
			}

			tmp = s[k * row + k];	/* swap s vector  */
			s[k * row + k] = s[max_pivot * row + k];
			s[max_pivot * row + k] = tmp;
		}

		mat_1[ k * row +  k] = s[k * row + k];	/* mat_1[k][k] */

		for (i = k + 1; i < row; i++)	/* lower matrix */
			mat_1[i * row + k] = (float)(s[i* row + k] / s[k* row +k]);

		for (j = k + 1; j < row; j++) {	/* upper matrix */
			tmp = 0;
			for( i = 0; i <= k - 1; i++)
				tmp += mat_1[ k * row + i] * mat_1[ i* row + j];

			mat_1[ k * row + j] -= tmp;
		}

	}
	bu_free( (char *)d,"rt_nurb_doolittle:d");		/* Free up the storage. */
	bu_free( (char *)s,"rt_nurb_doolittle:s");
	bu_free( (char *)ds,"rt_nurb_doolittle:ds" );
}

void
rt_nurb_forw_solve(const fastf_t *lu, const fastf_t *b, fastf_t *y, int n)		/* spl_solve lower trianglular matrix */




{
	register int i,j;
	fastf_t tmp;

	for(i = 0; i < n; i++)
	{
		tmp = 0.0;
		for(j = 0; j <= i - 1; j++)
			tmp += lu[i*n + j] * y[j];
		y[i] = b[i] - tmp;
	}
}

void
rt_nurb_back_solve(const fastf_t *lu, const fastf_t *y, fastf_t *x, int n)		/* spl_solve upper triangular matrix */




{
	register int i,j;
	fastf_t tmp;

	for( i = n - 1; i >= 0; i-- )
	{
		tmp = 0.0;
		for( j = i + 1; j < n; j++)
			tmp += lu[i*n + j] * x[j];
		x[i] = ( y[i] - tmp) / lu[i * n + i];
	}

}

void
rt_nurb_p_mat(const fastf_t *mat, int dim)
{
	int i;

	for( i = 0; i < dim; i++)
		fprintf(stderr,"%f\n", mat[i]);
	fprintf(stderr,"\n");
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
