/*                    M A P P E D F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <limits.h> /* for INT_MAX */
#include <math.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#  if !defined(MAP_FAILED)
#    define MAP_FAILED ((void *)-1)	/* Error return from mmap() */
#  endif
#endif
#include "bio.h"

#include "bu/debug.h"
#include "bu/file.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bu/vls.h"


/* list of currently open mapped files */
static struct bu_list bu_mapped_file_list = BU_LIST_INIT_ZERO;


struct bu_mapped_file *
bu_open_mapped_file(const char *name, const char *appl)
/* file name */
/* non-null only when app. will use 'apbuf' */
{
    struct bu_mapped_file *mp = (struct bu_mapped_file *)NULL;
    char *real_path = bu_realpath(name, NULL);
#ifdef HAVE_SYS_STAT_H
    struct stat sb;
    int fd = -1;	/* unix file descriptor */
    int readval;
    ssize_t bytes_to_go, nbytes;
#else
    FILE *fp = (FILE *)NULL;	/* stdio file pointer */
#endif
    int ret;

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_open_mapped_file(%s(canonical path - %s), %s)\n", name, real_path, appl?appl:"(NIL)");

    /* See if file has already been mapped, and can be shared */
    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    if (!BU_LIST_IS_INITIALIZED(&bu_mapped_file_list)) {
	BU_LIST_INIT(&bu_mapped_file_list);
    }

    for (BU_LIST_FOR(mp, bu_mapped_file, &bu_mapped_file_list)) {
	BU_CK_MAPPED_FILE(mp);

	/* find a match */

	if (!BU_STR_EQUAL(real_path, mp->name))
	    continue;
	if (appl && !BU_STR_EQUAL(appl, mp->appl))
	    continue;

	/* found a match */

	/* if mapped file still exists, verify size and modtime */
	if (!mp->dont_restat) {

	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fd = open(real_path, O_RDONLY | O_BINARY);
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    /* If file didn't vanish from disk, make sure it's the same file */
	    if (fd >= 0) {

#ifdef HAVE_SYS_STAT_H
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		ret = fstat(fd, &sb);
		bu_semaphore_release(BU_SEM_SYSCALL);

		if (ret < 0) {
		    /* odd, open worked but fstat failed.  assume it
		     * vanished from disk and the mapped copy is still
		     * OK.
		     */

		    bu_semaphore_acquire(BU_SEM_SYSCALL);
		    (void)close(fd);
		    bu_semaphore_release(BU_SEM_SYSCALL);
		    fd = -1;
		}
		if ((size_t)sb.st_size != mp->buflen) {
		    bu_log("bu_open_mapped_file(%s) WARNING: File size changed from %ld to %ld, opening new version.\n", real_path, mp->buflen, sb.st_size);
		    /* mp doesn't reflect the file any longer.  Invalidate. */
		    mp->appl = bu_strdup("__STALE__");
		    /* Can't invalidate old copy, it may still be in use. */
		    break;
		}
		if (sb.st_mtime != mp->modtime) {
		    bu_log("bu_open_mapped_file(%s) WARNING: File modified since last mapped, opening new version.\n", real_path);
		    /* mp doesn't reflect the file any longer.  Invalidate. */
		    mp->appl = bu_strdup("__STALE__");
		    /* Can't invalidate old copy, it may still be in use. */
		    break;
		}
		/* To be completely safe, should check st_dev and st_inum */
#endif
	    }

	    /* It is safe to reuse mp */
	    mp->uses++;

	    bu_semaphore_release(BU_SEM_MAPPEDFILE);
	    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
		bu_pr_mapped_file("open_reused", mp);

	    return mp;
	}

	/* It is safe to reuse mp */
	mp->uses++;
	return mp;

    }
    /* done iterating over mapped file list */
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    /* necessary in case we take a 'fail' path before BU_ALLOC() */
    mp = NULL;

    /* File is not yet mapped or has changed, open file read only if
     * we didn't find it earlier.
     */
#ifdef HAVE_SYS_STAT_H
    if (fd < 0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fd = open(real_path, O_RDONLY | O_BINARY);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (UNLIKELY(fd < 0)) {
	if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	    perror(real_path);
	goto fail;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    ret = fstat(fd, &sb);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(ret < 0)) {
	perror(real_path);
	goto fail;
    }

    if (UNLIKELY(sb.st_size == 0)) {
	bu_log("bu_open_mapped_file(%s) 0-length file\n", real_path);
	goto fail;
    }
#endif /* HAVE_SYS_STAT_H */

    /* Optimistically assume that things will proceed OK */
    BU_ALLOC(mp, struct bu_mapped_file);
    mp->name = bu_strdup(real_path);
    if (appl) mp->appl = bu_strdup(appl);

#ifdef HAVE_SYS_STAT_H
    mp->buflen = sb.st_size;
    mp->modtime = sb.st_mtime;
#  ifdef HAVE_SYS_MMAN_H

    /* Attempt to access as memory-mapped file */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    mp->buf = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(mp->buf == MAP_FAILED))
	perror(real_path);

    if (mp->buf != MAP_FAILED) {
	/* OK, its memory mapped in! */
	mp->is_mapped = 1;
	/* It's safe to close the fd now, the manuals say */
    } else
#  endif /* HAVE_SYS_MMAN_H */
    {
	/* Allocate a local zero'd buffer, and slurp it in always
	 * leaving space for a trailing zero.
	 */
	mp->buf = bu_calloc(1, sb.st_size+1, real_path);

	nbytes = 0;
	bytes_to_go = sb.st_size;
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	while (nbytes < sb.st_size) {
	    readval = read(fd, ((char *)(mp->buf)) + nbytes, ((bytes_to_go > INT_MAX) ? (INT_MAX) : (bytes_to_go)));
	    if (UNLIKELY(readval < 0)) {
		bu_semaphore_release(BU_SEM_SYSCALL);
		perror(real_path);
		bu_free(mp->buf, real_path);
		goto fail;
	    } else {
		nbytes += readval;
		bytes_to_go -= readval;
	    }
	}
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (UNLIKELY(nbytes != sb.st_size)) {
	    perror(real_path);
	    bu_free(mp->buf, real_path);
	    goto fail;
	}
    }

#else /* !HAVE_SYS_STAT_H */

    /* Read it in with stdio, with no clue how big it is */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fp = fopen(real_path, "rb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(fp == NULL)) {
	perror(real_path);
	goto fail;
    }
    /* Read it once to see how large it is */
    {
	char buf[32768] = {0};
	int got;
	mp->buflen = 0;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	while ((got = fread(buf, 1, sizeof(buf), fp)) > 0)
	    mp->buflen += got;
	rewind(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);

    }
    /* Allocate the necessary buffer */
    mp->buf = bu_calloc(1, mp->buflen+1, real_path);

    /* Read it again into the buffer */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    ret = fread(mp->buf, mp->buflen, 1, fp);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(ret != 1)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	perror("fread");
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);

	bu_log("bu_open_mapped_file() 2nd fread failed? len=%d\n", mp->buflen);
	bu_free(mp->buf, "non-unix fread buf");
	goto fail;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif

    if (fd >= 0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	(void)close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    mp->uses = 1;
    mp->l.magic = BU_MAPPED_FILE_MAGIC;

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    BU_LIST_APPEND(&bu_mapped_file_list, &mp->l);
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE)) {
	bu_pr_mapped_file("1st_open", mp);
    }
    if (real_path) {
	bu_free(real_path, "real_path alloc from bu_realpath");
    }
    return mp;

fail:
    if (fd >= 0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	(void)close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (mp) {
	bu_free(mp->name, "mp->name");
	if (mp->appl) bu_free(mp->appl, "mp->appl");
	/* Don't free mp->buf here, it might not be bu_malloced but mmaped */
	bu_free(mp, "mp from bu_open_mapped_file fail");
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_open_mapped_file(%s, %s) can't open file\n",
		real_path, appl ? appl: "(NIL)");

    if (real_path) {
	bu_free(real_path, "real_path alloc from bu_realpath");
    }

    return (struct bu_mapped_file *)NULL;
}


void
bu_close_mapped_file(struct bu_mapped_file *mp)
{
    if (UNLIKELY(!mp)) {
	bu_log("bu_close_mapped_file() called with null pointer\n");
	return;
    }

    BU_CK_MAPPED_FILE(mp);

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_pr_mapped_file("close:uses--", mp);

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    --mp->uses;
    bu_semaphore_release(BU_SEM_MAPPEDFILE);
}


void
bu_pr_mapped_file(const char *title, const struct bu_mapped_file *mp)
{
    if (UNLIKELY(!mp))
	return;

    BU_CK_MAPPED_FILE(mp);

    bu_log("%p mapped_file %s %p len=%ld mapped=%d, uses=%d %s\n",
	   (void *)mp, mp->name, mp->buf, mp->buflen,
	   mp->is_mapped, mp->uses,
	   title);
}


void
bu_free_mapped_files(int verbose)
{
    struct bu_mapped_file *mp, *next;

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_free_mapped_files(verbose=%d)\n", verbose);

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);

    next = BU_LIST_FIRST(bu_mapped_file, &bu_mapped_file_list);
    while (BU_LIST_NOT_HEAD(next, &bu_mapped_file_list)) {
	BU_CK_MAPPED_FILE(next);
	mp = next;
	next = BU_LIST_NEXT(bu_mapped_file, &mp->l);

	if (mp->uses > 0)  continue;

	/* Found one that needs to have storage released */
	if (UNLIKELY(verbose || (bu_debug&BU_DEBUG_MAPPED_FILE)))
	    bu_pr_mapped_file("freeing", mp);

	BU_LIST_DEQUEUE(&mp->l);

	/* If application pointed mp->apbuf at mp->buf, break that
	 * association so we don't double-free the buffer.
	 */
	if (mp->apbuf == mp->buf)  mp->apbuf = (void *)NULL;

#ifdef HAVE_SYS_MMAN_H
	if (mp->is_mapped) {
	    int ret;
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    ret = munmap(mp->buf, (size_t)mp->buflen);
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    if (UNLIKELY(ret < 0))
		perror("munmap");

	    /* XXX How to get this chunk of address space back to malloc()? */
	} else
#endif
	{
	    bu_free(mp->buf, "bu_mapped_file.buf[]");
	}
	mp->buf = (void *)NULL;		/* sanity */
	bu_free((void *)mp->name, "bu_mapped_file.name");

	if (mp->appl)
	    bu_free((void *)mp->appl, "bu_mapped_file.appl");

	bu_free((void *)mp, "struct bu_mapped_file");
    }
    bu_semaphore_release(BU_SEM_MAPPEDFILE);
}


struct bu_mapped_file *
bu_open_mapped_file_with_path(char *const *path, const char *name, const char *appl)

/* file name */
/* non-null only when app. will use 'apbuf' */
{
    char * const *pathp = path;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *ret;

    BU_ASSERT_PTR(name, !=, NULL);
    BU_ASSERT_PTR(pathp, !=, NULL);

    /* Do not resort to path for a rooted filename */
    if (name[0] == '/')
	return bu_open_mapped_file(name, appl);

    /* Try each path prefix in sequence */
    for (; *pathp != NULL; pathp++) {
	bu_vls_strcpy(&str, *pathp);
	bu_vls_putc(&str, '/');
	bu_vls_strcat(&str, name);

	ret = bu_open_mapped_file(bu_vls_addr(&str), appl);
	if (ret) {
	    bu_vls_free(&str);
	    return ret;
	}
    }

    /* Failure, none of the opens succeeded */
    bu_vls_free(&str);
    return (struct bu_mapped_file *)NULL;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
