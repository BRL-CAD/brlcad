/*
 *			D B _ O P E N . C
 *
 * Functions -
 *	db_open		Open the database
 *	db_create	Create a new database
 *	db_close	Close a database, releasing dynamic memory
 *
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert Jon Reschly Jr.
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

#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"

#ifdef HAVE_UNIX_IO
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"
#include "wdb.h"

#include "./debug.h"

#ifndef SEEK_SET
# define SEEK_SET	0
#endif

/*
 *  			D B _ O P E N
 *
 *  Open the named database.
 *  The 'mode' parameter specifies read-only or read-write mode.
 *
 *  Returns:
 *	DBI_NULL	error
 *	db_i *		success
 */
struct db_i *
db_open( name, mode )
CONST char	*name;
CONST char	*mode;
{
	register struct db_i	*dbip;
	register int		i;
#ifdef HAVE_UNIX_IO
	struct stat		sb;
#endif

	if(rt_g.debug&DEBUG_DB) bu_log("db_open(%s, %s)\n", name, mode );

	BU_GETSTRUCT( dbip, db_i );

	for( i=0; i<RT_DBNHASH; i++ )
		dbip->dbi_Head[i] = DIR_NULL;

	dbip->dbi_eof = -1L;
	dbip->dbi_localunit = 0;		/* mm */
	dbip->dbi_local2base = 1.0;
	dbip->dbi_base2local = 1.0;
	dbip->dbi_title = (char *)0;
	dbip->dbi_uses = 1;

#ifdef HAVE_UNIX_IO
	if( stat( name, &sb ) < 0 )
		goto fail;
#endif

	if( mode[0] == 'r' && mode[1] == '\0' )  {
		struct bu_mapped_file	*mfp;
		/* Read-only mode */
		mfp = bu_open_mapped_file( name, "db_i" );
		if( mfp == NULL )  goto fail;

		/* Is this a re-use of a previously mapped file? */
		if( mfp->apbuf )  {
			bu_free( (genptr_t)dbip, "db_open: unwanted db_i");
			dbip = (struct db_i *)mfp->apbuf;
			RT_CK_DBI(dbip);
			dbip->dbi_uses++;
			if(rt_g.debug&DEBUG_DB)
				bu_log("db_open(%s) dbip=x%x: reused previously mapped file\n", name, dbip);
			return dbip;
		}

		dbip->dbi_mf = mfp;
		dbip->dbi_eof = mfp->buflen;
		dbip->dbi_inmem = mfp->buf;
		dbip->dbi_mf->apbuf = (genptr_t)dbip;

#ifdef HAVE_UNIX_IO
		/* Do this too, so we can seek around on the file */
		if( (dbip->dbi_fd = open( name, O_RDONLY )) < 0 )
			goto fail;
		if( (dbip->dbi_fp = fdopen( dbip->dbi_fd, "r" )) == NULL )
			goto fail;
#else /* HAVE_UNIX_IO */
		if( (dbip->dbi_fp = fopen( name, "r")) == NULL )
			goto fail;
		dbip->dbi_fd = -1;
#endif
		dbip->dbi_read_only = 1;
	}  else  {
		/* Read-write mode */
#		ifdef HAVE_UNIX_IO
			if( (dbip->dbi_fd = open( name, O_RDWR )) < 0 )
				goto fail;
			if( (dbip->dbi_fp = fdopen( dbip->dbi_fd, "r+w" )) == NULL )
				goto fail;
#		else /* HAVE_UNIX_IO */
			if( (dbip->dbi_fp = fopen( name, "r+w")) == NULL )
				goto fail;
			dbip->dbi_fd = -1;
#		endif
		dbip->dbi_read_only = 0;
	}

	dbip->dbi_filename = bu_strdup(name);
	dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */

	if(rt_g.debug&DEBUG_DB)
		bu_log("db_open(%s) dbip=x%x\n", dbip->dbi_filename, dbip);
	return dbip;
fail:
	if(rt_g.debug&DEBUG_DB)
		bu_log("db_open(%s) FAILED\n", name);
	bu_free( (char *)dbip, "struct db_i" );
	return DBI_NULL;
}

/*
 *			D B _ C R E A T E
 *
 *  Create a new database containing just an IDENT record,
 *  regardless of whether it previously existed or not,
 *  and open it for reading and writing.
 *
 *
 *  Returns:
 *	DBI_NULL	error
 *	db_i *		success
 */
struct db_i *
db_create( name )
CONST char *name;
{
	union record new;

	if(rt_g.debug&DEBUG_DB) bu_log("db_create(%s, %s)\n", name );

	/* Prepare the IDENT record */
	bzero( (char *)&new, sizeof(new) );
	new.i.i_id = ID_IDENT;
	new.i.i_units = ID_MM_UNIT;
	strncpy( new.i.i_version, ID_VERSION, sizeof(new.i.i_version) );
	strcpy( new.i.i_title, "Untitled MGED Database" );

#ifdef HAVE_UNIX_IO
	{
		int	fd;
		if( (fd = creat(name, 0644)) < 0 ||
		    write( fd, (char *)&new, sizeof(new) ) != sizeof(new) )
			return(DBI_NULL);
		(void)close(fd);
	}
#else /* HAVE_UNIX_IO */
	{
		FILE	*fp;
		if( (fp = fopen( name, "w" )) == NULL )
			return(DBI_NULL);
		(void)fwrite( (char *)&new, 1, sizeof(new), fp );
		(void)fclose(fp);
	}
#endif

	return( db_open( name, "r+w" ) );
}

/*
 *			D B _ C L O S E
 *
 *  Close a database, releasing dynamic memory
 *  Wait until last user is done, though.
 */
void
db_close( dbip )
register struct db_i	*dbip;
{
	register int		i;
	register struct directory *dp, *nextdp;

	RT_CK_DBI(dbip);
	if(rt_g.debug&DEBUG_DB) bu_log("db_close(%s) x%x uses=%d\n",
		dbip->dbi_filename, dbip, dbip->dbi_uses );

	if( (--dbip->dbi_uses) > 0 )  return;

#ifdef HAVE_UNIX_IO
	(void)close( dbip->dbi_fd );
#endif
	fclose( dbip->dbi_fp );
	if( dbip->dbi_title )
		bu_free( dbip->dbi_title, "dbi_title" );
	if( dbip->dbi_filename )
		bu_free( dbip->dbi_filename, "dbi_filename" );

	db_free_anim( dbip );
	rt_color_free();		/* Free MaterHead list */

	/* Release map of database holes */
	rt_mempurge( &(dbip->dbi_freep) );
	rt_memclose();

	/* dbi_inmem */

	/* Free all directory entries */
	for( i=0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; )  {
			RT_CK_DIR(dp);
			nextdp = dp->d_forw;
			bu_free( dp->d_namep, "d_namep" );
			dp->d_namep = (char *)NULL;
			bu_free( (char *)dp, "dir");
			dp = nextdp;
		}
		dbip->dbi_Head[i] = DIR_NULL;	/* sanity*/
	}

	bu_free( (char *)dbip, "struct db_i" );
}

/*
 *			D B _ D U M P
 *
 *  Dump a full copy of one database into another.
 *  This is a good way of committing a ".inmem" database to a ".g" file.
 *  The input is a database instance, the output is a LIBWDB object,
 *  which could be a disk file or another database instance.
 *
 *  Returns -
 *	-1	error
 *	0	success
 */
int
db_dump( wdbp, dbip )
struct rt_wdb	*wdbp;		/* output */
struct db_i	*dbip;		/* input */
{
	register int		i;
	register struct directory *dp;
	struct bu_external	ext;

	RT_CK_DBI(dbip);
	RT_CK_WDB(wdbp);

	/* Output all directory entries */
	for( i=0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			RT_CK_DIR(dp);
			if( db_get_external( &ext, dp, dbip ) < 0 )  {
				bu_log("db_dump() read failed on %s, skipping\n", dp->d_namep );
				continue;
			}
			if( wdb_export_external( wdbp, &ext, dp->d_namep, dp->d_flags ) < 0 )  {
				bu_log("db_dump() write failed on %s, aborting\n", dp->d_namep);
				db_free_external( &ext );
				return -1;
			}
			db_free_external( &ext );
		}
	}
	return 0;
}
