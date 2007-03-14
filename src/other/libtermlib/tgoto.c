/*	$NetBSD: tgoto.c,v 1.25 2006/08/27 08:47:40 christos Exp $	*/

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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)tgoto.c	8.1 (Berkeley) 6/4/93";
#else
/* __RCSID("$NetBSD: tgoto.c,v 1.25 2006/08/27 08:47:40 christos Exp $"); */
#endif
#endif /* not lint */

#include <assert.h>
#include <errno.h>
#include <termcap.h>
#include <termcap_private.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	CTRL(c)	((c) & 037)

#define MAXRETURNSIZE 128

char	*UP;
char	*BC;

/*
 * Routine to perform cursor addressing.
 * CM is a string containing printf type escapes to allow
 * cursor addressing.  We start out ready to print the destination
 * line, and switch each time we print row or column.
 * The following escapes are defined for substituting row/column:
 *
 *	%d	as in printf
 *	%2	like %2d
 *	%3	like %3d
 *	%.	gives %c hacking special case characters
 *	%+x	like %c but adding x first
 *
 *	The codes below affect the state but don't use up a value.
 *
 *	%>xy	if value > x add y
 *	%r	reverses row/column
 *	%i	increments row/column (for one origin indexing)
 *	%%	gives %
 *	%B	BCD (2 decimal digits encoded in one byte)
 *	%D	Delta Data (backwards bcd)
 *
 * all other characters are ``self-inserting''.
 */
char *
tgoto(const char *CM, int destcol, int destline)
{
	static char result[MAXRETURNSIZE];

	(void)t_goto(NULL, CM, destcol, destline, result, sizeof(result));
	return result;
}

/*
 * New interface.  Functionally the same as tgoto but uses the tinfo struct
 * to set UP and BC.  The arg buffer is filled with the result string, limit
 * defines the maximum number of chars allowed in buffer.  The function
 * returns 0 on success, -1 otherwise, the result string contains an error
 * string on failure.
 */
int
t_goto(struct tinfo *info, const char *CM, int destcol, int destline,
    char *buffer, size_t limit)
{
	char added[32];
	const char *cp = CM;
	char *dp = buffer;
	int c;
	int oncol = 0;
	int which = destline;
	char *buf_lim = buffer + limit;
	char dig_buf[64];
	char *ap = added;
	char *eap = &added[sizeof(added) / sizeof(added[0])];
	int k;

	/* CM is checked below */
	_DIAGASSERT(buffer != NULL);

	if (info != NULL) {
		if (!UP)
			UP = info->up;
		if (!BC)
			BC = info->bc;
	}

	if (cp == 0) {
		(void)strlcpy(buffer, "no fmt", limit);
		errno = EINVAL;
		return -1;
	}
	added[0] = '\0';
	while ((c = *cp++) != '\0') {
		if (c != '%' || ((c = *cp++) == '%')) {
			*dp++ = c;
			if (dp >= buf_lim) {
				(void)strlcpy(buffer, "no space copying %",
				    limit);
				errno = E2BIG;
				return -1;
			}
			continue;
		}
		switch (c) {

#ifdef CM_N
		case 'n':
			destcol ^= 0140;
			destline ^= 0140;
			/* flip oncol here so it doesn't actually change */
			oncol = 1 - oncol;
			break;
#endif

		case '3':
		case '2':
		case 'd':
			/* Generate digits into temp buffer in reverse order */
			k = 0;
			do
				dig_buf[k++] = which % 10 | '0';
			while ((which /= 10) != 0);

			if (c != 'd') {
				c -= '0';
				if (k > c) {
					(void)snprintf(buffer, limit,
					    "digit buf overflow %d %d",
					    k, c);
					errno = EINVAL;
					return -1;
				}
				while (k < c)
					dig_buf[k++] = '0';
			}

			if (dp + k >= buf_lim) {
				(void)strlcpy(buffer, "digit buf copy", limit);
				errno = E2BIG;
				return -1;
			}
			/* then unwind into callers buffer */
			do
				*dp++ = dig_buf[--k];
			while (k);
			break;

#ifdef CM_GT
		case '>':
			if (which > *cp++)
				which += *cp++;
			else
				cp++;
			continue;
#endif

		case '+':
			which += *cp++;
			/* FALLTHROUGH */

		case '.':
			/*
			 * This code is worth scratching your head at for a
			 * while.  The idea is that various weird things can
			 * happen to nulls, EOT's, tabs, and newlines by the
			 * tty driver, arpanet, and so on, so we don't send
			 * them if we can help it.
			 *
			 * Tab is taken out to get Ann Arbors to work, otherwise
			 * when they go to column 9 we increment which is wrong
			 * because bcd isn't continuous.  We should take out
			 * the rest too, or run the thing through more than
			 * once until it doesn't make any of these, but that
			 * would make termlib (and hence pdp-11 ex) bigger,
			 * and also somewhat slower.  This requires all
			 * programs which use termlib to stty tabs so they
			 * don't get expanded.  They should do this anyway
			 * because some terminals use ^I for other things,
			 * like nondestructive space.
			 */
			if (which == 0 || which == CTRL('d') || 
			    /* which == '\t' || */ which == '\n') {
				if (oncol || UP) { /* Assumption: backspace works */
					char *add = oncol ? (BC ? BC : "\b") : UP;

					/*
					 * Loop needed because newline happens
					 * to be the successor of tab.
					 */
					do {
						char *as = add;

						while ((*ap++ = *as++) != '\0')
							if (ap >= eap) {
								(void)strlcpy(
								    buffer,
								    "add ovfl",
								    limit);
								errno = E2BIG;
								return -1;
							}
						which++;
					} while (which == '\n');
				}
			}
			*dp++ = which;
			if (dp >= buf_lim) {
				(void)strlcpy(buffer, "dot copy", limit);
				errno = E2BIG;
				return -1;
			}
			break;

		case 'r':
			oncol = 0;
			break;

		case 'i':
			destcol++;
			destline++;
			which++;
			continue;

#ifdef CM_B
		case 'B':
			which = (which / 10 << 4) + which % 10;
			continue;
#endif

#ifdef CM_D
		case 'D':
			which = which - 2 * (which % 16);
			continue;
#endif

		default:
			(void)snprintf(buffer, limit, "bad format `%c'", c);
			errno = EINVAL;
			return -1;
		}

		/* flip to other number... */
		oncol = 1 - oncol;
		which = oncol ? destcol : destline;
	}
	if (dp + (ap - added) >= buf_lim) {
		(void)strlcpy(buffer, "big added", limit);
		errno = E2BIG;
		return -1;
	}

	*ap = '\0';
	for (ap = added; (*dp++ = *ap++) != '\0';)
		continue;

	return 0;
}
