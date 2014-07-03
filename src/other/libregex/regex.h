/* Copyright (c) 1992 Henry Spencer.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer of the University of Toronto.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)regex.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _REGEX_H_
#define	_REGEX_H_

#if defined (_WIN32)
#  include <BaseTsd.h>
#endif

#include <limits.h>
#include <stddef.h>

#if !defined(ssize_t)
   typedef ptrdiff_t ssize_t;
#  define HAVE_SSIZE_T 1
#endif

#if defined(_WIN32) && !defined(_OFF_T_DEFINED)
   typedef SSIZE_T off_t;
#  define _OFF_T_DEFINED 1
#endif

/* On Windows 64bit, "off_t" is defined as a "long"
 * within "sys/types.h" which breaks "libregex". To
 * resolve this, we must define "off_t" before we
 * include "sys/types.h". Defining "off_t" as
 * "ssize_t" works for both Windows 32bit and 64bit.
 */

#include <sys/types.h>

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__off_t_defined) && !defined(_OFF_T) && !defined(_OFF_T_DECLARED)
   typedef ptrdiff_t off_t;
#  define __off_t_defined 1
#  define _OFF_T_DECLARED 1
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef __cplusplus
#  define __BEGIN_DECLS   extern "C" {
#  define __END_DECLS     }
#else
#  ifndef __BEGIN_DECLS
#    define __BEGIN_DECLS
   #endif
#  ifndef __END_DECLS
#    define __END_DECLS
#  endif
#endif


#ifndef REGEX_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef REGEX_EXPORT_DLL
#      define REGEX_EXPORT __declspec(dllexport)
#    else
#      define REGEX_EXPORT __declspec(dllimport)
#    endif
#  else
#    define REGEX_EXPORT
#  endif
#endif

/* types */

typedef off_t regoff_t;

typedef struct {
    int re_magic;
    size_t re_nsub;		/* number of parenthesized subexpressions */
    const char *re_endp;	/* end pointer for REG_PEND */
    struct re_guts *re_g;	/* none of your business :-) */
} regex_t;

typedef struct {
    regoff_t rm_so;		/* start of match */
    regoff_t rm_eo;		/* end of match */
} regmatch_t;

/* regcomp() flags */
#define	REG_BASIC	0000
#define	REG_EXTENDED	0001
#define	REG_ICASE	0002
#define	REG_NOSUB	0004
#define	REG_NEWLINE	0010
#define	REG_NOSPEC	0020
#define	REG_PEND	0040
#define	REG_DUMP	0200

/* regerror() flags */
#define	REG_NOMATCH	 1
#define	REG_BADPAT	 2
#define	REG_ECOLLATE	 3
#define	REG_ECTYPE	 4
#define	REG_EESCAPE	 5
#define	REG_ESUBREG	 6
#define	REG_EBRACK	 7
#define	REG_EPAREN	 8
#define	REG_EBRACE	 9
#define	REG_BADBR	10
#define	REG_ERANGE	11
#define	REG_ESPACE	12
#define	REG_BADRPT	13
#define	REG_EMPTY	14
#define	REG_ASSERT	15
#define	REG_INVARG	16
#define	REG_ATOI	255	/* convert name to number (!) */
#define	REG_ITOA	0400	/* convert number to name (!) */

/* regexec() flags */
#define	REG_NOTBOL	00001
#define	REG_NOTEOL	00002
#define	REG_STARTEND	00004
#define	REG_TRACE	00400	/* tracing of execution */
#define	REG_LARGE	01000	/* force large representation */
#define	REG_BACKR	02000	/* force use of backref code */

__BEGIN_DECLS
REGEX_EXPORT int	regcomp (regex_t *, const char *, int);
REGEX_EXPORT size_t	regerror (int, const regex_t *, char *, size_t);
REGEX_EXPORT int	regexec (const regex_t *, const char *, size_t, regmatch_t [], int);
REGEX_EXPORT void	regfree (regex_t *);
__END_DECLS

#endif /* !_REGEX_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
