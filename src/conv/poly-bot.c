/*                      P O L Y - B O T . C
 * BRL-CAD
 *
 * Copyright (C) 2000-2005 United States Government as represented by
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
 *
 */
/** @file poly-bot.c
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
#include "db.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/plane.h"
#include "../librt/debug.h"

#define POLY_BLOCK	512

static const char *usage="\
Usage: poly-bot < file_poly.g > file_bot.g\n\
   or  poly-bot file_poly.g file_bot.g\n\
 Convert polysolids to BOT solids in v4 database format only\n";

int
main(int argc, char **argv)
{
	FILE *ifp;
	FILE *ofp;
	union record record;
	union record *poly;
	long poly_limit=0;
	long curr_poly=0;
	struct bn_tol		tol;
	int polys=0;
	int frees=0;
	int others=0;
	int bots=0;
	int i;
	int num_rec;
	int first=1;

	ifp = stdin;
	ofp = stdout;

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

			case DBID_SKETCH:
				num_rec = bu_glong( (const unsigned char *)&record.skt.skt_count );
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying a SKETCH\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_EXTR:
				num_rec = bu_glong( (const unsigned char *)&record.extr.ex_count );
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying an EXTUSION\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_NMG:
				num_rec = bu_glong( (const unsigned char *)&record.nmg.N_count );
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying an ARBN\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_PIPE:
				num_rec = bu_glong( (const unsigned char *)&record.pwr.pwr_count );
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying a PIPE\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_ARBN:
				num_rec = bu_glong( (const unsigned char *)&record.n.n_grans );
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying an ARBN\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_STRSOL:
				num_rec = DB_SS_NGRAN - 1;
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying a STRSOL\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case ID_BSURF:
				num_rec = record.d.d_nknots + record.d.d_nctls;
				others += num_rec + 1;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying a NURB\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case DBID_BOT:
				bots++;
				if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
					bu_bomb( "Write failed!!!\n" );
				num_rec = bu_glong( (const unsigned char *)&record.bot.bot_nrec );
				for( i=0 ; i<num_rec ; i++ )
				{
					if( fread( (char *)&record, sizeof record, 1, ifp ) != 1 )
						bu_bomb( "Unexpected EOF encountered while copying a BOT\n" );
					if( fwrite( &record, sizeof( union record ), 1, ofp ) < 1 )
						bu_bomb( "Write failed!!!\n" );
				}
				break;
			case ID_P_HEAD:
			{
				struct rt_db_internal intern;
				struct bu_external ext;
				struct bu_external ext2;

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
				if( rt_functab[ID_POLY].ft_import( &intern, &ext, bn_mat_identity, (struct db_i *)NULL, &rt_uniresource ) )
				{
					bu_log( "Import failed for polysolid %s\n", poly[0].p.p_name );
					bu_bomb( "Import failed for polysolid\n" );
				}
				/* Don't free this ext buffer! */

				if( rt_pg_to_bot( &intern, &tol, &rt_uniresource ) < 0 )  {
					bu_log( "Unable to convert polysolid %s\n", poly[0].p.p_name );
					bu_bomb( "Unable to convert!!!\n" );
				}

				BU_INIT_EXTERNAL( &ext2 );
				if( rt_functab[ID_POLY].ft_export( &ext2, &intern, 1.0, (struct db_i *)NULL, &rt_uniresource ) < 0 )  {
					bu_log( "Unable to export v4 BoT %s\n", poly[0].p.p_name );
					bu_bomb( "Unable to convert!!!\n" );
				}
				rt_db_free_internal( &intern, &rt_uniresource );
				if( db_fwrite_external( ofp, poly[0].p.p_name, &ext2 ) < 0 )  {
					bu_log( "Unable to fwrite v4 BoT %s\n", poly[0].p.p_name );
					bu_bomb( "Unable to convert!!!\n" );
				}
				db_free_external( &ext2 );

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
						bu_log( "This is not a BRL-CAD 'v4' database, aborting.\n" );
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
	bu_log( "%d BOT solids copied without change\n", bots );
	bu_log( "%d other records copied without change\n", others );
	bu_log( "%d free records skipped\n", frees );

	fclose(ofp);
	return 0;
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
