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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <unistd.h>
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
 *  As a convenience, dbi_filepath is a C-style argv array of dirs to search
 *  when attempting to open related files (such as data files for EBM
 *  solids or texture-maps).  The default values are "." and the
 *  directory containing the ".g" file.  They may be overriden by
 *  setting the environment variable BRLCAD_FILE_PATH.
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
	register struct db_i	*dbip = DBI_NULL;
	register int		i;

	if(rt_g.debug&DEBUG_DB) bu_log("db_open(%s, %s)\n", name, mode );

	if( mode[0] == 'r' && mode[1] == '\0' )  {
		struct bu_mapped_file	*mfp;
		/* Read-only mode */
		mfp = bu_open_mapped_file( name, "db_i" );
		if( mfp == NULL )  goto fail;

		/* Is this a re-use of a previously mapped file? */
		if( mfp->apbuf )  {
			dbip = (struct db_i *)mfp->apbuf;
			RT_CK_DBI(dbip);
			dbip->dbi_uses++;
			if(rt_g.debug&DEBUG_DB)
				bu_log("db_open(%s) dbip=x%x: reused previously mapped file\n", name, dbip);
			return dbip;
		}

		BU_GETSTRUCT( dbip, db_i );
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
		BU_GETSTRUCT( dbip, db_i );
		dbip->dbi_eof = -1L;

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

	/* Initialize fields */
	for( i=0; i<RT_DBNHASH; i++ )
		dbip->dbi_Head[i] = DIR_NULL;

	dbip->dbi_local2base = 1.0;		/* mm */
	dbip->dbi_base2local = 1.0;
	dbip->dbi_title = (char *)0;
	dbip->dbi_uses = 1;

	/* Record the filename and file path */
	dbip->dbi_filename = bu_strdup(name);

	/* XXX At some point, expand with getenv("BRLCAD_FILE_PATH"); */
	{
		char **argv = (char **)bu_malloc( 3 * sizeof(char *), "dbi_filepath[3]" );
		argv[0] = bu_strdup( "." );
		argv[1] = bu_dirname( name );
		argv[2] = NULL;
		dbip->dbi_filepath = argv;
	}

	bu_ptbl_init( &dbip->dbi_clients, 128, "dbi_clients[]" );
	dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */

	if(rt_g.debug&DEBUG_DB)
		bu_log("db_open(%s) dbip=x%x\n", dbip->dbi_filename, dbip);
	return dbip;
fail:
	if(rt_g.debug&DEBUG_DB)
		bu_log("db_open(%s) FAILED\n", name);
	if(dbip) bu_free( (char *)dbip, "struct db_i" );
	return DBI_NULL;
}

/*
 *			D B _ C R E A T E
 *
 *  Create a new database containing just a header record,
 *  regardless of whether the database previously existed or not,
 *  and open it for reading and writing.
 *
 *  New in BRL-CAD Release 6.0 is that this routine also calls
 *  db_dirbuild(), so the caller shouldn't.
 *
 *
 *  Returns:
 *	DBI_NULL	error
 *	db_i *		success
 */
struct db_i *
db_create( CONST char *name, int version )
{
	FILE	*fp;
	struct db_i	*dbip;

	if(rt_g.debug&DEBUG_DB) bu_log("db_create(%s, %d)\n", name, version );

	if( (fp = fopen( name, "w" )) == NULL )  {
		perror(name);
		return(DBI_NULL);
	}

	switch( version )  {
	default:
	case 5:
		/* Create a v5 database */
		if( db5_fwrite_ident( fp, "Untitled v5 BRL-CAD Database", 1.0 ) < 0 )  {
			(void)fclose(fp);
			return DBI_NULL;
		}
		break;
	case 4:
		/* Create a v4 database */
		if( db_fwrite_ident( fp, "Untitled v4 BRL-CAD Database", 1.0 ) < 0 )  {
			(void)fclose(fp);
			return DBI_NULL;
		}
		break;
	}

	(void)fclose(fp);

	if( (dbip = db_open( name, "r+w" ) ) == DBI_NULL )
		return DBI_NULL;

	/* Do a quick scan to determine version, find _GLOBAL, etc. */
	if( db_dirbuild( dbip ) < 0 )
		return DBI_NULL;
	return dbip;
}

/*
 *			D B _ C L O S E _ C L I E N T
 *
 *  De-register a client of this database instance, and close out the instance.
 */
void
db_close_client( dbip, client )
struct db_i	*dbip;
long		*client;
{
	RT_CK_DBI(dbip);
	(void)bu_ptbl_rm( &dbip->dbi_clients, client );
	db_close(dbip);
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

	if( (--dbip->dbi_uses) > 0 )  {
		if(dbip->dbi_mf) bu_close_mapped_file( dbip->dbi_mf );
		return;
	}
	/* Use count is now zero */

	if( dbip->dbi_mf )  {
		/*
		 *  We're using an instance of a memory mapped file.
		 *  We have two choices:
		 *  Either deassociate from the memory mapped file
		 *  by clearing dbi_mf->apbuf, or
		 *  keeping our already-scanned dbip ready for
		 *  further use, with our dbi_uses counter at 0.
		 *  For speed of re-open, at the price of some address space,
		 *  the second choice is taken.
		 */
		bu_close_mapped_file( dbip->dbi_mf );
		return;
	}

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

	dbip->dbi_inmem = NULL;		/* sanity */

	bu_ptbl_free(&dbip->dbi_clients);

	/* Free all directory entries */
	for( i=0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; )  {
			RT_CK_DIR(dp);
			nextdp = dp->d_forw;
			RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */

			/* Put 'dp' back on the freelist */
			dp->d_forw = rt_uniresource.re_directory_hd;
			rt_uniresource.re_directory_hd = dp;
			dp->d_forw = NULL;

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
/* XXX Need to go to internal form, if database versions don't match */
			if( db_get_external( &ext, dp, dbip ) < 0 )  {
				bu_log("db_dump() read failed on %s, skipping\n", dp->d_namep );
				continue;
			}
			if( wdb_export_external( wdbp, &ext, dp->d_namep, dp->d_flags ) < 0 )  {
				bu_log("db_dump() write failed on %s, aborting\n", dp->d_namep);
				bu_free_external( &ext );
				return -1;
			}
			bu_free_external( &ext );
		}
	}
	return 0;
}

/*
 *			D B _ C L O N E _ D B I
 *
 *  Obtain an additional instance of this same database.
 *  The new client is registered at the same time.
 */
struct db_i *
db_clone_dbi( dbip, client )
struct db_i	*dbip;
long		*client;
{
	RT_CK_DBI(dbip);

	dbip->dbi_uses++;
	bu_ptbl_ins_unique( &dbip->dbi_clients, client );
	return dbip;
}

/*
 *			D B _ S Y N C
 *
 *  Ensure that the on-disk database has been completely written
 *  out of the operating system's cache.
 */
void
db_sync( dbip )
struct db_i	*dbip;
{
	RT_CK_DBI(dbip);

#ifdef HAVE_UNIX_IO
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fsync(dbip->dbi_fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
#else
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	sync();
	bu_semaphore_release(BU_SEM_SYSCALL);
#endif
}
