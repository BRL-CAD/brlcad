/*	$NetBSD: termcap.c,v 1.54 2006/12/19 02:02:03 uwe Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include "common.h"

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)termcap.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: termcap.c,v 1.54 2006/12/19 02:02:03 uwe Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <errno.h>
#include "pathnames.h"
#include "termcap_private.h"

#define	PBUFSIZ		MAXPATHLEN	/* max length of filename path */
#define	PVECSIZ		32		/* max number of names in path */
#define CAPSIZ		256		/* max capability size */

/*
 * termcap - routines for dealing with the terminal capability data base
 *
 * BUG:		Should use a "last" pointer in tbuf, so that searching
 *		for capabilities alphabetically would not be a n**2/2
 *		process when large numbers of capabilities are given.
 * Note:	If we add a last pointer now we will screw up the
 *		tc capability. We really should compile termcap.
 *
 * Essentially all the work here is scanning and decoding escapes
 * in string capabilities.  We don't use stdio because the editor
 * doesn't, and because living w/o it is not hard.
 */

static char *tbuf = NULL;		/* termcap buffer */
static struct tinfo *fbuf = NULL;	/* untruncated termcap buffer */

/*
 * Set the termcap entry to the arbitrary string passed in, this can
 * be used to provide a "dummy" termcap entry if a real one does not
 * exist.  This function will malloc the buffer and space for the
 * string.  If an error occurs return -1 otherwise return 0.
 */
int
t_setinfo(struct tinfo **bp, const char *entry)
{
	char capability[CAPSIZ], *cap_ptr;
	size_t limit;

	_DIAGASSERT(bp != NULL);
	_DIAGASSERT(entry != NULL);

	if ((*bp = malloc(sizeof(struct tinfo))) == NULL)
		return -1;

	if (((*bp)->info = (char *) malloc(strlen(entry) + 1)) == NULL)
		return -1;

	strcpy((*bp)->info, entry);

	cap_ptr = capability;
	limit = sizeof(capability) - 1;
	(*bp)->up = t_getstr(*bp, "up", &cap_ptr, &limit);
	if ((*bp)->up)
		(*bp)->up = strdup((*bp)->up);
	cap_ptr = capability;
	limit = sizeof(capability) - 1;
	(*bp)->bc = t_getstr(*bp, "bc", &cap_ptr, &limit);
	if ((*bp)->bc)
		(*bp)->bc = strdup((*bp)->bc);
	(*bp)->tbuf = NULL;

	return 0;
}

/*
 * Get an extended entry for the terminal name.  This differs from
 * tgetent only in a) the buffer is malloc'ed for the caller and
 * b) the termcap entry is not truncated to 1023 characters.
 */

int
t_getent(struct tinfo **bp, const char *name)
{
	char  *p;
	char  *cp;
	char **fname;
	char  *home;
	int    i, did_getset;
	size_t limit;
	char   pathbuf[PBUFSIZ];	/* holds raw path of filenames */
	char  *pathvec[PVECSIZ];	/* to point to names in pathbuf */
	char  *termpath;
	char  capability[CAPSIZ], *cap_ptr;
	int error;


	_DIAGASSERT(bp != NULL);
	_DIAGASSERT(name != NULL);

	if ((*bp = malloc(sizeof(struct tinfo))) == NULL)
		return 0;

	fname = pathvec;
	p = pathbuf;
	cp = getenv("TERMCAP");
	/*
	 * TERMCAP can have one of two things in it. It can be the
	 * name of a file to use instead of
	 * /usr/share/misc/termcap. In this case it better start with
	 * a "/". Or it can be an entry to use so we don't have to
	 * read the file. In this case cgetset() withh crunch out the
	 * newlines.  If TERMCAP does not hold a file name then a path
	 * of names is searched instead.  The path is found in the
	 * TERMPATH variable, or becomes _PATH_DEF ("$HOME/.termcap
	 * /usr/share/misc/termcap") if no TERMPATH exists.
	 */
	if (!cp || *cp != '/') {	/* no TERMCAP or it holds an entry */
		if ((termpath = getenv("TERMPATH")) != NULL)
			(void)strlcpy(pathbuf, termpath, sizeof(pathbuf));
		else {
			if ((home = getenv("HOME")) != NULL) {
				/* set up default */
				p += strlen(home);	/* path, looking in */
				(void)strlcpy(pathbuf, home,
				    sizeof(pathbuf)); /* $HOME first */
				if ((size_t)(p - pathbuf) < sizeof(pathbuf) - 1)
				    *p++ = '/';
			}	/* if no $HOME look in current directory */
			if ((size_t)(p - pathbuf) < sizeof(pathbuf) - 1) {
			    /* Added for BRL-CAD, search BRLCAD_DATA/etc dir too */
#ifdef BRLCAD_DATA
			    (void)strlcpy(p, _PATH_DEF " " BRLCAD_DATA "/etc",
				sizeof(pathbuf) - (p - pathbuf));
#else
			    (void)strlcpy(p, _PATH_DEF,
				sizeof(pathbuf) - (p - pathbuf));
#endif
			}
		}
	}
	else {
		/* user-defined name in TERMCAP; still can be tokenized */
		(void)strlcpy(pathbuf, cp, sizeof(pathbuf));
	}

	*fname++ = pathbuf;	/* tokenize path into vector of names */
	while (*++p)
		if (*p == ' ' || *p == ':') {
			*p = '\0';
			while (*++p)
				if (*p != ' ' && *p != ':')
					break;
			if (*p == '\0')
				break;
			*fname++ = p;
			if (fname >= pathvec + PVECSIZ) {
				fname--;
				break;
			}
		}
	*fname = NULL;			/* mark end of vector */

	/*
	 * try ignoring TERMCAP if it has a ZZ in it, we do this
	 * because a TERMCAP with ZZ in it indicates the entry has been
	 * exported by another program using the "old" interface, the
	 * termcap entry has been truncated and ZZ points to an address
	 * in the exporting programs memory space which is of no use
	 * here - anyone who is exporting the termcap entry and then
	 * reading it back again in the same program deserves to be
	 * taken out, beaten up, dragged about, shot and then hurt some
	 * more.
	 */
	did_getset = 0;
	if (cp && *cp && *cp != '/' && strstr(cp, ":ZZ") == NULL) {
		did_getset = 1;
		if (cgetset(cp) < 0) {
			error = -2;
			goto out;
		}
	}

	/*
	 * XXX potential security hole here in a set-id program if the
	 * user had setup name to be built from a path they can not
	 * normally read.
	 */
 	(*bp)->info = NULL;
 	i = cgetent(&((*bp)->info), pathvec, name);

	/*
	 * if we get an error and we skipped doing the cgetset before
	 * we try with TERMCAP in place - we may be using a truncated
	 * termcap entry but what else can one do?
	 */
	if ((i < 0) && (did_getset == 0)) {
		if (cp && *cp && *cp != '/')
			if (cgetset(cp) < 0) {
				error = -2;
				goto out;
			}
		i = cgetent(&((*bp)->info), pathvec, name);
	}

	/* no tc reference loop return code in libterm XXX */
	if (i == -3) {
		error = -1;
		goto out;
	}

	/*
	 * fill in t_goto capabilities - this prevents memory leaks
	 * and is more efficient than fetching these capabilities
	 * every time t_goto is called.
	 */
	if (i >= 0) {
		cap_ptr = capability;
		limit = sizeof(capability) - 1;
		(*bp)->up = t_getstr(*bp, "up", &cap_ptr, &limit);
		if ((*bp)->up)
			(*bp)->up = strdup((*bp)->up);
		cap_ptr = capability;
		limit = sizeof(capability) - 1;
		(*bp)->bc = t_getstr(*bp, "bc", &cap_ptr, &limit);
		if ((*bp)->bc)
			(*bp)->bc = strdup((*bp)->bc);
		(*bp)->tbuf = NULL;
	} else {
		error = i + 1;
		goto out;
	}

	return (i + 1);
out:
	free(*bp);
	*bp = NULL;
	return error;
}

/*
 * Get an entry for terminal name in buffer bp from the termcap file.
 */
int
tgetent(char *bp, const char *name)
{
	int i, plen, elen, c;
        char *ptrbuf = NULL;

	i = t_getent(&fbuf, name);

	if (i == 1) {
		/*
		 * stash the full buffer pointer as the ZZ capability
		 * in the termcap buffer passed.
		 */
                plen = asprintf(&ptrbuf, ":ZZ=%p", fbuf->info);
		(void)strlcpy(bp, fbuf->info, 1024);
                elen = strlen(bp);
		/*
		 * backup over the entry if the addition of the full
		 * buffer pointer will overflow the buffer passed.  We
		 * want to truncate the termcap entry on a capability
		 * boundary.
		 */
                if ((elen + plen) > 1023) {
			bp[1023 - plen] = '\0';
			for (c = (elen - plen); c > 0; c--) {
				if (bp[c] == ':') {
					bp[c] = '\0';
					break;
				}
			}
		}

		strcat(bp, ptrbuf);
                tbuf = bp;
	}

	return i;
}

/*
 * Return the (numeric) option id.
 * Numeric options look like
 *	li#80
 * i.e. the option string is separated from the numeric value by
 * a # character.  If the option is not found we return -1.
 * Note that we handle octal numbers beginning with 0.
 */
int
t_getnum(struct tinfo *info, const char *id)
{
	long num;

	_DIAGASSERT(info != NULL);
	_DIAGASSERT(id != NULL);

	if (cgetnum(info->info, id, &num) == 0)
		return (int)(num);
	else
		return (-1);
}

int
tgetnum(const char *id)
{
	return fbuf ? t_getnum(fbuf, id) : -1;
}

/*
 * Handle a flag option.
 * Flag options are given "naked", i.e. followed by a : or the end
 * of the buffer.  Return 1 if we find the option, or 0 if it is
 * not given.
 */
int t_getflag(struct tinfo *info, const char *id)
{
	_DIAGASSERT(info != NULL);
	_DIAGASSERT(id != NULL);

	return (cgetcap(info->info, id, ':') != NULL);
}

int
tgetflag(const char *id)
{
	return fbuf ? t_getflag(fbuf, id) : 0;
}

/*
 * Get a string valued option.
 * These are given as
 *	cl=^Z
 * Much decoding is done on the strings, and the strings are
 * placed in area, which is a ref parameter which is updated.
 * limit is the number of characters allowed to be put into
 * area, this is updated.
 */
char *
t_getstr(struct tinfo *info, const char *id, char **area, size_t *limit)
{
	char *s;
	int i;

	_DIAGASSERT(info != NULL);
	_DIAGASSERT(id != NULL);
	/* area may be NULL */


	if ((i = cgetstr(info->info, id, &s)) < 0) {
		errno = ENOENT;
		if ((area == NULL) && (limit != NULL))
			*limit = 0;
		return NULL;
	}

	if (area != NULL) {
		/*
		 * check if there is room for the new entry to be put into
		 * area
		 */
		if (limit != NULL && (*limit < (size_t) i)) {
			errno = E2BIG;
			free(s);
			return NULL;
		}

		(void)strcpy(*area, s);
		free(s);
		s = *area;
		*area += i + 1;
		if (limit != NULL)
			*limit -= i;
		return (s);
	} else {
		_DIAGASSERT(limit != NULL);
		*limit = i;
		free(s);
		return NULL;
	}
}

/*
 * Get a string valued option.
 * These are given as
 *	cl=^Z
 * Much decoding is done on the strings, and the strings are
 * placed in area, which is a ref parameter which is updated.
 * No checking on area overflow.
 */
char *
tgetstr(const char *id, char **area)
{
	struct tinfo dummy, *ti;
	char ids[3];

	_DIAGASSERT(id != NULL);

	if (fbuf == NULL)
		return NULL;

	/*
	 * XXX
	 * This is for all the boneheaded programs that relied on tgetstr
	 * to look only at the first 2 characters of the string passed...
	 */
	ids[0] = id[0];
	ids[1] = id[1];
	ids[2] = '\0';

	if ((id[0] == 'Z') && (id[1] == 'Z')) {
		ti = &dummy;
		dummy.info = tbuf;
	} else
		ti = fbuf;

	if (area == NULL || *area == NULL) {
		static char capability[CAPSIZ];
		size_t limit = sizeof(capability) - 1;
		char *ptr;

		ptr = capability;
		return t_getstr(ti, ids, &ptr, &limit);
	} else
		return t_getstr(ti, ids, area, NULL);
}

/*
 * Return a string valued option specified by id, allocating memory to
 * an internal buffer as necessary. The memory allocated can be
 * free'd by a call to t_freent().
 *
 * If the string is not found or memory allocation fails then NULL
 * is returned.
 */
char *
t_agetstr(struct tinfo *info, const char *id)
{
	size_t new_size;
	struct tbuf *tb;

	_DIAGASSERT(info != NULL);
	_DIAGASSERT(id != NULL);

	t_getstr(info, id, NULL, &new_size);

	/* either the string is empty or the capability does not exist. */
	if (new_size == 0)
		return NULL;

	if ((tb = info->tbuf) == NULL ||
	    (size_t) (tb->eptr - tb->ptr) < (new_size + 1)) {
		if (new_size < CAPSIZ)
			new_size = CAPSIZ;
		else
			new_size++;

		if ((tb = malloc(sizeof(*info->tbuf))) == NULL)
			return NULL;

		if ((tb->data = tb->ptr = tb->eptr = malloc(new_size))
		    == NULL) {
			free(tb);
			return NULL;
		}

		tb->eptr += new_size;

		if (info->tbuf != NULL)
			tb->next = info->tbuf;
		else
			tb->next = NULL;

		info->tbuf = tb;
	}
	return t_getstr(info, id, &tb->ptr, NULL);
}

/*
 * Free the buffer allocated by t_getent
 *
 */
void
t_freent(struct tinfo *info)
{
	struct tbuf *tb, *wb;
	_DIAGASSERT(info != NULL);
	free(info->info);
	if (info->up != NULL)
		free(info->up);
	if (info->bc != NULL)
		free(info->bc);
	for (tb = info->tbuf; tb;) {
		wb = tb;
		tb = tb->next;
		free(wb->data);
		free(wb);
	}
	free(info);
}

/*
 * Get the terminal name string from the termcap entry.
 *
 */
int
t_getterm(struct tinfo *info, char **area, size_t *limit)
{
	char *endp;
	size_t count;

	_DIAGASSERT(info != NULL);
	if ((endp = strchr(info->info, ':')) == NULL) {
		errno = EINVAL;
		return -1;
	}


	count = endp - info->info + 1;
	if (area == NULL) {
		_DIAGASSERT(limit != NULL);
		*limit = count;
		return 0;
	} else {
		if ((limit != NULL) && (count > *limit)) {
			errno = E2BIG;
			return -1;
		}

		(void)strlcpy(*area, info->info, count);
		if (limit != NULL)
			*limit -= count;
	}

	return 0;
}
