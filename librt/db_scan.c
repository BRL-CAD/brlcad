/*
 *			D B _ S C A N . C
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
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
	if(rt_g.debug&DEBUG_DB) bu_log("db_scan x%x %c (0%o)\n", \
		aaa,rrr.u_id,rrr.u_id ); }

/*
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
db_scan( dbip, handler, do_old_matter )
register struct db_i	*dbip;
int			(*handler)();
int			do_old_matter;
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
	if(rt_g.debug&DEBUG_DB) bu_log("db_scan( x%x, x%x )\n", dbip, handler);

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
				dbip->dbi_localunit = record.i.i_units;
				db_conversions( dbip, dbip->dbi_localunit );
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
				DIR_SOLID );
			break;
		case ID_ARS_B:
			bu_log("db_scan ERROR: Unattached ARS 'B' record\n");
			break;
		case ID_SOLID:
			handler( dbip, record.s.s_name, addr, nrec,
				DIR_SOLID );
			break;
		case DBID_STRSOL:
			for( ; nrec < DB_SS_NGRAN; nrec++ )  {
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, dbip->dbi_fp ) != 1 )
					break;
			}
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.ss.ss_name, addr, nrec,
				DIR_SOLID );
			break;
		case ID_MATERIAL:
			if( do_old_matter ) {
				/* This is common to RT and MGED */
				rt_color_addrec( &record, addr );
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
				DIR_SOLID );
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
				DIR_SOLID );
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
				DIR_SOLID );
			break;
		case DBID_PARTICLE:
			handler( dbip, record.part.p_name, addr, nrec,
				DIR_SOLID );
			break;
		case DBID_PIPE:
			j = bu_glong(record.pwr.pwr_count);
			nrec += j;
			while( j-- > 0 )
				fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.pwr.pwr_name, addr, nrec,
				DIR_SOLID );
			break;
		case DBID_NMG:
			j = bu_glong(record.nmg.N_count);
			nrec += j;
			while( j-- > 0 )
				(void)fread( (char *)&rec2, sizeof(rec2), 1, dbip->dbi_fp );
			next = ftell(dbip->dbi_fp);
			handler( dbip, record.nmg.N_name, addr, nrec,
				DIR_SOLID );
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
			handler( dbip, record.c.c_name, addr, nrec,
				record.c.c_flags == 'R' ?
					DIR_COMB|DIR_REGION : DIR_COMB );
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

/*
 *			D B _ I D E N T
 *
 *  Update the IDENT record with new title and units.
 *  To permit using db_get and db_put, a custom directory entry is crafted.
 *
 *  Note:  Special care is required, because the "title" arg may actually
 *  be passed in as dbip->dbi_title.
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db_ident( dbip, new_title, units )
struct db_i	*dbip;
char		*new_title;
int		units;
{
	struct directory	dir;
	union record		rec;
	char			*old_title;

	RT_CK_DBI(dbip);
	if(rt_g.debug&DEBUG_DB) bu_log("db_ident( x%x, '%s', %d )\n",
		dbip, new_title, units );

	if( dbip->dbi_read_only )
		return(-1);

	dir.d_namep = "/IDENT/";
	dir.d_addr = 0L;
	dir.d_len = 1;
	dir.d_magic = RT_DIR_MAGIC;
	if( db_get( dbip, &dir, &rec, 0, 1 ) < 0 ||
	    rec.u_id != ID_IDENT )
		return(-1);

	rec.i.i_title[0] = '\0';
	(void)strncpy(rec.i.i_title, new_title, sizeof(rec.i.i_title)-1 );

	old_title = dbip->dbi_title;
	dbip->dbi_title = bu_strdup( new_title );
	dbip->dbi_localunit = rec.i.i_units = units;

	if( old_title )
		rt_free( old_title, "old dbi_title" );

	return( db_put( dbip, &dir, &rec, 0, 1 ) );
}

/*
 *			D B _ C O N V E R S I O N S
 *
 *	Builds conversion factors given the local unit
 */
void
db_conversions( dbip, local )
struct db_i	*dbip;
int local;
{
	RT_CK_DBI(dbip);

	/* Base unit is MM */
	switch( local ) {

	case ID_NO_UNIT:
		/* no local unit specified ... use the base unit */
		dbip->dbi_localunit = ID_MM_UNIT;
		dbip->dbi_local2base = 1.0;
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

	case ID_IN_UNIT:
		/* local unit is inches */
		dbip->dbi_local2base = 25.4;		/* IN to MM */
		break;

	case ID_FT_UNIT:
		/* local unit is feet */
		dbip->dbi_local2base = 304.8;		/* FT to MM */
		break;

	default:
		dbip->dbi_local2base = 1.0;
		dbip->dbi_localunit = 6;
		break;
	}
	dbip->dbi_base2local = 1.0 / dbip->dbi_local2base;
}
