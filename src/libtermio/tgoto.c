/*
 * This code contains changes by
 *      Gunnar Ritter, Freiburg i. Br., Germany, 2002. All rights reserved.
 *
 * The conditions and no-warranty notice below apply to these changes.
 *
 *
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include "common.h"

/* from tgoto.c	5.1 (Berkeley) 6/5/85 */

#include "termcap.h"

#include "bu/str.h"

#define	CTRL(c)	(c & 037)

#define MAXRETURNSIZE 64

#ifdef	__STDC__
#include <string.h>
#endif

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
tgoto(char *CM, int destcol, int destline)
{
	static char result[MAXRETURNSIZE];
	static char added[10];
	char *cp = CM;
	register char *dp = result;
	register int c;
	int oncol = 0;
	register int which = destline;

	if (cp == 0) {
toohard:
		/*
		 * ``We don't do that under BOZO's big top''
		 */
		return ("OOPS");
	}
	added[0] = 0;
	while ((c = *cp++)) {
		if (c != '%') {
			*dp++ = c;
			continue;
		}
		switch (c = *cp++) {

#ifdef CM_N
		case 'n':
			destcol ^= 0140;
			destline ^= 0140;
			goto setwhich;
#endif

		case 'd':
			if (which < 10)
				goto one;
			if (which < 100)
				goto two;
			/* fall into... */
			goto three;
		case '3':
three:
			*dp++ = (which / 100) | '0';
			which %= 100;
			/* fall into... */
			goto two;
		case '2':
two:
			*dp++ = which / 10 | '0';
one:
			*dp++ = which % 10 | '0';
swap:
			oncol = 1 - oncol;
setwhich:
			which = oncol ? destcol : destline;
			continue;

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
			/* fall into... */
			goto casedot;

		case '.':
casedot:
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
			if (which == 0 || which == CTRL('d') || /* which == '\t' || */ which == '\n') {
				if (oncol || UP) /* Assumption: backspace works */
					/*
					 * Loop needed because newline happens
					 * to be the successor of tab.
					 */
					do {
						const char *cstr = oncol ? (BC ? BC : "\b") : UP;
						bu_strlcat(added, cstr, strlen(cstr));
						which++;
					} while (which == '\n');
			}
			*dp++ = which;
			goto swap;

		case 'r':
			oncol = 1;
			goto setwhich;

		case 'i':
			destcol++;
			destline++;
			which++;
			continue;

		case '%':
			*dp++ = c;
			continue;

#ifdef CM_B
		case 'B':
			which = (which/10 << 4) + which%10;
			continue;
#endif

#ifdef CM_D
		case 'D':
			which = which - 2 * (which%16);
			continue;
#endif

		default:
			goto toohard;
		}
	}
	bu_strlcpy(dp, added, MAXRETURNSIZE);
	return (result);
}
