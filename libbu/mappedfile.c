/*
 *			M A P P E D F I L E . C
 *
 *  Routines for sharing large read-only data files
 *  like height fields, bit map solids, texture maps, etc.
 *  Uses memory mapped files where available.
 *
 *  Each instance of the file has the raw data available as element "buf".
 *  If a particular application needs to transform the raw data in a
 *  manner that is identical across all uses of that application
 *  (e.g. height fields, EBMs, etc), then the application should
 *  provide a non-null "appl" string, to tag the format of the "apbuf".
 *  This will keep different applications from sharing that instance
 *  of the file.
 *  Thus, if the same filename is opened for interpretation as both
 *  an EBM and a height field, they will be assigned different mapped file
 *  structures, so that the "apbuf" pointers are distinct.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
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

#undef HAVE_SYS_MMAN_H
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "bu.h"

static struct bu_list	bu_mapped_file_list = {
	0,
	(struct bu_list *)NULL,
	(struct bu_list *)NULL
};	/* list of currently open mapped files */

#define FILE_LIST_SEMAPHORE_NUM	1	/* Anything but BU_SEM_SYSCALL */

/*
 *			B U _ O P E N _ M A P P E D _ F I L E
 *
 *  If the file can not be opened, as descriptive an error message as
 *  possible will be printed, to simplify code handling in the caller.
 */
struct bu_mapped_file *
bu_open_mapped_file( name, appl )
CONST char	*name;		/* file name */
CONST char	*appl;		/* non-null only when app. will use 'apbuf' */
{
	struct bu_mapped_file	*mp;
#ifdef HAVE_UNIX_IO
	struct stat		sb;
#endif
	int			ret;
	int			fd;	/* unix file descriptor */
#ifndef HAVE_UNIX_IO
	FILE			*fp;	/* stdio file pointer */
#endif

	bu_semaphore_acquire(FILE_LIST_SEMAPHORE_NUM);
	if( BU_LIST_UNINITIALIZED( &bu_mapped_file_list ) )  {
		BU_LIST_INIT( &bu_mapped_file_list );
	}
	for( BU_LIST_FOR( mp, bu_mapped_file, &bu_mapped_file_list ) )  {
		BU_CK_MAPPED_FILE(mp);
		if( strcmp( name, mp->name ) )  continue;
		if( appl && strcmp( appl, mp->appl ) )
			continue;
		/* File is already mapped */
		mp->uses++;
		bu_semaphore_release(FILE_LIST_SEMAPHORE_NUM);
		return mp;
	}
	bu_semaphore_release(FILE_LIST_SEMAPHORE_NUM);
	mp = (struct bu_mapped_file *)NULL;

	/* File is not yet mapped, open file read only. */
#ifdef HAVE_UNIX_IO
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ret = stat( name, &sb );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( ret < 0 )  {
		perror(name);
		goto fail;
	}
	if( sb.st_size == 0 )  {
		bu_log("bu_open_mapped_file(%s) 0-length file\n", name);
		goto fail;
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fd = open( name, O_RDONLY );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( fd < 0 )  {
		perror(name);
		goto fail;
	}
#endif /* HAVE_UNIX_IO */

	/* Optimisticly assume that things will proceed OK */
	GETSTRUCT( mp, bu_mapped_file );
	mp->name = bu_strdup( name );
	if( appl ) mp->appl = bu_strdup( appl );

#ifdef HAVE_UNIX_IO
	mp->buflen = sb.st_size;
#  ifdef HAVE_SYS_MMAN_H

	/* Attempt to access as memory-mapped file */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mp->buf = mmap(
		(caddr_t)0, sb.st_size, PROT_READ, MAP_PRIVATE,
		fd, (off_t)0 );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( mp->buf != (caddr_t)-1L )  {
	    	/* OK, it's memory mapped in! */
	    	mp->is_mapped = 1;
	    	/* It's safe to close the fd now, the manuals say */
	} else
#  endif /* HAVE_SYS_MMAN_H */
	{
		/* Allocate a local buffer, and slurp it in */
		mp->buf = bu_malloc( sb.st_size, mp->name );

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		ret = read( fd, mp->buf, sb.st_size );
		bu_semaphore_release(BU_SEM_SYSCALL);

		if( ret != sb.st_size )  {
			perror("read");
			bu_free( mp->buf, mp->name );
			goto fail;
		}
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);

#else /* !HAVE_UNIX_IO */

	/* Read it in with stdio, with no clue how big it is */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fp = fopen( name, "r");
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( fp == NULL )  {
		perror(name);
		goto fail;
	}
	/* Read it once to see how large it is */
	{
		char	buf[32768];
		int	got;
		mp->buflen = 0;

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		while( (got = fread( buf, 1, sizeof(buf), fp )) > 0 )
			mp->buflen += got;
		rewind(fp);
		bu_semaphore_release(BU_SEM_SYSCALL);

	}
	/* Malloc the necessary buffer */
	mp->buf = bu_malloc( mp->buflen, mp->name );

	/* Read it again into the buffer */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ret = fread( mp->buf, mp->buflen, 1, fp );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( ret != 1 )  {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		perror("fread");
		fclose(fp);
		bu_semaphore_release(BU_SEM_SYSCALL);

		bu_log("bu_open_mapped_file() 2nd fread failed? len=%d\n", mp->buflen);
		bu_free( mp->buf, "non-unix fread buf" );
		goto fail;
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);
#endif

	mp->uses = 1;
	mp->l.magic = BU_MAPPED_FILE_MAGIC;

	bu_semaphore_acquire(FILE_LIST_SEMAPHORE_NUM);
	BU_LIST_APPEND( &bu_mapped_file_list, &mp->l );
	bu_semaphore_release(FILE_LIST_SEMAPHORE_NUM);

	return mp;

fail:
	if( mp )  {
		bu_free( mp->name, "mp->name" );
		if( mp->appl ) bu_free( mp->appl, "mp->appl" );
		/* Don't free mp->buf here, it might not be bu_malloced but mmaped */
		bu_free( mp, "mp from bu_open_mapped_file fail");
	}
	bu_log("bu_open_mapped_file(%s) can't open file\n", name);
	return (struct bu_mapped_file *)NULL;
}
/*
 *			B U _ C L O S E _ M A P P E D _ F I L E
 */
void
bu_close_mapped_file( mp )
struct bu_mapped_file	*mp;
{
	int	ret;

	BU_CK_MAPPED_FILE(mp);

	bu_semaphore_acquire(FILE_LIST_SEMAPHORE_NUM);
	if( --mp->uses > 0 )  {
		bu_semaphore_release(FILE_LIST_SEMAPHORE_NUM);
		return;
	}
	BU_LIST_DEQUEUE( &mp->l );
	bu_semaphore_release(FILE_LIST_SEMAPHORE_NUM);

	/* If application pointed mp->apbuf at mp->buf, break that
	 * association so we don't double-free the buffer.
	 */
	if( mp->apbuf == mp->buf )  mp->apbuf = (genptr_t)NULL;

#ifdef HAVE_SYS_MMAN_H
	if( mp->is_mapped )  {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		ret = munmap( mp->buf, mp->buflen );
		bu_semaphore_release(BU_SEM_SYSCALL);
		if( ret < 0 )  perror("munmap");
		/* XXX How to get this chunk of address space back to malloc()? */
	} else
#endif
	{
		bu_free( mp->buf, "bu_mapped_file.buf[]" );
	}
	mp->buf = (genptr_t)NULL;		/* sanity */
	if( mp->apbuf )  {
		bu_free( mp->apbuf, "bu_close_mapped_file() apbuf[]" );
		mp->apbuf = (genptr_t)NULL;	/* sanity */
	}
	bu_free( (genptr_t)mp->name, "bu_mapped_file.name" );
	if( mp->appl )  bu_free( (genptr_t)mp->appl, "bu_mapped_file.appl" );
	bu_free( (genptr_t)mp, "struct bu_mapped_file" );
}
