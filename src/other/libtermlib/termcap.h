/*	$NetBSD: termcap.h,v 1.15 2005/02/04 15:52:08 perry Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 *
 *	@(#)termcap.h	8.1 (Berkeley) 6/4/93
 */

#ifndef _TERMCAP_H_
#define _TERMCAP_H_

#include <sys/types.h>

__BEGIN_DECLS
int   tgetent(char *, const char *);
char *tgetstr(const char *, char **);
int   tgetflag(const char *);
int   tgetnum(const char *);
char *tgoto(const char *, int, int);
void  tputs(const char *, int, int (*)(int));

/*
 * New interface
 */
struct tinfo;

int   t_getent(struct tinfo **, const char *);
int   t_getnum(struct tinfo *, const char *);
int   t_getflag(struct tinfo *, const char *);
char *t_getstr(struct tinfo *, const char *, char **, size_t *);
char *t_agetstr(struct tinfo *, const char *);
int   t_getterm(struct tinfo *, char **, size_t *);
int   t_goto(struct tinfo *, const char *, int, int, char *, size_t);
int   t_puts(struct tinfo *, const char *, int,
	     void (*)(char, void *), void *);
void  t_freent (struct tinfo *);
int   t_setinfo(struct tinfo **, const char *);

extern	char PC;
extern	char *BC;
extern	char *UP;
extern	short ospeed;
__END_DECLS

#endif /* !_TERMCAP_H_ */
