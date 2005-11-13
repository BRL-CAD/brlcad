/*                F I X _ P O L Y S O L I D S . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
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
/** @file fix_polysolids.c
 *			F I X _ P O L Y S O L I D . C
 *
 *  Program to fix polysolids with bad normals.
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "db.h"
#include "../librt/debug.h"

static int	verbose;
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static FILE	*fp_in;			/* input file pointer */

static struct rt_tess_tol	ttol;
static struct rt_tol		tol;

static char usage[] = "Usage: %s [-v] [-xX lvl] < brlcad_db.g > new db.g\n\
	options:\n\
		v - verbose\n\
		x - librt debug flag\n\
		X - nmg debug flag\n";


/*
 *			M A I N
 */
int
int
main(argc, argv)
int	argc;
char	*argv[];
{
	union record rec;
	struct rt_tol tol;
	int c;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct nmg_ptbl faces;
	int done=0;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = bu_getopt(argc, argv, "vx:X:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'x':
			sscanf( bu_optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( bu_optarg, "%x", &rt_g.NMG_debug );
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );
	m = nmg_mmr();
	r = RT_LIST_FIRST( nmgregion, &m->r_hd );
	while( 1 )
	{
		struct vertex *verts[5];
		union record rec2;
		int i;

		if( done == 2 )
		{
			rec = rec2;
			done = 0;
		}
		else
			if( fread( &rec, sizeof( union record ), 1, stdin) != 1 )
				break;

		switch( rec.u_id )
		{
			case ID_FREE:
				continue;
				break;
			case ID_P_HEAD:
				rt_log( "Polysolid (%s)\n", rec.p.p_name );
				s = nmg_msv( r );
				nmg_tbl( &faces, TBL_RST, (long *)NULL );
				while( !done )
				{
					struct faceuse *fu;
					struct loopuse *lu;
					struct edgeuse *eu;
					point_t pt;

					if( fread( &rec2, sizeof( union record ), 1, stdin ) != 1 )
						done = 1;
					if( rec2.u_id != ID_P_DATA )
						done = 2;

					if( done )
						break;

					for( i=0 ; i<5 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s, verts, rec2.q.q_count );
					lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
					eu = RT_LIST_FIRST( edgeuse, &lu->down_hd );
					for( i=0 ; i<rec2.q.q_count ; i++ )
					{
						VMOVE( pt, &rec2.q.q_verts[i] )
						nmg_vertex_gv( eu->vu_p->v_p, pt );
						eu = RT_LIST_NEXT( edgeuse, &eu->l );
					}

					if( nmg_calc_face_g( fu ) )
					{
						rt_log( "\tEliminating degenerate face\n" );
						nmg_kfu( fu );
					}
					else
						nmg_tbl( &faces, TBL_INS, (long *)fu );
				}
				nmg_rebound( m, &tol );
				(void)nmg_break_long_edges( s , &tol );
				(void)nmg_model_vertex_fuse( m, &tol );
				nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ), &tol );
				nmg_fix_normals( s, &tol );
				write_shell_as_polysolid( stdout, rec.p.p_name, s );
				break;
			default:
				fwrite( &rec, sizeof( union record ), 1, stdout );
				break;
		}
	}
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );
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
