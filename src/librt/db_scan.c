/*                       D B _ S C A N . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup db4 */

/*@{*/
/** @file db_scan.c
 *
 * Functions -
 *	db_scan		Sequentially read database, send objects to handler()
 *	db_ident	Update database IDENT record
 *      db_conversions  Update unit conversion factors
 *
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

#define DEBUG_PR(aaa, rrr) 	{\
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_scan x%x %c (0%o)\n", \
		aaa,rrr.u_id,rrr.u_id ); }

/**
 *			D B _ S C A N
 *
 *  This routine sequentially reads through the model database file and
 *  builds a directory of the object names, to allow rapid
 *  named access to objects.
 *
 *  Note that some multi-record database items include length fields.
 *  These length fields are not used here.
 *  Instead, the sizes of multi-record items are determined by
 *  reading ahead and computing the actual size.
 *  This prevents difficulties arising from external "adjustment" of
 *  the number of records without corresponding adjustment of the length fields.
 *  In the future, these length fields will be phased out.
 *
 *  The handler will be called with a variety of args.
 *  The handler is responsible for handling name strings of exactly
 *  NAMESIZE chars.
 *  The most common example of such a function is db_diradd().
 *
 *  Note that the handler may do I/O, including repositioning the
 *  file pointer, so this must be taken into account.
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db_scan(register struct db_i *dbip, int (*handler) (struct db_i *, const char *, long int, int, int, genptr_t), int do_old_matter, genptr_t client_data)



        		            	/* argument for handler */
{
	union record	record;		/* Initial record, holds name */
	union record	rec2;		/* additional record(s) */
	register long	addr;		/* start of current rec */
	register long	here;		/* intermediate positions */
	register long	next;		/* start of next rec */
	register int	nrec;		/* # records for this solid */
	register int	totrec;		/* # records for database */
	register int	j;

	RT_CK_DBI(dbip);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_scan( x%x, x%x )\n", dbip, handler);

	/* XXXX Note that this ignores dbip->dbi_inmem */
	/* In a portable way, read the header (even if not rewound) */
	rewind( dbip->dbi_fp );
	if( fread( (char *)&record, sizeof record, 1, dbip->dbi_fp ) != 1  ||
	    record.u_id != ID_IDENT )  {
		bu_log("db_scan ERROR:  File is lacking a proper MGED database header\n");
	    	return(-1);
	}
	rewind( dbip->dbi_fp );
	next = ftell(dbip->dbi_fp);

	here = addr = -1L;
	totrec = 0;
	while(1)  {
		nrec = 0;
		if( fseek(dbip->dbi_fp, next, 0) != 0 )  {
			bu_log("db_scan:  fseek(offset=%d) failure\n", next);
			return(-1);
		}
		addr = next;

		if( fread( (char *)&record, sizeof record, 1, dbip->dbi_fp ) != 1
		    || feof(dbip->dbi_fp) )
			break;
		next = ftell(dbip->dbi_fp);
		DEBUG_PR( addr, record );

		nrec++;
		switch( record.u_id )  {
		case ID_IDENT:
			if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
				bu_log("db_scan WARNING: File is Version %s, Program is version %s\n",
					record.i.i_version, ID_VERSION );
			}
			/* Record first IDENT records title string */
			if( dbip->dbi_title == (char *)0 )  {
				dbip->dbi_title = bu_strdup( record.i.i_title );
				db_conversions( dbip, record.i.i_units );
			}
			break;
		case ID_FREE:
			/* Inform db manager of avail. space */
			rt_memfree( &(dbip->dbi_freep), (unsigned)1,
				addr/sizeof(union record) );
			break;
		case ID_ARS_A:
			while(1) {
				here = ftell( dbip->dbi_fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
				DEBUG_PR( here, rec2 );
				if( rec2.u_id != ID_ARS_B )  {
					fseek( dbip->dbi_fp, here, 0 );
					break;
				}
				nrec++;
			}
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.a.a_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case ID_ARS_B:
			bu_log("db_scan ERROR: Unattached ARS 'B' record\n");
			break;
		case ID_SOLID:
			handler( dbip, record.s.s_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_STRSOL:
			for( ; nrec < DB_SS_NGRAN; nrec++ )  {
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
			}
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.ss.ss_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case ID_MATERIAL:
			if( do_old_matter ) {
				/* This is common to RT and MGED */
				rt_color_addrec(
					record.md.md_low,
					record.md.md_hi,
					record.md.md_r,
					record.md.md_g,
					record.md.md_b,
					addr );
			}
			break;
		case ID_P_HEAD:
			while(1) {
				here = ftell( dbip->dbi_fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
				DEBUG_PR( here, rec2 );
				if( rec2.u_id != ID_P_DATA )  {
					fseek( dbip->dbi_fp, here, 0 );
					break;
				}
				nrec++;
			}
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.p.p_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case ID_P_DATA:
			bu_log("db_scan ERROR: Unattached P_DATA record\n");
			break;
		case ID_BSOLID:
			while(1) {
				/* Find and skip subsequent BSURFs */
				here = ftell( dbip->dbi_fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
				DEBUG_PR( here, rec2 );
				if( rec2.u_id != ID_BSURF )  {
					fseek( dbip->dbi_fp, here, 0 );
					break;
				}

				/* Just skip over knots and control mesh */
				j = (rec2.d.d_nknots + rec2.d.d_nctls);
				nrec += j+1;
				while( j-- > 0 )
					fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
				next = ftell(dbip->dbi_fp);
			}
			handler( dbip, record.B.B_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case ID_BSURF:
			bu_log("db_scan ERROR: Unattached B-spline surface record\n");

			/* Just skip over knots and control mesh */
			j = (record.d.d_nknots + record.d.d_nctls);
			nrec += j;
			while( j-- > 0 )
				fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			break;
		case DBID_ARBN:
			j = bu_glong(record.n.n_grans);
			nrec += j;
			while( j-- > 0 )
				fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.n.n_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_PARTICLE:
			handler( dbip, record.part.p_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_PIPE:
			j = bu_glong(record.pwr.pwr_count);
			nrec += j;
			while( j-- > 0 )
				fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.pwr.pwr_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_NMG:
			j = bu_glong(record.nmg.N_count);
			nrec += j;
			while( j-- > 0 )
				(void)fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.nmg.N_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_SKETCH:
			j = bu_glong(record.skt.skt_count);
			nrec += j;
			while( j-- > 0 )
				(void)fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.skt.skt_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_EXTR:
			j = bu_glong(record.extr.ex_count);
			nrec += j;
			while( j-- > 0 )
				(void)fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.extr.ex_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_CLINE:
			handler( dbip, record.s.s_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case DBID_BOT:
			j = bu_glong( record.bot.bot_nrec );
			nrec += j;
			while( j-- > 0 )
				(void)fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.s.s_name, addr, nrec,
				DIR_SOLID, client_data );
			break;
		case ID_MEMB:
			bu_log("db_scan ERROR: Unattached combination MEMBER record\n");
			break;
		case ID_COMB:
			while(1) {
				here = ftell( dbip->dbi_fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
				DEBUG_PR( here, rec2 );
				if( rec2.u_id != ID_MEMB )  {
					fseek( dbip->dbi_fp, here, 0 );
					break;
				}
				nrec++;
			}
			next = ftell(dbip->dbi_fp);
			switch(record.c.c_flags)  {
			default:
			case DBV4_NON_REGION:
				j = DIR_COMB;
				break;
			case DBV4_REGION:
			case DBV4_REGION_FASTGEN_PLATE:
			case DBV4_REGION_FASTGEN_VOLUME:
				j = DIR_COMB|DIR_REGION;
				break;
			}
			handler( dbip, record.c.c_name, addr, nrec, j,
				client_data );
			break;
		default:
			bu_log("db_scan ERROR:  bad record %c (0%o), addr=x%x\n",
				record.u_id, record.u_id, addr );
			/* skip this record */
			break;
		}
		totrec += nrec;
	}
	dbip->dbi_nrec = totrec;
	dbip->dbi_eof = ftell( dbip->dbi_fp );
	rewind( dbip->dbi_fp );

	return( 0 );			/* OK */
}

/**
 *			D B _ U P D A T E _ I D E N T
 *
 *  Update the existing v4 IDENT record with new title and units.
 *  To permit using db_get and db_put, a custom directory entry is crafted.
 *
 *  Note:  Special care is required, because the "title" arg may actually
 *  be passed in as dbip->dbi_title.
 */
int
db_update_ident( struct db_i *dbip, const char *new_title, double local2mm )
{
	struct directory	dir;
	union record		rec;
	char			*old_title;
	int			v4units;

	RT_CK_DBI(dbip);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_update_ident( x%x, '%s', %g )\n",
		dbip, new_title, local2mm );

	BU_ASSERT_LONG( dbip->dbi_version, >, 0 );

	if( dbip->dbi_read_only )
		return(-1);

	if( dbip->dbi_version > 4 )  return db5_update_ident( dbip, new_title, local2mm );

	RT_DIR_SET_NAMEP(&dir, "/IDENT/");
	dir.d_addr = 0L;
	dir.d_len = 1;
	dir.d_magic = RT_DIR_MAGIC;
	dir.d_flags = 0;
	if( db_get( dbip, &dir, &rec, 0, 1 ) < 0 ||
	    rec.u_id != ID_IDENT )  {
	    	bu_log("db_update_ident() corrupted database header!\n");
	    	dbip->dbi_read_only = 1;
		return(-1);
	}

	rec.i.i_title[0] = '\0';
	(void)strncpy(rec.i.i_title, new_title, sizeof(rec.i.i_title)-1 );

	old_title = dbip->dbi_title;
	dbip->dbi_title = bu_strdup( new_title );

	if( (v4units = db_v4_get_units_code(bu_units_string(local2mm))) < 0 )  {
		bu_log("db_update_ident(): \
Due to a restriction in previous versions of the BRL-CAD database format, your\n\
editing units %g will not be remembered on your next editing session.\n\
This will not harm the integrity of your database.\n\
You may wish to consider upgrading your database using \"dbupgrade\".\n",
			local2mm);
		v4units = ID_MM_UNIT;
	}
	rec.i.i_units = v4units;

	if( old_title )
		bu_free( old_title, "old dbi_title" );

	return( db_put( dbip, &dir, &rec, 0, 1 ) );

}

/**
 *			D B _ F W R I T E _ I D E N T
 *
 *  Fwrite an IDENT record with given title and editing scale.
 *  Attempts to map the editing scale into a v4 database unit
 *  as best it can.  No harm done if it doesn't map.
 *
 *  This should be called by db_create() only.
 *  All others should call db_update_ident().
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db_fwrite_ident( FILE *fp, const char *title, double local2mm )
{
	union record		rec;
	int			code;

	code = db_v4_get_units_code(bu_units_string(local2mm));

	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_fwrite_ident( x%x, '%s', %g ) code=%d\n",
		fp, title, local2mm, code );

	bzero( (char *)&rec, sizeof(rec) );
	rec.i.i_id = ID_IDENT;
	rec.i.i_units = code;
	(void)strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	(void)strncpy(rec.i.i_title, title, sizeof(rec.i.i_title)-1 );

	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return -1;
	return 0;
}

/**
 *			D B _ C O N V E R S I O N S
 *
 *	Initialize conversion factors given the v4 database unit
 */
void
db_conversions(struct db_i *dbip, int local)

          					/* one of ID_??_UNIT */
{
	RT_CK_DBI(dbip);

	/* Base unit is MM */
	switch( local ) {

	case ID_NO_UNIT:
		/* no local unit specified ... use the base unit */
		dbip->dbi_local2base = 1.0;
		break;

	case ID_UM_UNIT:
		/* local unit is um */
		dbip->dbi_local2base = 0.001;		/* UM to MM */
		break;

	case ID_MM_UNIT:
		/* local unit is mm */
		dbip->dbi_local2base = 1.0;
		break;

	case ID_CM_UNIT:
		/* local unit is cm */
		dbip->dbi_local2base = 10.0;		/* CM to MM */
		break;

	case ID_M_UNIT:
		/* local unit is meters */
		dbip->dbi_local2base = 1000.0;		/* M to MM */
		break;

	case ID_KM_UNIT:
		/* local unit is km */
		dbip->dbi_local2base = 1000000.0;	/* KM to MM */
		break;

	case ID_IN_UNIT:
		/* local unit is inches */
		dbip->dbi_local2base = 25.4;		/* IN to MM */
		break;

	case ID_FT_UNIT:
		/* local unit is feet */
		dbip->dbi_local2base = 304.8;		/* FT to MM */
		break;

	case ID_YD_UNIT:
		/* local unit is yards */
		dbip->dbi_local2base = 914.4;		/* YD to MM */
		break;

	case ID_MI_UNIT:
		/* local unit is miles */
		dbip->dbi_local2base = 1609344;		/* MI to MM */
		break;

	default:
		dbip->dbi_local2base = 1.0;
		break;
	}
	dbip->dbi_base2local = 1.0 / dbip->dbi_local2base;
}

/**
 *			D B _ V 4 _ G E T _ U N I T S _ C O D E
 *
 *  Given a string, return the V4 database code representing
 *  the user's preferred editing units.
 *  The v4 database format does not have many choices.
 *
 *  Returns -
 *	-1	Not a legal V4 database code
 *	#	The V4 database code number
 */
int
db_v4_get_units_code( const char *str )
{
	if( !str )  return ID_NO_UNIT;	/* no units specified */

	if( strcmp(str, "mm") == 0 || strcmp(str, "millimeters") == 0 )
		return ID_MM_UNIT;
	if( strcmp(str, "um") == 0 || strcmp(str, "micrometers") == 0)
		return ID_UM_UNIT;
	if( strcmp(str, "cm") == 0 || strcmp(str, "centimeters") == 0)
		return ID_CM_UNIT;
	if( strcmp(str,"m")==0 || strcmp(str,"meters")==0 )
		return ID_M_UNIT;
	if( strcmp(str, "km") == 0 || strcmp(str, "kilometers") == 0)
		return ID_KM_UNIT;
	if( strcmp(str,"in")==0 || strcmp(str,"inches")==0 || strcmp(str,"inch")==0 )
		return ID_IN_UNIT;
	if( strcmp(str,"ft")==0 || strcmp(str,"feet")==0 || strcmp(str,"foot")==0 )
		return ID_FT_UNIT;
	if( strcmp(str,"yd")==0 || strcmp(str,"yards")==0 || strcmp(str,"yard")==0 )
		return ID_YD_UNIT;
	if( strcmp(str,"mi")==0 || strcmp(str,"miles")==0 || strcmp(str,"mile")==0 )
		return ID_MI_UNIT;

	return -1;		/* error */
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
