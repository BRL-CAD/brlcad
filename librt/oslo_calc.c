/* 
 *       C A L C _ O S L O . C
 *
 * Function -
 *     Calculate the Oslo refinement matrix.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1986 by the United States Army.
 *     All rights reserved.
 *
 * This algorithm was taken from the paper 
 * "Making the Oslo Algorithm More Efficient" by T. Lyche and K. Morken 
 * The algorithm referenced in the paper is algorithm 1 since we will be
 * dealing mostly with surfaces. This routine computes the refinement
 * matrix and returns a oslo structure which will allow a new curve or
 * surface to be built.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./nurb.h"

#define AMAX(i,j)    ( (i) > (j) ? (i) : (j) )
#define AMIN(i,j)    ( (i) < (j) ? (i) : (j) )

/* Simplification needed thanks to the SGI: */
/* ah[AH_INDEX(j,t)] gives result of former ah_index(j,t) macro */
#define AH_INDEX(_j,_t)   (( (_j) * ((_j)+1)/2) + (_t) - ((order-1) - (_j)))
#define ah_index(__j,__t)	ah[AH_INDEX(__j,__t)]

struct oslo_mat *
rt_nurb_calc_oslo(order, tau_kv, t_kv )
int order;
register struct knot_vector * tau_kv;	/* old knot vector */
register struct knot_vector * t_kv;	/* new knot vector */
{
	fastf_t * ah;
	fastf_t * newknots;			/* new knots */
	register int  i;
	register int  j;			/* d(j), j = 0 : # of new ctl points */
	int     mu,				/* mu:  tau[mu] <= t[j] < tau[mu+1]*/
	muprim;
	int     v,				/* Nu value (order of matrix) */
	p;
	int     iu,				/* upper bound loop counter */
	il,				/* lower bound loop counter */
	ih,
	n1;				/* upper bound of t knot vector - order*/
	int     ahi;			/* ah[] index */
	fastf_t beta1;
	fastf_t tj;
	struct oslo_mat * head, * o_ptr, *new_o;
	
	ah = (fastf_t *) rt_malloc (sizeof (fastf_t) * order * ( order + 1)/2,
	    "rt_nurb_calc_oslo: alpha matrix");
	newknots = (fastf_t *) rt_malloc (sizeof (fastf_t) * (order), 
	    "rt_nurb_calc_oslo: New knots");

	n1 = t_kv->k_size - order;

	mu = 0;				/* initialize mu */

	head = (struct oslo_mat *) rt_malloc (
	    sizeof( struct oslo_mat),
	    "rt_nurb_calc_oslo: oslo mat head" );
	o_ptr = head;

	for (j = 0; j < n1; j++) {

		if ( j != 0 )
		{
			new_o = (struct oslo_mat *) rt_malloc ( 
			    sizeof( struct oslo_mat), 
			    "rt_nurb_calc_oslo: oslo mat struct" );
			o_ptr->next = new_o;
			o_ptr = new_o;
		}

		while (tau_kv->knots[mu + 1] <= t_kv->knots[j])
			mu = mu + 1;		/* find the bounding mu */

		i = j + 1;
		muprim = mu;

		while ((t_kv->knots[i] == tau_kv->knots[muprim]) && i < (j + order)) {
			i++;
			muprim--;
		}

		ih = muprim + 1;

		for (v = 0, p = 1; p < order; p++) {
			if (t_kv->knots[j + p] == tau_kv->knots[ih])
				ih++;
			else
				newknots[++v - 1] = t_kv->knots[j + p];
		}

		/* Separating these two is needed to avoid the SGI 4D compiler bug */
		ahi = AH_INDEX(0, order - 1);
		ah[ahi] = 1.0;

		for (p = 1; p <= v; p++) {
			beta1 = 0.0;
			tj = newknots[p-1];

			if (p - 1 >= muprim) {
				beta1 = ah_index (p - 1, order - muprim);
				beta1 = ((tj - tau_kv->knots[0]) * beta1) /
				    (tau_kv->knots[p + order - v] - tau_kv->knots[0]);
			}
			i = muprim - p + 1;
			il = AMAX (1, i);
			i = n1 - 1 + v - p;
			iu = AMIN (muprim, i);

			for (i = il; i <= iu; i++) {
				register fastf_t d1, d2;
				register fastf_t beta;

				d1 = tj - tau_kv->knots[i];
				d2 = tau_kv->knots[i + p + order - v - 1] - tj;

				beta = ah_index (p - 1, i + order - muprim - 1) / (d1 + d2);

				ah_index (p, i + order - muprim - 2) = d2 * beta + beta1;
				beta1 = d1 * beta;
			}

			ah_index (p, iu + order - muprim - 1) = beta1;

			if (iu < muprim) {
				register fastf_t kkk;
				register fastf_t ahv;

				kkk = tau_kv->knots[n1 - 1 + order];
				ahv = ah_index (p - 1, iu + order - muprim);
				ah_index (p, iu + order - muprim - 1) =
				    beta1 + (kkk - tj) *
				    ahv / (kkk - tau_kv->knots[iu + 1]);
			}
		}

		o_ptr->o_vec = (fastf_t *) rt_malloc ( sizeof( fastf_t) * (v+1),
		    "rt_nurb_calc_oslo: oslo vector");

		o_ptr->offset = AMAX(muprim -v,0);
		o_ptr->osize = v;

		for ( i = v, p = 0; i >= 0; i--)
			o_ptr->o_vec[p++] =  ah_index(v, (order-1) - i);
	}

	rt_free ( (char *)ah, "rt_nurb_calc_oslo: alpha matrix" );
	rt_free ( (char *)newknots, "rt_nurb_calc_oslo: new knots" );
	o_ptr->next = (struct oslo_mat*) 0;
	return head;
}


/*
 *  rt_pr_oslo() - FOR DEBUGGING PURPOSES
 */
void
rt_nurb_pr_oslo( om)
struct oslo_mat * om;
{
	struct oslo_mat * omp;
	int j;

	for( omp = om; omp!= ( struct oslo_mat *) 0; omp = omp->next)
	{
		fprintf( stderr, "%x offset %d osize %d next %x\n",  omp,
		    omp->offset,  omp->osize,  omp->next);

		fprintf(stderr,"\t%f",  omp->o_vec[0]);

		for ( j = 1; j <= omp->osize; j++)
			fprintf(stderr,"\t%f",  omp->o_vec[j]);
		fprintf(  stderr, "\n");
	}
}

/* rt_nurb_free_oslo()
 * Free up the structures and links for the oslo matrix.
 */

void
rt_nurb_free_oslo( om )
struct oslo_mat * om;
{
	register struct oslo_mat * omp;
	
	while( om != (struct oslo_mat *) 0 )
	{
		omp = om;
		om = om->next;
		rt_free( (char *)omp->o_vec, "rt_nurb_free_oslo: ovec");
		rt_free( (char *)omp, "rt_nurb_free_oslo: struct oslo");
	}
}
