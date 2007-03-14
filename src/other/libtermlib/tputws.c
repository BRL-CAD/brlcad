/*	$NetBSD: tputws.c,v 1.1 2005/05/15 21:11:13 christos Exp $	*/

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
static char sccsid[] = "@(#)tputs.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: tputws.c,v 1.1 2005/05/15 21:11:13 christos Exp $");
#endif
#endif /* not lint */

#include <assert.h>
#include <wctype.h>
#include <wchar.h>
#include <termcap.h>
#include <stdio.h>
#include <stdlib.h>
#include "termcap_private.h"
#undef ospeed

/* internal functions */
int _tputws_convert(const wchar_t **, int);

/*
 * The following array gives the number of tens of milliseconds per
 * character for each speed as returned by gtty.  Thus since 300
 * baud returns a 7, there are 33.3 milliseconds per char at 300 baud.
 */
extern short	ospeed;
extern char	PC;

int
_tputws_convert(const wchar_t **ptr, int affcnt)
{
	int i = 0;

	_DIAGASSERT(ptr != NULL);
	_DIAGASSERT(*ptr != NULL);

	/*
	 * Convert the number representing the delay.
	 */
	if (iswdigit(**ptr)) {
		do
			i = i * 10 + *(*ptr)++ - '0';
		while (iswdigit(**ptr));
	}
	i *= 10;
	if (*(*ptr) == '.') {
		(*ptr)++;
		if (iswdigit(**ptr))
			i += *(*ptr) - '0';
		/*
		 * Only one digit to the right of the decimal point.
		 */
		while (iswdigit(**ptr))
			(*ptr)++;
	}

	/*
	 * If the delay is followed by a `*', then
	 * multiply by the affected lines count.
	 */
	if (*(*ptr) == '*')
		(*ptr)++, i *= affcnt;

	return i;
}

int
t_putws(struct tinfo *info, const wchar_t *cp, int affcnt,
    void (*outc)(wchar_t, void *), void *args)
{
	int i = 0;
	size_t limit;
	int mspc10;
	char pad[2], *pptr;
	char *pc;

	/* XXX: info may be NULL ? */
	/* cp is handled below */
	_DIAGASSERT(outc != NULL);
	_DIAGASSERT(args != NULL);

	if (info != NULL) {
		/*
		 * if we have info then get the pad char from the
		 * termcap entry if it exists, otherwise use the
		 * default NUL char.
		 */
		pptr = pad;
		limit = sizeof(pad);
		pc = t_getstr(info, "pc", &pptr, &limit);
		if (pc == NULL)
			pad[0] = '\0';
		else
			free(pc);
	}

	if (cp == 0)
		return -1;

	/* scan and convert delay digits (if any) */
	i = _tputws_convert(&cp, affcnt);

	/*
	 * The guts of the string.
	 */
	while (*cp)
		(*outc)(*cp++, args);

	/*
	 * If no delay needed, or output speed is
	 * not comprehensible, then don't try to delay.
	 */
	if (i == 0)
		return 0;
	if (ospeed <= 0 || ospeed >= TMSPC10SIZE)
		return 0;

	/*
	 * Round up by a half a character frame,
	 * and then do the delay.
	 * Too bad there are no user program accessible programmed delays.
	 * Transmitting pad characters slows many
	 * terminals down and also loads the system.
	 */
	mspc10 = __tmspc10[ospeed];
	i += mspc10 / 2;
	for (i /= mspc10; i > 0; i--)
		(*outc)(pad[0], args);

	return 0;
}
