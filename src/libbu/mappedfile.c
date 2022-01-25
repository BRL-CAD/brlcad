/*                    M A P P E D F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bu/vls.h"

/* Based on the public domain wrapper implemented by Mike Frysinger:
 * https://cgit.uclibc-ng.org/cgi/cgit/uclibc-ng.git/tree/utils/mmap-windows.c */
#ifdef HAVE_WINDOWS_H
#  define PROT_READ     0x1
#  define PROT_WRITE    0x2
/* This flag is only available in WinXP+ */
#  ifdef FILE_MAP_EXECUTE
#    define PROT_EXEC     0x4
#  else
#    define PROT_EXEC        0x0
#    define FILE_MAP_EXECUTE 0
#  endif

#  define MAP_SHARED    0x01
#  define MAP_PRIVATE   0x02
#  define MAP_ANONYMOUS 0x20
#  define MAP_ANON      MAP_ANONYMOUS
#  define MAP_FAILED    ((void *) -1)

#  ifdef HAVE_OFF_T_64BIT
#    define DWORD_HI(x) (x >> 32)
#    define DWORD_LO(x) ((x) & 0xffffffff)
#  else
#    define DWORD_HI(x) (0)
#    define DWORD_LO(x) (x)
#  endif


static void *
win_mmap(void *start, size_t length, int prot, int flags, int fd, b_off_t offset, void **handle)
{
    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
	return MAP_FAILED;
    if (fd == -1) {
	if (!(flags & MAP_ANON) || offset)
	    return MAP_FAILED;
    }
    else if (flags & MAP_ANON)
	return MAP_FAILED;

    DWORD flProtect;
    if (prot & PROT_WRITE) {
	if (prot & PROT_EXEC)
	    flProtect = PAGE_EXECUTE_READWRITE;
	else
	    flProtect = PAGE_READWRITE;
    }
    else if (prot & PROT_EXEC) {
	if (prot & PROT_READ)
	    flProtect = PAGE_EXECUTE_READ;
	else if (prot & PROT_EXEC)
	    flProtect = PAGE_EXECUTE;
    }
    else
	flProtect = PAGE_READONLY;

    b_off_t end = length + offset;
    HANDLE mmap_fd, h;
    if (fd == -1)
	mmap_fd = INVALID_HANDLE_VALUE;
    else
	mmap_fd = (HANDLE)_get_osfhandle(fd);
    h = CreateFileMapping(mmap_fd, NULL, flProtect, DWORD_HI(end), DWORD_LO(end), NULL);
    if (h == NULL) {
	return MAP_FAILED;
    } else {
	*handle = (void *)h;
    }

    DWORD dwDesiredAccess;
    if (prot & PROT_WRITE)
	dwDesiredAccess = FILE_MAP_WRITE;
    else
	dwDesiredAccess = FILE_MAP_READ;
    if (prot & PROT_EXEC)
	dwDesiredAccess |= FILE_MAP_EXECUTE;
    if (flags & MAP_PRIVATE)
	dwDesiredAccess |= FILE_MAP_COPY;
    void *ret = MapViewOfFile(h, dwDesiredAccess, DWORD_HI(offset), DWORD_LO(offset), length);
    if (ret == NULL) {
	CloseHandle(h);
	ret = MAP_FAILED;
    }
    return ret;
}


static int
win_munmap(void *addr, size_t length, void *hv)
{
    HANDLE h = (HANDLE)hv;
    UnmapViewOfFile(addr);
    CloseHandle(h);
    return 0;
}
#endif


/* list of currently open mapped files */
#define NUM_INITIAL_MAPPED_FILES 8

static struct bu_mapped_file_list {
    size_t size, capacity;
    struct bu_mapped_file **mapped_files;
} all_mapped_files = { 0, 0, NULL };


static struct bu_mapped_file *
mapped_file_find(const char *name, const char *appl)
{
    size_t i;
    struct bu_mapped_file *mp = (struct bu_mapped_file *)NULL;

    /* See if file has already been mapped, and can be shared */
    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);

    for (i = 0; i < all_mapped_files.size; i++) {

	if (!BU_STR_EQUAL(name, all_mapped_files.mapped_files[i]->name))
	    continue;
	if (!BU_STR_EQUAL(appl, all_mapped_files.mapped_files[i]->appl))
	    continue;

	/* found a match */
	mp = all_mapped_files.mapped_files[i];
	break;
    }

    /* done iterating over mapped file list */
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    return mp;
}


static struct bu_mapped_file *
mapped_file_incr(struct bu_mapped_file *mp)
{
    BU_ASSERT(mp != NULL);

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    mp->uses++;
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    return mp;
}


static void
mapped_file_invalidate(struct bu_mapped_file *mp)
{
    if (!mp)
	return;

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    if (mp->appl)
	bu_free(mp->appl, "appl");
    mp->appl = NULL;
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    return;
}


static int
mapped_file_is_valid(struct bu_mapped_file *mp)
{
    int fd;
    int ret;
    struct stat sb;

    if (!mp || mp->name == NULL)
	return 0;

    /* does the file still exist */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fd = open(mp->name, O_RDONLY | O_BINARY);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fd < 0) {
	/* file vanished from disk.  assume mapped copy is OK. */
	return 1;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    ret = fstat(fd, &sb);
    (void)close(fd);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (ret < 0) {
	/* odd, open worked but fstat failed.  assume file vanished
	 * but mapped copy is still OK.
	 */
	return 1;
    }

    if ((size_t)sb.st_size != mp->buflen) {
	if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE)) {
	    bu_log("bu_open_mapped_file(%s) WARNING: File size changed from %zu to %jd, opening new version.\n", mp->name, mp->buflen, (intmax_t)sb.st_size);
	}
	/* doesn't reflect the file any longer. */
	return 0;
    } else if (sb.st_mtime != mp->modtime) {
	if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE)) {
	    bu_log("bu_open_mapped_file(%s) WARNING: File modified since last mapped, opening new version.\n", mp->name);
	}
	/* doesn't reflect the file any longer. */
	return 0;
    }

    /* To be more safe, could also check st_dev and st_inum but that's
     * not portable.
     */

    return 1;
}


static struct bu_mapped_file *
mapped_file_add(struct bu_mapped_file *mp)
{
    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
    {
	size_t i;

	/* init mapped file storage container.  mapped files are
	 * stored as an array of dynamically allocated pointers.
	 */
	if (all_mapped_files.capacity == 0) {
	    all_mapped_files.capacity = NUM_INITIAL_MAPPED_FILES;
	    all_mapped_files.size = 0;
	    all_mapped_files.mapped_files = (struct bu_mapped_file **)bu_calloc(all_mapped_files.capacity, sizeof(struct bu_mapped_file *), "initial mapped file pointers");
	} else if (all_mapped_files.size == all_mapped_files.capacity) {
	    all_mapped_files.capacity *= 2;
	    all_mapped_files.mapped_files = (struct bu_mapped_file **)bu_realloc(all_mapped_files.mapped_files, all_mapped_files.capacity * sizeof(struct bu_mapped_file *), "more mapped file pointers");
	    for (i = all_mapped_files.size; i < all_mapped_files.capacity; i++)
		all_mapped_files.mapped_files[i] = NULL;
	}

	/* calling bu_free_mapped_files() will release any mapped
	 * files no longer in use, so we may need to reallocate.
	 * first time through will also allocate.
	 */
	if (all_mapped_files.mapped_files[all_mapped_files.size] == NULL) {
	    all_mapped_files.mapped_files[all_mapped_files.size] = (struct bu_mapped_file *)bu_calloc(1, sizeof(struct bu_mapped_file), "new mapped file holder");
	}

	/* store our mapped file */
	*all_mapped_files.mapped_files[all_mapped_files.size] = *mp; /* struct copy */
	mp = all_mapped_files.mapped_files[all_mapped_files.size]; /* returning the dynamic allocation */
	all_mapped_files.size++;
    }
    bu_semaphore_release(BU_SEM_MAPPEDFILE);

    return mp;
}


struct bu_mapped_file *
bu_open_mapped_file(const char *name, const char *appl)
/* file name */
/* non-null only when app. will use 'apbuf' */
{
    struct bu_mapped_file newlymapped = BU_MAPPED_FILE_INIT_ZERO;
    struct bu_mapped_file *mp = (struct bu_mapped_file *)NULL;
    struct stat sb;
    int fd = -1;	/* unix file descriptor */
    int readval;
    int ret;

    if (!name)
	return NULL;

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_open_mapped_file(%s , %s)\n", name, appl?appl:"(NULL)");

    mp = mapped_file_find(name, appl);
    if (mapped_file_is_valid(mp)) {
	return mapped_file_incr(mp);
    } else {
	mapped_file_invalidate(mp);
    }

    /* MAP A NEW ONE: File is not yet mapped or has changed.  Open
     * file read only and start filling in a new mappedfile.
     */
    mp = &newlymapped;

    mp->name = bu_strdup(name);
    if (appl)
	mp->appl = bu_strdup(appl);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fd = open(name, O_RDONLY | O_BINARY);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(fd < 0)) {
	if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	    perror("open");
	goto fail;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    ret = fstat(fd, &sb);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(ret < 0)) {
	perror("fstat");
	goto fail;
    }

    if (UNLIKELY(sb.st_size == 0)) {
	bu_log("bu_open_mapped_file(%s) 0-length file\n", name);
	goto fail;
    }

    mp->uses = 1;
    mp->buflen = sb.st_size;
    mp->modtime = sb.st_mtime;
    mp->buf = NULL;

    /* Attempt to memory-map the file */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
#if defined(HAVE_SYS_MMAN_H)
    mp->buf = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
#elif defined(HAVE_WINDOWS_H)
    /* FIXME: shouldn't need to preserve handle */
    mp->buf = win_mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0, &(mp->handle));
#endif /* HAVE_SYS_MMAN_H */
    bu_semaphore_release(BU_SEM_SYSCALL);

    /* If cannot memory-map, read it in manually */
    if (mp->buf && mp->buf != MAP_FAILED) {
	mp->is_mapped = 1;
    } else {
	ssize_t bytes_to_go = sb.st_size;
	ssize_t nbytes = 0;

	if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE)) {
	    perror("mmap");
	}

	/* Allocate a local empty buffer, and slurp the whole file.
	 * leave space for a trailing zero.
	 */
	mp->buf = bu_calloc(1, sb.st_size+1, name);

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	while (nbytes < sb.st_size) {
	    readval = read(fd, ((char *)(mp->buf)) + nbytes, ((bytes_to_go > INT_MAX) ? (INT_MAX) : (bytes_to_go)));
	    if (UNLIKELY(readval < 0)) {
		bu_semaphore_release(BU_SEM_SYSCALL);
		perror("read");
		goto fail;
	    } else {
		nbytes += readval;
		bytes_to_go -= readval;
	    }
	}
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (UNLIKELY(nbytes != sb.st_size)) {
	    perror(name);
	    goto fail;
	}
    }

    if (fd >= 0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	(void)close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE)) {
	bu_pr_mapped_file("1st_open", mp);
    }

    return mapped_file_add(mp);

fail:
    if (fd >= 0) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	(void)close(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (mp->name) {
	bu_free(mp->name, "mp->name");
	mp->name = NULL;
    }
    if (mp->appl) {
	bu_free(mp->appl, "mp->appl");
	mp->appl = NULL;
    }
    if (mp->buf) {
	if (mp->is_mapped) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
#if defined(HAVE_SYS_MMAN_H)
	    munmap(mp->buf, (size_t)mp->buflen);
#elif defined(HAVE_WINDOWS_H)
	    win_munmap(mp->buf, (size_t)mp->buflen, mp->handle);
#endif
	    bu_semaphore_release(BU_SEM_SYSCALL);
	} else if (mp->buf != MAP_FAILED) {
	    bu_free(mp->buf, name);
	}
	mp->buf = NULL;
    }

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_open_mapped_file(%s, %s) can't open file\n", name, appl ? appl: "(NULL)");

    return (struct bu_mapped_file *)NULL;
}


void
bu_close_mapped_file(struct bu_mapped_file *mp)
{
    if (UNLIKELY(!mp)) {
	return;
    }

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

    bu_log("%p mapped_file %s %p len=%zu mapped=%d, uses=%d %s\n",
	   (void *)mp, mp->name, mp->buf, mp->buflen,
	   mp->is_mapped, mp->uses,
	   title);
}


void
bu_free_mapped_files(int verbose)
{
    struct bu_mapped_file *mp;
    size_t i;

    if (UNLIKELY(bu_debug&BU_DEBUG_MAPPED_FILE))
	bu_log("bu_free_mapped_files(verbose=%d)\n", verbose);

    bu_semaphore_acquire(BU_SEM_MAPPEDFILE);

    for (i = 0; i < all_mapped_files.size; i++) {
	mp = all_mapped_files.mapped_files[i];

	if (mp->uses > 0)
	    continue;

	/* Found one that needs to have storage released */
	if (UNLIKELY(verbose || (bu_debug&BU_DEBUG_MAPPED_FILE)))
	    bu_pr_mapped_file("freeing", mp);

	mp->apbuf = (void *)NULL;

	if (mp->is_mapped) {
	    int ret;
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
#ifdef HAVE_SYS_MMAN_H
	    ret = munmap(mp->buf, (size_t)mp->buflen);
#else
#  ifdef HAVE_WINDOWS_H
	    ret = win_munmap(mp->buf, (size_t)mp->buflen, mp->handle);
#  endif
#endif
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    if (UNLIKELY(ret < 0))
		perror("munmap");

	    /* XXX How to get this chunk of address space back to malloc()? */
	} else {
	    bu_free(mp->buf, "bu_mapped_file.buf[]");
	}
	mp->buf = (void *)NULL;		/* sanity */
	bu_free((void *)mp->name, "bu_mapped_file.name");

	if (mp->appl)
	    bu_free((void *)mp->appl, "bu_mapped_file.appl");

	/* release this one */
	memset(mp, 0, sizeof(struct bu_mapped_file)); /* sanity */
	bu_free(mp, "free mapped file holder");

	/* shift pointers - move everything down one index slot in the array */
	for (size_t j = i; j < all_mapped_files.size - 1; j++) {
	    all_mapped_files.mapped_files[j] = all_mapped_files.mapped_files[j+1];
	}
	all_mapped_files.mapped_files[all_mapped_files.size - 1] = NULL; /* zero out the last (now invalid) pointer */
	all_mapped_files.size--;
	/* Next item to inspect is now in the same index as the item we just removed */
	i--;
    }
    /* release the array if we get back to empty */
    if (all_mapped_files.size == 0 && all_mapped_files.capacity > 0) {
	bu_free(all_mapped_files.mapped_files, "free mapped file pointers");
	all_mapped_files.capacity = 0;
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

    BU_ASSERT(name != NULL);
    BU_ASSERT(pathp != NULL);

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
