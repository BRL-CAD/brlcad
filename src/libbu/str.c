/*                          S T R . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2024 United States Government as represented by
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
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <ctype.h>
#include <stdio.h> /* for fprintf */

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "bu/debug.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/str.h"

#ifndef HAVE_DECL_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_DECL_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif


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
    if (UNLIKELY(!dst || !src || size <= 0)) {
	return 0;
    }
    if (!label) {
	label = "bu_strlcat";
    }

    dstsize = strlen(dst);
    srcsize = strlen(src);

    if (UNLIKELY(dstsize == size - 1)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] concatenation string is already full at %lu chars\n", label, (unsigned long)size-1);
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (UNLIKELY(dstsize > size - 1)) {
	/* probably missing null-termination or is not an initialized buffer */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] concatenation string is already full, exceeds size (%lu > %lu)\n", label, (unsigned long)dstsize, (unsigned long)size-1);
	bu_semaphore_release(BU_SEM_SYSCALL);
    } else if (UNLIKELY(srcsize > size - dstsize - 1)) {
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


    if (UNLIKELY(!dst && label)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: NULL destination string, size %lu [%s]\n", (unsigned long)size, label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (UNLIKELY(!dst || !src || size <= 0)) {
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
    char *base = NULL;
    size_t len;

    if (UNLIKELY(!cp && label)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fprintf(stderr, "WARNING: [%s] NULL copy buffer\n", label);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (!label) {
	label = "bu_strdup";
    }

    if (cp) {
	len = strlen(cp)+1;
	base = (char *)bu_malloc(len, label);

	memcpy(base, cp, len);
    }
    return base;
}


int
bu_strcmp(const char *string1, const char *string2)
{
    const char *s1 = "";
    const char *s2 = "";

    /* "" and NULL are considered equivalent which helps prevent
     * strcmp() from crashing.
     */

    if (string1)
	s1 = string1;

    if (string2)
	s2 = string2;

    return strcmp(s1, s2);
}


int
bu_strncmp(const char *string1, const char *string2, size_t n)
{
    const char *s1 = "";
    const char *s2 = "";

    /* "" and NULL are considered equivalent which helps prevent
     * strncmp() from crashing.
     */

    if (string1)
	s1 = string1;

    if (string2)
	s2 = string2;

    return strncmp(s1, s2, n);
}


int
bu_strcasecmp(const char *string1, const char *string2)
{
    const char *s1 = "";
    const char *s2 = "";

    /* "" and NULL are considered equal */

    if (string1)
	s1 = string1;

    if (string2)
	s2 = string2;

#if defined(strcasecmp) || defined(HAVE_STRCASECMP)
    return strcasecmp(s1, s2);
#else
    while (tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
	if (*s1 == '\0')
	    return 0;
	s1++;
	s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
#endif
}


int
bu_strncasecmp(const char *string1, const char *string2, size_t n)
{
    const char *s1 = "";
    const char *s2 = "";

    /* "" and NULL are considered equal */

    if (string1)
	s1 = string1;

    if (string2)
	s2 = string2;

    if (n == 0)
	return 0;

#if defined(strcasecmp) || defined(HAVE_STRCASECMP)
    return strncasecmp(s1, s2, n);
#else
    while (tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
	if (--n == 0 || *s1 == '\0')
	    return 0;
	s1++;
	s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
#endif
}

unsigned long long
bu_data_hash(const void *data, size_t len)
{
    if (!data || !len)
	return 0;

    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, (const char *)data, len);
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    return hash;
}

struct bu_data_hash_impl {
    XXH64_state_t *h_state;
};

struct bu_data_hash_state *
bu_data_hash_create()
{
    struct bu_data_hash_state *s;
    BU_GET(s, struct bu_data_hash_state);
    BU_GET(s->i, struct bu_data_hash_impl);
    s->i->h_state = XXH64_createState();
    XXH64_reset(s->i->h_state, 0);
    return s;
}

void
bu_data_hash_destroy(struct bu_data_hash_state *s)
{
    if (!s)
	return;
    if (s->i) {
	if (s->i->h_state)
	    XXH64_freeState(s->i->h_state);
	s->i->h_state = NULL;
	BU_PUT(s->i, struct bu_data_hash_impl);
    }
    s->i = NULL;
    BU_PUT(s, struct bu_data_hash_state);
}

void
bu_data_hash_update(struct bu_data_hash_state *s, const void *data, size_t len)
{
    if (!s || !data || !len)
	return;
    XXH64_update(s->i->h_state, data, len);
}

unsigned long long
bu_data_hash_val(struct bu_data_hash_state *s)
{
    if (!s || !s->i || !s->i->h_state)
	return 0;
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(s->i->h_state);
    return (unsigned long long)hash_val;
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
