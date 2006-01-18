/*                       T E A _ N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file tea_nmg.c
 *
 * Convert the Utah Teapot description from the IEEE CG&A database to the
 * BRL-CAD t-NURBS NMG format. (Note that this has the closed bottom)
 *
 *  Authors -
 *	John R. Anderson
 *	Paul R. Stay
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

/* Header files which are used for this example */

#include "common.h"



#include <stdio.h>		/* Direct the output to stdout */
#include "machine.h"		/* BRL-CAD specific machine data types */
#include "vmath.h"		/* BRL-CAD Vector macros */
#include "nmg.h"
#include "nurb.h"		/* BRL-CAD Spline data structures */
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"	/* RT_G_DEBUG flag settings */

#include "./tea.h"		/* IEEE Data Structures */
#include "./ducks.h"		/* Teapot Vertex data */
#include "./patches.h"		/* Teapot Patch data */

extern dt ducks[DUCK_COUNT];		/* Vertex data of teapot */
extern pt patches[PATCH_COUNT];		/* Patch data of teapot */

char *Usage = "This program ordinarily generates a database on stdout.\n\
	Your terminal probably wouldn't like it.";

static struct shell *s;
static struct model *m;
static struct bn_tol tol;

void dump_patch(int (*patch)[4]);

struct rt_wdb *outfp;

int
main(int argc, char **argv) 			/* really has no arguments */

{
	struct nmgregion *r;
	char * id_name = "BRL-CAD t-NURBS NMG Example";
	char * tea_name = "UtahTeapot";
	char * uplot_name = "teapot.pl";
	struct bu_list vhead;
	FILE *fp;
	int i;

        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	BU_LIST_INIT( &rt_g.rtg_vlfree );

	outfp = wdb_fopen( "tea_nmg.g" );

	rt_g.debug |= DEBUG_ALLRAYS;	/* Cause core dumps on rt_bomb(), but no extra messages */

	while ((i=getopt(argc, argv, "d")) != EOF) {
		switch (i) {
		case 'd' : rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL; break;
		default	:
			(void)fprintf(stderr,
				"Usage: %s [-d] > database.g\n", *argv);
			return(-1);
		}
	}

	mk_id( outfp, id_name);

	m = nmg_mm();
	NMG_CK_MODEL( m );
	r = nmg_mrsv( m );
	NMG_CK_REGION( r );
	s = BU_LIST_FIRST( shell , &r->s_hd );
	NMG_CK_SHELL( s );

	/* Step through each patch and create a NMG TNURB face
	 * representing the patch then dump them out.
	 */

	for( i = 0; i < PATCH_COUNT; i++)
	{
		dump_patch( patches[i] );
	}

	/* Connect up the coincident vertexuses and edges */
	(void)nmg_model_fuse( m , &tol );

	/* write NMG to output file */
	(void)mk_nmg( outfp , tea_name , m );
	wdb_close(outfp);

	/* Make a vlist drawing of the model */
	BU_LIST_INIT( &vhead );
	nmg_m_to_vlist( &vhead , m , 0 );

	/* Make a UNIX plot file from this vlist */
	if( (fp=fopen( uplot_name , "w" )) == NULL )
	{
		bu_log( "Cannot open plot file: %s\n" , uplot_name );
		perror( "teapot_nmg" );
	}
	else
		rt_vlist_to_uplot( fp , &vhead );

	return(0);
}

/* IEEE patch number of the Bi-Cubic Bezier patch and convert it
 * to a B-Spline surface (Bezier surfaces are a subset of B-spline surfaces
 * and output it to a BRL-CAD binary format.
 */
void
dump_patch(int (*patch)[4])
{
	struct vertex *verts[4];
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int i,j, pt_type;
	fastf_t *mesh=NULL;
	fastf_t *ukv=NULL;
	fastf_t *vkv=NULL;

	/* U and V parametric Direction Spline parameters
	 * Cubic = order 4,
	 * knot size is Control point + order = 8
	 * control point size is 4
	 * point size is 3
	 */

	for( i=0 ; i<4 ; i++ )
		verts[i] = (struct vertex *)NULL;

	fu = nmg_cface( s , verts , 4 );
	NMG_CK_FACEUSE( fu );

	for( i=0 ; i<4 ; i++ )
	{
		struct vertexuse *vu;
		vect_t uvw;
		point_t pnt;
		int k,j;

		switch( i )
		{
			default:
			case 0:
				VSET( uvw , 0.0 , 0.0 , 0.0 );
				k = 0;
				j = 0;
				break;
			case 1:
				VSET( uvw , 1.0 , 0.0 , 0.0 );
				k = 0;
				j = 3;
				break;
			case 2:
				VSET( uvw , 1.0 , 1.0 , 0.0 );
				k = 3;
				j = 3;
				break;
			case 3:
				VSET( uvw , 0.0 , 1.0 , 0.0 );
				k = 3;
				j = 0;
				break;
		}

		VSET( pnt ,
			ducks[patch[k][j]-1].x * 1000 ,
			ducks[patch[k][j]-1].y * 1000 ,
			ducks[patch[k][j]-1].z * 1000 );
		nmg_vertex_gv( verts[i] , pnt );

		for( BU_LIST_FOR( vu , vertexuse , &verts[i]->vu_hd ) )
			nmg_vertexuse_a_cnurb( vu , uvw );
	}

	pt_type = RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, 0); /* see nurb.h for details */

	nmg_face_g_snurb( fu , 4 , 4 , 8 , 8 , ukv , vkv , 4 , 4 , pt_type , mesh );

	NMG_CK_FACE( fu->f_p );
	NMG_CK_FACE_G_SNURB( fu->f_p->g.snurb_p );
	mesh = fu->f_p->g.snurb_p->ctl_points;

	/* Copy the control points */

	for( i = 0; i< 4; i++)
	for( j = 0; j < 4; j++)
	{
		*mesh = ducks[patch[i][j]-1].x * 1000;
		*(mesh+1) = ducks[patch[i][j]-1].y * 1000;
		*(mesh+2) = ducks[patch[i][j]-1].z * 1000;
		mesh += 3;
	}

	/* Both u and v knot vectors are [ 0 0 0 0 1 1 1 1] */
	ukv = fu->f_p->g.snurb_p->u.knots;
	vkv = fu->f_p->g.snurb_p->v.knots;
	/* set the knot vectors */
	for( i=0 ; i<4 ; i++ )
	{
		*(ukv+i) = 0.0;
		*(vkv+i) = 0.0;
	}
	for( i=0 ; i<4 ; i++ )
	{
		*(ukv+4+i) = 1.0;
		*(vkv+4+i) = 1.0;
	}

	/* set eu geometry */
	pt_type = RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_UV,0); /* see nurb.h for details */
	lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
	NMG_CK_LOOPUSE( lu );
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
#if 0
		nmg_edge_g_cnurb( eu , 2 , 0 , (fastf_t *)NULL , 2 ,
			pt_type , (fastf_t *)NULL );
#else
		nmg_edge_g_cnurb_plinear( eu );
#endif
	}
	nmg_face_bb( fu->f_p , &tol );
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
