/*                           V L S . C
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

/** \addtogroup libbu */
/*@{*/
/** @file vls.c
 *  The variable length string package.
 *
 *  Assumption:  libc-provided sprintf() function is safe to use in parallel,
 *  on parallel systems.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

static const char libbu_vls_RCSid[] = "@(#)$Header$ (BRL)";

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#if defined(HAVE_STDARG_H)
/* ANSI C */
#  include <stdarg.h>
#endif
#if !defined(HAVE_STDARG_H) && defined(HAVE_VARARGS_H)
/* VARARGS */
#  include <varargs.h>
#endif

#include "machine.h"
#include "bu.h"

#if defined(HAVE_VARARGS_H) || defined(HAVE_STDARG_H)
BU_EXTERN(void	bu_vls_vprintf, (struct bu_vls *vls, const char *fmt, va_list ap));
#endif

const char bu_vls_message[] = "bu_vls_str";
extern const char bu_strdup_message[];

/**
 *			B U _ V L S _ I N I T
 *
 *  No storage should be allocated at this point,
 *  and bu_vls_addr() must be able to live with that.
 */
void
bu_vls_init(register struct bu_vls *vp)
{
    if (vp == (struct bu_vls  *)NULL)
	bu_bomb("bu_vls_init() passed NULL pointer");

    /* if it's already a vls, perform a sanity check that we're not
     * leaking memory.
     */
#if defined(DEBUG) && 0
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
 *			B U _ V L S _ I N I T _ I F _ U N I N I T
 *
 *  If a VLS is unitialized, initialize it.
 *  If it is already initialized, leave it alone, caller wants to
 *  append to it.
 */
void
bu_vls_init_if_uninit(register struct bu_vls *vp)
{
    if (vp == (struct bu_vls  *)NULL)
	bu_bomb("bu_vls_init_if_uninit() passed NULL pointer");

    if( vp->vls_magic == BU_VLS_MAGIC )  return;
    bu_vls_init( vp );
}


/**
 *			B U _ V L S _ V L S I N I T
 *
 *  Allocate storage for a struct bu_vls, call bu_vls_init on it, and return
 *  the result.  Allows for creation of dynamically allocated vls strings.
 */
struct bu_vls *
bu_vls_vlsinit(void)
{
    register struct bu_vls	*vp;

    vp = (struct bu_vls *)bu_malloc(sizeof(struct bu_vls), "bu_vls_vlsinit struct");
    bu_vls_init(vp);

    return vp;
}

/**
 *			B U _ V L S _ A D D R
 *
 *  Return a pointer to the null-terminated string in the vls array.
 *  If no storage has been allocated yet, give back a valid string.
 */
char *
bu_vls_addr(register const struct bu_vls *vp)
{
    static char	nullbuf[4];

    BU_CK_VLS(vp);

    if( vp->vls_max == 0 || vp->vls_str == (char *)NULL )  {
	/* A zero-length VLS is a null string */
	nullbuf[0] = '\0';
	return(nullbuf);
    }

    /* Sanity checking */
    if( vp->vls_max < 0 ||
	vp->vls_len < 0 ||
	vp->vls_offset < 0 ||
	vp->vls_str == (char *)NULL ||
	vp->vls_len + vp->vls_offset >= vp->vls_max )  {
	bu_log("bu_vls_addr: bad VLS.  max=%d, len=%d, offset=%d\n",
	       vp->vls_max, vp->vls_len, vp->vls_offset);
	bu_bomb("bu_vls_addr\n");
    }

    return( vp->vls_str+vp->vls_offset );
}

/**
 *			B U _ V L S _ E X T E N D
 */
void
bu_vls_extend(register struct bu_vls *vp, unsigned int extra)
{
    BU_CK_VLS(vp);
    if( extra < 40 )  extra = 40;
    if( vp->vls_max <= 0 || vp->vls_str == (char *)0 )  {
	vp->vls_max = extra;
	vp->vls_str = (char *)bu_malloc( vp->vls_max, bu_vls_message );
	vp->vls_len = 0;
	vp->vls_offset = 0;
	*vp->vls_str = '\0';
	return;
    }
    if( vp->vls_offset + vp->vls_len + extra >= vp->vls_max )  {
	vp->vls_max += extra;
	if( vp->vls_max < 120 )  vp->vls_max = 120;
	vp->vls_str = (char *)bu_realloc( vp->vls_str, vp->vls_max,
					  bu_vls_message );
    }
}

/**
 *			B U _ V L S _ S E T L E N
 *
 *  Ensure that the vls has a length of at least 'newlen', and make
 *  that the current length.
 *  Useful for subroutines that are planning on mucking with the data
 *  array themselves.  Not advisable, but occasionally useful.
 *  Does not change the offset from the front of the buffer, if any.
 *  Does not initialize the value of any of the new bytes.
 */
void
bu_vls_setlen(struct bu_vls *vp, int newlen)
{
    BU_CK_VLS(vp);
    if( vp->vls_len >= newlen )  return;
    bu_vls_extend( vp, newlen - vp->vls_len );
    vp->vls_len = newlen;
}

/**
 *			B U _ V L S _ S T R L E N
 *
 *  Return length of the string, in bytes, not including the null terminator.
 */
int
bu_vls_strlen(register const struct bu_vls *vp)
{
    BU_CK_VLS(vp);
    if( vp->vls_len <= 0 )  return  0;
    return  vp->vls_len;
}

/**
 *			B U _ V L S _ T R U N C
 *
 *  Truncate string to at most 'len' characters.
 *  If 'len' is negative, trim off that many from the end.
 *  If 'len' is zero, don't release storage -- user is probably
 *  just going to refill it again, e.g. with bu_vls_gets().
 */
void
bu_vls_trunc(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);
    if( len < 0 )  len = vp->vls_len + len;	/* now an absolute length */
    if( vp->vls_len <= len )  return;
    if( len == 0 )  vp->vls_offset = 0;
    vp->vls_str[len+vp->vls_offset] = '\0';	/* force null termination */
    vp->vls_len = len;
}

/**
 *			B U _ V L S _ T R U N C 2
 *
 *  Son of bu_vls_trunc.
 *  Same as bu_vls_trunc except that it doesn't take negative len.
 */
void
bu_vls_trunc2(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);
    if( vp->vls_len <= len )  return;
    if( len < 0 )  len = 0;
    if( len == 0 )  vp->vls_offset = 0;
    vp->vls_str[len+vp->vls_offset] = '\0';	/* force null termination */
    vp->vls_len = len;
}

/**
 *			B U _ V L S _ N I B B L E
 *
 *  "Nibble" 'len' characters off the front of the string.
 *  Changes the length and offset;  no data is copied.
 *  'len' may be positive or negative.
 *  If negative, characters are un-nibbled.
 */
void
bu_vls_nibble(register struct bu_vls *vp, int len)
{
    BU_CK_VLS(vp);
    if( len < 0 && (-len) > vp->vls_offset )  len = -vp->vls_offset;
    if (len >= vp->vls_len) {
	bu_vls_trunc( vp, 0 );
	return;
    }
    vp->vls_len -= len;
    vp->vls_offset += len;
}

/**
 *			B U _ V L S _ F R E E
 *
 *  Releases the memory used for the string buffer.
 */
void
bu_vls_free(register struct bu_vls *vp)
{
    BU_CK_VLS(vp);
    if( vp->vls_str )  {
	vp->vls_str[0] = '?';		/* Sanity */
	bu_free( vp->vls_str, "bu_vls_free" );
	vp->vls_str = (char *)0;
    }
    vp->vls_offset = vp->vls_len = vp->vls_max = 0;
}

/**
 *			B U _ V L S _ V L S F R E E
 *
 *  Releases the memory used for the string buffer and the memory for
 *  the vls structure
 */
void
bu_vls_vlsfree(register struct bu_vls *vp)
{
    if ( *(long *)vp != BU_VLS_MAGIC) return;

    bu_vls_free( vp );
    bu_free( vp, "bu_vls_vlsfree" );
}

/**
 *			B U _ V L S _ S T R D U P
 *
 *  Make an "ordinary" string copy of a vls string.  Storage for the regular
 *  string is acquired using malloc.
 *  The source string is not affected.
 */
char *
bu_vls_strdup(register const struct bu_vls *vp)
{
    register char *str;
    register int len;

    len = bu_vls_strlen(vp);
    str = bu_malloc(len+1, bu_strdup_message );
    strncpy(str, bu_vls_addr(vp), len);
    str[len] = '\0';
    return str;
}

/**
 *			B U _ V L S _ S T R G R A B
 *
 *  Like bu_vls_strdup(), but destructively grab the string from the
 *  source argument 'vp'.  This is more efficient than bu_vls_strdup() for
 *  those instances where the source argument 'vp' is no longer needed
 *  by the caller, as it avoides a potentially long buffer copy.
 *
 *  The source string is destroyed, as if bu_vls_free() had been called.
 */
char *
bu_vls_strgrab(register struct bu_vls *vp)
{
    register char *str;

    BU_CK_VLS(vp);
    if( vp->vls_offset != 0 )  {
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
 *			B U _ V L S _ S T R C P Y
 *
 *  Empty the vls string, and copy in a regular string.
 */
void
bu_vls_strcpy(register struct bu_vls *vp, const char *s)
{
    register int	len;

    BU_CK_VLS(vp);
    if( s == (const char *)NULL )  return;
    if( (len = strlen(s)) <= 0 )  {
	vp->vls_len = 0;
	vp->vls_offset = 0;
	if(vp->vls_max > 0)
	    vp->vls_str[0] = '\0';
	return;
    }
    vp->vls_offset = 0;		/* cancel offset before extending */
    if( len+1 >= vp->vls_max )  bu_vls_extend( vp, len+1 );
    bcopy( s, vp->vls_str, len+1 );		/* include null */
    vp->vls_len = len;
}

/**
 *			B U _ V L S _ S T R N C P Y
 *
 *  Empty the vls string, and copy in a regular string, up to N bytes long.
 */
void
bu_vls_strncpy(register struct bu_vls *vp, const char *s, long int n)
{
    register int	len;

    BU_CK_VLS(vp);
    if( s == (const char *)NULL )  return;
    len = strlen(s);
    if( len > n )  len = n;
    if( len <= 0 )  {
	vp->vls_len = 0;	/* ensure string is empty */
	return;
    }
    vp->vls_offset = 0;		/* cancel offset before extending */
    if( len+1 >= vp->vls_max )  bu_vls_extend( vp, len+1 );
    bcopy( s, vp->vls_str, len );
    vp->vls_str[len] = '\0';		/* force null termination */
    vp->vls_len = len;
}

/**
 *			B U _ V L S _ S T R C A T
 *
 *  Concatenate a new string onto the end of the existing vls string.
 */
void
bu_vls_strcat(register struct bu_vls *vp, const char *s)
{
    register int	len;

    BU_CK_VLS(vp);
    if( s == (const char *)NULL )  return;
    if( (len = strlen(s)) <= 0 )  return;
    if( vp->vls_offset + vp->vls_len + len+1 >= vp->vls_max )
	bu_vls_extend( vp, len+1 );
    bcopy( s, vp->vls_str +vp->vls_offset + vp->vls_len, len+1 );	/* include null */
    vp->vls_len += len;
}

/**
 *			B U _ V L S _ S T R N C A T
 *
 *  Concatenate a new string onto the end of the existing vls string.
 */
void
bu_vls_strncat(register struct bu_vls *vp, const char *s, long int n)
{
    register int	len;

    BU_CK_VLS(vp);
    if( s == (const char *)NULL )  return;
    len = strlen(s);
    if( len > n )  len = n;
    if( len <= 0 )  return;			/* do nothing */
    if( vp->vls_offset + vp->vls_len + len+1 >= vp->vls_max )
	bu_vls_extend( vp, len+1 );
    bcopy( s, vp->vls_str + vp->vls_offset + vp->vls_len, len );
    vp->vls_len += len;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0';	/* force null termination */
}

/**
 *			B U _ V L S _ V L S C A T
 *
 *  Concatenate a new vls string onto the end of an existing vls string.
 *  The storage of the source string is not affected.
 */
void
bu_vls_vlscat(register struct bu_vls *dest, register const struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);
    if( src->vls_len <= 0 )  return;
    if( dest->vls_offset + dest->vls_len + src->vls_len+1 >= dest->vls_max )
	bu_vls_extend( dest, src->vls_len+1 );
    /* copy source string, including null */
    bcopy( src->vls_str+src->vls_offset,
	   dest->vls_str +dest->vls_offset + dest->vls_len,
	   src->vls_len+1 );
    dest->vls_len += src->vls_len;
}

/**
 *			V L S _ V L S C A T Z A P
 *
 *  Concatenate a new vls string onto the end of an existing vls string.
 *  The storage of the source string is released (zapped).
 */
void
bu_vls_vlscatzap(register struct bu_vls *dest, register struct bu_vls *src)
{
    BU_CK_VLS(src);
    BU_CK_VLS(dest);
    if( src->vls_len <= 0 )  return;
    bu_vls_vlscat( dest, src );
    bu_vls_trunc( src, 0 );
}

/**
 *			B U _ V L S _ F R O M _ A R G V
 *
 *  Given and argc & argv pair, convert them into a vls string of space-
 *  separated words.
 */
void
bu_vls_from_argv(register struct bu_vls *vp, int argc, const char *argv[])
{
    BU_CK_VLS(vp);
    for( ; argc > 0; argc--, argv++ )  {
	bu_vls_strcat( vp, *argv );
	if( argc > 1 )  bu_vls_strcat( vp, " " );
    }
}

/**
 *			B U _ A R G V _ F R O M _ S T R I N G
 *
 *  Build argv[] array from input buffer, by splitting whitespace
 *  separated "words" into null terminated strings.
 *  The input buffer is altered by this process.
 *  The argv[] array points into the input buffer.
 *  The input buffer should not be freed until argv has been freed
 *  or passes out of scope.
 *
 *  'lim' indicates the number of elements in the argv[] array.
 *
 *  Returns -
 *	 0	no words in input
 *	nwords	number of words of input, now in argv[]
 *
 *  Built from rt_split_cmd(), but without the shell escape support.
 */
int
bu_argv_from_string(char **argv, int lim, register char *lp)
{
    register int	nwords;			/* number of words seen */
    register char	*lp1;

    argv[0] = "_NIL_";		/* sanity */

    while( *lp != '\0' && isspace( *lp ) )
	lp++;

    if( *lp == '\0' )
	return 0;		/* No words */

    /* some non-space string has been seen, argv[0] is set */
    nwords = 1;
    argv[0] = lp;

    for( ; *lp != '\0'; lp++ )  {
	if( !isspace( *lp ) )
	    continue;	/* skip over current word */

	*lp = '\0';		/* terminate current word */
	lp1 = lp + 1;
	if( *lp1 != '\0' && !isspace( *lp1 ) )  {
	    /* Begin next word */
	    if( nwords >= lim-1 )
		break;	/* argv[] full */

	    argv[nwords++] = lp1;
	}
    }
    argv[nwords] = (char *)0;	/* safety */
    return nwords;
}

/**
 *			B U _ V L S _ F W R I T E
 */
void
bu_vls_fwrite(FILE *fp, const struct bu_vls *vp)
{
    int status;

    BU_CK_VLS(vp);
    if( vp->vls_len <= 0 )  return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    status = fwrite( vp->vls_str + vp->vls_offset, vp->vls_len, 1, fp );
    bu_semaphore_release(BU_SEM_SYSCALL);

    if( status != 1 ) {
	perror("fwrite");
	bu_bomb("bu_vls_fwrite() write error\n");
    }
}

/**
 *			B U _ V L S _ W R I T E
 */
void
bu_vls_write( int fd, const struct bu_vls *vp )
{

    BU_CK_VLS(vp);
    if( vp->vls_len <= 0 )  return;

#if !defined(HAVE_UNIX_IO)
    bu_bomb("bu_vls_write(): This isn't UNIX\n");
#else
    {
	int status;
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	status = write( fd, vp->vls_str + vp->vls_offset, vp->vls_len );
	bu_semaphore_release(BU_SEM_SYSCALL);

	if( status != vp->vls_len ) {
	    perror("write");
	    bu_bomb("bu_vls_write() write error\n");
	}
    }
#endif
}

/**
 *			B U _ V L S _ R E A D
 *
 *  Read the remainder of a UNIX file onto the end of a vls.
 *
 *  Returns -
 *	nread	number of characters read
 *	0	if EOF encountered immediately
 *	-1	read error
 */
int
bu_vls_read( struct bu_vls *vp, int fd )
{
    int	ret = 0;

    BU_CK_VLS(vp);

#if !defined(HAVE_UNIX_IO)
    bu_bomb("bu_vls_read(): This isn't UNIX\n");
#else
    {
	int	todo;
	int	got;
	for(;;)  {
	    bu_vls_extend( vp, 4096 );
	    todo = vp->vls_max - vp->vls_len - vp->vls_offset - 1;

	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    got = read(fd, vp->vls_str+vp->vls_offset+vp->vls_len, todo );
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    if( got < 0 )  {
		/* Read error, abandon the read */
		return -1;
	    }
	    if(got == 0)  break;
	    vp->vls_len += got;
	    ret += got;
	}

	/* force null termination */
	vp->vls_str[vp->vls_len+vp->vls_offset] = '\0';
    }
#endif
    return ret;
}

/**
 *			B U _ V L S _ G E T S
 *
 *  Append a newline-terminated string from the file pointed to by "fp"
 *  to the end of the vls pointed to by "vp".
 *  The newline from the file is read, but not stored into the vls.
 *
 *  The most common error is to forget to bu_vls_trunc(vp,0) before
 *  reading the next line into the vls.
 *
 *  Returns -
 *	>=0	the length of the resulting vls
 *	 -1	on EOF where no characters were added to the vls.
 */
int
bu_vls_gets(register struct bu_vls *vp, register FILE *fp)
{
    int	startlen;
    int	c;

    BU_CK_VLS(vp);

    startlen = bu_vls_strlen(vp);
    bu_vls_extend( vp, 80 );		/* Ensure room to grow */
    for( ;; )  {
	/* Talk about inefficiency... */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	c = getc(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );

	/* XXX Alternatively, code up something with fgets(), chunking */

	if( c == EOF || c == '\n' )  break;
	bu_vls_putc( vp, c );
    }
    if( c == EOF && bu_vls_strlen(vp) <= startlen )  return -1;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0';	/* force null termination */
    return bu_vls_strlen(vp);
}

/**
 *                      B U _ V L S _ P U T C
 *
 *  Append the given character to the vls.
 */
void
bu_vls_putc(register struct bu_vls *vp, int c)
{
    BU_CK_VLS(vp);

    if( vp->vls_offset + vp->vls_len+1 >= vp->vls_max )  bu_vls_extend( vp, 80 );
    vp->vls_str[vp->vls_offset + vp->vls_len++] = (char)c;
    vp->vls_str[vp->vls_offset + vp->vls_len] = '\0';	/* force null termination */
}

/**
 *			B U _ V L S _ T R I M S P A C E
 *
 *  Remove leading and trailing white space from a vls string.
 */
void
bu_vls_trimspace( struct bu_vls *vp )
{
    BU_CK_VLS(vp);

    /* Remove trailing white space */
    while( isspace( bu_vls_addr(vp)[bu_vls_strlen(vp)-1] ) )
	bu_vls_trunc( vp, -1 );

    /* Remove leading white space */
    while( isspace( *bu_vls_addr(vp) ) )
	bu_vls_nibble( vp, 1 );
}

#if defined(HAVE_VARARGS_H) || defined(HAVE_STDARG_H)
/**
 *  			B U _ V L S _ V P R I N T F
 *
 *  Format a string into a vls.  This version should work on practically
 *  any machine, but it serves to highlight the the grossness of the varargs
 *  package requiring the size of a parameter to be known at compile time.
 *
 *  %s continues to be a regular 'C' string, null terminated.
 *  %S is a pointer to a (struct bu_vls *) string.
 *
 *  This routine appends to the given vls similar to how vprintf
 *  appends to stdout (see bu_vls_vsprintf for overwriting the vls).
 */
void
bu_vls_vprintf(struct bu_vls *vls, const char *fmt, va_list ap)
{
    register const char	*sp;			/* start pointer */
    register const char	*ep;			/* end pointer */
    register int len;

#define LONGINT  0x001
#define FIELDLEN 0x002
#define SHORTINT 0x003

    int flags;
    int fieldlen=-1;
    char fbuf[64] = {0}, buf[1024] = {0};			/* % format buffer */

    BU_CK_VLS(vls);
    bu_vls_extend(vls, 96);

    sp = fmt;
    while( *sp ) {
	/* Initial state:  just printing chars */
	fmt = sp;
	while (*sp != '%' && *sp)
	    sp++;

	if (sp != fmt)
	    bu_vls_strncat(vls, fmt, sp-fmt);

	if (*sp == '\0')
	    break;

	/* Saw a percent sign, find end of fmt specifier */

	flags = 0;
	ep = sp;
	while( *ep )  {
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
	if (len > sizeof(fbuf)-1) len = sizeof(fbuf)-1;
	strncpy(fbuf, sp, len);
	fbuf[len] = '\0'; /* ensure null termination */

	/* Grab parameter from arg list, and print it */
	switch( *ep ) {
	    case 's':
		{
		    register char *str;

		    str = va_arg(ap, char *);
		    if (str)  {
			if (flags & FIELDLEN) {
			    int	stringlen = strlen(str);
			    int	left_justify;

			    if ((left_justify = (fieldlen < 0)))
				fieldlen *= -1;

			    if (stringlen >= fieldlen)
				bu_vls_strncat(vls, str, fieldlen);
			    else {
				struct bu_vls		padded;
				int			i;

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
			    bu_vls_strncat(vls, "(null)", fieldlen);
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
				bu_vls_strncat(vls, bu_vls_addr(vp), fieldlen);
			    else {
				struct bu_vls		padded;
				int			i;

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
			    bu_vls_strncat(vls, "(null)", fieldlen);
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
			sprintf(buf, fbuf, fieldlen, ld);
		    else
			sprintf(buf, fbuf, ld);
		    bu_vls_strcat(vls, buf);
		} else
#endif
		    {
			register double d;

			d = va_arg(ap, double);
			if (flags & FIELDLEN)
			    sprintf(buf, fbuf, fieldlen, d);
			else
			    sprintf(buf, fbuf, d);
			bu_vls_strcat(vls, buf);
		    }
		break;
	    case 'd':
	    case 'x':
		if (flags & LONGINT) {
		    /* Long int */
		    register long ll;

		    ll = va_arg(ap, long);
		    if (flags & FIELDLEN)
			sprintf(buf, fbuf, fieldlen, ll);
		    else
			sprintf(buf, fbuf, ll);
		    bu_vls_strcat(vls, buf);
		} else if (flags & SHORTINT) {
		    /* short int */
		    register short int sh;
		    sh = (short int)va_arg(ap, int);
		    if (flags & FIELDLEN)
			sprintf(buf, fbuf, fieldlen, sh);
		    else
			sprintf(buf, fbuf, sh);
		    bu_vls_strcat(vls, buf);
		} else {
		    /* Regular int */
		    register int j;

		    j = va_arg(ap, int);
		    if (flags & FIELDLEN)
			sprintf(buf, fbuf, fieldlen, j);
		    else
			sprintf(buf, fbuf, j);
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
			sprintf(buf, fbuf, fieldlen, j);
		    else
			sprintf(buf, fbuf, j);
		    bu_vls_strcat(vls, buf);
		    break;
		}
	}
	sp = ep+1;
    }

    va_end(ap);
}
#else
#  error "No implementation provided for bu_vls_vprintf()"
#endif  /* !defined(HAVE_VARARGS_H) && !defined(HAVE_STDARG_H) */


#if defined(HAVE_STDARG_H)
/**
 *                 B U _ V L S _ P R I N T F
 *
 * Initializes the va_list, then calls the above bu_vls_vprintf.
 */
void
bu_vls_printf(struct bu_vls *vls, char *fmt, ...)  /* ANSI C */
{
    va_list ap;
    va_start(ap, fmt);
    BU_CK_VLS(vls);
    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}

#else  /* !HAVE_STDARG_H */
#  if defined(HAVE_VARARGS_H)

void
bu_vls_printf(va_dcl va_alist)                            /* VARARGS */
{
    va_list ap;
    struct bu_vls *vls;
    char *fmt;

    va_start(ap);
    vls = va_arg(ap, struct bu_vls *);
    fmt = va_arg(ap, char *);
    BU_CK_VLS(vls);
    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}

#  else  /* !HAVE_VARARGS_H */

void
bu_vls_printf(struct bu_vls *vls, char *fmt, a,b,c,d,e,f,g,h,i,j)       /* Cray XMP */
{
    char append_buf[65536] = {0};   /* yuck -- fixed length buffer. */

    BU_CK_VLS(vls);
    sprintf(append_buf, fmt, a,b,c,d,e,f,g,h,i,j);
    if (append_buf[sizeof(append_buf)-1] != '\0') {
	/* Attempting to bu_log() the WHOLE append_buf would just overflow again */
	append_buf[120] = '\0';
	bu_log("bu_vls_printf buffer overflow\nWhile building string '%s'...\n",
	       append_buf);
	bu_bomb("bu_vls_printf buffer overflow\n");
    }

    bu_vls_strcat(vls, append_buf);
}
#  endif  /* HAVE_VARARGS_H */
#endif  /* HAVE_STDARG_H */


#if defined(HAVE_STDARG_H)

/**
 *  			B U _ V L S _ S P R I N T F
 *
 *  Format a string into a vls, setting the vls to the given print
 *  specifier expansion.  This routine truncates any existing vls
 *  contents beforehand (i.e. it doesn't append, see bu_vls_vprintf
 *  for appending to the vls).
 *
 *  %s continues to be a regular 'C' string, null terminated.
 *  %S is a pointer to a (struct bu_vls *) string.
 */
void
bu_vls_sprintf(struct bu_vls *vls, char *fmt, ...)  /* ANSI C */
{
    va_list ap;
    va_start(ap, fmt);
    BU_CK_VLS(vls);
    bu_vls_trunc(vls, 0); /* poof */
    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}

#else  /* !HAVE_STDARG_H */
#  if defined(HAVE_VARARGS_H)

void
bu_vls_sprintf(va_dcl va_alist)                            /* VARARGS */
{
    va_list ap;
    struct bu_vls *vls;
    char *fmt;

    va_start(ap);
    vls = va_arg(ap, struct bu_vls *);
    fmt = va_arg(ap, char *);
    BU_CK_VLS(vls);
    bu_vls_trunc(vls, 0); /* poof */
    bu_vls_vprintf(vls, fmt, ap);
    va_end(ap);
}

#  else  /* !HAVE_VARARGS_H */

void
bu_vls_sprintf(struct bu_vls *vls, char *fmt, a,b,c,d,e,f,g,h,i,j)       /* Cray XMP */
{
    BU_CK_VLS(vls);
    bu_vls_trunc(vls, 0); /* poof */
    bu_vls_printf(vls, fmt, a, b, c, d, e, f, g, h, i, j);
}
#  endif  /* HAVE_VARARGS_H */
#endif  /* HAVE_STDARG_H */

/**
 *			B U _ V L S _ S P A C E S
 *
 *  Efficiently append 'cnt' spaces to the current vls.
 */
void
bu_vls_spaces(register struct bu_vls *vp, int cnt)
{
    BU_CK_VLS(vp);
    if( cnt <= 0 )  return;
    if( vp->vls_offset + vp->vls_len + cnt+1 >= vp->vls_max )
	bu_vls_extend( vp, cnt );
    memset( vp->vls_str + vp->vls_offset + vp->vls_len, ' ', cnt );
    vp->vls_len += cnt;
}

/**
 *			B U _ V L S _ P R I N T _ P O S I T I O N S _ U S E D
 *
 *  Returns number of printed spaces used on final output line of a
 *  potentially multi-line vls.  Useful for making decisions on when
 *  to line-wrap.
 *  Accounts for normal UNIX tab-expansion:
 *	         1         2         3         4
 *	1234567890123456789012345678901234567890
 *	        x       x       x       x
 *
 *	0-7 --> 8, 8-15 --> 16, 16-23 --> 24, etc.
 */
int
bu_vls_print_positions_used(const struct bu_vls *vp)
{
    char	*start;
    int	used;

    BU_CK_VLS(vp);

    if( (start = strrchr( bu_vls_addr(vp), '\n' )) == NULL )
	start = bu_vls_addr(vp);
    used = 0;
    while( *start != '\0' )  {
	if( *start == '\t' )  {
	    used += 8 - (used % 8);
	} else {
	    used++;
	}
	start++;
    }
    return used;
}

/**
 *			B U _ V L S _ D E T A B
 *
 *  Given a vls, return a version of that string which has had all
 *  "tab" characters converted to the appropriate number of spaces
 *  according to the UNIX tab convention.
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
    bu_vls_extend( vp, bu_vls_strlen(&src)+50 );

    cp = bu_vls_addr( &src );
    used = 0;
    while( *cp != '\0' )  {
	if( *cp == '\t' )  {
	    int	todo;
	    todo = 8 - (used % 8);
	    bu_vls_spaces( vp, todo );
	    used += todo;
	} else if( *cp == '\n' )  {
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
 *		B U _ V L S _ P R E P E N D
 *
 *  Add a string to the begining of the vls.
 */
void
bu_vls_prepend(struct bu_vls *vp, char *str)
{
    int len = strlen(str);

    bu_vls_extend(vp, len);

    /* memmove is supposed to be safe even if strings overlap */
    memmove( vp->vls_str+vp->vls_offset+len, vp->vls_str+vp->vls_offset, vp->vls_len );

    /* insert the data at the head of the string */
    memcpy( vp->vls_str+vp->vls_offset, str, len);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
