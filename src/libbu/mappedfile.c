/*                    M A P P E D F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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

/** \addtogroup mf */
/*@{*/

/** @file mappedfile.c
 * @brief Routines for sharing large read-only data files.
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
 *
 *  @author	Michael John Muuss
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */

#ifndef lint
static const char libbu_mappedfile_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"

#ifdef HAVE_UNIX_IO
#  include <sys/types.h>
#  include <sys/stat.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#  if !defined(MAP_FAILED)
#    define MAP_FAILED	((void *)-1)	/* Error return from mmap() */
#  endif
#endif

#include "bu.h"


static struct bu_list	bu_mapped_file_list = {
	0,
	(struct bu_list *)NULL,
	(struct bu_list *)NULL
};	/* list of currently open mapped files */

/**
 *			B U _ O P E N _ M A P P E D _ F I L E
 *
 *  If the file can not be opened, as descriptive an error message as
 *  possible will be printed, to simplify code handling in the caller.
 *
 *  Mapped files are always opened read-only.
 *
 *  If the system does not support mapped files, the data is read into memory.
 */
struct bu_mapped_file *
bu_open_mapped_file(const char *name, const char *appl)
          	      		/* file name */
          	      		/* non-null only when app. will use 'apbuf' */
{
	struct bu_mapped_file	*mp = (struct bu_mapped_file *)NULL;
#ifdef HAVE_UNIX_IO
	struct stat		sb;
	int			fd;	/* unix file descriptor */
#else
	FILE			*fp = (FILE *)NULL;	/* stdio file pointer */
#endif
	int			ret;

	if( bu_debug&BU_DEBUG_MAPPED_FILE )
#ifdef HAVE_SBRK
		bu_log("bu_open_mapped_file(%s, %s) sbrk=x%lx\n", name, appl?appl:"(NIL)", (long)sbrk(0));
#else
		bu_log("bu_open_mapped_file(%s, %s)\n", name, appl?appl:"(NIL)");
#endif

	/* See if file has already been mapped, and can be shared */
	bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
	if( BU_LIST_UNINITIALIZED( &bu_mapped_file_list ) )  {
		BU_LIST_INIT( &bu_mapped_file_list );
	}
	for( BU_LIST_FOR( mp, bu_mapped_file, &bu_mapped_file_list ) )  {
		BU_CK_MAPPED_FILE(mp);
		if( strcmp( name, mp->name ) )  continue;
		if( appl && strcmp( appl, mp->appl ) )
			continue;
		/* File is already mapped -- verify size and modtime */
#ifdef HAVE_UNIX_IO
		if( !mp->dont_restat )  {
			bu_semaphore_acquire(BU_SEM_SYSCALL);
			ret = stat( name, &sb );
			bu_semaphore_release(BU_SEM_SYSCALL);
			if( ret < 0 )  goto do_reuse;	/* File vanished from disk, mapped copy still OK */
			if( sb.st_size != mp->buflen )  {
				bu_log("bu_open_mapped_file(%s) WARNING: File size changed from %ld to %ld, opening new version.\n",
					name, (long)mp->buflen, (long)sb.st_size );
				goto dont_reuse;
			}
			if( (long)sb.st_mtime != mp->modtime )  {
				bu_log("bu_open_mapped_file(%s) WARNING: File modified since last mapped, opening new version.\n",
					name);
				goto dont_reuse;
			}
			/* To be completely safe, should check st_dev and st_inum */
		}
#endif
do_reuse:
		/* It is safe to reuse mp */
		mp->uses++;
		bu_semaphore_release(BU_SEM_MAPPEDFILE);
		if( bu_debug&BU_DEBUG_MAPPED_FILE )
			bu_pr_mapped_file("open_reused", mp);
		return mp;
dont_reuse:
		/* mp doesn't reflect the file any longer.  Invalidate. */
		mp->appl = "__STALE__";
		/* Can't invalidate old copy, it may still be in use. */
		/* Fall through, and open the new version */
	}
	bu_semaphore_release(BU_SEM_MAPPEDFILE);
	mp = (struct bu_mapped_file *)NULL;

	/* File is not yet mapped, open file read only. */
#ifdef HAVE_UNIX_IO
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fd = open( name, O_RDONLY );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( fd < 0 )  {
		if (bu_debug&BU_DEBUG_DB)
			perror(name);
		goto fail;
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ret = fstat( fd, &sb );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( ret < 0 )  {
		perror(name);
		goto fail;
	}

	if( sb.st_size == 0 )  {
		bu_log("bu_open_mapped_file(%s) 0-length file\n", name);
		goto fail;
	}
#endif /* HAVE_UNIX_IO */

	/* Optimisticly assume that things will proceed OK */
	BU_GETSTRUCT( mp, bu_mapped_file );
	mp->name = bu_strdup( name );
	if( appl ) mp->appl = bu_strdup( appl );

#ifdef HAVE_UNIX_IO
	mp->buflen = (size_t)sb.st_size;
	mp->modtime = (long)sb.st_mtime;
#  ifdef HAVE_SYS_MMAN_H

	/* Attempt to access as memory-mapped file */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	mp->buf = mmap((caddr_t)0, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, (off_t)0 );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( mp->buf == MAP_FAILED )  perror(name);
	if( mp->buf != MAP_FAILED )  {
	    	/* OK, it's memory mapped in! */
	    	mp->is_mapped = 1;
	    	/* It's safe to close the fd now, the manuals say */
	} else
#  endif /* HAVE_SYS_MMAN_H */
	{
		/* Allocate a local buffer, and slurp it in */
		mp->buf = bu_malloc( (size_t)sb.st_size, name );

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		ret = read( fd, mp->buf, (size_t)sb.st_size );
		bu_semaphore_release(BU_SEM_SYSCALL);

		if( ret != sb.st_size )  {
			perror(name);
			bu_free( mp->buf, name );
			bu_semaphore_acquire(BU_SEM_SYSCALL);
			(void)close(fd);
			bu_semaphore_release(BU_SEM_SYSCALL);
			goto fail;
		}
	}

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	(void)close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);

#else /* !HAVE_UNIX_IO */

	/* Read it in with stdio, with no clue how big it is */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
#if defined(_WIN32) && !defined(__CYGWIN__)
	fp = fopen( name, "rb");
#else
	fp = fopen( name, "r");
#endif
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( fp == NULL )  {
		perror(name);
		goto fail;
	}
	/* Read it once to see how large it is */
	{
		char	buf[32768] = {0};
		int	got;
		mp->buflen = 0;

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		while( (got = fread( buf, 1, sizeof(buf), fp )) > 0 )
			mp->buflen += got;
		rewind(fp);
		bu_semaphore_release(BU_SEM_SYSCALL);

	}
	/* Malloc the necessary buffer */
	mp->buf = bu_malloc( mp->buflen, name );

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

	bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
	BU_LIST_APPEND( &bu_mapped_file_list, &mp->l );
	bu_semaphore_release(BU_SEM_MAPPEDFILE);

	if( bu_debug&BU_DEBUG_MAPPED_FILE )  {
		bu_pr_mapped_file("1st_open", mp);
#ifdef HAVE_SBRK
		bu_log("bu_open_mapped_file() sbrk=x%lx\n", (long)sbrk(0));
#endif
	}
	return mp;

fail:
	if( mp )  {
		bu_free( mp->name, "mp->name" );
		if( mp->appl ) bu_free( mp->appl, "mp->appl" );
		/* Don't free mp->buf here, it might not be bu_malloced but mmaped */
		bu_free( mp, "mp from bu_open_mapped_file fail");
	}

	if (bu_debug&BU_DEBUG_DB)
	  bu_log("bu_open_mapped_file(%s, %s) can't open file\n",
		 name, appl?appl:"(NIL)" );

	return (struct bu_mapped_file *)NULL;
}

/**
 *			B U _ C L O S E _ M A P P E D _ F I L E
 *
 *  Release a use of a mapped file.
 *  Because it may be re-used shortly, e.g. by the next frame of
 *  an animation, don't release the memory even on final close,
 *  so that it's available when next needed.
 *  Call bu_free_mapped_files() after final close to reclaim space.
 *  But only do that if you're SURE that ALL these files will never again
 *  need to be mapped by this process.  Such as when running multi-frame
 *  animations.
 */
void
bu_close_mapped_file(struct bu_mapped_file *mp)
{
	BU_CK_MAPPED_FILE(mp);

	if( bu_debug&BU_DEBUG_MAPPED_FILE )
		bu_pr_mapped_file("close:uses--", mp);

	if (! mp) {
	    bu_log("bu_close_mapped_file() called with null pointer\n");
	    return;
	}

	bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
	--mp->uses;
	bu_semaphore_release(BU_SEM_MAPPEDFILE);
}

/**
 *			B U _ P R _ M A P P E D _ F I L E
 */
void
bu_pr_mapped_file(const char *title, const struct bu_mapped_file *mp)
{
	BU_CK_MAPPED_FILE(mp);

	bu_log("%8lx mapped_file %s %lx len=%ld mapped=%d, uses=%d %s\n",
		(long)mp, mp->name, (long)mp->buf, mp->buflen,
		mp->is_mapped, mp->uses,
		title );
}

/**
 *			B U _ F R E E _ M A P P E D _ F I L E S
 *
 *  Release storage being used by mapped files with no remaining users.
 *  This entire routine runs inside a critical section, for parallel protection.
 *  Only call this routine if you're SURE that ALL these files will never
 *  again need to be mapped by this process.  Such as when running multi-frame
 *  animations.
 */
void
bu_free_mapped_files(int verbose)
{
	struct bu_mapped_file	*mp, *next;

	if( bu_debug&BU_DEBUG_MAPPED_FILE )
		bu_log("bu_free_mapped_files(verbose=%d)\n", verbose);

	bu_semaphore_acquire(BU_SEM_MAPPEDFILE);

	next = BU_LIST_FIRST( bu_mapped_file, &bu_mapped_file_list );
	while( BU_LIST_NOT_HEAD( next, &bu_mapped_file_list ) )  {
		BU_CK_MAPPED_FILE(next);
		mp = next;
		next = BU_LIST_NEXT( bu_mapped_file, &mp->l );

		if( mp->uses > 0 )  continue;

		/* Found one that needs to have storage released */
		if(verbose || (bu_debug&BU_DEBUG_MAPPED_FILE))
			bu_pr_mapped_file( "freeing", mp );

		BU_LIST_DEQUEUE( &mp->l );

		/* If application pointed mp->apbuf at mp->buf, break that
		 * association so we don't double-free the buffer.
		 */
		if( mp->apbuf == mp->buf )  mp->apbuf = (genptr_t)NULL;

#ifdef HAVE_SYS_MMAN_H
		if( mp->is_mapped )  {
			int	ret;
			bu_semaphore_acquire(BU_SEM_SYSCALL);
			ret = munmap( mp->buf, (size_t)mp->buflen );
			bu_semaphore_release(BU_SEM_SYSCALL);
			if( ret < 0 )  perror("munmap");
			/* XXX How to get this chunk of address space back to malloc()? */
		} else
#endif
		{
			bu_free( mp->buf, "bu_mapped_file.buf[]" );
		}
		mp->buf = (genptr_t)NULL;		/* sanity */
		bu_free( (genptr_t)mp->name, "bu_mapped_file.name" );
		if( mp->appl )  bu_free( (genptr_t)mp->appl, "bu_mapped_file.appl" );
		bu_free( (genptr_t)mp, "struct bu_mapped_file" );
	}
	bu_semaphore_release(BU_SEM_MAPPEDFILE);
}

/**
 *	B U _ O P E N _ M A P P E D _ F I L E _ W I T H _ P A T H
 *
 *  A wrapper for bu_open_mapped_file() which uses a search path
 *  to locate the file.
 *  The search path is specified as a normal C argv array,
 *  terminated by a null string pointer.
 *  If the file name begins with a slash ('/') the path is not used.
 */
struct bu_mapped_file *
bu_open_mapped_file_with_path(char *const *path, const char *name, const char *appl)

          	      		/* file name */
          	      		/* non-null only when app. will use 'apbuf' */
{
	char	* const *pathp = path;
	struct bu_vls	str;
	struct bu_mapped_file	*ret;

	BU_ASSERT_PTR( name, !=, NULL );
	BU_ASSERT_PTR( pathp, !=, NULL );

	/* Do not resort to path for a rooted filename */
	if( name[0] == '/' )
		return bu_open_mapped_file( name, appl );

	bu_vls_init(&str);

	/* Try each path prefix in sequence */
	for( ; *pathp != NULL; pathp++ )  {
		bu_vls_strcpy( &str, *pathp );
		bu_vls_putc( &str, '/' );
		bu_vls_strcat( &str, name );

		ret = bu_open_mapped_file( bu_vls_addr(&str), appl );
		if( ret )  {
			bu_vls_free( &str );
			return ret;
		}
	}

	/* Failure, none of the opens succeeded */
	bu_vls_free( &str );
	return (struct bu_mapped_file *)NULL;
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
