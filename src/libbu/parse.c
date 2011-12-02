/*                         P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "bu.h"


/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','

#define CKMEM(_len) {							\
	register ssize_t offset;					\
	if ((offset = (ep - cp) - (_len)) < 0) {			\
	    do {							\
		offset += ext->ext_nbytes;	/* decr by new growth */ \
		ext->ext_nbytes <<= 1;					\
	    } while (offset < 0);					\
	    offset = cp - (char *)ext->ext_buf;				\
	    ext->ext_buf = (genptr_t)bu_realloc((char *) ext->ext_buf,	\
						ext->ext_nbytes, "bu_struct_export"); \
	    ep = (char *) ext->ext_buf + ext->ext_nbytes;		\
	    cp = (char *) ext->ext_buf + offset;			\
	}								\
    }


#define BU_GETPUT_MAGIC_1 0x15cb
#define BU_GETPUT_MAGIC_2 0xbc51
#define BU_INIT_GETPUT_1(_p) {						\
	BU_CK_EXTERNAL(_p);						\
	((unsigned char *) _p->ext_buf)[1] = (BU_GETPUT_MAGIC_1 & 0xFF); \
	((unsigned char *) _p->ext_buf)[0] = (BU_GETPUT_MAGIC_1 >> 8) & 0xFF; \
    }
#define BU_INIT_GETPUT_2(_p, _l) {					\
	BU_CK_EXTERNAL(_p);						\
	((unsigned char *) _p->ext_buf)[_l-1] = (BU_GETPUT_MAGIC_2 & 0xFF); \
	((unsigned char *) _p->ext_buf)[_l-2] = (BU_GETPUT_MAGIC_2 >> 8) & 0xFF; \
    }


#define BU_CK_GETPUT(_p) {						\
	register unsigned long _i;					\
	register size_t _len;						\
	BU_CK_EXTERNAL(_p);						\
	if (UNLIKELY(!(_p->ext_buf))) {					\
	    bu_log("ERROR: BU_CK_GETPUT null ext_buf, file %s, line %d\n", \
		   __FILE__, __LINE__);					\
	    bu_bomb("NULL pointer");					\
	}								\
	if (UNLIKELY(_p->ext_nbytes < 6)) {				\
	    bu_log("ERROR: BU_CK_GETPUT buffer only %zu bytes, file %s, line %d\n", \
		   _p->ext_nbytes, __FILE__, __LINE__);			\
	    bu_bomb("getput buffer too small");				\
	}								\
	_i = (((unsigned char *)(_p->ext_buf))[0] << 8) |		\
	    ((unsigned char *)(_p->ext_buf))[1];			\
	if (UNLIKELY(_i != BU_GETPUT_MAGIC_1)) {			\
	    bu_log("ERROR: BU_CK_GETPUT buffer %p, magic1 s/b %x, was %s(0x%lx), file %s, line %d\n", \
		   (void *)_p->ext_buf, BU_GETPUT_MAGIC_1,		\
		   bu_identify_magic(_i), _i, __FILE__, __LINE__);	\
	    bu_bomb("Bad getput buffer");				\
	}								\
	_len = (((unsigned char *)(_p->ext_buf))[2] << 24) |		\
	    (((unsigned char *)(_p->ext_buf))[3] << 16) |		\
	    (((unsigned char *)(_p->ext_buf))[4] <<  8) |		\
	    ((unsigned char *)(_p->ext_buf))[5];			\
	if (UNLIKELY(_len > _p->ext_nbytes)) {				\
	    bu_log("ERROR: BU_CK_GETPUT buffer %p, expected len=%zu, ext_nbytes=%zu, file %s, line %d\n", \
		   (void *)_p->ext_buf, (size_t)_len, _p->ext_nbytes,	\
		   __FILE__, __LINE__);					\
	    bu_bomb("Bad getput buffer");				\
	}								\
	_i = (((unsigned char *)(_p->ext_buf))[_len-2] << 8) |		\
	    ((unsigned char *)(_p->ext_buf))[_len-1];			\
	if (UNLIKELY(_i != BU_GETPUT_MAGIC_2)) {			\
	    bu_log("ERROR: BU_CK_GETPUT buffer %p, magic2 s/b %x, was %s(0x%lx), file %s, line %d\n", \
		   (void *)_p->ext_buf, BU_GETPUT_MAGIC_2,		\
		   bu_identify_magic(_i), _i, __FILE__, __LINE__);	\
	    bu_bomb("Bad getput buffer");				\
	}								\
    }


int
bu_struct_export(struct bu_external *ext, const genptr_t base, const struct bu_structparse *imp)
{
    register char *cp;			/* current possition in buffer */
    char *ep;				/* &ext->ext_buf[ext->ext_nbytes] */
    const struct bu_structparse *ip;	/* current imexport structure */
    char *loc;				/* where host-format data is */
    size_t len;
    register size_t i;

    BU_EXTERNAL_INIT(ext);

    ext->ext_nbytes = 480;
    ext->ext_buf = (genptr_t)bu_malloc(ext->ext_nbytes,
				       "bu_struct_export output ext->ext_buf");
    BU_INIT_GETPUT_1(ext);
    cp = (char *) ext->ext_buf + 6; /* skip magic and length */
    ep = cp + ext->ext_nbytes;

    for (ip = imp; ip->sp_fmt[0] != '\0'; ip++) {

	loc = ((char *)base) + ip->sp_offset;

	switch (ip->sp_fmt[0]) {
	    case 'i':
		{
		    /* DEPRECATED: use %p instead. */
		    static int warned = 0;
		    if (!warned) {
			bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
			warned++;
		    }
		}
		continue;
	    case '%':
		/* See below */
		break;
	    default:
		/* Unknown */
		bu_free((char *) ext->ext_buf, "output ext_buf");
		return 0;
	}
	/* [0] == '%', use printf-like format char */
	switch (ip->sp_fmt[1]) {
	    case 'f':
		/* Double-precision floating point */
		len = ip->sp_count * 8;
		CKMEM(len);
		htond((unsigned char *)cp, (unsigned char *)loc, ip->sp_count);
		cp += len;
		continue;
	    case 'd':
		/* 32-bit network integer, from "int" */
		CKMEM(ip->sp_count * 4);
		{
		    register unsigned long l;
		    for (i = ip->sp_count; i > 0; i--) {
			l = *((int *)loc);
			cp[3] = l;
			cp[2] = l >> 8;
			cp[1] = l >> 16;
			cp[0] = l >> 24;
			loc += sizeof(int);
			cp += 4;
		    }
		}
		continue;
	    case 'i':
		/* 16-bit integer, from "int" */
		CKMEM(ip->sp_count * 2);
		{
		    register unsigned short s;
		    for (i = ip->sp_count; i > 0; i--) {
			s = *((int *)loc);
			cp[1] = s;
			cp[0] = s >> 8;
			loc += sizeof(int); /* XXX */
			cp += 2;
		    }
		}
		continue;
	    case 's':
		{
		    /* char array is transmitted as a 4 byte character
		     * count, followed by a null terminated, word
		     * padded char array.  The count includes any pad
		     * bytes, but not the count itself.
		     *
		     * ip->sp_count == sizeof(char array)
		     */
		    register size_t lenstr;

		    /* include the terminating null */
		    lenstr = (int)strlen(loc) + 1;

		    len = lenstr;

		    /* output an integer number of words */
		    if ((len & 0x03) != 0)
			len += 4 - (len & 0x03);

		    CKMEM(len + 4);

		    /* put the length on the front of the string
		     */
		    cp[3] = (char)len;
		    cp[2] = (char)(len >> 8);
		    cp[1] = (char)(len >> 16);
		    cp[0] = (char)(len >> 24);

		    cp += 4;

		    memcpy(cp, loc, lenstr);
		    cp += lenstr;
		    while (lenstr++ < len) *cp++ = '\0';
		}
		continue;
	    case 'c':
		{
		    CKMEM(ip->sp_count + 4);
		    cp[3] = (char)ip->sp_count;
		    cp[2] = (char)(ip->sp_count >> 8);
		    cp[1] = (char)(ip->sp_count >> 16);
		    cp[0] = (char)(ip->sp_count >> 24);
		    cp += 4;
		    memcpy(cp, loc, ip->sp_count);
		    cp += ip->sp_count;
		}
		continue;
	    case 'p':
		{
		    /* Indirect to another structure */
		    /* FIXME: unimplemented */
		    bu_log("INTERNAL ERROR: attempt to indirectly export bu_structparse table, unimplemented\n");
		}
		continue;
	    default:
		bu_free((char *) ext->ext_buf, "output ext_buf");
		return 0;
	}
    }
    CKMEM(2);	/* get room for the trailing magic number */
    cp += 2;

    i = cp - (char *)ext->ext_buf;
    /* Fill in length in external buffer */
    ((char *)ext->ext_buf)[5] = (char)i;
    ((char *)ext->ext_buf)[4] = (char)(i >> 8);
    ((char *)ext->ext_buf)[3] = (char)(i >>16);
    ((char *)ext->ext_buf)[2] = (char)(i >>24);
    BU_INIT_GETPUT_2(ext, i);
    ext->ext_nbytes = i;	/* XXX this changes nbytes if i < 480 ? */
    return 1;
}


int
bu_struct_import(genptr_t base, const struct bu_structparse *imp, const struct bu_external *ext)
{
    register const unsigned char *cp;	/* current possition in buffer */
    const struct bu_structparse *ip;	/* current imexport structure */
    char *loc;		/* where host-format data is */
    size_t len;
    size_t bytes_used;
    register size_t i;

    BU_CK_GETPUT(ext);

    cp = (unsigned char *)ext->ext_buf+6;
    bytes_used = 0;
    for (ip = imp; ip->sp_fmt[0] != '\0'; ip++) {

	loc = ((char *)base) + ip->sp_offset;

	switch (ip->sp_fmt[0]) {
	    case 'i':
		{
		    /* DEPRECATED: use %p instead. */
		    static int warned = 0;
		    if (!warned) {
			bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
			warned++;
		    }
		}
		continue;
	    case '%':
		/* See below */
		break;
	    default:
		/* Unknown */
		return -1;
	}
	/* [0] == '%', use printf-like format char */
	switch (ip->sp_fmt[1]) {
	    case 'f':
		/* Double-precision floating point */
		len = ip->sp_count * 8;
		ntohd((unsigned char *)loc, cp, ip->sp_count);
		cp += len;
		bytes_used += len;
		break;
	    case 'd':
		/* 32-bit network integer, from "int" */
		{
		    register long l;
		    for (i = ip->sp_count; i > 0; i--) {
			l =	(cp[0] << 24) |
			    (cp[1] << 16) |
			    (cp[2] <<  8) |
			    cp[3];
			*(int *)loc = l;
			loc += sizeof(int); /* XXX */
			cp += 4;
		    }
		    bytes_used += ip->sp_count * 4;
		}
		break;
	    case 'i':
		/* 16-bit integer, from "int" */
		for (i = ip->sp_count; i > 0; i--) {
		    *(int *)loc =	(cp[0] <<  8) |
			cp[1];
		    loc += sizeof(int); /* XXX */
		    cp += 2;
		}
		bytes_used += ip->sp_count * 2;
		break;
	    case 's':
		{
		    /* char array transmitted as a 4 byte character
		     * count, followed by a null terminated, word
		     * padded char array
		     *
		     * ip->sp_count == sizeof(char array)
		     */
		    register unsigned long lenstr;

		    lenstr = (cp[0] << 24) |
			(cp[1] << 16) |
			(cp[2] <<  8) |
			cp[3];

		    cp += 4;

		    /* don't read more than the buffer can hold */
		    if ((unsigned long)ip->sp_count < lenstr)
			memcpy(loc, cp, ip->sp_count);
		    else
			memcpy(loc, cp, lenstr);

		    /* ensure proper null termination */
		    loc[ip->sp_count-1] = '\0';

		    cp += lenstr;
		    bytes_used += lenstr;
		}
		break;
	    case 'c':
		{
		    register unsigned long lenarray;

		    lenarray = (cp[0] << 24) |
			(cp[1] << 16) |
			(cp[2] <<  8) |
			cp[3];
		    cp += 4;

		    if ((unsigned long)ip->sp_count < lenarray) {
			memcpy(loc, cp, ip->sp_count);
		    } else {
			memcpy(loc, cp, lenarray);
		    }
		    cp += lenarray;
		    bytes_used += lenarray;
		}
		break;
	    case 'p':
		{
		    /* Indirect to another structure */
		    /* FIXME: unimplemented */
		    bu_log("INTERNAL ERROR: attempt to indirectly import bu_structparse table, unimplemented\n");
		}
		break;
	    default:
		return -1;
	}
	if (ip->sp_hook) {
	    ip->sp_hook(ip, ip->sp_name, (const char *)base, NULL);
	}
    }

    /* This number may differ from that stored as "claimed_length" */
    BU_ASSERT_LONG(bytes_used, <, INT_MAX);
    return (int)bytes_used;
}


size_t
bu_struct_put(FILE *fp, const struct bu_external *ext)
{
    BU_CK_GETPUT(ext);

    return fwrite(ext->ext_buf, 1, ext->ext_nbytes, fp);
}


size_t
bu_struct_get(struct bu_external *ext, FILE *fp)
{
    size_t i;
    uint32_t len;

    BU_EXTERNAL_INIT(ext);
    ext->ext_buf = (genptr_t) bu_malloc(6, "bu_struct_get buffer head");
    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */

    i = fread((char *) ext->ext_buf, 1, 6, fp);	/* res_syscall */
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */

    if (i != 6) {
	if (i == 0)
	    return 0;

	perror("fread");
	bu_log("ERROR: bu_struct_get bad fread (%ld), file %s, line %d\n",
	       i, __FILE__, __LINE__);
	return 0;
    }

    i = (((unsigned char *)(ext->ext_buf))[0] << 8)
	| ((unsigned char *)(ext->ext_buf))[1];

    len = (((unsigned char *)(ext->ext_buf))[2] << 24)
	| (((unsigned char *)(ext->ext_buf))[3] << 16)
	| (((unsigned char *)(ext->ext_buf))[4] <<  8)
	| ((unsigned char *)(ext->ext_buf))[5];

    if (UNLIKELY(i != BU_GETPUT_MAGIC_1)) {
	bu_log("ERROR: bad getput buffer header %p, s/b %x, was %s(0x%lx), file %s, line %d\n",
	       (void *)ext->ext_buf, BU_GETPUT_MAGIC_1,
	       bu_identify_magic(i), i, __FILE__, __LINE__);
	bu_bomb("bad getput buffer");
    }
    ext->ext_nbytes = len;
    ext->ext_buf = (genptr_t) bu_realloc((char *) ext->ext_buf, len,
					 "bu_struct_get full buffer");
    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
    i = fread((char *) ext->ext_buf + 6, 1, len-6, fp);	/* res_syscall */
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */

    if (UNLIKELY(i != len-6)) {
	bu_log("ERROR: bu_struct_get bad fread (%ld), file %s, line %d\n",
	       i, __FILE__, __LINE__);
	ext->ext_nbytes = 0;
	bu_free(ext->ext_buf, "bu_struct_get full buffer");
	ext->ext_buf = NULL;
	return 0;
    }

    i = (((unsigned char *)(ext->ext_buf))[len-2] << 8)
	| ((unsigned char *)(ext->ext_buf))[len-1];

    if (UNLIKELY(i != BU_GETPUT_MAGIC_2)) {
	bu_log("ERROR: bad getput buffer %p, s/b %x, was %s(0x%lx), file %s, line %d\n",
	       (void *)ext->ext_buf, BU_GETPUT_MAGIC_2,
	       bu_identify_magic(i), i, __FILE__, __LINE__);
	ext->ext_nbytes = 0;
	bu_free(ext->ext_buf, "bu_struct_get full buffer");
	ext->ext_buf = NULL;
	return 0;
    }

    return (size_t)len;
}


void
bu_struct_wrap_buf(struct bu_external *ext, genptr_t buf)
{
    register long i, len;

    BU_EXTERNAL_INIT(ext);
    ext->ext_buf = buf;
    i = (((unsigned char *)(ext->ext_buf))[0] << 8) |
	((unsigned char *)(ext->ext_buf))[1];
    len = (((unsigned char *)(ext->ext_buf))[2] << 24) |
	(((unsigned char *)(ext->ext_buf))[3] << 16) |
	(((unsigned char *)(ext->ext_buf))[4] <<  8) |
	((unsigned char *)(ext->ext_buf))[5];
    if (UNLIKELY(i != BU_GETPUT_MAGIC_1)) {
	bu_log("ERROR: bad getput buffer header %p, s/b %x, was %s(0x%lx), file %s, line %d\n",
	       (void *)ext->ext_buf, BU_GETPUT_MAGIC_1,
	       bu_identify_magic(i), i, __FILE__, __LINE__);
	bu_bomb("bad getput buffer");
    }
    ext->ext_nbytes = len;
    i = (((unsigned char *)(ext->ext_buf))[len-2] <<8) |
	((unsigned char *)(ext->ext_buf))[len-1];
    if (UNLIKELY(i != BU_GETPUT_MAGIC_2)) {
	bu_log("ERROR: bad getput buffer %p, s/b %x, was %s(0x%lx), file %s, line %d\n",
	       (void *)ext->ext_buf, BU_GETPUT_MAGIC_2,
	       bu_identify_magic(i), i, __FILE__, __LINE__);
	bu_bomb("Bad getput buffer");
    }
}


/**
 * Parse an array of one or more doubles.
 *
 * Returns: 0 when successful
 *         <0 upon failure
 */
HIDDEN int
parse_double(const char *str, size_t count, double *loc)
{
    size_t i;
    int dot_seen;
    const char *numstart;
    double tmp_double;
    char buf[128];
    int len;

    for (i=0; i < count && *str; ++i) {
	numstart = str;

	/* skip sign */
	if (*str == '-' || *str == '+') str++;

	/* skip matissa */
	dot_seen = 0;
	for (; *str; str++) {
	    if (*str == '.' && !dot_seen) {
		dot_seen = 1;
		continue;
	    }
	    if (!isdigit(*str))
		break;
	}

	/* If no mantissa seen, then there is no float here */
	if (str == (numstart + dot_seen))
	    return -1;

	/* there was a mantissa, so we may have an exponent */
	if (*str == 'E' || *str == 'e') {
	    str++;

	    /* skip exponent sign */
	    if (*str == '+' || *str == '-') str++;

	    while (isdigit(*str)) str++;
	}

	len = str - numstart;
	if ((size_t)len > sizeof(buf)-1)
	    len = sizeof(buf)-1;
	strncpy(buf, numstart, len);
	buf[len] = '\0';

	if (UNLIKELY(sscanf(buf, "%lf", &tmp_double) != 1))
	    return -1;

	*loc++ = tmp_double;

	/* skip any whitespace before separator */
	while (*str && isspace(*str))
	    str++;

	/* skip the separator */
	if (*str && !isdigit(*str) && *str != '-' && *str != '+' && *str != '.')
	    str++;

	/* skip any whitespace after separator */
	while (*str && isspace(*str))
	    str++;
    }
    return 0;
}


/**
 *
 * @return -2 parse error
 * @return -1 not found
 * @return  0 entry found and processed
 */
HIDDEN int
parse_struct_lookup(register const struct bu_structparse *sdp, register const char *name, const char *base, const char *const value)
/* structure description */
/* struct member name */
/* begining of structure */
/* string containing value */
{
    register char *loc;
    size_t i;
    int retval = 0;

    for (; sdp->sp_name != (char *)0; sdp++) {

	if (!BU_STR_EQUAL(sdp->sp_name, name)	/* no name match */
	    && sdp->sp_fmt[0] != 'i'
	    && sdp->sp_fmt[1] != 'p')		/* no include desc */

	    continue;

	/* if we get this far, we've got a name match with a name in
	 * the structure description
	 */

	loc = (char *)(base + sdp->sp_offset);

	if (sdp->sp_fmt[0] == 'i') {
	    static int warned = 0;
	    if (!warned) {
		bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
		warned++;
	    }
	    /* Indirect to another structure */
	    if (parse_struct_lookup((struct bu_structparse *)sdp->sp_count, name, base, value) == 0)
		return 0;	/* found */
	    else
		continue;
	}
	if (sdp->sp_fmt[0] != '%') {
	    bu_log("parse_struct_lookup(%s): unknown format '%s'\n",
		   name, sdp->sp_fmt);
	    return -1;
	}

	switch (sdp->sp_fmt[1]) {
	    case 'c':
	    case 's':
		{
		    register size_t j;

		    /* copy the string, converting escaped double
		     * quotes to just double quotes
		     */
		    for (i=j=0;
			 j < sdp->sp_count && value[i] != '\0';
			 loc[j++] = value[i++])
			if (value[i] == '\\' &&
			    value[i+1] == '"')
			    ++i;

		    /* Don't null terminate chars, only strings */
		    if (sdp->sp_count > 1) {
			/* OK, it's a string */
			if (j < sdp->sp_count-1)
			    loc[j] = '\0';
			else
			    loc[sdp->sp_count-1] = '\0';
		    }
		}
		break;
	    case 'S': /* XXX - DEPRECATED [7.14] */
		printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		/* fall through */
	    case 'V':
		{
		    struct bu_vls *vls = (struct bu_vls *)loc;
		    bu_vls_strcpy(vls, value);
		}
		break;
	    case 'i':
		{
		    register short *ip = (short *)loc;
		    register short tmpi;
		    register const char *cp;
		    register const char *pv = value;

		    for (i=0; i < sdp->sp_count && *pv; ++i) {
			tmpi = atoi(pv);

			cp = pv;
			if (*cp && (*cp == '+' || *cp == '-'))
			    cp++;

			while (*cp && isdigit(*cp))
			    cp++;

			/* make sure we actually had an
			 * integer out there
			 */
			if (cp == pv ||
			    (cp == pv+1 &&
			     (*pv == '+' || *pv == '-'))) {
			    retval = -2;
			    break;
			} else {
			    *(ip++) = tmpi;
			    pv = cp;
			}

			/* skip any whitespace before separator */
			while (*pv && isspace(*pv))
			    pv++;

			/* skip the separator */
			if (*pv && !isdigit(*pv) && *pv != '+' && *pv != '-')
			    pv++;

			/* skip any whitespace after separator */
			while (*pv && isspace(*pv))
			    pv++;
		    }
		}
		break;
	    case 'd':
		{
		    register int *ip = (int *)loc;
		    register int tmpi;
		    register char const *cp;
		    register const char *pv = value;

		    /* Special case:  '=!' toggles a boolean */
		    if (*pv == '!') {
			*ip = *ip ? 0 : 1;
			pv++;
			break;
		    }
		    /* Normal case: an integer */
		    for (i=0; i < sdp->sp_count && *pv; ++i) {
			tmpi = atoi(pv);

			cp = pv;
			if (*cp && (*cp == '+' || *cp == '-'))
			    cp++;

			while (*cp && isdigit(*cp))
			    cp++;

			/* make sure we actually had an
			 * integer out there
			 */
			if (cp == pv ||
			    (cp == pv+1 &&
			     (*pv == '+' || *pv == '-'))) {
			    retval = -2;
			    break;
			} else {
			    *(ip++) = tmpi;
			    pv = cp;
			}

			/* skip any whitespace before separator */
			while (*pv && isspace(*pv))
			    pv++;

			/* skip the separator */
			if (*pv && !isdigit(*pv) && *pv != '+' && *pv != '-')
			    pv++;

			/* skip any whitespace after separator */
			while (*pv && isspace(*pv))
			    pv++;
		    }
		}
		break;
	    case 'f':
		retval = parse_double(value, sdp->sp_count, (double *)loc);
		break;
	    case 'p':
		retval = parse_struct_lookup((struct bu_structparse *)sdp->sp_count, name, base, value);
		if (retval == 0) {
		    return 0; /* found */
		}
		continue;
	    default:
		bu_log("parse_struct_lookup(%s): unknown format '%s'\n",
		       name, sdp->sp_fmt);
		return -1;
	}
	if (sdp->sp_hook) {
	    sdp->sp_hook(sdp, name, base, value);
	}
	return retval;		/* OK or parse error */
    }
    return -1;			/* Not found */
}


int
bu_struct_parse(const struct bu_vls *in_vls, const struct bu_structparse *desc, const char *base)
/* string to parse through */
/* structure description */
/* base addr of users struct */
{
    struct bu_vls vls;
    register char *cp;
    char *name;
    char *value;
    int retval;

    BU_CK_VLS(in_vls);
    if (UNLIKELY(desc == (struct bu_structparse *)NULL)) {
	bu_log("NULL \"struct bu_structparse\" pointer\n");
	return -1;
    }

    /* Duplicate the input string.  This algorithm is destructive. */
    bu_vls_init(&vls);
    bu_vls_vlscat(&vls, in_vls);
    cp = bu_vls_addr(&vls);

    while (*cp) {
	/* NAME = VALUE white-space-separator */

	/* skip any leading whitespace */
	while (*cp != '\0' && isspace(*cp))
	    cp++;

	/* Find equal sign */
	name = cp;
	while (*cp != '\0' && *cp != '=')
	    cp++;

	if (*cp == '\0') {
	    if (name == cp) break;

	    /* end of string in middle of arg */
	    bu_log("bu_structparse: input keyword '%s' is not followed by '=' in '%s'\nInput must be in keyword=value format.\n",
		   name, bu_vls_addr(in_vls));
	    bu_vls_free(&vls);
	    return -2;
	}

	*cp++ = '\0';

	/* Find end of value. */
	if (*cp == '"') {
	    /* strings are double-quote (") delimited skip leading " &
	     * find terminating " while skipping escaped quotes (\")
	     */
	    for (value = ++cp; *cp != '\0'; ++cp)
		if (*cp == '"' &&
		    (cp == value || *(cp-1) != '\\'))
		    break;

	    if (*cp != '"') {
		bu_log("bu_structparse: keyword '%s'=\" without closing \"\n",
		       name);
		bu_vls_free(&vls);
		return -3;
	    }
	} else {
	    /* non-strings are white-space delimited */
	    value = cp;
	    while (*cp != '\0' && !isspace(*cp))
		cp++;
	}

	if (*cp != '\0')
	    *cp++ = '\0';

	/* Lookup name in desc table and modify */
	retval = parse_struct_lookup(desc, name, base, value);
	if (retval == -1) {
	    bu_log("bu_structparse:  '%s=%s', keyword not found in:\n",
		   name, value);
	    bu_struct_print("troublesome one", desc, base);
	} else if (retval == -2) {
	    bu_vls_free(&vls);
	    return -2;
	}

    }
    bu_vls_free(&vls);
    return 0;
}


/**
 *
 * XXX Should this be here, or could it be with the matrix support?
 * pretty-print a matrix
 */
HIDDEN void
parse_matprint(const char *name, register const double *mat)
{
    int delta = (int)strlen(name)+2;

    /* indent the body of the matrix */
    bu_log_indent_delta(delta);

    bu_log(" %s=%12E %12E %12E %12E\n",
	   name, mat[0], mat[1], mat[2], mat[3]);

    bu_log("%12E %12E %12E %12E\n",
	   mat[4], mat[5], mat[6], mat[7]);

    bu_log("%12E %12E %12E %12E\n",
	   mat[8], mat[9], mat[10], mat[11]);

    bu_log_indent_delta(-delta);

    bu_log("%12E %12E %12E %12E\n",
	   mat[12], mat[13], mat[14], mat[15]);
}


HIDDEN void
parse_vls_matprint(struct bu_vls *vls,
		 const char *name,
		 register const double *mat)
{
    int delta = (int)strlen(name)+2;

    /* indent the body of the matrix */
    bu_log_indent_delta(delta);

    bu_vls_printf(vls, " %s=%12E %12E %12E %12E\n",
		  name, mat[0], mat[1], mat[2], mat[3]);
    bu_log_indent_vls(vls);

    bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		  mat[4], mat[5], mat[6], mat[7]);
    bu_log_indent_vls(vls);

    bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		  mat[8], mat[9], mat[10], mat[11]);
    bu_log_indent_vls(vls);

    bu_log_indent_delta(-delta);

    bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		  mat[12], mat[13], mat[14], mat[15]);
}


void
bu_vls_struct_item(struct bu_vls *vp, const struct bu_structparse *sdp, const char *base, int sep_char)

/* item description */
/* base address of users structure */
/* value separator */
{
    register char *loc;

    if (UNLIKELY(sdp == (struct bu_structparse *)NULL)) {
	bu_log("NULL \"struct bu_structparse\" pointer\n");
	return;
    }

    loc = (char *)(base + sdp->sp_offset);

    if (sdp->sp_fmt[0] == 'i') {
	bu_log("Cannot print type 'i' yet!\n");
	return;
    }

    if (sdp->sp_fmt[0] != '%') {
	bu_log("bu_vls_struct_item:  %s: unknown format '%s'\n",
	       sdp->sp_name, sdp->sp_fmt);
	return;
    }

    switch (sdp->sp_fmt[1]) {
	case 'c':
	case 's':
	    if (sdp->sp_count < 1)
		break;
	    if (sdp->sp_count == 1)
		bu_vls_printf(vp, "%c", *loc);
	    else
		bu_vls_printf(vp, "%s", (char *)loc);
	    break;
	case 'S': /* XXX - DEPRECATED [7.14] */
	    printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
	    /* fall through */
	case 'V':
	    {
		register struct bu_vls *vls = (struct bu_vls *)loc;

		bu_vls_vlscat(vp, vls);
	    }
	    break;
	case 'i':
	    {
		register size_t i = sdp->sp_count;
		register short *sp = (short *)loc;

		bu_vls_printf(vp, "%d", *sp++);
		while (--i > 0) bu_vls_printf(vp, "%c%d", sep_char, *sp++);
	    }
	    break;
	case 'd':
	    {
		register size_t i = sdp->sp_count;
		register int *dp = (int *)loc;

		bu_vls_printf(vp, "%d", *dp++);
		while (--i > 0) bu_vls_printf(vp, "%c%d", sep_char, *dp++);
	    }
	    break;
	case 'f':
	    {
		register size_t i = sdp->sp_count;
		register double *dp = (double *)loc;

		bu_vls_printf(vp, "%.25G", *dp++);
		while (--i > 0) bu_vls_printf(vp, "%c%.25G", sep_char, *dp++);
	    }
	    break;
	case 'x':
	    {
		register size_t i = sdp->sp_count;
		register int *dp = (int *)loc;

		bu_vls_printf(vp, "%08x", *dp++);
		while (--i > 0) bu_vls_printf(vp, "%c%08x", sep_char, *dp++);
	    }
	    break;
	case 'p':
	    {
		/* Indirect to another structure */
		/* FIXME: unimplemented */
		bu_log("INTERNAL ERROR: Cannot print type '%%p' yet!\n");
	    }
	    break;
	default:
	    break;
    }
}


int
bu_vls_struct_item_named(struct bu_vls *vp, const struct bu_structparse *parsetab, const char *name, const char *base, int sep_char)
{
    register const struct bu_structparse *sdp;

    for (sdp = parsetab; sdp->sp_name != NULL; sdp++)
	if (BU_STR_EQUAL(sdp->sp_name, name)) {
	    bu_vls_struct_item(vp, sdp, base, sep_char);
	    return 0;
	}

    return -1;
}


void
bu_struct_print(const char *title, const struct bu_structparse *parsetab, const char *base)

/* structure description */
/* base address of users structure */
{
    register const struct bu_structparse *sdp;
    register char *loc;
    register int lastoff = -1;

    bu_log("%s\n", title);
    if (UNLIKELY(parsetab == (struct bu_structparse *)NULL)) {
	bu_log("NULL \"struct bu_structparse\" pointer\n");
	return;
    }
    for (sdp = parsetab; sdp->sp_name != (char *)0; sdp++) {

	/* Skip alternate keywords for same value */
	if (lastoff == (int)sdp->sp_offset)
	    continue;
	lastoff = (int)sdp->sp_offset;

	loc = (char *)(base + sdp->sp_offset);

	if (sdp->sp_fmt[0] == 'i') {
	    /* DEPRECATED: use %p instead. */
	    static int warned = 0;
	    if (!warned) {
		bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
		warned++;
	    }
	    bu_struct_print(sdp->sp_name, (struct bu_structparse *)sdp->sp_count, base);
	    continue;
	}

	if (sdp->sp_fmt[0] != '%') {
	    bu_log("bu_struct_print:  %s: unknown format '%s'\n",
		   sdp->sp_name, sdp->sp_fmt);
	    continue;
	}
	switch (sdp->sp_fmt[1]) {
	    case 'c':
	    case 's':
		if (sdp->sp_count < 1)
		    break;
		if (sdp->sp_count == 1)
		    bu_log(" %s='%c'\n", sdp->sp_name, *loc);
		else
		    bu_log(" %s=\"%s\"\n", sdp->sp_name,
			   (char *)loc);
		break;
	    case 'S': /* XXX - DEPRECATED [7.14] */
		printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		/* fall through */
	    case 'V':
		{
		    int delta = (int)strlen(sdp->sp_name)+2;
		    register struct bu_vls *vls =
			(struct bu_vls *)loc;

		    bu_log_indent_delta(delta);
		    bu_log(" %s=(vls_magic)0x%lx (vls_offset)%zu (vls_len)%zu (vls_max)%zu\n",
			   sdp->sp_name, vls->vls_magic,
			   vls->vls_offset,
			   vls->vls_len, vls->vls_max);
		    bu_log_indent_delta(-delta);
		    bu_log("\"%s\"\n", vls->vls_str+vls->vls_offset);
		}
		break;
	    case 'i':
		{
		    register size_t i = sdp->sp_count;
		    register short *sp = (short *)loc;

		    bu_log(" %s=%d", sdp->sp_name, *sp++);

		    while (--i > 0) bu_log("%c%d", COMMA, *sp++);

		    bu_log("\n");
		}
		break;
	    case 'd':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;

		    bu_log(" %s=%d", sdp->sp_name, *dp++);

		    while (--i > 0) bu_log("%c%d", COMMA, *dp++);

		    bu_log("\n");
		}
		break;
	    case 'f':
		{
		    register size_t i = sdp->sp_count;
		    register double *dp = (double *)loc;

		    if (sdp->sp_count == 16) {
			parse_matprint(sdp->sp_name, dp);
		    } else if (sdp->sp_count <= 3) {
			bu_log(" %s=%.25G", sdp->sp_name, *dp++);

			while (--i > 0)
			    bu_log("%c%.25G", COMMA, *dp++);

			bu_log("\n");
		    } else {
			int delta = (int)strlen(sdp->sp_name)+2;

			bu_log_indent_delta(delta);

			bu_log(" %s=%.25G\n", sdp->sp_name, *dp++);

			while (--i > 1)
			    bu_log("%.25G\n", *dp++);

			bu_log_indent_delta(-delta);
			bu_log("%.25G\n", *dp);
		    }
		}
		break;
	    case 'x':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;

		    bu_log(" %s=%08x", sdp->sp_name, *dp++);

		    while (--i > 0) bu_log("%c%08x", COMMA, *dp++);

		    bu_log("\n");
		}
		break;
	    case 'p':
		bu_struct_print(sdp->sp_name, (struct bu_structparse *)sdp->sp_count, base);
		break;
	    default:
		bu_log(" bu_struct_print: Unknown format: %s=%s ??\n",
		       sdp->sp_name, sdp->sp_fmt);
		break;
	}
    }
}


HIDDEN void
parse_vls_print_double(struct bu_vls *vls, const char *name, register size_t count, register const double *dp)
{
    register int tmpi;
    register char *cp;

    size_t increase = strlen(name) + 3 + 32 * count;
    bu_vls_extend(vls, (unsigned int)increase);

    cp = vls->vls_str + vls->vls_offset + vls->vls_len;
    snprintf(cp, increase, "%s%s=%.27G", (vls->vls_len?" ":""), name, *dp++);
    tmpi = (int)strlen(cp);
    vls->vls_len += tmpi;

    while (--count > 0) {
	cp += tmpi;
	sprintf(cp, "%c%.27G", COMMA, *dp++);
	tmpi = (int)strlen(cp);
	vls->vls_len += tmpi;
    }
}


void
bu_vls_struct_print(struct bu_vls *vls, register const struct bu_structparse *sdp, const char *base)
/* vls to print into */
/* structure description */
/* structure ponter */
{
    register char *loc;
    register int lastoff = -1;
    register char *cp;
    size_t increase;

    BU_CK_VLS(vls);

    if (UNLIKELY(sdp == (struct bu_structparse *)NULL)) {
	bu_log("NULL \"struct bu_structparse\" pointer\n");
	return;
    }

    for (; sdp->sp_name != (char*)NULL; sdp++) {
	/* Skip alternate keywords for same value */

	if (lastoff == (int)sdp->sp_offset)
	    continue;
	lastoff = (int)sdp->sp_offset;

	loc = (char *)(base + sdp->sp_offset);

	if (sdp->sp_fmt[0] == 'i') {
	    struct bu_vls sub_str;

	    /* DEPRECATED: use %p instead. */
	    static int warned = 0;
	    if (!warned) {
		bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
		warned++;
	    }

	    bu_vls_init(&sub_str);
	    bu_vls_struct_print(&sub_str, (struct bu_structparse *)sdp->sp_count, base);
	    bu_vls_vlscat(vls, &sub_str);
	    bu_vls_free(&sub_str);
	    continue;
	}

	if (sdp->sp_fmt[0] != '%') {
	    bu_log("bu_struct_print:  %s: unknown format '%s'\n",
		   sdp->sp_name, sdp->sp_fmt);
	    break;
	}

	switch (sdp->sp_fmt[1]) {
	    case 'c':
	    case 's':
		if (sdp->sp_count < 1)
		    break;
		if (sdp->sp_count == 1) {
		    increase = strlen(sdp->sp_name)+6;
		    bu_vls_extend(vls, (unsigned int)increase);
		    cp = vls->vls_str + vls->vls_offset + vls->vls_len;
		    if (*loc == '"')
			snprintf(cp, increase, "%s%s=\"%s\"",
				 (vls->vls_len?" ":""),
				 sdp->sp_name, "\\\"");
		    else
			snprintf(cp, increase, "%s%s=\"%c\"",
				 (vls->vls_len?" ":""),
				 sdp->sp_name,
				 *loc);
		    vls->vls_len += (int)strlen(cp);
		} else {
		    struct bu_vls tmpstr;
		    bu_vls_init(&tmpstr);

		    /* quote the quote characters */
		    while(*loc) {
			    if (*loc == '"') {
				    bu_vls_putc(&tmpstr, '\\');
			    }
			    bu_vls_putc(&tmpstr, *loc);
			    loc++;
		    }
		    bu_vls_printf(vls, "%s=\"%s\"", sdp->sp_name, bu_vls_addr(&tmpstr));
		    bu_vls_free(&tmpstr);
		}
		break;
	    case 'S': /* XXX - DEPRECATED [7.14] */
		printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		/* fall through */
	    case 'V':
		{
		    register struct bu_vls *vls_p = (struct bu_vls *)loc;

		    increase =  bu_vls_strlen(vls_p) + 5 + strlen(sdp->sp_name);
		    bu_vls_extend(vls, (unsigned int)increase);

		    cp = vls->vls_str + vls->vls_offset + vls->vls_len;
		    snprintf(cp, increase, "%s%s=\"%s\"",
			     (vls->vls_len?" ":""),
			     sdp->sp_name,
			     bu_vls_addr(vls_p));
		    vls->vls_len += (int)strlen(cp);
		}
		break;
	    case 'i':
		{
		    register size_t i = sdp->sp_count;
		    register short *sp = (short *)loc;
		    register int tmpi;

		    increase = 64 * i + strlen(sdp->sp_name) + 3;
		    bu_vls_extend(vls, (unsigned int)increase);


		    cp = vls->vls_str + vls->vls_offset + vls->vls_len;
		    snprintf(cp, increase, "%s%s=%d",
			     (vls->vls_len?" ":""),
			     sdp->sp_name, *sp++);
		    tmpi = (int)strlen(cp);
		    vls->vls_len += tmpi;

		    while (--i > 0) {
			cp += tmpi;
			sprintf(cp, "%c%d", COMMA, *sp++);
			tmpi = (int)strlen(cp);
			vls->vls_len += tmpi;
		    }
		}
		break;
	    case 'd':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;

		    bu_vls_printf(vls, "%s%s=%d", " ", sdp->sp_name, *dp++);

		    while (--i > 0) {
			bu_vls_printf(vls, "%c%d", COMMA, *dp++);
		    }
		}
		break;
	    case 'f':
		parse_vls_print_double(vls, sdp->sp_name, sdp->sp_count,
				     (double *)loc);
		break;
	    case 'p':
		{
		    struct bu_vls sub_str;
		    bu_vls_init(&sub_str);
		    bu_vls_struct_print(&sub_str, (struct bu_structparse *)sdp->sp_count, base);
		    bu_vls_vlscat(vls, &sub_str);
		    bu_vls_free(&sub_str);
		    continue;
		}

	    default:
		bu_log(" %s=%s ??\n", sdp->sp_name, sdp->sp_fmt);
		bu_bomb("unexpected case encountered in bu_vls_struct_print\n");
		break;
	}
    }
}


void
bu_vls_struct_print2(struct bu_vls *vls_out,
		     const char *title,
		     const struct bu_structparse *parsetab,	/* structure description */
		     const char *base)	  	/* base address of users structure */
{
    register const struct bu_structparse *sdp;
    register char *loc;
    register int lastoff = -1;

    bu_vls_printf(vls_out, "%s\n", title);
    if (UNLIKELY(parsetab == (struct bu_structparse *)NULL)) {
	bu_vls_printf(vls_out, "NULL \"struct bu_structparse\" pointer\n");
	return;
    }

    for (sdp = parsetab; sdp->sp_name != (char *)0; sdp++) {

	/* Skip alternate keywords for same value */
	if (lastoff == (int)sdp->sp_offset)
	    continue;
	lastoff = (int)sdp->sp_offset;

	loc = (char *)(base + sdp->sp_offset);

	if (sdp->sp_fmt[0] == 'i') {
	    /* DEPRECATED: use %p instead. */
	    static int warned = 0;
	    if (!warned) {
		bu_log("DEVELOPER DEPRECATION NOTICE: Use of \"i\" is replaced by \"%%p\" for chained bu_structparse tables.\n");
		warned++;
	    }
	    bu_vls_struct_print2(vls_out, sdp->sp_name, (struct bu_structparse *)sdp->sp_count, base);
	    continue;
	}

	if (sdp->sp_fmt[0] != '%') {
	    bu_vls_printf(vls_out, "bu_vls_struct_print:  %s: unknown format '%s'\n",
			  sdp->sp_name, sdp->sp_fmt);
	    continue;
	}

	switch (sdp->sp_fmt[1]) {
	    case 'c':
	    case 's':
		if (sdp->sp_count < 1)
		    break;
		if (sdp->sp_count == 1)
		    bu_vls_printf(vls_out, " %s='%c'\n", sdp->sp_name, *loc);
		else
		    bu_vls_printf(vls_out, " %s=\"%s\"\n", sdp->sp_name,
				  (char *)loc);
		break;
	    case 'S': /* XXX - DEPRECATED [7.14] */
		printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		/* fall through */
	    case 'V':
		{
		    int delta = (int)strlen(sdp->sp_name)+2;
		    register struct bu_vls *vls =
			(struct bu_vls *)loc;

		    bu_log_indent_delta(delta);
		    bu_vls_printf(vls_out, " %s=(vls_magic)%ld (vls_offset)%zu (vls_len)%zu (vls_max)%zu\n",
				  sdp->sp_name, vls->vls_magic,
				  vls->vls_offset,
				  vls->vls_len, vls->vls_max);
		    bu_log_indent_vls(vls_out);
		    bu_log_indent_delta(-delta);
		    bu_vls_printf(vls_out, "\"%s\"\n", vls->vls_str+vls->vls_offset);
		}
		break;
	    case 'i':
		{
		    register size_t i = sdp->sp_count;
		    register short *sp = (short *)loc;

		    bu_vls_printf(vls_out, " %s=%d", sdp->sp_name, *sp++);

		    while (--i > 0)
			bu_vls_printf(vls_out, "%c%d", COMMA, *sp++);

		    bu_vls_printf(vls_out, "\n");
		}
		break;
	    case 'd':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;

		    bu_vls_printf(vls_out, " %s=%d", sdp->sp_name, *dp++);

		    while (--i > 0)
			bu_vls_printf(vls_out, "%c%d", COMMA, *dp++);

		    bu_vls_printf(vls_out, "\n");
		}
		break;
	    case 'f':
		{
		    register size_t i = sdp->sp_count;
		    register double *dp = (double *)loc;

		    if (sdp->sp_count == 16) {
			parse_vls_matprint(vls_out, sdp->sp_name, dp);
		    } else if (sdp->sp_count <= 3) {
			bu_vls_printf(vls_out, " %s=%.25G", sdp->sp_name, *dp++);

			while (--i > 0)
			    bu_vls_printf(vls_out, "%c%.25G", COMMA, *dp++);

			bu_vls_printf(vls_out, "\n");
		    } else {
			int delta = (int)strlen(sdp->sp_name)+2;

			bu_log_indent_delta(delta);
			bu_vls_printf(vls_out, " %s=%.25G\n", sdp->sp_name, *dp++);
			bu_log_indent_vls(vls_out);

			while (--i > 1) {
			    bu_vls_printf(vls_out, "%.25G\n", *dp++);
			    bu_log_indent_vls(vls_out);
			}

			bu_log_indent_delta(-delta);
			bu_vls_printf(vls_out, "%.25G\n", *dp);
		    }
		}
		break;
	    case 'x':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;

		    bu_vls_printf(vls_out, " %s=%08x", sdp->sp_name, *dp++);

		    while (--i > 0)
			bu_vls_printf(vls_out, "%c%08x", COMMA, *dp++);

		    bu_vls_printf(vls_out, "\n");
		}
		break;
	    case 'p':
		bu_vls_struct_print2(vls_out, sdp->sp_name, (struct bu_structparse *)sdp->sp_count, base);
		break;
	    default:
		bu_vls_printf(vls_out, " bu_vls_struct_print2: Unknown format: %s=%s ??\n",
			      sdp->sp_name, sdp->sp_fmt);
		break;
	}
    }
}


void
bu_parse_mm(const struct bu_structparse *sdp, const char *name, char *base, const char *value)
/* structure description */
/* struct member name */
/* begining of structure */
/* string containing value */
{
    double *p = (double *)(base+sdp->sp_offset);

    /* reconvert with optional units, name if-statement just to quell unused warning */
    if (name)
	*p = bu_mm_value(value);
    else
	*p = bu_mm_value(value);
}


#define STATE_UNKNOWN 0
#define STATE_IN_KEYWORD 1
#define STATE_IN_VALUE 2
#define STATE_IN_QUOTED_VALUE 3

int
bu_key_eq_to_key_val(const char *in, const char **next, struct bu_vls *vls)
{
    const char *iptr=in;
    const char *start;
    int state=STATE_IN_KEYWORD;

    BU_CK_VLS(vls);

    *next = NULL;

    while (*iptr) {
	const char *prev='\0';

	switch (state) {
	    case STATE_IN_KEYWORD:
		/* skip leading white space */
		while (isspace(*iptr))
		    iptr++;

		if (!(*iptr))
		    break;

		if (*iptr == ';') {
		    /* found end of a stack element */
		    *next = iptr+1;
		    return 0;
		}

		/* copy keyword up to '=' */
		start = iptr;
		while (*iptr && *iptr != '=')
		    iptr++;

		bu_vls_strncat(vls, start, iptr - start);

		/* add a single space after keyword */
		bu_vls_putc(vls, ' ');

		if (!*iptr)
		    break;

		/* skip over '=' in input */
		iptr++;

		/* switch to value state */
		state = STATE_IN_VALUE;

		break;
	    case STATE_IN_VALUE:
		/* skip excess white space */
		while (isspace(*iptr))
		    iptr++;

		/* check for quoted value */
		if (*iptr == '"') {
		    /* switch to quoted value state */
		    state = STATE_IN_QUOTED_VALUE;

		    /* skip over '"' */
		    iptr++;

		    break;
		}

		/* copy value up to next white space or end of string */
		start = iptr;
		while (*iptr && *iptr != ';' && !isspace(*iptr))
		    iptr++;

		bu_vls_strncat(vls, start, iptr - start);

		if (*iptr) {
		    /* more to come */
		    bu_vls_putc(vls, ' ');

		    /* switch back to keyword state */
		    state = STATE_IN_KEYWORD;
		}

		break;
	    case STATE_IN_QUOTED_VALUE:
		/* copy byte-for-byte to end quote (watch out for escaped quote)
		 * replace quotes with '{' '}' */

		bu_vls_strcat(vls, " {");
		while (1) {
		    if (*iptr == '"' && *prev != '\\') {
			bu_vls_putc(vls, '}');
			iptr++;
			break;
		    }
		    bu_vls_putc(vls, *iptr);
		    prev = iptr++;
		}

		if (*iptr && *iptr != ';') /* more to come */
		    bu_vls_putc(vls, ' ');

		/* switch back to keyword state */
		state = STATE_IN_KEYWORD;

		break;
	}
    }
    return 0;
}


int
bu_shader_to_tcl_list(const char *in, struct bu_vls *vls)
{
    size_t len;
    int shader_name_len=0;
    char *iptr;
    const char *shader;
    char *copy = bu_strdup(in);
    char *next = copy;


    BU_CK_VLS(vls);

    while (next) {
	iptr = next;

	/* find start of shader name */
	while (isspace(*iptr))
	    iptr++;

	shader = iptr;

	/* find end of shader name */
	while (*iptr && !isspace(*iptr) && *iptr != ';')
	    iptr++;

	shader_name_len = iptr - shader;

	if (shader_name_len == 5 && !strncmp(shader, "stack", 5)) {

	    /* stack shader, loop through all shaders in stack */
	    int done=0;

	    bu_vls_strcat(vls, "stack {");

	    while (!done) {
		const char *shade1;

		/* find start of shader */
		while (isspace(*iptr))
		    iptr++;

		if (*iptr == '\0')
		    break;

		shade1 = iptr;

		/* find end of shader */
		while (*iptr && *iptr != ';')
		    iptr++;

		if (*iptr == '\0')
		    done = 1;

		*iptr = '\0';

		bu_vls_putc(vls, '{');

		if (bu_shader_to_tcl_list(shade1, vls)) {
		    bu_free(copy, BU_FLSTR);
		    return 1;
		}

		bu_vls_strcat(vls, "} ");

		if (!done)
		    iptr++;
	    }
	    bu_vls_putc(vls, '}');
	    bu_free(copy, BU_FLSTR);
	    return 0;
	}

	if (shader_name_len == 6 && !strncmp(shader, "envmap", 6)) {
	    bu_vls_strcat(vls, "envmap {");
	    if (bu_shader_to_tcl_list(iptr, vls)) {
		bu_free(copy, BU_FLSTR);
		return 1;
	    }
	    bu_vls_putc(vls, '}');
	    bu_free(copy, BU_FLSTR);
	    return 0;
	}

	bu_vls_strncat(vls, shader, shader_name_len);

	/* skip more white space */
	while (*iptr && isspace(*iptr))
	    iptr++;

	/* iptr now points at start of parameters, if any */
	if (*iptr && *iptr != ';') {
	    bu_vls_strcat(vls, " {");
	    len = bu_vls_strlen(vls);
	    if (bu_key_eq_to_key_val(iptr, (const char **)&next, vls)) {
		bu_free(copy, BU_FLSTR);
		return 1;
	    }
	    if (bu_vls_strlen(vls) > len)
		bu_vls_putc(vls, '}');
	    else
		bu_vls_trunc(vls, len-2);
	} else if (*iptr && *iptr == ';') {
	    next = ++iptr;
	} else {
	    next = (char *)NULL;
	}
    }

    bu_free(copy, BU_FLSTR);
    return 0;
}


/**
 *
 * Given a Tcl list, return a copy of the 'index'th entry,
 * which may itself be a list.
 *
 * Note: caller is responsible for freeing the returned string.
 */
HIDDEN char *
parse_list_elem(const char *in, int idx)
{
    int depth=0;
    int count=0;
    int len=0;
    const char *ptr=in;
    const char *prev=NULL;
    const char *start=NULL;
    const char *end=NULL;
    char *out=NULL;

    while (*ptr) {
	/* skip leading white space */
	while (*ptr && isspace(*ptr)) {
	    prev = ptr;
	    ptr++;
	}

	if (!*ptr)
	    break;

	if (depth == 0 && count == idx)
	    start = ptr;

	if (*ptr == '{') {
	    depth++;
	    prev = ptr;
	    ptr++;
	} else if (*ptr == '}') {
	    depth--;
	    if (depth == 0)
		count++;
	    if (start && depth == 0) {
		end = ptr;
		break;
	    }
	    prev = ptr;
	    ptr++;
	} else {
	    while (*ptr &&
		   (!isspace(*ptr) || *prev == '\\') &&
		   (*ptr != '}' || *prev == '\\') &&
		   (*ptr != '{' || *prev == '\\'))
	    {
		prev = ptr;
		ptr++;
	    }
	    if (depth == 0)
		count++;

	    if (start && depth == 0) {
		end = ptr-1;
		break;
	    }
	}
    }

    if (!start)
	return (char *)NULL;

    if (*start == '{') {
	if (!end || *end != '}') {
	    bu_log("Error in list (uneven braces?): %s\n", in);
	    return (char *)NULL;
	}

	/* remove enclosing braces */
	start++;
	while (start < end && isspace(*start))
	    start++;

	end--;
	while (end > start && isspace(*end) && *(end-1) != '\\')
	    end--;

	if (start == end)
	    return (char *)NULL;
    }

    len = end - start + 1;
    out = (char *)bu_malloc(len+1, "parse_list_elem:out");
    strncpy(out, start, len);
    *(out + len) = '\0';

    return out;
}


/**
 *
 * Return number of items in a string, interpreted as a Tcl list.
 */
int
parse_tcl_list_length(const char *in)
{
    int count=0;
    int depth=0;
    const char *ptr=in;
    const char *prev=NULL;

    while (*ptr) {
	/* skip leading white space */
	while (*ptr && isspace(*ptr)) {
	    prev = ptr;
	    ptr++;
	}

	if (!*ptr)
	    break;

	if (*ptr == '{') {
	    if (depth == 0)
		count++;
	    depth++;
	    prev = ptr;
	    ptr++;
	} else if (*ptr == '}') {
	    depth--;
	    prev = ptr;
	    ptr++;
	} else {
	    if (depth == 0)
		count++;

	    while (*ptr &&
		   (!isspace(*ptr) || *prev == '\\') &&
		   (*ptr != '}' || *prev == '\\') &&
		   (*ptr != '{' || *prev == '\\'))
	    {
		prev = ptr;
		ptr++;
	    }
	}
    }

    return count;
}


HIDDEN int
parse_key_val_to_vls(struct bu_vls *vls, char *params)
{
    int len;
    int j;

    len = parse_tcl_list_length(params);

    if (len == 1) {
	bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, params);
	return 0;
    }

    if (len%2) {
	bu_log("parse_key_val_to_vls: Error: shader parameters must be even numbered!!\n\t%s\n", params);
	return 1;
    }

    for (j=0; j<len; j += 2) {
	char *keyword;
	char *value;

	keyword = parse_list_elem(params, j);
	value = parse_list_elem(params, j+1);

	bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, keyword);
	bu_vls_putc(vls, '=');
	if (parse_tcl_list_length(value) > 1) {
	    bu_vls_putc(vls, '"');
	    bu_vls_strcat(vls, value);
	    bu_vls_putc(vls, '"');
	} else {
	    bu_vls_strcat(vls, value);
	}

	bu_free(keyword, "parse_key_val_to_vls() keyword");
	bu_free(value, "parse_key_val_to_vls() value");

    }
    return 0;
}


int
bu_shader_to_key_eq(const char *in, struct bu_vls *vls)
{
    int len;
    int ret=0;
    char *shader;
    char *params;

    BU_CK_VLS(vls);

    len = parse_tcl_list_length(in);

    if (len == 0)
	return 0;

    if (len == 1) {
	/* shader with no parameters */
	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, in);
	return 0;
    }

    if (len != 2) {
	bu_log("bu_shader_to_key_eq: Error: shader must have two elements (not %d)!!\n\t%s\n", len, in);
	return 1;
    }

    shader = parse_list_elem(in, 0);
    params = parse_list_elem(in, 1);

    if (BU_STR_EQUAL(shader, "envmap")) {
	/* environment map */

	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, "envmap");

	bu_shader_to_key_eq(params, vls);
    } else if (BU_STR_EQUAL(shader, "stack")) {
	/* stacked shaders */

	int i;

	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, "stack");

	/* get number of shaders in the stack */
	len = parse_tcl_list_length(params);

	/* process each shader in the stack */
	for (i=0; i<len; i++) {
	    char *shader1;

	    /* each parameter must be a shader specification in itself */
	    shader1 = parse_list_elem(params, i);

	    if (i > 0)
		bu_vls_putc(vls, ';');
	    bu_shader_to_key_eq(shader1, vls);
	    bu_free(shader1, "shader1");
	}
    } else {
	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, shader);
	ret = parse_key_val_to_vls(vls, params);
    }

    bu_free(shader, "shader");
    bu_free(params, "params");

    return ret;
}


int
bu_fwrite_external(FILE *fp, const struct bu_external *ep)
{
    size_t got;

    BU_CK_EXTERNAL(ep);

    got = fwrite(ep->ext_buf, 1, ep->ext_nbytes, fp);
    if (UNLIKELY(got != (size_t)ep->ext_nbytes)) {
	perror("fwrite");
	bu_log("bu_fwrite_external() attempted to write %ld, got %ld\n", (long)ep->ext_nbytes, (long)got);
	return -1;
    }
    return 0;
}


void
bu_hexdump_external(FILE *fp, const struct bu_external *ep, const char *str)
{
    const unsigned char *cp;
    const unsigned char *endp;
    size_t i, j, k;

    BU_CK_EXTERNAL(ep);

    fprintf(fp, "%s:\n", str);

    if (UNLIKELY(ep->ext_nbytes <= 0))
	fprintf(fp, "\tWarning: 0 length external buffer\n");

    cp = (const unsigned char *)ep->ext_buf;
    endp = cp + ep->ext_nbytes;
    for (i=0; i < ep->ext_nbytes; i += 16) {
	const unsigned char *sp = cp;

	for (j=0; j < 4; j++) {
	    for (k=0; k < 4; k++) {
		if (cp >= endp)
		    fprintf(fp, "   ");
		else
		    fprintf(fp, "%2.2x ", *cp++);
	    }
	    fprintf(fp, " ");
	}
	fprintf(fp, " |");

	for (j=0; j < 16; j++, sp++) {
	    if (sp >= endp) break;
	    if (isprint(*sp))
		putc(*sp, fp);
	    else
		putc('.', fp);
	}

	fprintf(fp, "|\n");
    }
}


void
bu_free_external(register struct bu_external *ep)
{
    BU_CK_EXTERNAL(ep);
    if (LIKELY(ep->ext_buf != NULL)) {
	bu_free(ep->ext_buf, "bu_external ext_buf");
	ep->ext_buf = GENPTR_NULL;
    }
}


void
bu_copy_external(struct bu_external *op, const struct bu_external *ip)
{
    BU_CK_EXTERNAL(ip);
    BU_EXTERNAL_INIT(op);

    if (UNLIKELY(op == ip))
	return;

    op->ext_nbytes = ip->ext_nbytes;
    op->ext_buf = bu_malloc(ip->ext_nbytes, "bu_copy_external");
    memcpy(op->ext_buf, ip->ext_buf, ip->ext_nbytes);
}


char *
bu_next_token(char *str)
{
    char *ret;

    ret = str;
    while (!isspace(*ret) && *ret !='\0')
	ret++;
    while (isspace(*ret))
	ret++;

    return ret;
}


void
bu_structparse_get_terse_form(struct bu_vls *logstr, const struct bu_structparse *sp)
{
    struct bu_vls str;
    size_t i;

    bu_vls_init(&str);

    while (sp->sp_name != NULL) {
	bu_vls_printf(logstr, "%s ", sp->sp_name);
	/* These types are specified by lengths, e.g. %80s */
	if (BU_STR_EQUAL(sp->sp_fmt, "%c") ||
	    BU_STR_EQUAL(sp->sp_fmt, "%s") ||
	    BU_STR_EQUAL(sp->sp_fmt, "%S") || /* XXX - DEPRECATED [7.14] */
	    BU_STR_EQUAL(sp->sp_fmt, "%V")) {
	    if (sp->sp_count > 1) {
		/* Make them all look like %###s */
		bu_vls_printf(logstr, "%%%lds", sp->sp_count);
	    } else {
		/* Singletons are specified by their actual character */
		bu_vls_printf(logstr, "%%c");
	    }
	} else {
	    if (sp->sp_count < 2) {
		bu_vls_printf(logstr, "%s ", sp->sp_fmt);
	    } else {
		/* Vectors are specified by repetition, e.g. {%f %f %f} */
		bu_vls_printf(logstr, "{%s", sp->sp_fmt);
		for (i = 1; i < sp->sp_count; i++)
		    bu_vls_printf(logstr, " %s", sp->sp_fmt);
		bu_vls_printf(logstr, "} ");
	    }
	}
	++sp;
    }
    bu_vls_free(&str);
}


int
bu_structparse_argv(struct bu_vls *logstr,
		    int argc,
		    const char **argv,
		    const struct bu_structparse *desc,
		    char *base)
{
    register const struct bu_structparse *sdp = NULL;
    register size_t j;
    register size_t ii;
    const char *cp = NULL;
    char *loc = NULL;
    struct bu_vls str;

    if (UNLIKELY(desc == (struct bu_structparse *)NULL)) {
	bu_vls_printf(logstr, "bu_structparse_argv: NULL desc pointer\n");
	return BRLCAD_ERROR;
    }

    /* Run through each of the attributes and their arguments. */

    bu_vls_init(&str);
    while (argc > 0) {
	/* Find the attribute which matches this argument. */
	for (sdp = desc; sdp->sp_name != NULL; sdp++) {
	    if (!BU_STR_EQUAL(sdp->sp_name, *argv))
		continue;

	    /* if we get this far, we've got a name match
	     * with a name in the structure description
	     */
	    loc = (char *)(base+((int)sdp->sp_offset));
	    if (sdp->sp_fmt[0] != '%') {
		bu_vls_printf(logstr, "bu_structparse_argv: unknown format\n");
		return BRLCAD_ERROR;
	    }

	    --argc;
	    ++argv;

	    switch (sdp->sp_fmt[1]) {
		case 'c':
		case 's':
		    /* copy the string to an array of length
		     * sdp->sp_count, converting escaped double quotes
		     * to just double quotes
		     */
		    if (argc < 1) {
			bu_vls_printf(logstr,
				      "not enough values for \"%s\" argument: should be %ld",
				      sdp->sp_name,
				      sdp->sp_count);
			return BRLCAD_ERROR;
		    }
		    for (ii = j = 0;
			 j < sdp->sp_count && argv[0][ii] != '\0';
			 loc[j++] = argv[0][ii++])
			;
		    if (ii < sdp->sp_count)
			loc[ii] = '\0';
		    if (sdp->sp_count > 1) {
			loc[sdp->sp_count-1] = '\0';
			bu_vls_printf(logstr, "%s %s ", sdp->sp_name, loc);
		    } else {
			bu_vls_printf(logstr, "%s %c ", sdp->sp_name, *loc);
		    }
		    break;
		case 'S': /* XXX - DEPRECATED [7.14] */
		    printf("DEVELOPER DEPRECATION NOTICE: Using %%S for string printing is deprecated, use %%V instead\n");
		    /* fall through */
		case 'V':
		    {
			/* copy the string to a bu_vls (string of
			 * variable length provided by libbu)
			 */
			struct bu_vls *vls = (struct bu_vls *)loc;

			/* argv[0] contains the string.  i.e., we have
			 * exactly one value
			 */
			BU_ASSERT(sdp->sp_count == 1);

			if (argc < 1) {
			    bu_vls_printf(logstr,
					  "not enough values for \"%V\" argument: should be %ld",
					  sdp->sp_name,
					  sdp->sp_count);
			    return BRLCAD_ERROR;
			}

			bu_vls_printf(logstr, "%s ", sdp->sp_name);

			bu_vls_strcpy(vls, argv[0]);

			bu_vls_printf(logstr, "%s ", argv[0]);
			break;
		    }
		case 'i': {
		    register short *sh = (short *)loc;
		    register int tmpi;

		    if (argc < 1) {
			bu_vls_printf(logstr,
				      "not enough values for \"%s\" argument: should have %ld",
				      sdp->sp_name,
				      sdp->sp_count);
			return BRLCAD_ERROR;
		    }

		    bu_vls_printf(logstr, "%s ", sdp->sp_name);

		    /* Special case:  '=!' toggles a boolean */
		    if (argv[0][0] == '!') {
			*sh = *sh ? 0 : 1;
			bu_vls_printf(logstr, "%hd ", *sh);
			break;
		    }
		    /* Normal case: an integer */
		    cp = *argv;
		    for (ii = 0; ii < sdp->sp_count; ++ii) {
			if (*cp == '\0') {
			    bu_vls_printf(logstr,
					  "not enough values for \"%s\" argument: should have %ld",
					  sdp->sp_name,
					  sdp->sp_count);
			    return BRLCAD_ERROR;
			}

			BU_SP_SKIP_SEP(cp);
			tmpi = atoi(cp);
			if (*cp && (*cp == '+' || *cp == '-'))
			    cp++;
			while (*cp && isdigit(*cp))
			    cp++;
			/* make sure we actually had an
			 * integer out there
			 */

			if (cp == *argv ||
			    (cp == *argv+1 &&
			     (argv[0][0] == '+' ||
			      argv[0][0] == '-'))) {
			    bu_vls_printf(logstr,
					  "value \"%s\" to argument %s isn't an integer",
					  argv[0],
					  sdp->sp_name);
			    return BRLCAD_ERROR;
			} else {
			    *(sh++) = tmpi;
			}
			BU_SP_SKIP_SEP(cp);
		    }

		    if (sdp->sp_count > 1)
			bu_vls_printf(logstr, "{%s} ", argv[0]);
		    else
			bu_vls_printf(logstr, "%s ", argv[0]);
		    break;
		}
		case 'd': {
		    register int *ip = (int *)loc;
		    register int tmpi;

		    if (argc < 1) {
			/* XXX - when was ii defined */
			bu_vls_printf(logstr,
				      "not enough values for \"%s\" argument: should have %ld",
				      sdp->sp_name,
				      sdp->sp_count);
			return BRLCAD_ERROR;
		    }

		    bu_vls_printf(logstr, "%s ", sdp->sp_name);

		    /* Special case:  '=!' toggles a boolean */
		    if (argv[0][0] == '!') {
			*ip = *ip ? 0 : 1;
			bu_vls_printf(logstr, "%d ", *ip);
			break;
		    }
		    /* Normal case: an integer */
		    cp = *argv;
		    for (ii = 0; ii < sdp->sp_count; ++ii) {
			if (*cp == '\0') {
			    bu_vls_printf(logstr,
					  "not enough values for \"%s\" argument: should have %ld",
					  sdp->sp_name,
					  sdp->sp_count);
			    return BRLCAD_ERROR;
			}

			BU_SP_SKIP_SEP(cp);
			tmpi = atoi(cp);
			if (*cp && (*cp == '+' || *cp == '-'))
			    cp++;
			while (*cp && isdigit(*cp))
			    cp++;
			/* make sure we actually had an
			 * integer out there
			 */

			if (cp == *argv ||
			    (cp == *argv+1 &&
			     (argv[0][0] == '+' ||
			      argv[0][0] == '-'))) {
			    bu_vls_printf(logstr,
					  "value \"%s\" to argument %s isn't an integer",
					  argv[0],
					  sdp->sp_name);
			    return BRLCAD_ERROR;
			} else {
			    *(ip++) = tmpi;
			}
			BU_SP_SKIP_SEP(cp);
		    }
		    if (sdp->sp_count > 1)
			bu_vls_printf(logstr, "{%s} ", argv[0]);
		    else
			bu_vls_printf(logstr, "%s ", argv[0]);
		    break;
		}
		case 'f': {
		    int dot_seen;
		    double tmp_double;
		    register double *dp;
		    const char *numstart;

		    dp = (double *)loc;

		    if (argc < 1) {
			bu_vls_printf(&str,
				      "not enough values for \"%s\" argument: should have %ld, only %d given",
				      sdp->sp_name,
				      sdp->sp_count, argc);
			return BRLCAD_ERROR;
		    }

		    bu_vls_printf(logstr, "%s ", sdp->sp_name);

		    cp = *argv;
		    for (ii = 0; ii < sdp->sp_count; ii++) {
			if (*cp == '\0') {
			    bu_vls_printf(logstr,
					  "not enough values for \"%s\" argument: should have %ld, only %zu given",
					  sdp->sp_name,
					  sdp->sp_count,
					  ii);
			    return BRLCAD_ERROR;
			}

			BU_SP_SKIP_SEP(cp);
			numstart = cp;
			if (*cp == '-' || *cp == '+') cp++;

			/* skip matissa */
			dot_seen = 0;
			for (; *cp; cp++) {
			    if (*cp == '.' && !dot_seen) {
				dot_seen = 1;
				continue;
			    }
			    if (!isdigit(*cp))
				break;
			}

			/* If no mantissa seen, then there is no float
			 * here.
			 */
			if (cp == (numstart + dot_seen)) {
			    bu_vls_printf(logstr,
					  "value \"%s\" to argument %s isn't a float",
					  argv[0],
					  sdp->sp_name);
			    return BRLCAD_ERROR;
			}

			/* there was a mantissa, so we may have an
			 * exponent.
			 */
			if (*cp == 'E' || *cp == 'e') {
			    cp++;

			    /* skip exponent sign */
			    if (*cp == '+' || *cp == '-')
				cp++;
			    while (isdigit(*cp))
				cp++;
			}

			bu_vls_trunc(&str, 0);
			bu_vls_strcpy(&str, numstart);
			bu_vls_trunc(&str, cp-numstart);
			if (sscanf(bu_vls_addr(&str), "%lf", &tmp_double) != 1) {
			    bu_vls_printf(logstr,
					  "value \"%s\" to argument %s isn't a float",
					  numstart,
					  sdp->sp_name);
			    bu_vls_free(&str);
			    return BRLCAD_ERROR;
			}
			bu_vls_free(&str);

			*dp++ = tmp_double;

			BU_SP_SKIP_SEP(cp);
		    }
		    if (sdp->sp_count > 1)
			bu_vls_printf(logstr, "{%s} ", argv[0]);
		    else
			bu_vls_printf(logstr, "%s ", argv[0]);
		    break;
		}
		case 'p': {
		    /* Indirect to another structure */
		    /* FIXME: unimplemented */
		    bu_log("INTERNAL ERROR: referencing indirect bu_structparse table, unimplemented\n");
		}

		default: {
		    bu_vls_printf(logstr,
				  "%s line:%d Parse error, unknown format: '%s' for element \"%s\"",
				  __FILE__, __LINE__, sdp->sp_fmt,
				  sdp->sp_name);

		    return BRLCAD_ERROR;
		}
	    }

	    if (sdp->sp_hook) {
		sdp->sp_hook(sdp, sdp->sp_name, base, *argv);
	    }
	    --argc;
	    ++argv;


	    break;
	}


	if (sdp->sp_name == NULL) {
	    bu_vls_printf(logstr, "invalid attribute %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
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
