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
 *
 * Since we only want the last row of the alpha's as outlined in the
 * paper we can use a one dimensional array for the ah.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

#define AMAX(i,j)    ( (i) > (j) ? (i) : (j) )
#define AMIN(i,j)    ( (i) < (j) ? (i) : (j) )

struct oslo_mat *
rt_nurb_calc_oslo(order, tau_kv, t_kv, res )
register int order;
register CONST struct knot_vector * tau_kv;	/* old knot vector */
register struct knot_vector * t_kv;		/* new knot vector */
struct resource *res;
{
	register fastf_t	*t_p;
	register CONST fastf_t	*tau_p;
	fastf_t ah[20];
	fastf_t newknots[20];			/* new knots */
	register int  j;			/* d(j), j = 0 : # of new ctl points */
	int     mu,				/* mu:  tau[mu] <= t[j] < tau[mu+1]*/
		muprim,
		v,				/* Nu value (order of matrix) */
		p,
		iu,				/* upper bound loop counter */
		il,				/* lower bound loop counter */
		ih,
		n1;				/* upper bound of t knot vector - order*/

	fastf_t tj;

	struct oslo_mat * head, * o_ptr, *new_o;

	n1 = t_kv->k_size - order;

	t_p = t_kv->knots;
	tau_p = tau_kv->knots;

	mu = 0;				/* initialize mu */

	if( res )
		head = (struct oslo_mat *) pmalloc (
		    sizeof( struct oslo_mat),
		    &res->re_pmem );
	else
		head = (struct oslo_mat *) rt_malloc (
		    sizeof( struct oslo_mat),
		    "rt_nurb_calc_oslo: oslo mat head" );

	o_ptr = head;

	for (j = 0; j < n1; j++) {
		register int  i;

		if ( j != 0 )
		{
			if( res )
				new_o = (struct oslo_mat *) pmalloc ( 
				    sizeof( struct oslo_mat), 
				    &res->re_pmem );
			else
				new_o = (struct oslo_mat *) rt_malloc ( 
				    sizeof( struct oslo_mat), 
				    "rt_nurb_calc_oslo: oslo mat struct" );

			o_ptr->next = new_o;
			o_ptr = new_o;
		}

		/* find the bounding mu */
		while (tau_p[mu + 1] <= t_p[j]) mu++;

		muprim = mu;

		i = j + 1;

		while ((t_p[i] == tau_p[muprim]) && i < (j + order)) {
			i++;
			muprim--;
		}

		ih = muprim + 1;

		for (v = 0, p = 1; p < order; p++) {
			if (t_p[j + p] == tau_p[ih])
				ih++;
			else
				newknots[++v - 1] = t_p[j + p];
		}

		ah[order-1] = 1.0;

		for (p = 1; p <= v; p++) {

			fastf_t beta1;
			int o_m;

			beta1 = 0.0;
			o_m = order - muprim;

			tj = newknots[p-1];

			if (p > muprim) {
				beta1 = ah[o_m];
				beta1 = ((tj - tau_p[0]) * beta1) /
				    (tau_p[p + order - v] - tau_p[0]);
			}
			i = muprim - p + 1;
			il = AMAX (1, i);
			i = n1 - 1 + v - p;
			iu = AMIN (muprim, i);

			for (i = il; i <= iu; i++) {
				fastf_t d1, d2;
				fastf_t beta;

				d1 = tj - tau_p[i];
				d2 = tau_p[i + p + order - v - 1] - tj;

				beta = ah[i + o_m - 1] / (d1 + d2);

				ah[i + o_m - 2] = d2 * beta + beta1;
				beta1 = d1 * beta;
			}

			ah[iu + o_m - 1] = beta1;

			if (iu < muprim) {
				register fastf_t kkk;
				register fastf_t ahv;

				kkk = tau_p[n1 - 1 + order];
				ahv = ah[iu + o_m];
				ah[iu + o_m - 1] =
				    beta1 + (kkk - tj) *
				    ahv / (kkk - tau_p[iu + 1]);
			}
		}

		if( res )
			o_ptr->o_vec = (fastf_t *) pmalloc ( sizeof( fastf_t) * (v+1),
			    &res->re_pmem);
		else
			o_ptr->o_vec = (fastf_t *) rt_malloc ( sizeof( fastf_t) * (v+1),
			    "rt_nurb_calc_oslo: oslo vector");

		o_ptr->offset = AMAX(muprim -v,0);
		o_ptr->osize = v;

		for ( i = v, p = 0; i >= 0; i--)
			o_ptr->o_vec[p++] =  ah[(order-1) - i];
	}

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
rt_nurb_free_oslo( om, res )
struct oslo_mat * om;
struct resource *res;
{
	register struct oslo_mat * omp;

	while( om != (struct oslo_mat *) 0 )
	{
		omp = om;
		om = om->next;
		if( res )
		{
			pfree( (char *)omp->o_vec, &res->re_pmem);
			pfree( (char *)omp, &res->re_pmem);
		}
		else
		{
			rt_free( (char *)omp->o_vec, "rt_nurb_free_oslo: ovec");
			rt_free( (char *)omp, "rt_nurb_free_oslo: struct oslo");
		}
	}
}
