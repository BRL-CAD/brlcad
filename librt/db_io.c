/*
 *			D B _ I O . C
 *
 * Functions -
 *	db_getmrec	Read all records into malloc()ed memory chunk
 *	db_get		Get records from database
 *	db_put		Put records to database
 *
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./debug.h"

/*
 *  			D B _ G E T M R E C
 *
 *  Retrieve all records in the database pertaining to an object,
 *  and place them in malloc()'ed storage, which the caller is
 *  responsible for free()'ing.
 *
 *  Returns -
 *	union record *		OK
 *	(union record *)0	failure
 */
union record *
db_getmrec( dbip, dp )
struct db_i	*dbip;
struct directory *dp;
{
	union record	*where;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_getmrec:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB) rt_log("db_getmrec(%s) x%x, x%x\n",
		dp->d_namep, dbip, dp );

	if( dp->d_addr < 0 )
		return( (union record *)0 );	/* dummy DB entry */
	if( (where = (union record *)rt_malloc(
		dp->d_len * sizeof(union record),
		dp->d_namep)
	    ) == ((union record *)0) )
		return( (union record *)0 );

	if( db_get( dbip, dp, where, 0, dp->d_len ) < 0 )  {
		rt_free( (char *)where, dp->d_namep );
		return( (union record *)0 );	/* VERY BAD */
	}
	return( where );
}

/*
 *  			D B _ G E T
 *
 *  Retrieve several records from the database,
 *  "offset" granules into this entry.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
db_get( dbip, dp, where, offset, len )
struct db_i	*dbip;
struct directory *dp;
union record	*where;
int		offset;
int		len;
{
	register int	want;
	register int	got;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_get:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB) rt_log("db_get(%s) x%x, x%x x%x off=%d len=%d\n",
		dp->d_namep, dbip, dp, where, offset, len );

	if( dp->d_addr < 0 )  {
		where->u_id = '\0';	/* undefined id */
		return;
	}
	if( offset < 0 || offset+len > dp->d_len )  {
		rt_log("db_get(%s):  xfer %d..%x exceeds 0..%d\n",
			dp->d_namep, offset, offset+len, dp->d_len );
		where->u_id = '\0';	/* undefined id */
		return;
	}
	if( dbip->dbi_inmem )  {
		register int	start;

		want = len * sizeof(union record);
		start = dp->d_addr + offset * sizeof(union record);
#if defined(SYSV)
		memcpy( (char *)where, dbip->dbi_inmem + start, want );
#else
		bcopy( dbip->dbi_inmem + start, (char *)where, want );
#endif
		return;
	}
#if unix
	want = len * sizeof(union record);
	(void)lseek( dbip->dbi_fd,
		(long)(dp->d_addr + offset * sizeof(union record)), 0 );
	got = read( dbip->dbi_fd, (char *)where, want );
#else
	want = len;
	(void)fseek( dbip->dbi_fp,
		(long)(dp->d_addr + offset * sizeof(union record)), 0 );
	got = fread( (char *)where, want, sizeof(union record), dbip->dbi_fp );
#endif
	if( got != want )  {
		perror("db_get");
		rt_log("db_get(%s):  read error.  Wanted %d, got %d bytes\n",
			dp->d_namep, want, got );
		where->u_id = '\0';	/* undefined id */
		return;
	}
}

/*
 *  			D B _ P U T
 *
 *  Store several records to the database,
 *  "offset" granules into this entry.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
db_put( dbip, dp, where, offset, len )
struct db_i	*dbip;
struct directory *dp;
union record	*where;
int		offset;
int		len;
{
	register int	want;
	register int	got;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_put:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB) rt_log("db_put(%s) x%x, x%x x%x off=%d len=%d\n",
		dp->d_namep, dbip, dp, where, offset, len );

	if( dbip->dbi_read_only )  {
		rt_log("db_put(%s):  READ-ONLY file\n",
			dbip->dbi_filename);
		return;
	}
	if( offset < 0 || offset+len > dp->d_len )  {
		rt_log("db_put(%s):  xfer %d..%x exceeds 0..%d\n",
			dp->d_namep, offset, offset+len, dp->d_len );
		return;
	}
#if unix
	want = len * sizeof(union record);
	(void)lseek( dbip->dbi_fd,
		(long)(dp->d_addr + offset * sizeof(union record)), 0 );
	got = write( dbip->dbi_fd, (char *)where, want );
#else
	want = len;
	(void)fseek( dbip->dbi_fp,
		(long)(dp->d_addr + offset * sizeof(union record)), 0 );
	got = fwrite( (char *)where, want, sizeof(union record), dbip->dbi_fp );
#endif
	if( got != want )  {
		perror("db_put");
		rt_log("db_put(%s):  write error.  Sent %d, achieved %d bytes\n",
			dp->d_namep, want, got );
	}
}
