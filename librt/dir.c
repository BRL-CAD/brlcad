/*
 *			D I R . C
 *
 * Ray Tracing program, GED database directory manager.
 *
 *  Functions -
 *	rt_dirbuild	Read GED database, build directory
 *	rt_dir_lookup	Look up name in directory
 *	rt_dir_add		Add entry to directory
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/db.h"
#include "../h/raytrace.h"
#include "rtdir.h"
#include "debug.h"

static struct directory *DirHead = DIR_NULL;	/* rt_i, eventually */

/*
 *			R T _ D I R B U I L D
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 *
 * Returns -
 *	0	Success
 *	-1	Fatal Error
 */
int
rt_dirbuild(filename, buf, len)
char *filename;
char *buf;
int len;
{
	static union record	record;
	static long	addr;

	bzero( (char *)&rt_i, sizeof(rt_i) );
	if( (rt_i.fd = open(filename, 0)) < 0 )  {
		perror(filename);
		return(-1);
	}
	rt_i.needprep = 1;
	rt_i.file = rt_strdup( filename );
	buf[0] = '\0';

	(void)lseek( rt_i.fd, 0L, 0 );
	(void)read( rt_i.fd, (char *)&record, sizeof record );
	if( record.u_id != ID_IDENT )  {
		rt_log("WARNING:  File is not a proper GED database\n");
		rt_log("This database should be converted before further use.\n");
	}
	(void)lseek( rt_i.fd, 0L, 0 );
	while(1)  {
		addr = lseek( rt_i.fd, 0L, 1 );
		if( (unsigned)read( rt_i.fd, (char *)&record, sizeof record )
				!= sizeof record )
			break;

		if( record.u_id == ID_IDENT )  {
			if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
				rt_log("WARNING: File is Version %s, Program is version %s\n",
					record.i.i_version, ID_VERSION );
			}
			if( buf[0] == '\0' )
				strncpy( buf, record.i.i_title, len );
			continue;
		}
		if( record.u_id == ID_FREE )  {
			continue;
		}
		if( record.u_id == ID_ARS_A )  {
			rt_dir_add( record.a.a_name, addr );

			/* Skip remaining B type records.	*/
			(void)lseek( rt_i.fd,
				(long)(record.a.a_totlen) *
				(long)(sizeof record),
				1 );
			continue;
		}

		if( record.u_id == ID_SOLID )  {
			rt_dir_add( record.s.s_name, addr );
			continue;
		}
		if( record.u_id == ID_MATERIAL )  {
			rt_color_addrec( &record, addr );
			continue;
		}
		if( record.u_id == ID_P_HEAD )  {
			union record rec;
			register int nrec;
			register int j;
			nrec = 1;
			while(1) {
				j = read( rt_i.fd, (char *)&rec, sizeof(rec) );
				if( j != sizeof(rec) )
					break;
				if( rec.u_id != ID_P_DATA )  {
					lseek( rt_i.fd, -(sizeof(rec)), 1 );
					break;
				}
				nrec++;
			}
			rt_dir_add( record.p.p_name, addr );
			continue;
		}
		if( record.u_id == ID_BSOLID )  {
			rt_dir_add( record.B.B_name, addr );
			continue;
		}
		if( record.u_id == ID_BSURF )  {
			register int j;
			/* Just skip over knots and control mesh */
			j = (record.d.d_nknots + record.d.d_nctls) *
				sizeof(union record);
			lseek( rt_i.fd, j, 1 );
			rt_dir_add( record.p.p_name, addr );
			continue;
		}
		if( record.u_id != ID_COMB )  {
			rt_log("rt_dirbuild:  unknown record %c (0%o)\n",
				record.u_id, record.u_id );
			/* skip this record */
			continue;
		}

		rt_dir_add( record.c.c_name, addr );
		/* Skip over member records */
		(void)lseek( rt_i.fd,
			(long)record.c.c_length * (long)sizeof record,
			1 );
	}
	return(0);	/* OK */
}

/*
 *			R T _ D I R _ L O O K U P
 *
 * This routine takes a name, and looks it up in the
 * directory table.  If the name is present, a pointer to
 * the directory struct element is returned, otherwise
 * NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only
 * the return code indicates failure.
 */
struct directory *
rt_dir_lookup( str, noisy )
register char *str;
{
	register struct directory *dp;

	for( dp = DirHead; dp != DIR_NULL; dp=dp->d_forw )  {
		if(
			str[0] == dp->d_namep[0]  &&	/* speed */
			str[1] == dp->d_namep[1]  &&	/* speed */
			strcmp( str, dp->d_namep ) == 0
		)
			return(dp);
	}

	if( noisy )
		rt_log("rt_dir_lookup:  could not find '%s'\n", str );
	return( DIR_NULL );
}

/*
 *			R T _ D I R _ A D D
 *
 * Add an entry to the directory
 */
struct directory *
rt_dir_add( name, laddr )
register char *name;
long laddr;
{
	register struct directory *dp;

	GETSTRUCT( dp, directory );
	dp->d_namep = rt_strdup( name );
	dp->d_addr = laddr;
	dp->d_forw = DirHead;
	DirHead = dp;
	return( dp );
}
