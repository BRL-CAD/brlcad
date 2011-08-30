/*                         S T R L . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bu.h"


size_t
bu_strlcatm(char *dst, const char *src, size_t size, const char *label)
{
    size_t srcsize;
    size_t dstsize;

    if (!dst && label) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: NULL destination string, size %lu [%s]\n", (unsigned long)size, label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (!dst || !src || size <= 0) {
	return 0;
    }
    if (!label) {
	label = "bu_strlcat";
    }

    dstsize = strlen(dst);
    srcsize = strlen(src);

    if (dstsize == size - 1) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] concatenation string is already full at %lu chars\n", label, (unsigned long)size-1);
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (dstsize > size - 1) {
	/* probably missing null-termination or is not an initialized buffer */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] concatenation string is already full, exceeds size (%lu > %lu)\n", label, (unsigned long)dstsize, (unsigned long)size-1);
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (srcsize > size - dstsize - 1) {
	if (UNLIKELY(bu_debug)) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fprintf(stderr, "WARNING: [%s] string truncation, exceeding %lu char max concatenating %lu chars (started with %lu)\n", label, (unsigned long)size-1, (unsigned long)srcsize, (unsigned long)dstsize);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}
    }

#ifdef HAVE_STRLCAT
    /* don't return to ensure consistent null-termination behavior in following */
    (void)strlcat(dst, src, size);
#else
    (void)strncat(dst, src, size - dstsize - 1);
#endif

    /* be sure to null-terminate, contrary to strncat behavior */
    if (dstsize + srcsize < size - 1) {
	dst[dstsize + srcsize] = '\0';
    } else {
	dst[size-1] = '\0'; /* sanity */
    }

    return strlen(dst);
}


size_t
bu_strlcpym(char *dst, const char *src, size_t size, const char *label)
{
    size_t srcsize;


    if (!dst && label) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: NULL destination string, size %lu [%s]\n", (unsigned long)size, label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (!dst || !src || size <= 0) {
	return 0;
    }
    if (!label) {
	label = "bu_strlcpy";
    }

    srcsize = strlen(src);

    if (UNLIKELY(bu_debug)) {
	if (srcsize > size - 1) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fprintf(stderr, "WARNING: [%s] string truncation, exceeding %lu char max copying %lu chars\n", label, (unsigned long)size-1, (unsigned long)srcsize);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}
    }

#ifdef HAVE_STRLCPY
    /* don't return to ensure consistent null-termination behavior in following */
    (void)strlcpy(dst, src, size);
#else
    (void)strncpy(dst, src, size - 1);
#endif

    /* be sure to always null-terminate, contrary to strncpy behavior */
    if (srcsize < size - 1) {
	dst[srcsize] = '\0';
    } else {
	dst[size-1] = '\0'; /* sanity */
    }

    return strlen(dst);
}


char *
bu_strdupm(register const char *cp, const char *label)
{
    char *base;
    size_t len;

    if (!cp && label) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] NULL copy buffer\n", label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (!label) {
	label = "bu_strdup";
    }

    len = strlen(cp)+1;
    base = bu_malloc(len, label);

    if (UNLIKELY(bu_debug&BU_DEBUG_MEM_LOG)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "%p strdup%llu \"%s\"\n", (void *)base, (unsigned long long)len, cp);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    memcpy(base, cp, len);
    return base;
}


int
bu_strcmpm(const char *string1, const char *string2, const char *label)
{
    const char *s1 = "";
    const char *s2 = "";

    if ((!string1 || !string2) && label) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] string comparison with NULL\n", label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    /* "" and NULL are considered as equal */

    if (string1)
	s1 = string1;

    if (string2)
	s2 = string2;

    return strcmp(s1, s2);
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
