/*
 *			P O L Y - B O T
 *
 *  Program to convert all the polysolids in a BRL-CAD model (in a .g file) to BOT solids
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "db.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/plane.h"
#include "../librt/debug.h"

#define POLY_BLOCK	512

static char *usage="\
Usage: poly-bot < file_poly.g > file_bot.g\n\
   or  poly-bot file_poly.g file_bot.g\n\
 Convert polysolids to BOT solids\n";

main( argc, argv )
int argc;
char *argv[];
{
	FILE *ifp;
	FILE *ofp;
	union record record;
	union record *poly;
	long poly_limit=0;
	long curr_poly=0;
	struct rt_tess_tol	ttol;
	struct bn_tol		tol;
	int polys=0;
	int frees=0;
	int others=0;
	int first=1;

	ifp = stdin;
	ofp = stdout;

        ttol.magic = RT_TESS_TOL_MAGIC;
        /* Defaults, updated by command line options. */
        ttol.abs = 0.0;
        ttol.rel = 0.01;
        ttol.norm = 0.0;

        /* XXX These need to be improved */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	if( argc >= 3 ) {
		ifp = fopen(argv[1],"r");
		if( !ifp )  perror(argv[1]);
		ofp = fopen(argv[2],"w");
		if( !ofp )  perror(argv[2]);
		if (ifp == NULL || ofp == NULL) {
			(void)fprintf(stderr, "poly-bot: can't open files.");
			exit(1);
		}
	}
	if (isatty(fileno(ifp))) {
		(void)fprintf(stderr, usage);
		exit(1);
	}

	poly = (union record *)bu_malloc( POLY_BLOCK * sizeof( union record ), "poly" );
	poly_limit = POLY_BLOCK;

	/* Read database file */
	while( fread( (char *)&record, sizeof record, 1, ifp ) == 1  && !feof(ifp) )
	{
top:
		switch( record.u_id )
		{
			case ID_FREE:
				frees++;
				continue;
			case ID_P_HEAD:
			{
				struct rt_db_internal intern;
				struct bu_external ext;
				struct rt_bot_internal *bot;
				struct soltab st;
				struct rt_i rti;
				struct tri_specific *tri;
				int i;

				rti.rti_tol = tol;

				polys++;
				curr_poly = 0;
				poly[curr_poly++] = record;	/* struct copy */
				bu_log( "Converting %s\n", poly[0].p.p_name );
				while( fread( (char *)&record, sizeof record, 1, ifp ) == 1  &&
					!feof(ifp) &&
					record.u_id == ID_P_DATA )
				{
					if( curr_poly >= poly_limit )
					{
						poly_limit += POLY_BLOCK;
						poly = (union record *)bu_realloc( poly, poly_limit*sizeof( union record ), "poly realloc" );
					}
					poly[curr_poly++] = record;	/* struct copy */
				}
				BU_INIT_EXTERNAL( &ext );
				ext.ext_nbytes = curr_poly * sizeof( union record );
				ext.ext_buf = (char *)poly;
				if( rt_pg_import( &intern, &ext, bn_mat_identity, (struct db_i *)NULL ) )
				{
					bu_log( "Import failed for polysolid %s\n", poly[0].p.p_name );
					bu_bomb( "Import failed for polysolid\n" );
				}
				st.st_specific = (genptr_t)NULL;
				if( rt_pg_prep( &st, &intern, &rti ) )
				{
					bu_log( "Unable to convert polysolid %s\n", poly[0].p.p_name );
					bu_bomb( "Unable to convert!!!\n" );
				}

				tri = (struct tri_specific *)st.st_specific;
				bot = (struct rt_bot_internal *)bu_calloc( 1, sizeof( struct rt_bot_internal), "bot" );
				bot->magic = RT_BOT_INTERNAL_MAGIC;
				bot->mode = RT_BOT_SOLID;
				bot->orientation = RT_BOT_CCW;
				bot->num_vertices = 0;
				bot->num_faces = 0;

				while( tri )
				{
					bot->num_faces++;
					tri = tri->tri_forw;
				}

				bot->faces = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "bot->faces" );
				bot->num_vertices = bot->num_faces * 3;
				bot->vertices = (fastf_t *)bu_calloc( bot->num_vertices * 3, sizeof( fastf_t ), "bot->vertices" );

				i = 0;
				tri = (struct tri_specific *)st.st_specific;
				while( tri )
				{
					bot->faces[i] = i;
					bot->faces[i + 1] = i + 1;
					bot->faces[i + 2] = i + 2;
					VMOVE( &bot->vertices[i*3], tri->tri_A );
					VADD2( &bot->vertices[(i+1)*3], tri->tri_A, tri->tri_BA );
					VADD2( &bot->vertices[(i+2)*3], tri->tri_A, tri->tri_CA );
					tri = tri->tri_forw;
					i += 3;
				}

				(void)bot_vertex_fuse( bot );

				mk_export_fwrite( ofp, poly[0].p.p_name, (genptr_t)bot, ID_BOT );

				tri = (struct tri_specific *)st.st_specific;
				while( tri )
				{
					struct tri_specific *tmp;

					tmp = tri;
					tri = tri->tri_forw;
					bu_free( (char *)tmp, "tri_specific" );
				}
				bu_free( (char *)bot->faces, "bot->faces" );
				bu_free( (char *)bot->vertices, "bot->vertices" );
				bu_free( (char *)bot, "bot" );
				if( feof( ifp ) )
					break;
				goto top;
			}
			case ID_P_DATA:
				/* This should not happen!!!! */
				bu_log( "ERROR: Unattached polysolid data record!!!!\n" );
				continue;
			default:
				if( first )
				{
					if( record.u_id != ID_IDENT ) {
						bu_log( "This is not a 'v4' database!!!\n" );
						exit( 1 );
					}
					first = 0;
				}
				others++;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
		}
	}

	bu_log( "%d polysolids converted to BOT solids\n", polys );
	bu_log( "%d other records copied without change\n", others );
	bu_log( "%d free records skipped\n", frees );
}
