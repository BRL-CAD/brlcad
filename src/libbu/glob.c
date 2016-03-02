/*                         G L O B . C
 *
 * Based on OpenBSD: glob.c, v 1.44 2015/09/14
 *          NetBSD:  glob.c, v 1.35 2013/03/20
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

/*
 * glob(3) -- a superset of the one defined in POSIX 1003.2.
 *
 * The [!...] convention to negate a range is supported (SysV, Posix, ksh).
 *
 * Optional extra services, controlled by flags not defined by POSIX:
 *
 * BU_GLOB_QUOTE:
 *	Escaping convention: \ inhibits any special meaning the following
 *	character might have (except \ at end of string is retained).
 * BU_GLOB_MAGCHAR:
 *	Set in gl_flags if pattern contained a globbing character.
 * BU_GLOB_NOMAGIC:
 *	Same as BU_GLOB_NOCHECK, but it will only append pattern if it did
 *	not contain any magic characters.  [Used in csh style globbing]
 * BU_GLOB_TILDE:
 *	expand ~user/foo to the /home/dir/of/user/foo
 * BU_GLOB_BRACE:
 *	expand {1,2}{a,b} to 1a 1b 2a 2b 
 * BU_GLOB_PERIOD:
 *	allow metacharacters to match leading dots in filenames.
 * BU_GLOB_NO_DOTDIRS:
 *	. and .. are hidden from wildcards, even if BU_GLOB_PERIOD is set.
 * gl_matchc:
 *	Number of matches in the current invocation of glob.
 */

#include "common.h"

#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"

#include "uce-dirent.h"
#include "./charclass.h"

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'
#define	LBRACE		'{'
#define	RBRACE		'}'
#define	SLASH		'/'
#define	COMMA		','

#ifndef DEBUG

#define	M_QUOTE		0x8000
#define	M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

typedef u_short Char;

#else

#define	M_QUOTE		0x80
#define	M_PROTECT	0x40
#define	M_MASK		0xff
#define	M_ASCII		0x7f

typedef char Char;

#endif


#define	CHAR(c)		((Char)((c)&M_ASCII))
#define	META(c)		((Char)((c)|M_QUOTE))
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	M_CLASS		META(':')
#define	ismeta(c)	(((c)&M_QUOTE) != 0)

#define	BU_GLOB_LIMIT_MALLOC	65536
#define	BU_GLOB_LIMIT_STAT		2048
#define	BU_GLOB_LIMIT_READDIR	16384

/* Limit of recursion during matching attempts. */
#define BU_GLOB_LIMIT_RECUR	64

struct glob_lim {
    size_t	glim_malloc;
    size_t	glim_stat;
    size_t	glim_readdir;
};

struct glob_path_stat {
    char		*gps_path;
    struct bu_stat	*gps_stat;
};

static int	 compare(const void *, const void *);
static int	 compare_gps(const void *, const void *);
static int	 g_Ctoc(const Char *, char *, u_int);
static int	 g_lstat(Char *, struct bu_stat *, bu_glob_t *);
static void     *g_opendir(Char *, bu_glob_t *);
static Char	*g_strchr(const Char *, int);
static int	 g_strncmp(const Char *, const char *, size_t);
/*static int	 g_stat(Char *, struct bu_stat *, bu_glob_t *);*/
static int	 glob0(const Char *, bu_glob_t *, struct glob_lim *);
static int	 glob1(Char *, Char *, bu_glob_t *, struct glob_lim *);
static int	 glob2(Char *, Char *, Char *, Char *, Char *, Char *,
	bu_glob_t *, struct glob_lim *);
static int	 glob3(Char *, Char *, Char *, Char *, Char *,
	Char *, Char *, bu_glob_t *, struct glob_lim *);
static int	 globextend(const Char *, bu_glob_t *, struct glob_lim *,
	struct bu_stat *);
static const Char *
globtilde(const Char *, Char *, size_t, bu_glob_t *);
static int	 globexp1(const Char *, bu_glob_t *, struct glob_lim *);
static int	 globexp2(const Char *, const Char *, bu_glob_t *,
	struct glob_lim *);
static int	 match(Char *, Char *, Char *, int);

int
bu_glob(const char *pattern, int flags, bu_glob_t *pglob)
{
    const u_char *patnext;
    int c;
    Char *bufnext, *bufend, patbuf[PATH_MAX];
    struct glob_lim limit = { 0, 0, 0 };

    patnext = (u_char *) pattern;
    if (!(flags & BU_GLOB_APPEND)) {
	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;
	pglob->gl_statv = NULL;
    }
    pglob->gl_flags = flags & ~BU_GLOB_MAGCHAR;
    pglob->gl_matchc = 0;

    if (strnlen(pattern, PATH_MAX) == PATH_MAX)
	return(BU_GLOB_NOMATCH);

    if (pglob->gl_pathc < 0 || pglob->gl_pathc >= INT_MAX)
	return BU_GLOB_NOSPACE;

    bufnext = patbuf;
    bufend = bufnext + PATH_MAX - 1;
    if (flags & BU_GLOB_NOESCAPE)
	while (bufnext < bufend && (c = *patnext++) != EOS) 
	    *bufnext++ = c;
    else {
	/* Protect the quoted characters. */
	while (bufnext < bufend && (c = *patnext++) != EOS) 
	    if (c == QUOTE) {
		if ((c = *patnext++) == EOS) {
		    c = QUOTE;
		    --patnext;
		}
		*bufnext++ = c | M_PROTECT;
	    } else
		*bufnext++ = c;
    }
    *bufnext = EOS;

    if (flags & BU_GLOB_BRACE)
	return globexp1(patbuf, pglob, &limit);
    else
	return glob0(patbuf, pglob, &limit);
}

/*
 * Expand recursively a glob {} pattern. When there is no more expansion
 * invoke the standard globbing routine to glob the rest of the magic
 * characters
 */
static int
globexp1(const Char *pattern, bu_glob_t *pglob, struct glob_lim *limitp)
{
    const Char* ptr = pattern;

    /* Protect a single {}, for find(1), like csh */
    if (pattern[0] == LBRACE && pattern[1] == RBRACE && pattern[2] == EOS)
	return glob0(pattern, pglob, limitp);

    if ((ptr = (const Char *) g_strchr(ptr, LBRACE)) != NULL)
	return globexp2(ptr, pattern, pglob, limitp);

    return glob0(pattern, pglob, limitp);
}


/*
 * Recursive brace globbing helper. Tries to expand a single brace.
 * If it succeeds then it invokes globexp1 with the new pattern.
 * If it fails then it tries to glob the rest of the pattern and returns.
 */
static int
globexp2(const Char *ptr, const Char *pattern, bu_glob_t *pglob,
	struct glob_lim *limitp)
{
    int     i, rv;
    Char   *lm, *ls;
    const Char *pe, *pm, *pl;
    Char    patbuf[PATH_MAX];

    /* copy part up to the brace */
    for (lm = patbuf, pm = pattern; pm != ptr; *lm++ = *pm++)
	;
    *lm = EOS;
    ls = lm;

    /* Find the balanced brace */
    for (i = 0, pe = ++ptr; *pe; pe++)
	if (*pe == LBRACKET) {
	    /* Ignore everything between [] */
	    for (pm = pe++; *pe != RBRACKET && *pe != EOS; pe++)
		;
	    if (*pe == EOS) {
		/* 
		 * We could not find a matching RBRACKET.
		 * Ignore and just look for RBRACE
		 */
		pe = pm;
	    }
	} else if (*pe == LBRACE)
	    i++;
	else if (*pe == RBRACE) {
	    if (i == 0)
		break;
	    i--;
	}

    /* Non matching braces; just glob the pattern */
    if (i != 0 || *pe == EOS)
	return glob0(patbuf, pglob, limitp);

    for (i = 0, pl = pm = ptr; pm <= pe; pm++) {
	switch (*pm) {
	    case LBRACKET:
		/* Ignore everything between [] */
		for (pl = pm++; *pm != RBRACKET && *pm != EOS; pm++)
		    ;
		if (*pm == EOS) {
		    /* 
		     * We could not find a matching RBRACKET.
		     * Ignore and just look for RBRACE
		     */
		    pm = pl;
		}
		break;

	    case LBRACE:
		i++;
		break;

	    case RBRACE:
		if (i) {
		    i--;
		    break;
		}
		/* FALLTHROUGH */
	    case COMMA:
		if (i && *pm == COMMA)
		    break;
		else {
		    /* Append the current string */
		    for (lm = ls; (pl < pm); *lm++ = *pl++)
			;

		    /* 
		     * Append the rest of the pattern after the
		     * closing brace
		     */
		    for (pl = pe + 1; (*lm++ = *pl++) != EOS;)
			;

		    /* Expand the current pattern */
		    rv = globexp1(patbuf, pglob, limitp);
		    if (rv && rv != BU_GLOB_NOMATCH)
			return rv;

		    /* move after the comma, to the next string */
		    pl = pm + 1;
		}
		break;

	    default:
		break;
	}
    }
    return 0;
}



/*
 * expand tilde.
 */
static const Char *
globtilde(const Char *pattern, Char *patbuf, size_t patbuf_len, bu_glob_t *pglob)
{
    int i;
    int charcnt = 0;
    char *h;
    const Char *p;
    Char *b, *eb;

    if (*pattern != TILDE || !(pglob->e_tilde))
	return pattern;

    /* Copy up to the end of the string or / */
    eb = &patbuf[patbuf_len - 1];
    for (p = pattern + 1, h = (char *) patbuf;
	    h < (char *)eb && *p && *p != SLASH; *h++ = *p++)
	;

    *h = EOS;

    charcnt = pglob->e_tilde(h, pglob->dptr);

    /* TODO - for .g files, ~ returns all objects as toplevels which may
     * match subsequent patterns.  Think about how to handle this... -
     * perhaps ~/obj1/a*.s should be the same as obj1/a*.s, which would mean
     * simply stripping the first / as well as ~ from the path... (if the / )
     * is present - ~obj1/a*.s should probably resolve to obj1/a*.s, unless
     * ~ is escaped... */

    if (charcnt) {
	if (charcnt > 0) {
	    /* Copy the home directory */
	    for (b = patbuf; b < eb && *h; *b++ = *h++)
		;
	} else {
	    for (i = 0; i < -1*charcnt; i++) p++;
	}
    }

    /* Append the rest of the pattern */
    while (b < eb && (*b++ = *p++) != EOS)
	;
    *b = EOS;

    return patbuf;
}

static int
g_strncmp(const Char *s1, const char *s2, size_t n)
{
    int rv = 0;

    while (n--) {
	rv = *(Char *)s1 - *(const unsigned char *)s2++;
	if (rv)
	    break;
	if (*s1++ == '\0')
	    break;
    }
    return rv;
}

static int
g_charclass(const Char **patternp, Char **bufnextp)
{
    const Char *pattern = *patternp + 1;
    Char *bufnext = *bufnextp;
    const Char *colon;
    CHARCLASS *cc;
    size_t len;

    if ((colon = g_strchr(pattern, ':')) == NULL || colon[1] != ']')
	return 1;	/* not a character class */

    len = (size_t)(colon - pattern);
    for (cc = charclasses; cc->idstring != NULL; cc++) {
	if (!g_strncmp(pattern, cc->idstring, len) && cc->idstring[len] == '\0')
	    break;
    }
    if (cc->idstring == NULL)
	return -1;	/* invalid character class */
    *bufnext++ = M_CLASS;
    *bufnext++ = (Char)(cc - &charclasses[0]);
    *bufnextp = bufnext;
    *patternp += len + 3;

    return 0;
}

/*
 * The main glob() routine: compiles the pattern (optionally processing
 * quotes), calls glob1() to do the real pattern matching, and finally
 * sorts the list (unless unsorted operation is requested).  Returns 0
 * if things went well, nonzero if errors occurred.  It is not an error
 * to find no matches.
 */
static int
glob0(const Char *pattern, bu_glob_t *pglob, struct glob_lim *limitp)
{
    const Char *qpatnext;
    int c, err, oldpathc;
    Char *bufnext, patbuf[PATH_MAX];

    qpatnext = globtilde(pattern, patbuf, PATH_MAX, pglob);
    oldpathc = pglob->gl_pathc;
    bufnext = patbuf;

    /* We don't need to check for buffer overflow any more. */
    while ((c = *qpatnext++) != EOS) {
	switch (c) {
	    case LBRACKET:
		c = *qpatnext;
		if (c == NOT)
		    ++qpatnext;
		if (*qpatnext == EOS ||
			g_strchr(qpatnext+1, RBRACKET) == NULL) {
		    *bufnext++ = LBRACKET;
		    if (c == NOT)
			--qpatnext;
		    break;
		}
		*bufnext++ = M_SET;
		if (c == NOT)
		    *bufnext++ = M_NOT;
		c = *qpatnext++;
		do {
		    if (c == LBRACKET && *qpatnext == ':') {
			do {
			    err = g_charclass(&qpatnext,
				    &bufnext);
			    if (err)
				break;
			    c = *qpatnext++;
			} while (c == LBRACKET && *qpatnext == ':');
			if (err == -1 &&
				!(pglob->gl_flags & BU_GLOB_NOCHECK))
			    return BU_GLOB_NOMATCH;
			if (c == RBRACKET)
			    break;
		    }
		    *bufnext++ = CHAR(c);
		    if (*qpatnext == RANGE &&
			    (c = qpatnext[1]) != RBRACKET) {
			*bufnext++ = M_RNG;
			*bufnext++ = CHAR(c);
			qpatnext += 2;
		    }
		} while ((c = *qpatnext++) != RBRACKET);
		pglob->gl_flags |= BU_GLOB_MAGCHAR;
		*bufnext++ = M_END;
		break;
	    case QUESTION:
		pglob->gl_flags |= BU_GLOB_MAGCHAR;
		*bufnext++ = M_ONE;
		break;
	    case STAR:
		pglob->gl_flags |= BU_GLOB_MAGCHAR;
		/* collapse adjacent stars to one [or three if globstar]
		 * to avoid exponential behavior
		 */
		if (bufnext == patbuf || bufnext[-1] != M_ALL ||
			((pglob->gl_flags & BU_GLOB_STAR) != 0 &&
			 (bufnext - 1 == patbuf || bufnext[-2] != M_ALL ||
			  bufnext - 2 == patbuf || bufnext[-3] != M_ALL)))
		    *bufnext++ = M_ALL;
		break;
	    default:
		*bufnext++ = CHAR(c);
		break;
	}
    }
    *bufnext = EOS;

    if ((err = glob1(patbuf, patbuf+PATH_MAX-1, pglob, limitp)) != 0)
	return(err);

    /*
     * If there was no match we are going to append the pattern 
     * if BU_GLOB_NOCHECK was specified or if BU_GLOB_NOMAGIC was specified
     * and the pattern did not contain any magic characters
     * BU_GLOB_NOMAGIC is there just for compatibility with csh.
     */
    if (pglob->gl_pathc == oldpathc) {
	if ((pglob->gl_flags & BU_GLOB_NOCHECK) ||
		((pglob->gl_flags & BU_GLOB_NOMAGIC) &&
		 !(pglob->gl_flags & BU_GLOB_MAGCHAR)))
	    return(globextend(pattern, pglob, limitp, NULL));
	else
	    return(BU_GLOB_NOMATCH);
    }
    if (!(pglob->gl_flags & BU_GLOB_NOSORT)) {
	if ((pglob->gl_flags & BU_GLOB_KEEPSTAT)) {
	    /* Keep the paths and stat info synced during sort */
	    struct glob_path_stat *path_stat;
	    int i;
	    int n = pglob->gl_pathc - oldpathc;
	    int o = oldpathc;

	    path_stat = (struct glob_path_stat *)bu_calloc(n, sizeof(*path_stat),"glob_path_stat");

	    if (path_stat == NULL)
		return BU_GLOB_NOSPACE;
	    for (i = 0; i < n; i++) {
		path_stat[i].gps_path = pglob->gl_pathv[o + i];
		path_stat[i].gps_stat = pglob->gl_statv[o + i];
	    }
	    qsort(path_stat, n, sizeof(*path_stat), compare_gps);
	    for (i = 0; i < n; i++) {
		pglob->gl_pathv[o + i] = path_stat[i].gps_path;
		pglob->gl_statv[o + i] = path_stat[i].gps_stat;
	    }
	    free(path_stat);
	} else {
	    qsort(pglob->gl_pathv + oldpathc,
		    pglob->gl_pathc - oldpathc, sizeof(char *),
		    compare);
	}
    }
    return(0);
}

static int
compare(const void *p, const void *q)
{
    return(bu_strcmp(*(char **)p, *(char **)q));
}

static int
compare_gps(const void *_p, const void *_q)
{
    const struct glob_path_stat *p = (const struct glob_path_stat *)_p;
    const struct glob_path_stat *q = (const struct glob_path_stat *)_q;

    return(bu_strcmp(p->gps_path, q->gps_path));
}

static int
glob1(Char *pattern, Char *pattern_last, bu_glob_t *pglob, struct glob_lim *limitp)
{
    Char pathbuf[PATH_MAX];

    /* A null pathname is invalid -- POSIX 1003.1 sect. 2.4. */
    if (*pattern == EOS)
	return(0);
    return(glob2(pathbuf, pathbuf+PATH_MAX-1,
		pathbuf, pathbuf+PATH_MAX-1,
		pattern, pattern_last, pglob, limitp));
}

/*
 * The functions glob2 and glob3 are mutually recursive; there is one level
 * of recursion for each segment in the pattern that contains one or more
 * meta characters.
 */
static int
glob2(Char *pathbuf, Char *pathbuf_last, Char *pathend, Char *pathend_last,
	Char *pattern, Char *pattern_last, bu_glob_t *pglob, struct glob_lim *limitp)
{
    struct bu_stat sb;
    Char *p, *q;
    int anymeta;

    /*
     * Loop over pattern segments until end of pattern or until
     * segment with meta character found.
     */
    for (anymeta = 0;;) {
	if (*pattern == EOS) {		/* End of pattern? */
	    *pathend = EOS;

	    if ((pglob->gl_flags & BU_GLOB_LIMIT) &&
		    limitp->glim_stat++ >= BU_GLOB_LIMIT_STAT) {
		errno = 0;
		*pathend++ = SEP;
		*pathend = EOS;
		return(BU_GLOB_NOSPACE);
	    }
	    if (g_lstat(pathbuf, &sb, pglob))
		return(0);

	    ++pglob->gl_matchc;
	    return(globextend(pathbuf, pglob, limitp, &sb));
	}

	/* Find end of next segment, copy tentatively to pathend. */
	q = pathend;
	p = pattern;
	while (*p != EOS && *p != SEP) {
	    if (ismeta(*p))
		anymeta = 1;
	    if (q+1 > pathend_last)
		return (1);
	    *q++ = *p++;
	}

	if (!anymeta) {		/* No expansion, do next segment. */
	    pathend = q;
	    pattern = p;
	    while (*pattern == SEP) {
		if (pathend+1 > pathend_last)
		    return (1);
		*pathend++ = *pattern++;
	    }
	} else
	    /* Need expansion, recurse. */
	    return(glob3(pathbuf, pathbuf_last, pathend,
			pathend_last, pattern, p, pattern_last,
			pglob, limitp));
    }
    /* NOTREACHED */
}

static int
glob3(Char *pathbuf, Char *pathbuf_last, Char *pathend, Char *pathend_last,
	Char *pattern, Char *restpattern, Char *restpattern_last, bu_glob_t *pglob,
	struct glob_lim *limitp)
{
    struct bu_dirent dp;
    void *dirp;
    int err;
    char buf[PATH_MAX];

    /*
     * The readdirfunc declaration can't be prototyped, because it is
     * assigned, below, to two functions which are prototyped in glob.h
     * and dirent.h as taking pointers to differently typed opaque
     * structures.
     */
    int (*readdirfunc)(struct bu_dirent *, void *, void *);

    if (pathend > pathend_last)
	return (1);
    *pathend = EOS;
    errno = 0;

    if ((dirp = g_opendir(pathbuf, pglob)) == NULL) {
	/* TODO: don't call for ENOENT or ENOTDIR? */
	if (pglob->gl_errfunc) {
	    if (g_Ctoc(pathbuf, buf, sizeof(buf)))
		return(BU_GLOB_ABORTED);
	    if (pglob->gl_errfunc(buf, errno, pglob->dptr) ||
		    pglob->gl_flags & BU_GLOB_ERR)
		return(BU_GLOB_ABORTED);
	}
	return(0);
    }

    err = 0;

    /* Search directory for matching names. */
    readdirfunc = pglob->gl_readdir;
    while ((*readdirfunc)(&dp, dirp, pglob->dptr) > 0) {
	u_char *sc;
	Char *dc;

	if ((pglob->gl_flags & BU_GLOB_LIMIT) &&
		limitp->glim_readdir++ >= BU_GLOB_LIMIT_READDIR) {
	    errno = 0;
	    *pathend++ = SEP;
	    *pathend = EOS;
	    err = BU_GLOB_NOSPACE;
	    break;
	}

	/*
	 * Initial DOT must be matched literally, unless we have
	 * BU_GLOB_PERIOD set.
	 */
	if ((pglob->gl_flags & BU_GLOB_PERIOD) == 0)
	    if (dp.d_name[0] == DOT && *pattern != DOT)
		continue;

	/*
	 * If GLOB_NO_DOTDIRS is set, . and .. vanish.
	 */
	if ((pglob->gl_flags & BU_GLOB_NO_DOTDIRS) &&
		(dp.d_name[0] == DOT) &&
		((dp.d_name[1] == EOS) ||
		 ((dp.d_name[1] == DOT) && (dp.d_name[2] == EOS))))
	    continue;

	dc = pathend;
	sc = (u_char *) dp.d_name;
	while (dc < pathend_last && (*dc++ = *sc++) != EOS)
	    ;
	if (dc >= pathend_last) {
	    *dc = EOS;
	    err = 1;
	    break;
	}

	if (!match(pathend, pattern, restpattern, BU_GLOB_LIMIT_RECUR)) {
	    *pathend = EOS;
	    continue;
	}
	err = glob2(pathbuf, pathbuf_last, --dc, pathend_last,
		restpattern, restpattern_last, pglob, limitp);
	if (err)
	    break;
    }

    (*pglob->gl_closedir)(dirp, pglob->dptr);
    return(err);
}


/*
 * Extend the gl_pathv member of a bu_glob_t structure to accommodate a new item,
 * add the new item, and update gl_pathc.
 *
 * This assumes the BSD realloc, which only copies the block when its size
 * crosses a power-of-two boundary; for v7 realloc, this would cause quadratic
 * behavior.
 *
 * Return 0 if new item added, error code if memory couldn't be allocated.
 *
 * Invariant of the bu_glob_t structure:
 *	Either gl_pathc is zero and gl_pathv is NULL; or gl_pathc > 0 and
 *	gl_pathv points to (gl_pathc + 1) items.
 */
static int
globextend(const Char *path, bu_glob_t *pglob, struct glob_lim *limitp,
	struct bu_stat *sb)
{
    char **pathv;
    ssize_t i;
    size_t newn, len;
    char *copy = NULL;
    const Char *p;
    struct bu_stat **statv;

    newn = 2 + pglob->gl_pathc;
    if (pglob->gl_pathc >= INT_MAX ||
	    newn >= INT_MAX ||
	    SIZE_MAX / sizeof(*pathv) <= newn ||
	    SIZE_MAX / sizeof(*statv) <= newn) {
nospace:
	for (i = 0; i < (ssize_t)(newn - 2); i++) {
	    if (pglob->gl_pathv && pglob->gl_pathv[i])
		free(pglob->gl_pathv[i]);
	    if ((pglob->gl_flags & BU_GLOB_KEEPSTAT) != 0 &&
		    pglob->gl_pathv && pglob->gl_pathv[i])
		free(pglob->gl_statv[i]);
	}
	if (pglob->gl_pathv) {
	    free(pglob->gl_pathv);
	    pglob->gl_pathv = NULL;
	}
	if (pglob->gl_statv) {
	    free(pglob->gl_statv);
	    pglob->gl_statv = NULL;
	}
	return(BU_GLOB_NOSPACE);
    }

    pathv = (char **)bu_reallocarray(pglob->gl_pathv, newn, sizeof(*pathv), "pathv");
    if (pathv == NULL)
	goto nospace;
    pglob->gl_pathv = pathv;

    if ((pglob->gl_flags & BU_GLOB_KEEPSTAT) != 0) {
	statv = (struct bu_stat **)bu_reallocarray(pglob->gl_statv, newn, sizeof(*statv), "statv");
	if (statv == NULL)
	    goto nospace;
	pglob->gl_statv = statv;
	if (sb == NULL)
	    statv[pglob->gl_pathc] = NULL;
	else {
	    limitp->glim_malloc += sizeof(**statv);
	    if ((pglob->gl_flags & BU_GLOB_LIMIT) &&
		    limitp->glim_malloc >= BU_GLOB_LIMIT_MALLOC) {
		errno = 0;
		return(BU_GLOB_NOSPACE);
	    }
	    if ((statv[pglob->gl_pathc] =
			(struct bu_stat *)malloc(sizeof(**statv))) == NULL)
		goto copy_error;
	    memcpy(statv[pglob->gl_pathc], sb,
		    sizeof(*sb));
	}
	statv[pglob->gl_pathc + 1] = NULL;
    }

    for (p = path; *p++;)
	;
    len = (size_t)(p - path);
    limitp->glim_malloc += len;
    if ((copy = (char *)malloc(len)) != NULL) {
	if (g_Ctoc(path, copy, len)) {
	    free(copy);
	    return(BU_GLOB_NOSPACE);
	}
	pathv[pglob->gl_pathc++] = copy;
    }
    pathv[pglob->gl_pathc] = NULL;

    if ((pglob->gl_flags & BU_GLOB_LIMIT) &&
	    (newn * sizeof(*pathv)) + limitp->glim_malloc >
	    BU_GLOB_LIMIT_MALLOC) {
	errno = 0;
	return(BU_GLOB_NOSPACE);
    }
copy_error:
    return(copy == NULL ? BU_GLOB_NOSPACE : 0);
}


/*
 * pattern matching function for filenames.  Each occurrence of the *
 * pattern causes a recursion level.
 */
static int
match(Char *name, Char *pat, Char *patend, int recur)
{
    int ok, negate_range;
    Char c, k;

    if (recur-- == 0)
	return(BU_GLOB_NOSPACE);

    while (pat < patend) {
	c = *pat++;
	switch (c & M_MASK) {
	    case M_ALL:
		while (pat < patend && (*pat & M_MASK) == M_ALL)
		    pat++;	/* eat consecutive '*' */
		if (pat == patend)
		    return(1);
		do {
		    if (match(name, pat, patend, recur))
			return(1);
		} while (*name++ != EOS);
		return(0);
	    case M_ONE:
		if (*name++ == EOS)
		    return(0);
		break;
	    case M_SET:
		ok = 0;
		if ((k = *name++) == EOS)
		    return(0);
		if ((negate_range = ((*pat & M_MASK) == M_NOT)) != EOS)
		    ++pat;
		while (((c = *pat++) & M_MASK) != M_END) {
		    if ((c & M_MASK) == M_CLASS) {
			Char idx = *pat & M_MASK;
			if ((size_t)idx < NCCLASSES &&
				charclasses[(size_t)idx].checkfun(k))
			    ok = 1;
			++pat;
		    }
		    if ((*pat & M_MASK) == M_RNG) {
			if (c <= k && k <= pat[1])
			    ok = 1;
			pat += 2;
		    } else if (c == k)
			ok = 1;
		}
		if (ok == negate_range)
		    return(0);
		break;
	    default:
		if (*name++ != c)
		    return(0);
		break;
	}
    }
    return(*name == EOS);
}

/* Free allocated data belonging to a bu_glob_t structure. */
    void
globfree(bu_glob_t *pglob)
{
    int i;
    char **pp;

    if (pglob->gl_pathv != NULL) {
	pp = pglob->gl_pathv;
	for (i = pglob->gl_pathc; i--; ++pp)
	    free(*pp);
	free(pglob->gl_pathv);
	pglob->gl_pathv = NULL;
    }
    if (pglob->gl_statv != NULL) {
	for (i = 0; i < pglob->gl_pathc; i++) {
	    free(pglob->gl_statv[i]);
	}
	free(pglob->gl_statv);
	pglob->gl_statv = NULL;
    }
}

    static void *
g_opendir(Char *str, bu_glob_t *pglob)
{
    char buf[PATH_MAX];

    if (!*str)
	bu_strlcpy(buf, ".", sizeof buf);
    else {
	if (g_Ctoc(str, buf, sizeof(buf)))
	    return(NULL);
    }

    return((*pglob->gl_opendir)(buf, pglob->dptr));
}

static int
g_lstat(Char *fn, struct bu_stat *sb, bu_glob_t *pglob)
{
    char buf[PATH_MAX];

    if (g_Ctoc(fn, buf, sizeof(buf)))
	return(-1);
    return((*pglob->gl_lstat)(buf, sb, pglob->dptr));
}

#if 0
static int
g_stat(Char *fn, struct bu_stat *sb, bu_glob_t *pglob)
{
    char buf[PATH_MAX];

    if (!fn || !sb || !pglob) return -1;

    if (g_Ctoc(fn, buf, sizeof(buf)))
	return(-1);
    return((*pglob->gl_stat)(buf, sb, pglob->dptr));
}
#endif

static Char *
g_strchr(const Char *str, int ch)
{
    do {
	if (*str == ch)
	    return ((Char *)str);
    } while (*str++);
    return (NULL);
}

static int
g_Ctoc(const Char *str, char *buf, u_int len)
{

    while (len--) {
	if ((*buf++ = *str++) == EOS)
	    return (0);
    }
    return (1);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
