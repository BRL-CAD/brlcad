/*                           V L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @addtogroup vls */
/** @{ */
/** @file vls.c
 *
 * @brief The variable length string package.
 *
 * The variable length string package.
 *
 * Assumption:  libc-provided sprintf() function is safe to use in parallel,
 * on parallel systems.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"


/* non-published globals */
extern const char bu_vls_message[];
extern const char bu_strdup_message[];

/* private constants */

/** minimum initial allocation size */
static const int _VLS_ALLOC_MIN = 40;

/** minimum vls allocation increment size */
static const int _VLS_ALLOC_STEP = 120;

/** minimum vls buffer allocation size */
static const int _VLS_ALLOC_READ = 4096;


/**
 * b u _ v l s _ i n i t
 *
 * No storage should be allocated at this point, and bu_vls_addr()
 * must be able to live with that.
 */
void
bu_vls_init(register struct bu_vls *vp)
{
    if (vp == (struct bu_vls *)NULL)
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


/**
 * b u _ v l s _ i n i t _ i f _ u n i n i t
 *
 * If a VLS is unitialized, initialize it.  If it is already
 * initialized, leave it alone, caller wants to append to it.
 */
void
bu_vls_init_if_uninit(register struct bu_vls *vp)
{
    if (vp == (struct bu_vls *)NULL)
	bu_bomb("bu_vls_init_if_uninit() passed NULL pointer");

    if ( vp->vls_magic == BU_VLS_MAGIC )
	return;
    bu_vls_init( vp );
}


/**
 * b u _ v l s _ v l s i n i t
 *
 * Allocate storage for a struct bu_vls, call bu_vls_init on it, and
 * return the result.  Allows for creation of dynamically allocated
 * VLS strings.
 */
struct bu_vls *
bu_vls_vlsinit(void)
{
    register struct bu_vls *vp;

    vp = (struct bu_vls *)bu_malloc(sizeof(struct bu_vls), "bu_vls_vlsinit struct");
    bu_vls_init(vp);

    return vp;
}


/**
 * b u _ v l s _ a d d r
 *
 * Return a pointer to the null-terminated string in the vls array.
 * If no storage has been allocated yet, give back a valid string.
 */
char *
bu_vls_addr(register const struct bu_vls *vp)
{
    static char nullbuf[4] = {0, 0, 0, 0};
    BU_CK_VLS(vp);

    if ( vp->vls_max == 0 || vp->vls_str == (char *)NULL )  {
	/* A zero-length VLS is a null string */
	nullbuf[0] = '\0'; /* sanity */
	return nullbuf;
    }

    /* Sanity checking */
    if ( vp->vls_max < 0 ||
	 vp->vls_len < 0 ||
	 vp->vls_offset < 0 ||
	 vp->vls_str == (char *)NULL ||
	 vp->vls_len + vp->vls_offset >= vp->vls_max )
    {
	bu_log("bu_vls_addr: bad VLS.  max=%d, len=%d, offset=%d\n",
	       vp->vls_max, vp->vls_len, vp->vls_offset);
	bu_bomb("bu_vls_addr\n");
    }

    return vp->vls_str+vp->vls_offset;
}


/**
 * b u _ v l s _ e x t e n d
 *
 * Ensure that the provided VLS has at least 'extra' characters of
 * space available.
 */
void
bu_vls_extend(register struct bu_vls *vp, unsigned int extra)
{
    BU_CK_VLS(vp);

    /* increment by at least 40 bytes */
    if ( extra < _VLS_ALLOC_MIN )
	extra = _VLS_ALLOC_MIN;

    /* first time allocation */
    if ( vp->vls_max <= 0 || vp->vls_str == (char *)0 ) {
	vp->vls_max = extra;
	vp->vls_str = (char *)bu_malloc( (size_t)vp->vls_max, bu_vls_message );
	vp->vls_len = 0;
	vp->vls_offset = 0;
	*vp->vls_str = '\0';
	return;
    }

    /* need more space? */
    if ( vp->vls_offset + vp->vls_len + extra >= (size_t)vp->vls_max )  {
	vp->vls_max += extra;
	if ( vp->vls_max < _VLS_ALLOC_STEP ) {
	    /* extend to at least this much */
	    vp->vls_max = _VLS_ALLOC_STEP;
	}
	vp->vls_str = (char *)bu_realloc( vp->vls_str, (size_t)vp->vls_max, bu_vls_message );
    }
}


/**
 * b u _ v l s _ s e t l e n
 *
 * Ensure that the vls has a length of at least 'newlen', and make
 * that the current length.
 *
 * Useful for subroutines that are planning on mucking with the data
 * array themselves.  Not advisable, but occasionally useful.
 *
 * Does not change the offset from the front of the buffer, if any.
 * Does not initialize the value of any of the new bytes.
 */
void
bu_vls_setlen(struct bu_vls *vp, int newlen)
{
    BU_CK_VLS(vp);

    if ( vp->vls_len >= newlen )
	return;

    bu_vls_extend( vp, (unsigned)newlen - vp->vls_len );
    vp->vls_len = newlen;
}


/**
 * b u _ v l s _ s t r l e n
 *
 * Return length of the string, in bytes, not including the null
 * terminator.
 */
int
bu_vls_strlen(register const struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    if ( vp->vls_len <= 0 )
	return 0;

    return vp->vls_len;
}


/**
 * b u _ v l s _ t r u n c
 *
 * Truncate string to at most 'len' characters.  If 'len' is negative,
 * trim off that many from the end.  If 'len' is zero, don't release
 * storage -- user is probably just going to refill it again,
 * e.g. with bu_vls_gets().
 */
void
bu_vls_trunc(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if ( len < 0 ) {
	/* now an absolute length */
	len = vp->vls_len + len;
    }
    if ( vp->vls_len <= len )
	return;
    if ( len == 0 )
	vp->vls_offset = 0;

    vp->vls_str[len+vp->vls_offset] = '\0'; /* force null termination */
    vp->vls_len = len;
}


/**
 * b u _ v l s _ t r u n c 2
 *
 * Son of bu_vls_trunc().  Same as bu_vls_trunc() except that it
 * doesn't truncate (or do anything) if the len is negative.
 */
void
bu_vls_trunc2(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if ( vp->vls_len <= len )
	return;

    if ( len < 0 )
	len = 0;
    if ( len == 0 )
	vp->vls_offset = 0;

    vp->vls_str[len+vp->vls_offset] = '\0'; /* force null termination */
    vp->vls_len = len;
}


/**
 * b u _ v l s _ n i b b l e
 *
 * "Nibble" 'len' characters off the front of the string.  Changes the
 * length and offset; no data is copied.
 *
 * 'len' may be positive or negative. If negative, characters are
 * un-nibbled.
 */
void
bu_vls_nibble(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);

    if ( len < 0 && (-len) > vp->vls_offset )
	len = -vp->vls_offset;
    if (len >= vp->vls_len) {
	bu_vls_trunc( vp, 0 );
	return;
    }

    vp->vls_len -= len;
    vp->vls_offset += len;
}


/**
 * b u _ v l s _ f r e e
 *
 * Releases the memory used for the string buffer.
 */
void
bu_vls_free(register struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    if ( vp->vls_str )  {
	vp->vls_str[0] = '?'; /* Sanity */
	bu_free( vp->vls_str, "bu_vls_free" );
	vp->vls_str = (char *)0;
    }

    vp->vls_offset = vp->vls_len = vp->vls_max = 0;
}


/**
 * b u _ v l s _ v l s f r e e
 *
 * Releases the memory used for the string buffer and the memory for
 * the vls structure
 */
void
bu_vls_vlsfree(register struct bu_vls *vp)
{
    if ( *(unsigned long *)vp != BU_VLS_MAGIC)
	return;

    bu_vls_free( vp );
    bu_free( vp, "bu_vls_vlsfree" );
}


/**
 * b u _ v l s _ s t r d u p
 *
 * Make an "ordinary" string copy of a vls string.  Storage for the
 * regular string is acquired using malloc.
 *
 * The source string is not affected.
 */
char *
bu_vls_strdup(register const struct bu_vls *vp)
{
    register char *str;
    register size_t len;

    BU_CK_VLS(vp);

    len = bu_vls_strlen(vp);
    str = bu_malloc(len+1, bu_strdup_message );
    strncpy(str, bu_vls_addr(vp), len);
    str[len] = '\0'; /* sanity */
    return str;
}


/**
 * b u _ v l s _ s t r g r a b
 *
 * Like bu_vls_strdup(), but destructively grab the string from the
 * source argument 'vp'.  This is more efficient than bu_vls_strdup()
 * for those instances where the source argument 'vp' is no longer
 * needed by the caller, as it avoides a potentially long buffer copy.
 *
 * The source string is destroyed, as if bu_vls_free() had been
 * called.
 */
char *
bu_vls_strgrab(register struct bu_vls *vp)
{
    register char *str;

    BU_CK_VLS(vp);

    if ( vp->vls_offset != 0 )  {
	str = bu_vls_strdup( vp );
	bu_vls_free( vp );
	return str;
    }

    str = bu_vls_addr( vp );
    vp->vls_str = (char *)0;
    vp->vls_offset = vp->vls_len = vp->vls_max = 0;
    return str;
}


/**
 * b u _ v l s _ s t r c p y
 *
 * Empty the vls string, and copy in a regular string.
 */
void
bu_vls_strcpy(register struct bu_vls *vp, const char *s)
{
    register size_t len;

    BU_CK_VLS(vp);

    if ( s == (const char *)NULL )
	return;

    if ( (len = strlen(s)) <= 0 )  {
	vp->vls_len = 0;
	vp->vls_offset = 0;
	if (vp->vls_max > 0)
	    vp->vls_str[0] = '\0';
	return;
    }

    /* cancel offset before extending */
    vp->vls_offset = 0;
    if ( len+1 >= (size_t)vp->vls_max )
	bu_vls_extend( vp, len+1 );

    memcpy(vp->vls_str, s, len+1); /* include null */
    vp->vls_len = len;
}


/**
 * b u _ v l s _ s t r n c p y
 *
 * Empty the vls string, and copy in a regular string, up to N bytes
 * long.
 */
void
bu_vls_strncpy(register struct bu_vls *vp, const char *s, size_t n)
{
    register size_t len;

    BU_CK_VLS(vp);

    if ( s == (const char *)NULL )
	return;

    len = strlen(s);
    if ( len > n )
	len = n;
    if ( len <= 0 )  {
	vp->vls_len = 0; /* ensure string is empty */
	return;
    }

    /* cancel offset before extending */
    vp->vls_offset = 0;
    if ( len+1 >= (size_t)vp->vls_max )
	bu_vls_extend( vp, len+1 );

    memcpy(vp->vls_str, s, len);
    vp->vls_str[len] = '\0'; /* force null termination */
    vp->vls_len = len;
}


/**
 * b u _ v l s _ s t r c a t
 *
 * Concatenate a new string onto the end of the existing vls string.
 */
void
bu_vls_strcat(register struct bu_vls *vp, const char *s)
{
    register size_t len;

    BU_CK_VLS(vp);

    if ( s == (const char *)NULL )
	return;
    if ( (len = strlen(s)) <= 0 )
	return;

    if ( (size_t)vp->vls_offset + (size_t)vp->vls_len + len+1 >= (size_t)vp->vls_max )
	bu_vls_extend( vp, len+1 );

    memcpy(vp->vls_str +vp->vls_offset + vp->vls_len, s, len+1); /* include null */
    vp->vls_len += len;
}


/**
 * b u _ v l s _ s t r n c a t
 *
 * Concatenate a new string onto the end of the existing vls string.
 */
void
bu_vls_strncat(register struct bu_vls *vp, const char *s, size_t n)
{
    register size_t len;

    BU_CK_VLS(vp);

    if ( s == (const char *)NULL )
	return;

    len = strlen(s);
    if ( len > n )
	len = n;
    if ( len <= 0 )
	return;

    if ( (size_t)vp->vls_offset + (size_t)vp->vls_len + len+1 >= (size_t)vp->vls_max )
	bu_vls_extend( vp, len+1 );

    memcpy(vp->vls_str + vp->vls_offset + vp->vls_len, s, len);
    vp->vls_len += len;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0'; /* force null termination */
}


/**
 * b u _ v l s _ v l s c a t
 *
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is not affected.
 */
void
bu_vls_vlscat(register struct bu_vls *dest, register const struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);

    if ( src->vls_len <= 0 )
	return;

    if ( dest->vls_offset + dest->vls_len + src->vls_len+1 >= dest->vls_max )
	bu_vls_extend( dest, (unsigned)src->vls_len+1 );

    /* copy source string, including null */
    memcpy(dest->vls_str +dest->vls_offset + dest->vls_len, src->vls_str+src->vls_offset, (size_t)src->vls_len+1);
    dest->vls_len += src->vls_len;
}


/**
 * b u _ v l s _ v l s c a t z a p
 *
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is released (zapped).
 */
void
bu_vls_vlscatzap(register struct bu_vls *dest, register struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);

    if ( src->vls_len <= 0 )
	return;

    bu_vls_vlscat( dest, src );
    bu_vls_trunc( src, 0 );
}


/**
 * b u _ v l s _ s t r c m p
 *
 * Lexicographically compare to vls strings.  Returns an integer
 * greater than, equal to, or less than 0, according as the string s1
 * is greater than, equal to, or less than the string s2.
 */
int
bu_vls_strcmp(struct bu_vls *s1, struct bu_vls *s2)
{
    BU_CK_VLS(s1);
    BU_CK_VLS(s2);

    /* A zero-length VLS is a null string, account for it */
    if ( s1->vls_max == 0 || s1->vls_str == (char *)NULL ) {
	/* s1 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s1, 1);
    }
    if ( s2->vls_max == 0 || s2->vls_str == (char *)NULL ) {
	/* s2 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s2, 1);
    }

    /* neither empty, straight up comparison */
    return strcmp(s1->vls_str+s1->vls_offset, s2->vls_str+s2->vls_offset);
}


/**
 * b u _ v l s _ s t r n c m p
 *
 * Lexicographically compare two vls strings up to n characters.
 * Returns an integer greater than, equal to, or less than 0,
 * according as the string s1 is greater than, equal to, or less than
 * the string s2.
 */
int
bu_vls_strncmp(struct bu_vls *s1, struct bu_vls *s2, size_t n)
{
    BU_CK_VLS(s1);
    BU_CK_VLS(s2);

    if (n <= 0) {
	/* they match at zero chars */
	return 0;
    }

    /* A zero-length VLS is a null string, account for it */
    if ( s1->vls_max == 0 || s1->vls_str == (char *)NULL ) {
	/* s1 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s1, 1);
    }
    if ( s2->vls_max == 0 || s2->vls_str == (char *)NULL ) {
	/* s2 is empty */
	/* ensure first-time allocation for null-termination */
	bu_vls_extend(s2, 1);
    }

    return strncmp(s1->vls_str+s1->vls_offset, s2->vls_str+s2->vls_offset, n);
}


/**
 * b u _ v l s _ f r o m _ a r g v
 *
 * Given and argc & argv pair, convert them into a vls string of
 * space-separated words.
 */
void
bu_vls_from_argv(register struct bu_vls *vp, int argc, const char *argv[])
{
    BU_CK_VLS(vp);

    for (/* nada */; argc > 0; argc--, argv++)  {
	bu_vls_strcat( vp, *argv );
	if ( argc > 1 )  bu_vls_strcat( vp, " " );
    }
}


/**
 * b u _ a r g v _ f r o m _ s t r i n g
 *
 * Build argv[] array from input buffer, by splitting whitespace
 * separated "words" into null terminated strings.
 *
 * 'lim' indicates the maximum number of elements that can be stored
 * in the argv[] array not including a terminating NULL.
 *
 * The input buffer is altered by this process.  The argv[] array
 * points into the input buffer.  The argv[] array needs to have at
 * least lim+1 pointers allocated for lim items plus a terminating
 * pointer to NULL.  The input buffer should not be freed until argv
 * has been freed or passes out of scope.
 *
 * Returns -
 *	0	no words in input
 *	argc	number of words of input, now in argv[]
 */
int
bu_argv_from_string(char *argv[], int lim, char *lp)
{
    register int argc = 0; /* number of words seen */
    register int skip = 0;

    if (!argv) {
	/* do this instead of crashing */
	bu_bomb("bu_argv_from_string received a null argv\n");
    }
    argv[0] = (char *)NULL;

    if (lim <= 0 || !lp) {
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

#ifdef HAVE_SHELL_ESCAPE
    /* Handle "!" shell escape char so the shell can parse the line */
    if (*lp == '!') {
	int ret;

	ret = system(lp+1);
	if (ret != 0)  {
	    perror("system(3)");
	    bu_log("bu_argv_from_string() FAILED: %s\n", lp);
	}

	/* No words, return NULL */
	return 0;
    }
#endif

    /* some non-space string has been seen, set argv[0] */
    argc = 0;
    argv[argc] = lp;

    for (; *lp != '\0'; lp++) {

	/* skip over current word */
	if (!isspace(*lp))
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


/**
 * b u _ v l s _ f w r i t e
 *
 * Write the VLS to the provided file pointer.
 */
void
bu_vls_fwrite(FILE *fp, const struct bu_vls *vp)
{
    int status;

    BU_CK_VLS(vp);

    if ( vp->vls_len <= 0 )
	return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    status = fwrite( vp->vls_str + vp->vls_offset, (size_t)vp->vls_len, 1, fp );
    bu_semaphore_release(BU_SEM_SYSCALL);

    if ( status != 1 ) {
	perror("fwrite");
	bu_bomb("bu_vls_fwrite() write error\n");
    }
}


/**
 * b u _ v l s _ w r i t e
 *
 * Write the VLS to the provided file descriptor.
 */
void
bu_vls_write( int fd, const struct bu_vls *vp )
{
    int status;

    BU_CK_VLS(vp);

    if ( vp->vls_len <= 0 )
	return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    status = write( fd, vp->vls_str + vp->vls_offset, (size_t)vp->vls_len );
    bu_semaphore_release(BU_SEM_SYSCALL);
    
    if ( status != vp->vls_len ) {
	perror("write");
	bu_bomb("bu_vls_write() write error\n");
    }
}


/**
 * b u _ v l s _ r e a d
 *
 * Read the remainder of a UNIX file onto the end of a vls.
 *
 * Returns -
 *	nread	number of characters read
 *	0	if EOF encountered immediately
 *	-1	read error
 */
int
bu_vls_read( struct bu_vls *vp, int fd )
{
    size_t todo;
    int	got;
    int	ret = 0;

    BU_CK_VLS(vp);

    for (;;)  {
	bu_vls_extend( vp, _VLS_ALLOC_READ );
	todo = (size_t)vp->vls_max - vp->vls_len - vp->vls_offset - 1;
	
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	got = read(fd, vp->vls_str+vp->vls_offset+vp->vls_len, todo );
	bu_semaphore_release(BU_SEM_SYSCALL);
	
	if ( got < 0 )  {
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


/**
 * b u _ v l s _ g e t s
 *
 * Append a newline-terminated string from the file pointed to by "fp"
 * to the end of the vls pointed to by "vp".  The newline from the
 * file is read, but not stored into the vls.
 *
 * The most common error is to forget to bu_vls_trunc(vp, 0) before
 * reading the next line into the vls.
 *
 * Returns -
 *	>=0	the length of the resulting vls
 *	 -1	on EOF where no characters were added to the vls.
 */
int
bu_vls_gets(register struct bu_vls *vp, register FILE *fp)
{
    int	startlen;
    int endlen;
    char buffer[BUFSIZ*10] = {0};
    char *bufp;

    BU_CK_VLS(vp);

    startlen = bu_vls_strlen(vp);

    bufp = bu_fgets(buffer, BUFSIZ*10, fp);

    if (!bufp)
	return -1;

    /* strip the trailing EOL (or at least part of it) */
    if ((bufp[strlen(bufp)-1] == '\n') ||
	(bufp[strlen(bufp)-1] == '\r'))
	bufp[strlen(bufp)-1] = '\0';

    /* handle \r\n lines */
    if (bufp[strlen(bufp)-1] == '\r')
	bufp[strlen(bufp)-1] = '\0';

    bu_vls_printf(vp, "%s", bufp);

    /* sanity check */
    endlen = bu_vls_strlen(vp);
    if (endlen < startlen )
	return -1;

    return endlen;
}


/**
 * b u _ v l s _ p u t c
 *
 * Append the given character to the vls.
 */
void
bu_vls_putc(register struct bu_vls *vp, int c)
{
    BU_CK_VLS(vp);

    if ( vp->vls_offset + vp->vls_len+1 >= vp->vls_max )
	bu_vls_extend( vp, _VLS_ALLOC_STEP );

    vp->vls_str[vp->vls_offset + vp->vls_len++] = (char)c;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0'; /* force null termination */
}


/**
 * b u _ v l s _ t r i m s p a c e
 *
 * Remove leading and trailing white space from a vls string.
 */
void
bu_vls_trimspace( struct bu_vls *vp )
{
    BU_CK_VLS(vp);

    /* Remove trailing white space */
    while ( isspace( bu_vls_addr(vp)[bu_vls_strlen(vp)-1] ) )
	bu_vls_trunc( vp, -1 );

    /* Remove leading white space */
    while ( isspace( *bu_vls_addr(vp) ) )
	bu_vls_nibble( vp, 1 );
}


/**
 * b u _ v l s _ v p r i n t f
 *
 * Format a string into a vls.  This version should work on
 * practically any machine, but it serves to highlight the the
 * grossness of the varargs package requiring the size of a parameter
 * to be known at compile time.
 *
 * %s continues to be a regular 'C' string, null terminated.
 * %S is a pointer to a (struct bu_vls *) string.
 *
 * This routine appends to the given vls similar to how vprintf
 * appends to stdout (see bu_vls_vsprintf for overwriting the vls).
 */
void
bu_vls_vprintf(struct bu_vls *vls, const char *fmt, va_list ap)
{
    register const char	*sp; /* start pointer */
    register const char	*ep; /* end pointer */
    register int len;

#define LONGINT  0x001
#define FIELDLEN 0x002
#define SHORTINT 0x003

    int flags;
    int fieldlen=-1;

    char fbuf[64] = {0}; /* % format buffer */
    char buf[BUFSIZ] = {0};

    if (!vls || !fmt || fmt[0] == '\0') {
	/* nothing to print to or from */
	return;
    }

    BU_CK_VLS(vls);

    bu_vls_extend(vls, _VLS_ALLOC_STEP);

    sp = fmt;
    while ( *sp ) {
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
	while ( *ep )  {
	    ++ep;
	    if (*ep == ' ' || *ep == '#' || *ep == '-' ||
		*ep == '+' || *ep == '.' || isdigit(*ep))
		continue;
	    else if (*ep == 'l' || *ep == 'U' || *ep == 'O')
		flags |= LONGINT;
	    else if (*ep == '*') {
		fieldlen = va_arg(ap, int);
		flags |= FIELDLEN;
	    } else if (*ep == 'h') {
		flags |= SHORTINT;
	    } else
		/* Anything else must be the end of the fmt specifier */
		break;
	}

	/* Copy off the format string */
	len = ep-sp+1;
	if ((size_t)len > sizeof(fbuf)-1)
	    len = sizeof(fbuf)-1;
	strncpy(fbuf, sp, (size_t)len);
	fbuf[len] = '\0'; /* ensure null termination */

	/* Grab parameter from arg list, and print it */
	switch ( *ep ) {
	    case 's':
	    {
		register char *str;

		str = va_arg(ap, char *);
		if (str)  {
		    if (flags & FIELDLEN) {
			int stringlen = strlen(str);
			int left_justify;

			if ((left_justify = (fieldlen < 0)))
			    fieldlen *= -1; /* make positive */

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
	    case 'S':
	    {
		register struct bu_vls *vp;

		vp = va_arg(ap, struct bu_vls *);
		if (vp) {
		    BU_CK_VLS(vp);
		    if (flags & FIELDLEN) {
			int	stringlen = bu_vls_strlen(vp);
			int	left_justify;

			if ((left_justify = (fieldlen < 0)))
			    fieldlen *= -1;

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
#if defined(LONGDBL)
		if (flags & LONGDBL) {
		    register long double ld;

		    ld = va_arg(ap, long double);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, ld);
		    else
			snprintf(buf, BUFSIZ, fbuf, ld);
		    bu_vls_strcat(vls, buf);
		} else
#endif
		{
		    register double d;
		    
		    d = va_arg(ap, double);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, d);
		    else
			snprintf(buf, BUFSIZ, fbuf, d);
		    bu_vls_strcat(vls, buf);
		}
		break;
	    case 'd':
	    case 'p':
	    case 'x':
		if (flags & LONGINT) {
		    /* Long int */
		    register long ll;

		    ll = va_arg(ap, long);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, ll);
		    else
			snprintf(buf, BUFSIZ, fbuf, ll);
		    bu_vls_strcat(vls, buf);
		} else if (flags & SHORTINT) {
		    /* short int */
		    register short int sh;
		    sh = (short int)va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, sh);
		    else
			snprintf(buf, BUFSIZ, fbuf, sh);
		    bu_vls_strcat(vls, buf);
		} else {
		    /* Regular int */
		    register int j;

		    j = va_arg(ap, int);
		    if (flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbuf, fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbuf, j);
		    bu_vls_strcat(vls, buf);
		}
		break;
	    case '%':
		bu_vls_putc(vls, '%');
		break;
	    default:  /* Something weird, maybe %c */
	    {
		register int j;

		/* We hope, whatever it is, it fits in an int and the resulting
		   stringlet is smaller than sizeof(buf) bytes */
		
		j = va_arg(ap, int);
		if (flags & FIELDLEN)
		    snprintf(buf, BUFSIZ, fbuf, fieldlen, j);
		else
		    snprintf(buf, BUFSIZ, fbuf, j);
		bu_vls_strcat(vls, buf);
		break;
	    }
	}
	sp = ep+1;
    }

    va_end(ap);
}


/**
 * b u _ v l s _ p r i n t f
 *
 * Initializes the va_list, then calls the above bu_vls_vprintf.
 */
void
bu_vls_printf(struct bu_vls *vls, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    BU_CK_VLS(vls);

    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}


/**
 * b u _ v l s _ s p r i n t f
 *
 * Format a string into a vls, setting the vls to the given print
 * specifier expansion.  This routine truncates any existing vls
 * contents beforehand (i.e. it doesn't append, see bu_vls_vprintf for
 * appending to the vls).
 *
 * %s continues to be a regular 'C' string, null terminated.
 * %S is a pointer to a (struct bu_vls *) string.
 */
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


/**
 * b u _ v l s _ s p a c e s
 *
 * Efficiently append 'cnt' spaces to the current vls.
 */
void
bu_vls_spaces(register struct bu_vls *vp, int cnt)
{
    BU_CK_VLS(vp);

    if ( cnt <= 0 )
	return;
    if ( vp->vls_offset + vp->vls_len + cnt+1 >= vp->vls_max )
	bu_vls_extend( vp, (unsigned)cnt );

    memset(vp->vls_str + vp->vls_offset + vp->vls_len, ' ', (size_t)cnt);
    vp->vls_len += cnt;
}


/**
 * b u _ v l s _ p r i n t _ p o s i t i o n s _ u s e d
 *
 * Returns number of printed spaces used on final output line of a
 * potentially multi-line vls.  Useful for making decisions on when to
 * line-wrap.
 *
 * Accounts for normal UNIX tab-expansion:
 *	         1         2         3         4
 *	1234567890123456789012345678901234567890
 *	        x       x       x       x
 *
 *	0-7 --> 8, 8-15 --> 16, 16-23 --> 24, etc.
 */
int
bu_vls_print_positions_used(const struct bu_vls *vp)
{
    char *start;
    int	used;

    BU_CK_VLS(vp);

    if ( (start = strrchr( bu_vls_addr(vp), '\n' )) == NULL )
	start = bu_vls_addr(vp);

    used = 0;
    while ( *start != '\0' )  {
	if ( *start == '\t' )  {
	    used += 8 - (used % 8);
	} else {
	    used++;
	}
	start++;
    }

    return used;
}


/**
 * b u _ v l s _ d e t a b
 *
 * Given a vls, return a version of that string which has had all
 * "tab" characters converted to the appropriate number of spaces
 * according to the UNIX tab convention.
 */
void
bu_vls_detab(struct bu_vls *vp)
{
    struct bu_vls	src;
    register char	*cp;
    int		used;

    BU_CK_VLS(vp);

    bu_vls_init( &src );
    bu_vls_vlscatzap( &src, vp );	/* make temporary copy of src */
    bu_vls_extend( vp, (unsigned)bu_vls_strlen(&src) + _VLS_ALLOC_STEP );

    cp = bu_vls_addr( &src );
    used = 0;
    while ( *cp != '\0' )  {
	if ( *cp == '\t' )  {
	    int	todo;
	    todo = 8 - (used % 8);
	    bu_vls_spaces( vp, todo );
	    used += todo;
	} else if ( *cp == '\n' )  {
	    bu_vls_putc( vp, '\n' );
	    used = 0;
	} else {
	    bu_vls_putc( vp, *cp );
	    used++;
	}
	cp++;
    }

    bu_vls_free( &src );
}


/**
 * b u _ v l s _ p r e p e n d
 *
 * Add a string to the begining of the vls.
 */
void
bu_vls_prepend(struct bu_vls *vp, char *str)
{
    size_t len = strlen(str);

    bu_vls_extend(vp, len);

    /* memmove is supposed to be safe even if strings overlap */
    memmove( vp->vls_str+vp->vls_offset+len, vp->vls_str+vp->vls_offset, (size_t)vp->vls_len );

    /* insert the data at the head of the string */
    memcpy(vp->vls_str+vp->vls_offset, str, len);
}


/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
