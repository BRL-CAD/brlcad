/*	N U R B _ U T I L . C
 *
 *  Function -
 *	Utilities for NURB curves and surfaces.
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nurb.h"

/* Create a paceholder for a nurb surface. */

struct snurb *
rt_nurb_new_snurb(u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, pt_type)
int u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, pt_type;
{
	register struct snurb * srf;
	int pnum;
	
	srf = (struct snurb *) rt_malloc( sizeof ( struct snurb),
		"rt_nurb_new: snurb struct");

	srf->next = (struct snurb *) 0;
	srf->order[0] = u_order;
	srf->order[1] = v_order;
	srf->dir = ROW;

	srf->u_knots = ( struct knot_vector *) rt_malloc ( 
		sizeof ( struct knot_vector ), "rt_nurb_new_snurb: u kv struct");    
	srf->v_knots = ( struct knot_vector *) rt_malloc ( 
		sizeof ( struct knot_vector ), "rt_nurb_new_snurb: v kv struct");

	srf->u_knots->k_size = n_u_knots;
	srf->v_knots->k_size = n_v_knots;

	srf->u_knots->knots = (fastf_t *) rt_malloc ( 
		n_u_knots * sizeof (fastf_t ), "rt_nurb_new_snurb: u kv knot values");
	srf->v_knots->knots = (fastf_t *) rt_malloc ( 
		n_v_knots * sizeof (fastf_t ), "rt_nurb_new_snurb: v kv knot values");

	srf->mesh = (struct s_mesh *) rt_malloc ( 
	sizeof ( struct s_mesh ), "rt_nurb_new_snurb: control mesh");

	srf->mesh->s_size[0] = n_rows;
	srf->mesh->s_size[1] = n_cols;
	srf->mesh->pt_type = pt_type;
	
	pnum = sizeof (fastf_t) * n_rows * n_cols * EXTRACT_COORDS(pt_type);
	srf->mesh->ctl_points = ( fastf_t *) rt_malloc( 
		pnum, "rt_nurb_new_snurb: control mesh points");

	return srf;
}

/* Create a place holder for a new nurb curve. */
struct cnurb *
rt_nurb_new_cnurb( order, n_knots, n_pts, pt_type)
int order, n_knots, n_pts, pt_type;
{
	register struct cnurb * crv;

	crv = (struct cnurb *) rt_malloc( sizeof(struct cnurb),
		"rt_nurb_new_cnurb: cnurb struct");

	crv->next = (struct cnurb *) 0;
	crv->order = order;

	crv->knot = ( struct knot_vector *) rt_malloc ( 
		sizeof ( struct knot_vector ), 
		"rt_nurb_new_cnurb: struct knotvector");    

	crv->knot->k_size = n_knots;
	crv->knot->knots = (fastf_t *)
		rt_malloc(n_knots * sizeof(fastf_t),
			"rt_nurb_new_cnurb: knot values");

	crv->mesh = (struct c_mesh *) rt_malloc(
		sizeof( struct c_mesh), 
		"rt_nurb_new_cnurb: struct c_mesh");

	crv->mesh->c_size = n_pts;
	crv->mesh->pt_type = pt_type;

	crv->mesh->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * EXTRACT_COORDS(pt_type) *
			n_pts, 
			"rt_nurb_new_cnurb: mesh point values");

	return crv;
}

/* free routine for a nurb surface */

void
rt_nurb_free_snurb( srf )
struct snurb * srf;
{
    /* assume that links to other surface and curves are already deleted */

    rt_free( (char *)srf->u_knots->knots, "rt_nurb_free_snurb: u kv knots" );
    rt_free( (char *)srf->v_knots->knots, "rt_nurb_free_snurb: v kv knots" );
    rt_free( (char *)srf->u_knots, "rt_nurb_free_snurb: u kv struct");
    rt_free( (char *)srf->v_knots, "rt_nurb_free_snurb: v kv struct");

    rt_free( (char *)srf->mesh->ctl_points, "rt_nurb_free_snurb: mesh points");
    rt_free( (char *)srf->mesh, "rt_nurb_free_snurb: mesh struct" );

    rt_free( (char *)srf, "rt_nurb_free_snurb: bspline struct" );
}


/* free routine for a nurb curve */

void
rt_nurb_free_cnurb( crv)
struct cnurb * crv;
{
	rt_free( (char*)crv->knot->knots, "rt_nurb_free_cnurb: knots");
	rt_free( (char*)crv->knot, "rt_nurb_free_cnurb: knot struct");
	rt_free( (char*)crv->mesh->ctl_points, "rt_nurb_free_cnurb: control points");
	rt_free( (char*)crv->mesh, "rt_nurb_free_cnurb: mesh struct");
	rt_free( (char*)crv, "rt_nurb_free_cnurb: cmesh struct");

}

void
rt_nurb_c_print( crv)
struct cnurb * crv;
{
	register fastf_t * ptr;
	int i,j;

	fprintf(stderr,"curve = {\n");
	fprintf(stderr,"\tOrder = %d\n", crv->order);
	fprintf(stderr,"\tKnot Vector = {\n\t\t");

	for( i = 0; i < crv->knot->k_size; i++)
		fprintf(stderr,"%3.2f  ", crv->knot->knots[i]);

	fprintf(stderr,"\n\t}\n");
	fprintf(stderr,"\t");
	rt_nurb_print_pt_type(crv->mesh->pt_type);
	fprintf(stderr,"\tmesh = {\n");
	for( ptr = &crv->mesh->ctl_points[0], i= 0;
		i < crv->mesh->c_size; i++, ptr += EXTRACT_COORDS(crv->mesh->pt_type))
	{
		fprintf(stderr,"\t\t");
		for(j = 0; j < EXTRACT_COORDS(crv->mesh->pt_type); j++)
			fprintf(stderr,"%4.5f\t", ptr[j]);
		fprintf(stderr,"\n");

	}
	fprintf(stderr,"\t}\n}\n");
	

}

void
rt_nurb_s_print( c, srf )
char * c;
struct snurb * srf;
{

    fprintf ( stderr, "%s\n", c );

    fprintf ( stderr, "order %d %d\n", srf->order[0], srf->order[1] );

    fprintf(stderr, "u knot vector \n");

    rt_nurb_pr_kv( srf->u_knots );

    fprintf(stderr, "v knot vector \n");

    rt_nurb_pr_kv( srf->v_knots );

    rt_nurb_pr_mesh( srf->mesh );

}

void
rt_nurb_pr_kv( kv )
struct knot_vector * kv;
{
    register fastf_t * ptr = kv->knots;
    int i;

    fprintf( stderr, "[%d]\t", kv->k_size );


    for( i = 0; i < kv->k_size; i++)
    {
	fprintf( stderr, "%2.5f  ", *ptr++);
    }
    fprintf(stderr,"\n");
}

void
rt_nurb_pr_mesh( m )
struct s_mesh * m;
{

	int i,j,k;
	fastf_t * m_ptr = m->ctl_points;
	int evp = EXTRACT_COORDS(m->pt_type);

	fprintf(stderr,"\t[%d] [%d]\n", m->s_size[0], m->s_size[1] );

	for( i = 0; i < m->s_size[0]; i++)
	{
		for( j =0; j < m->s_size[1]; j++)
		{
			fprintf(stderr,"\t");

			for(k = 0; k < evp; k++)
				fprintf(stderr,"%f    ", m_ptr[k]);

			fprintf(stderr,"\n");
			m_ptr += EXTRACT_COORDS(m->pt_type);
		}
		fprintf(stderr,"\n");
	}
}

void
rt_nurb_print_pt_type(c)
int c;
{
	fastf_t flt;
	int coords, rat;

	coords = EXTRACT_COORDS(c);
	rat = EXTRACT_RAT(c);
	
	if( EXTRACT_PT_TYPE(c) == PT_XY)
		fprintf(stderr,"Point Type = PT_XY");
	else 
	if( EXTRACT_PT_TYPE(c) == PT_XYZ)
		fprintf(stderr,"Point Type = PT_XYX");
	else 
	if( EXTRACT_PT_TYPE(c) == PT_UV)
		fprintf(stderr,"Point Type = PT_UV");

	if( rat )
		fprintf(stderr,"W\n");
	else
		fprintf(stderr,"\n");
}

