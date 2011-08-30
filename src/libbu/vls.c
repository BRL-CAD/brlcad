/*                           V L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#ifdef HAVE_STDINT_H
#   include <stdint.h>
#endif

#include "bio.h"

#include "bu.h"


/* non-published globals */
extern const char bu_vls_message[];
extern const char bu_strdup_message[];

/* private constants */

/* minimum initial allocation size */
static const unsigned int _VLS_ALLOC_MIN = 32;

/* minimum vls allocation increment size */
static const size_t _VLS_ALLOC_STEP = 128;

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

    vp = (struct bu_vls *)bu_malloc(sizeof(struct bu_vls), "bu_vls_vlsinit struct");
    bu_vls_init(vp);

    return vp;
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

    /* first time allocation */
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
    strncpy(str, bu_vls_addr(vp), len);
    str[len] = '\0'; /* sanity */
    return str;
}


char *
bu_vls_strgrab(struct bu_vls *vp)
{
    char *str;

    BU_CK_VLS(vp);

    if (vp->vls_offset != 0) {
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

    return strncmp(s1->vls_str+s1->vls_offset, s2->vls_str+s2->vls_offset, n);
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


int
bu_argv_from_string(char *argv[], int lim, char *lp)
{
    int argc = 0; /* number of words seen */
    int skip = 0;
    int quoted = 0;

    if (UNLIKELY(!argv)) {
	/* do this instead of crashing */
	bu_bomb("bu_argv_from_string received a null argv\n");
    }
    argv[0] = (char *)NULL;

    if (UNLIKELY(lim <= 0 || !lp)) {
	/* nothing to do, only return NULL */
	return 0;
    }

    /* skip leading whitespace */
    while (*lp != '\0' && isspace(*lp))
	lp++;

    if (*lp == '\0') {
	/* no words, only return NULL */
	return 0;
    }

    /* some non-space string has been seen, set argv[0] */
    argc = 0;
    argv[argc] = lp;

    for (; *lp != '\0'; lp++) {

	if (*lp == '"') {
	    if (!quoted) {
		/* start collecting quoted string */
		quoted = 1;

		/* skip past the quote character */
		argv[argc] = lp + 1;
		continue;
	    }

	    /* end qoute */
	    quoted = 0;
	    *lp++ = '\0';

	    /* skip leading whitespace */
	    while (*lp != '\0' && isspace(*lp)) {
		/* null out spaces */
		*lp = '\0';
		lp++;
	    }

	    skip = 0;
	    goto nextword;
	}

	/* skip over current word */
	if (quoted || !isspace(*lp))
	    continue;

	skip = 0;

	/* terminate current word, skip space until we find the start
	 * of the next word nulling out the spaces as we go along.
	 */
	while (*(lp+skip) != '\0' && isspace(*(lp+skip))) {
	    lp[skip] = '\0';
	    skip++;
	}

	if (*(lp + skip) == '\0')
	    break;

    nextword:
	/* make sure argv[] isn't full, need room for NULL */
	if (argc >= lim-1)
	    break;

	/* start of next word */
	argc++;
	argv[argc] = lp + skip;

	/* jump over the spaces, remember the loop's lp++ */
	lp += skip - 1;
    }

    /* always NULL-terminate the array */
    argc++;
    argv[argc] = (char *)NULL;
    return argc;
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
	   isspace(bu_vls_addr(vp)[bu_vls_strlen(vp)-1]))
	bu_vls_trunc(vp, -1);

    /* Remove leading white space */
    while ((vp->vls_len > 0) &&
	   isspace(*bu_vls_addr(vp)))
	bu_vls_nibble(vp, 1);
}

void
bu_vls_vprintf(struct bu_vls *vls, const char *fmt, va_list ap)
{
    const char *sp; /* start pointer */
    const char *ep; /* end pointer */
    int len;

#define LONG_INT 0x001
#define FIELDLEN 0x002
#define SHORTINT 0x004
#define LLONGINT 0x008
#define SHHRTINT 0x010
#define INTMAX_T 0x020
#define PTRDIFFT 0x040
#define SIZETINT 0x080

    int flags;
    int fieldlen = -1;
    int left_justify = 0;

    char fbuf[64] = {0}; /* % format buffer */
    char buf[BUFSIZ] = {0};

    if (UNLIKELY(!vls || !fmt || fmt[0] == '\0')) {
	/* nothing to print to or from */
	return;
    }

    BU_CK_VLS(vls);

    bu_vls_extend(vls, (unsigned int)_VLS_ALLOC_STEP);

    sp = fmt;
    while (*sp) {
	/* Initial state:  just printing chars */
	fmt = sp;
	while (*sp != '%' && *sp)
	    sp++;

	if (sp != fmt)
	    bu_vls_strncat(vls, fmt, (size_t)(sp-fmt));

	if (*sp == '\0')
	    break;

	/* Saw a percent sign, find end of fmt specifier */

	flags = 0;
	ep = sp;
	while (*ep) {
	    ++ep;
	    if (*ep == ' ' || *ep == '#' || *ep == '+' || *ep == '.' || isdigit(*ep)) {
		continue;
	    } else if (*ep == '-') {
		left_justify = 1;
	    } else if (*ep == 'l' || *ep == 'U' || *ep == 'O') {
		if (flags & LONG_INT) {
		    flags ^= LONG_INT;
		    flags |= LLONGINT;
		} else {
		    flags |= LONG_INT;
		}
	    } else if (*ep == '*') {
		fieldlen = va_arg(ap, int);
		flags |= FIELDLEN;
	    } else if (*ep == 'h') {
		if (flags & SHORTINT) {
		    flags ^= SHORTINT;
		    flags |= SHHRTINT;
		} else {
		    flags |= SHORTINT;
		}
	    } else if (*ep == 'j') {
		flags |= INTMAX_T;
	    } else if (*ep == 't') {
		flags |= PTRDIFFT;
	    } else if (*ep == 'z') {
		flags |= SIZETINT;
	    } else
		/* Anything else must be the end of the fmt specifier */
		break;
	}

	/* libc left-justifies if there's a '-' char, even if the
	 * value is already negative so no need to check current value
	 * of left_justify.
	 */
	if (fieldlen < 0) {
	    fieldlen = -fieldlen;
	    left_justify = 1;
	}

	/* Copy off the format string */
	len = ep-sp+1;
	if ((size_t)len > sizeof(fbuf))
	    len = sizeof(fbuf);
	/* intentionally avoid bu_strlcpy here since the source field
	 * may be legitimately truncated.  FIXME: verify that claim.
	 */
	strncpy(fbuf, sp, (size_t)len-1);
	fbuf[len] = '\0'; /* sanity */

#ifndef HAVE_C99_FORMAT_SPECIFIERS
	/* if the format string uses the %z width specifier, we need
	 * to replace it with something more palatable to this busted
	 * compiler.
	 */

	if (flags & SIZETINT) {
	    char *fp = fbuf;
	    while (*fp) {
		if (*fp == '%') {
		    /* found the next format specifier */
		    while (*fp) {
			fp++;
			/* possible characters that can preceed the
			 * field length character (before the type).
			 */
			if (isdigit(*fp)
			    || *fp == '$'
			    || *fp == '#'
			    || *fp == '+'
			    || *fp == '.'
			    || *fp == '-'
			    || *fp == '*') {
			    continue;
			}
			if (*fp == 'z') {
			    /* assume MSVC replacing instances of %z
			     * with %I (capital i) until we encounter
			     * anything different.
			     */
			    *fp = 'I';
			}

			break;
		    }
		    if (*fp == '\0') {
			break;
		    }
		}
		fp++;
	    }
	}
#endif

	/* Grab parameter from arg list, and print it */
	switch (*ep) {
	    case 's':
		{
		    char *str;

		    str = va_arg(ap, char *);
		    if (str) {
			if (flags & FIELDLEN) {
			    int stringlen = (int)strlen(str);

			    if (stringlen >= fieldlen)
				bu_vls_strncat(vls, str, (size_t)fieldlen);
			    else {
				struct bu_vls padded;
				int i;

				bu_vls_init(&padded);
				if (left_justify)
				    bu_vls_strcat(&padded, str);
				for (i = 0; i < fieldlen - stringlen; ++i)
				    bu_vls_putc(&padded, ' ');
				if (!left_justify)
				    bu_vls_strcat(&padded, str);
				bu_vls_vlscat(vls, &padded);
			    }
			} else {
			    bu_vls_strcat(vls, str);
			}
		    }  else  {
			if (flags & FIELDLEN)
			    bu_vls_strncat(vls, "(null)", (size_t)fieldlen);
			else
			    bu_vls_strcat(vls, "(null)");
		    }
		}
		break;
	    case 'S': /* XXX - DEPRECATED [7.14] */
		printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		/* fall through */
	    case 'V':
		{
		    struct bu_vls *vp;

		    vp = va_arg(ap, struct bu_vls *);
		    if (vp) {
			BU_CK_VLS(vp);
			if (flags & FIELDLEN) {
			    int stringlen = bu_vls_strlen(vp);

			    if (stringlen >= fieldlen)
				bu_vls_strncat(vls, bu_vls_addr(vp), (size_t)fieldlen);
			    else {
				struct bu_vls padded;
				int i;

				bu_vls_init(&padded);
				if (left_justify)
				    bu_vls_vlscat(&padded, vp);
				for (i = 0; i < fieldlen - stringlen; ++i)
				    bu_vls_putc(&padded, ' ');
				if (!left_justify)
				    bu_vls_vlscat(&padded, vp);
				bu_vls_vlscat(vls, &padded);
			    }
			} else {
			    bu_vls_vlscat(vls, vp);
			}
		    }  else  {
			if (flags & FIELDLEN)
			    bu_vls_strncat(vls, "(null)", (size_t)fieldlen);
			else
			    bu_vls_strcat(vls, "(null)");
		    }
		}
		break;
	    case 'e':
	    case 'E':
	    case 'f':
	    case 'g':
	    case 'G':
		/* All floating point ==> "double" */
		{
		    double d;

		    d = va_arg(ap, double);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, d);
		    else
			snprintf(buf, BUFSIZ, fbuf, d);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'o':
	    case 'u':
	    case 'x':
		if (flags & LONG_INT) {
		    /* Unsigned long int */
		    unsigned long l;

		    l = va_arg(ap, unsigned long);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, l);
		    else
			snprintf(buf, BUFSIZ, fbuf, l);
		} else if (flags & LLONGINT) {
		    /* Unsigned long long int */
		    unsigned long long ll;

		    ll = va_arg(ap, unsigned long long);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, ll);
		    else
			snprintf(buf, BUFSIZ, fbuf, ll);
		} else if (flags & SHORTINT || flags & SHHRTINT) {
		    /* unsigned short int */
		    unsigned short int sh;
		    sh = (unsigned short int)va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, sh);
		    else
			snprintf(buf, BUFSIZ, fbuf, sh);
		} else if (flags & INTMAX_T) {
		    intmax_t im;
		    im = va_arg(ap, intmax_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, im);
		    else
			snprintf(buf, BUFSIZ, fbuf, im);
		} else if (flags & PTRDIFFT) {
		    ptrdiff_t pd;
		    pd = va_arg(ap, ptrdiff_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, pd);
		    else
			snprintf(buf, BUFSIZ, fbuf, pd);
		} else if (flags & SIZETINT) {
		    size_t st;
		    st = va_arg(ap, size_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, st);
		    else
			snprintf(buf, BUFSIZ, fbuf, st);
		} else {
		    /* Regular unsigned int */
		    unsigned int j;

		    j = (unsigned int)va_arg(ap, unsigned int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbuf, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'd':
	    case 'i':
		if (flags & LONG_INT) {
		    /* Long int */
		    long l;

		    l = va_arg(ap, long);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, l);
		    else
			snprintf(buf, BUFSIZ, fbuf, l);
		} else if (flags & LLONGINT) {
		    /* Long long int */
		    long long ll;

		    ll = va_arg(ap, long long);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, ll);
		    else
			snprintf(buf, BUFSIZ, fbuf, ll);
		} else if (flags & SHORTINT || flags & SHHRTINT) {
		    /* short int */
		    short int sh;
		    sh = (short int)va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, sh);
		    else
			snprintf(buf, BUFSIZ, fbuf, sh);
		} else if (flags & INTMAX_T) {
		    intmax_t im;
		    im = va_arg(ap, intmax_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, im);
		    else
			snprintf(buf, BUFSIZ, fbuf, im);
		} else if (flags & PTRDIFFT) {
		    ptrdiff_t pd;
		    pd = va_arg(ap, ptrdiff_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, pd);
		    else
			snprintf(buf, BUFSIZ, fbuf, pd);
		} else if (flags & SIZETINT) {
		    size_t st;
		    st = va_arg(ap, size_t);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, st);
		    else
			snprintf(buf, BUFSIZ, fbuf, st);
		} else {
		    /* Regular int */
		    int j;

		    j = va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbuf, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'n':
	    case 'p':
		/* all pointer == "void *" */
		{
		    void *vp;
		    vp = (void *)va_arg(ap, void *);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, vp);
		    else
			snprintf(buf, BUFSIZ, fbuf, vp);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case '%':
		bu_vls_putc(vls, '%');
		break;
	    default:  /* Something weird, maybe %c */
		{
		    int j;

		    /* We hope, whatever it is, it fits in an int and the resulting
		       stringlet is smaller than sizeof(buf) bytes */

		    j = va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbuf, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	}
	sp = ep+1;
    }

    va_end(ap);
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
    struct bu_vls src;
    char *cp;
    int used;

    BU_CK_VLS(vp);

    bu_vls_init(&src);
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
