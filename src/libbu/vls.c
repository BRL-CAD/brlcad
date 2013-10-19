/*                           V L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#ifdef HAVE_STDINT_H
#   include <stdint.h>
#endif

#include "bio.h"

#include "bu.h"

#include "./vls_internals.h"

/* non-published globals */
extern const char bu_vls_message[];
extern const char bu_strdup_message[];

/* private constants */

/* minimum initial allocation size */
static const unsigned int _VLS_ALLOC_MIN = 32;

/* minimum vls buffer allocation size */
static const unsigned int _VLS_ALLOC_READ = 4096;

void
bu_vls_init(struct bu_vls *vp)
{
    if (UNLIKELY(vp == (struct bu_vls *)NULL))
	bu_bomb("bu_vls_init() passed NULL pointer");

#if defined(DEBUG) && 0
    /* if already a vls, sanity check for a potential memory leak. */
    if (vp->vls_magic == BU_VLS_MAGIC) {
	if (vp->vls_str && vp->vls_len > 0 && vp->vls_max > 0) {
	    bu_log("bu_vls_init potential leak [%s] (vls_len=%d)\n", vp->vls_str, vp->vls_len);
	}
    }
#endif

    vp->vls_magic = BU_VLS_MAGIC;
    vp->vls_str = (char *)0;
    vp->vls_len = vp->vls_max = vp->vls_offset = 0;
}


void
bu_vls_init_if_uninit(struct bu_vls *vp)
{
    bu_log("DEPRECATION WARNING: bu_vls_init_if_uninit() should no longer be called.\n\t\tUse bu_vls_init() instead.\n");

    if (UNLIKELY(vp == (struct bu_vls *)NULL))
	bu_bomb("bu_vls_init_if_uninit() passed NULL pointer");

    if (vp->vls_magic == BU_VLS_MAGIC)
	return;
    bu_vls_init(vp);
}


struct bu_vls *
bu_vls_vlsinit(void)
{
    struct bu_vls *vp;

    BU_ALLOC(vp, struct bu_vls);
    bu_vls_init(vp);

    return vp;
}


char *
bu_vls_cstr(const struct bu_vls *vp)
{
    /* a wrapper for bu_vls_addr, but name is intended to be more intuitive to use */
    return bu_vls_addr(vp);
}


char *
bu_vls_addr(const struct bu_vls *vp)
{
    static char nullbuf[4] = {0, 0, 0, 0};
    BU_CK_VLS(vp);

    if (vp->vls_max == 0 || vp->vls_str == (char *)NULL) {
	/* A zero-length VLS is a null string */
	nullbuf[0] = '\0'; /* sanity */
	return nullbuf;
    }

    /* Sanity checking */
    if (vp->vls_str == (char *)NULL ||
	vp->vls_len + vp->vls_offset >= vp->vls_max)
    {
	bu_log("bu_vls_addr: bad VLS.  max=%zu, len=%zu, offset=%zu\n",
	       vp->vls_max, vp->vls_len, vp->vls_offset);
	bu_bomb("bu_vls_addr\n");
    }

    return vp->vls_str+vp->vls_offset;
}


void
bu_vls_extend(struct bu_vls *vp, unsigned int extra)
{
    BU_CK_VLS(vp);

    /* allocate at least 32 bytes (8 or 4 words) extra */
    if (extra < _VLS_ALLOC_MIN)
	extra = _VLS_ALLOC_MIN;

    /* first time allocation.
     *
     * performance testing using a static buffer indicated an
     * approximate 25% gain by avoiding this first allocation but not
     * worth the complexity involved (e.g., extending the struct or
     * hijacking vls_str) and it'd be error-prone whenever the vls
     * implementation changes.
     */
    if (vp->vls_max <= 0 || vp->vls_str == (char *)0) {
	vp->vls_str = (char *)bu_malloc((size_t)extra, bu_vls_message);
	vp->vls_max = extra;
	vp->vls_len = 0;
	vp->vls_offset = 0;
	*vp->vls_str = '\0';
	return;
    }

    /* make sure to increase in step sized increments */
    if ((size_t)extra < _VLS_ALLOC_STEP)
	extra = (unsigned int)_VLS_ALLOC_STEP;
    else if ((size_t)extra % _VLS_ALLOC_STEP != 0)
	extra += (unsigned int)(_VLS_ALLOC_STEP - (extra % _VLS_ALLOC_STEP));

    /* need more space? */
    if (vp->vls_offset + vp->vls_len + extra >= vp->vls_max) {
	vp->vls_str = (char *)bu_realloc(vp->vls_str, (vp->vls_max + extra), bu_vls_message);
	vp->vls_max += extra;
    }
}


void
bu_vls_setlen(struct bu_vls *vp, size_t newlen)
{
    BU_CK_VLS(vp);

    if (vp->vls_len >= newlen)
	return;

    bu_vls_extend(vp, (unsigned)newlen - vp->vls_len);
    vp->vls_len = newlen;
}


size_t
bu_vls_strlen(const struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    return vp->vls_len;
}


void
bu_vls_trunc(struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if (len < 0) {
	/* now an absolute length */
	len = vp->vls_len + len;
    }
    if (vp->vls_len <= (size_t)len)
	return;
    if (len == 0)
	vp->vls_offset = 0;

    vp->vls_str[len+vp->vls_offset] = '\0'; /* force null termination */
    vp->vls_len = len;
}


void
bu_vls_trunc2(struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if (vp->vls_len <= (size_t)len)
	return;

    if (len < 0)
	len = 0;
    if (len == 0)
	vp->vls_offset = 0;

    vp->vls_str[len+vp->vls_offset] = '\0'; /* force null termination */
    vp->vls_len = len;
}


void
bu_vls_nibble(struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if (len < 0 && (-len) > (ssize_t)vp->vls_offset)
	len = -vp->vls_offset;
    if ((size_t)len >= vp->vls_len) {
	bu_vls_trunc(vp, 0);
	return;
    }

    vp->vls_len -= len;
    vp->vls_offset += len;
}


void
bu_vls_free(struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    if (vp->vls_str && vp->vls_max > 0) {
	vp->vls_str[0] = '?'; /* Sanity */
	bu_free(vp->vls_str, "bu_vls_free");
	vp->vls_str = (char *)0;
    }

    vp->vls_offset = vp->vls_len = vp->vls_max = 0;
}


void
bu_vls_vlsfree(struct bu_vls *vp)
{
    if (UNLIKELY(*(unsigned long *)vp != BU_VLS_MAGIC))
	return;

    bu_vls_free(vp);
    bu_free(vp, "bu_vls_vlsfree");
}


char *
bu_vls_strdup(const struct bu_vls *vp)
{
    char *str;
    size_t len;

    BU_CK_VLS(vp);

    len = bu_vls_strlen(vp);
    str = bu_malloc(len+1, bu_strdup_message);
    bu_strlcpy(str, bu_vls_addr(vp), len+1);

    return str;
}


char *
bu_vls_strgrab(struct bu_vls *vp)
{
    char *str;

    BU_CK_VLS(vp);

    if (vp->vls_offset != 0 || !vp->vls_str) {
	str = bu_vls_strdup(vp);
	bu_vls_free(vp);
	return str;
    }

    str = bu_vls_addr(vp);
    vp->vls_str = (char *)0;
    vp->vls_offset = vp->vls_len = vp->vls_max = 0;
    return str;
}


void
bu_vls_strcpy(struct bu_vls *vp, const char *s)
{
    size_t len;

    BU_CK_VLS(vp);

    if (UNLIKELY(s == (const char *)NULL))
	return;

    len = strlen(s);
    if (UNLIKELY(len <= 0)) {
	vp->vls_len = 0;
	vp->vls_offset = 0;
	if (vp->vls_max > 0)
	    vp->vls_str[0] = '\0';
	return;
    }

    /* cancel offset before extending */
    vp->vls_offset = 0;
    if (len+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned int)len+1);

    memcpy(vp->vls_str, s, len+1); /* include null */
    vp->vls_len = len;
}


void
bu_vls_strncpy(struct bu_vls *vp, const char *s, size_t n)
{
    size_t len;

    BU_CK_VLS(vp);

    if (UNLIKELY(s == (const char *)NULL))
	return;

    len = strlen(s);
    if (len > n)
	len = n;
    if (UNLIKELY(len <= 0)) {
	vp->vls_len = 0; /* ensure string is empty */
	return;
    }

    /* cancel offset before extending */
    vp->vls_offset = 0;
    if (len+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned int)len+1);

    memcpy(vp->vls_str, s, len);
    vp->vls_str[len] = '\0'; /* force null termination */
    vp->vls_len = (int)len;
}


void
bu_vls_strcat(struct bu_vls *vp, const char *s)
{
    size_t len;

    BU_CK_VLS(vp);

    if (UNLIKELY(s == (const char *)NULL))
	return;

    len = strlen(s);
    if (UNLIKELY(len <= 0))
	return;

    if (vp->vls_offset + vp->vls_len + len+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned int)len+1);

    memcpy(vp->vls_str +vp->vls_offset + vp->vls_len, s, len+1); /* include null */
    vp->vls_len += (int)len;
}


void
bu_vls_strncat(struct bu_vls *vp, const char *s, size_t n)
{
    size_t len;

    BU_CK_VLS(vp);

    if (UNLIKELY(s == (const char *)NULL))
	return;

    len = strlen(s);
    if (len > n)
	len = n;
    if (UNLIKELY(len <= 0))
	return;

    if (vp->vls_offset + vp->vls_len + len+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned int)len+1);

    memcpy(vp->vls_str + vp->vls_offset + vp->vls_len, s, len);
    vp->vls_len += (int)len;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0'; /* force null termination */
}


void
bu_vls_vlscat(struct bu_vls *dest, const struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);

    if (UNLIKELY(src->vls_len <= 0))
	return;

    if (dest->vls_offset + dest->vls_len + src->vls_len+1 >= dest->vls_max)
	bu_vls_extend(dest, (unsigned int)src->vls_len+1);

    /* copy source string, including null */
    memcpy(dest->vls_str +dest->vls_offset + dest->vls_len, src->vls_str+src->vls_offset, src->vls_len+1);
    dest->vls_len += src->vls_len;
}


void
bu_vls_vlscatzap(struct bu_vls *dest, struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);

    if (UNLIKELY(src->vls_len <= 0))
	return;

    bu_vls_vlscat(dest, src);
    bu_vls_trunc(src, 0);
}


int
bu_vls_strcmp(struct bu_vls *s1, struct bu_vls *s2)
{
    BU_CK_VLS(s1);
    BU_CK_VLS(s2);

    /* A zero-length VLS is a null string, account for it */
    if (s1->vls_max == 0 || s1->vls_str == (char *)NULL) {
	/* s1 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s1, 1);
    }
    if (s2->vls_max == 0 || s2->vls_str == (char *)NULL) {
	/* s2 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s2, 1);
    }

    /* neither empty, straight up comparison */
    return bu_strcmp(s1->vls_str+s1->vls_offset, s2->vls_str+s2->vls_offset);
}


int
bu_vls_strncmp(struct bu_vls *s1, struct bu_vls *s2, size_t n)
{
    BU_CK_VLS(s1);
    BU_CK_VLS(s2);

    if (UNLIKELY(n <= 0)) {
	/* they match at zero chars */
	return 0;
    }

    /* A zero-length VLS is a null string, account for it */
    if (s1->vls_max == 0 || s1->vls_str == (char *)NULL) {
	/* s1 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s1, 1);
    }
    if (s2->vls_max == 0 || s2->vls_str == (char *)NULL) {
	/* s2 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s2, 1);
    }

    return bu_strncmp(s1->vls_str+s1->vls_offset, s2->vls_str+s2->vls_offset, n);
}


void
bu_vls_from_argv(struct bu_vls *vp, int argc, const char *argv[])
{
    BU_CK_VLS(vp);

    for (/* nada */; argc > 0; argc--, argv++) {
	bu_vls_strcat(vp, *argv);
	if (argc > 1)  bu_vls_strcat(vp, " ");
    }
}


void
bu_vls_fwrite(FILE *fp, const struct bu_vls *vp)
{
    size_t status;

    BU_CK_VLS(vp);

    if (UNLIKELY(vp->vls_len <= 0))
	return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    status = fwrite(vp->vls_str + vp->vls_offset, vp->vls_len, 1, fp);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(status != 1)) {
	perror("fwrite");
	bu_bomb("bu_vls_fwrite() write error\n");
    }
}


void
bu_vls_write(int fd, const struct bu_vls *vp)
{
    ssize_t status;

    BU_CK_VLS(vp);

    if (UNLIKELY(vp->vls_len <= 0))
	return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    status = write(fd, vp->vls_str + vp->vls_offset, vp->vls_len);
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (UNLIKELY(status < 0 || (size_t)status != vp->vls_len)) {
	perror("write");
	bu_bomb("bu_vls_write() write error\n");
    }
}


int
bu_vls_read(struct bu_vls *vp, int fd)
{
    size_t todo;
    int got;
    int ret = 0;

    BU_CK_VLS(vp);

    for (;;) {
	bu_vls_extend(vp, _VLS_ALLOC_READ);
	todo = vp->vls_max - vp->vls_len - vp->vls_offset - 1;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	got = (int)read(fd, vp->vls_str+vp->vls_offset+vp->vls_len, todo);
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (UNLIKELY(got < 0)) {
	    /* Read error, abandon the read */
	    return -1;
	}
	if (got == 0)
	    break;

	vp->vls_len += got;
	ret += got;
    }

    /* force null termination */
    vp->vls_str[vp->vls_len+vp->vls_offset] = '\0';

    return ret;
}


int
bu_vls_gets(struct bu_vls *vp, FILE *fp)
{
    int startlen;
    int endlen;
    int done;
    char *bufp;
    char buffer[BUFSIZ+1] = {0};

    BU_CK_VLS(vp);

    startlen = bu_vls_strlen(vp);

    do {
	bufp = bu_fgets(buffer, BUFSIZ+1, fp);

	if (!bufp)
	    return -1;

	/* keep reading if we just filled the buffer */
	if ((strlen(bufp) == BUFSIZ) && (bufp[BUFSIZ-1] != '\n') && (bufp[BUFSIZ-1] != '\r')) {
	    done = 0;
	} else {
	    done = 1;
	}

	/* strip the trailing EOL (or at least part of it) */
	if ((bufp[strlen(bufp)-1] == '\n') || (bufp[strlen(bufp)-1] == '\r'))
	    bufp[strlen(bufp)-1] = '\0';

	/* handle \r\n lines */
	if (bufp[strlen(bufp)-1] == '\r')
	    bufp[strlen(bufp)-1] = '\0';

	bu_vls_printf(vp, "%s", bufp);
    } while (!done);

    /* sanity check */
    endlen = bu_vls_strlen(vp);
    if (endlen < startlen)
	return -1;

    return endlen;
}

void
bu_vls_putc(struct bu_vls *vp, int c)
{
    BU_CK_VLS(vp);

    if (vp->vls_offset + vp->vls_len+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned int)_VLS_ALLOC_STEP);

    vp->vls_str[vp->vls_offset + vp->vls_len++] = (char)c;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0'; /* force null termination */
}

void
bu_vls_trimspace(struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    /* Remove trailing white space */
    while ((vp->vls_len > 0) &&
	   isspace((int)(bu_vls_addr(vp)[bu_vls_strlen(vp)-1])))
	bu_vls_trunc(vp, -1);

    /* Remove leading white space */
    while ((vp->vls_len > 0) &&
	   isspace((int)(*bu_vls_addr(vp))))
	bu_vls_nibble(vp, 1);
}

void
bu_vls_printf(struct bu_vls *vls, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    BU_CK_VLS(vls);

    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}


void
bu_vls_sprintf(struct bu_vls *vls, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    BU_CK_VLS(vls);

    bu_vls_trunc(vls, 0); /* poof */
    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}


void
bu_vls_spaces(struct bu_vls *vp, int cnt)
{
    BU_CK_VLS(vp);

    if (UNLIKELY(cnt <= 0))
	return;
    if (vp->vls_offset + vp->vls_len + cnt+1 >= vp->vls_max)
	bu_vls_extend(vp, (unsigned)cnt);

    memset(vp->vls_str + vp->vls_offset + vp->vls_len, ' ', (size_t)cnt);
    vp->vls_len += cnt;
}


int
bu_vls_print_positions_used(const struct bu_vls *vp)
{
    char *start;
    int used;

    BU_CK_VLS(vp);

    start = strrchr(bu_vls_addr(vp), '\n');
    if (start == NULL)
	start = bu_vls_addr(vp);

    used = 0;
    while (*start != '\0') {
	if (*start == '\t') {
	    used += 8 - (used % 8);
	} else {
	    used++;
	}
	start++;
    }

    return used;
}


void
bu_vls_detab(struct bu_vls *vp)
{
    struct bu_vls src = BU_VLS_INIT_ZERO;
    char *cp;
    int used;

    BU_CK_VLS(vp);

    bu_vls_vlscatzap(&src, vp);	/* make temporary copy of src */
    bu_vls_extend(vp, (unsigned int)bu_vls_strlen(&src) + (unsigned int)_VLS_ALLOC_STEP);

    cp = bu_vls_addr(&src);
    used = 0;
    while (*cp != '\0') {
	if (*cp == '\t') {
	    int todo;
	    todo = 8 - (used % 8);
	    bu_vls_spaces(vp, todo);
	    used += todo;
	} else if (*cp == '\n') {
	    bu_vls_putc(vp, '\n');
	    used = 0;
	} else {
	    bu_vls_putc(vp, *cp);
	    used++;
	}
	cp++;
    }

    bu_vls_free(&src);
}


void
bu_vls_prepend(struct bu_vls *vp, char *str)
{
    size_t len = strlen(str);

    bu_vls_extend(vp, (unsigned int)len);

    /* memmove is supposed to be safe even if strings overlap */
    memmove(vp->vls_str+vp->vls_offset+len, vp->vls_str+vp->vls_offset, vp->vls_len);

    /* insert the data at the head of the string */
    memcpy(vp->vls_str+vp->vls_offset, str, len);

    vp->vls_len += (int)len;
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
