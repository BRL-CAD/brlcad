/*	$NetBSD: regex2.h,v 1.5 1995/02/27 13:29:40 cgd Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)regex2.h	8.4 (Berkeley) 3/20/94
 */

/*
 * First, the stuff that ends up in the outside-world include file
 = typedef off_t regoff_t;
 = typedef struct {
 = 	int re_magic;
 = 	size_t re_nsub;		// number of parenthesized subexpressions
 = 	const char *re_endp;	// end pointer for REG_PEND
 = 	struct re_guts *re_g;	// none of your business :-)
 = } regex_t;
 = typedef struct {
 = 	regoff_t rm_so;		// start of match
 = 	regoff_t rm_eo;		// end of match
 = } regmatch_t;
 */
/*
 * internals of regex_t
 */
#define	MAGIC1	((('r'^0200)<<8) | 'e')

/*
 * The internal representation is a *strip*, a sequence of
 * operators ending with an endmarker.  (Some terminology etc. is a
 * historical relic of earlier versions which used multiple strips.)
 * Certain oddities in the representation are there to permit running
 * the machinery backwards; in particular, any deviation from sequential
 * flow must be marked at both its source and its destination.  Some
 * fine points:
 *
 * - OPLUS_ and O_PLUS are *inside* the loop they create.
 * - OQUEST_ and O_QUEST are *outside* the bypass they create.
 * - OCH_ and O_CH are *outside* the multi-way branch they create, while
 *   OOR1 and OOR2 are respectively the end and the beginning of one of
 *   the branches.  Note that there is an implicit OOR2 following OCH_
 *   and an implicit OOR1 preceding O_CH.
 *
 * In state representations, an operator's bit is on to signify a state
 * immediately *preceding* "execution" of that operator.
 */
typedef unsigned long sop;	/* strip operator */
typedef long sopno;
#define	OPRMASK	0xf8000000
#define	OPDMASK	0x07ffffff
#define	OPSHIFT	((unsigned)27)
#define	OP(n)	((n)&OPRMASK)
#define	OPND(n)	((n)&OPDMASK)
#define	SOP(op, opnd)	((op)|(opnd))
/* operators			   meaning	operand			*/
/*						(back, fwd are offsets)	*/
#define	OEND	0x08000000 /* (1L<<OPSHIFT)	_* endmarker	-			*/
#define	OCHAR	0x10000000 /* (2L<<OPSHIFT)	_* character	unsigned char		*/
#define	OBOL	0x18000000 /* (3L<<OPSHIFT)	_* left anchor	-			*/
#define	OEOL	0x20000000 /* (4L<<OPSHIFT)	_* right anchor	-			*/
#define	OANY	0x28000000 /* (5L<<OPSHIFT)	_* .		-			*/
#define	OANYOF	0x30000000 /* (6L<<OPSHIFT)	_* [...]	set number		*/
#define	OBACK_	0x38000000 /* (7L<<OPSHIFT)	_* begin \d	paren number		*/
#define	O_BACK	0x40000000 /* (8L<<OPSHIFT)	_* end \d	paren number		*/
#define	OPLUS_	0x48000000 /* (9L<<OPSHIFT)	_* + prefix	fwd to suffix		*/
#define	O_PLUS	0x50000000 /* (10L<<OPSHIFT)	_* + suffix	back to prefix		*/
#define	OQUEST_	0x58000000 /* (11L<<OPSHIFT)	_* ? prefix	fwd to suffix		*/
#define	O_QUEST	0x60000000 /* (12L<<OPSHIFT)	_* ? suffix	back to prefix		*/
#define	OLPAREN	0x68000000 /* (13L<<OPSHIFT)	_* (		fwd to )		*/
#define	ORPAREN	0x70000000 /* (14L<<OPSHIFT)	_* )		back to (		*/
#define	OCH_	0x78000000 /* (15L<<OPSHIFT)	_* begin choice	fwd to OOR2		*/
#define	OOR1	0x80000000 /* (16L<<OPSHIFT)	_* | pt. 1	back to OOR1 or OCH_	*/
#define	OOR2	0x88000000 /* (17L<<OPSHIFT)	_* | pt. 2	fwd to OOR2 or O_CH	*/
#define	O_CH	0x90000000 /* (18L<<OPSHIFT)	_* end choice	back to OOR1		*/
#define	OBOW	0x98000000 /* (19L<<OPSHIFT)	_* begin word	-			*/
#define	OEOW	0xa0000000 /* (20L<<OPSHIFT)	_* end word	-			*/




/*
 * Structure for [] character-set representation.  Character sets are
 * done as bit vectors, grouped 8 to a byte vector for compactness.
 * The individual set therefore has both a pointer to the byte vector
 * and a mask to pick out the relevant bit of each byte.  A hash code
 * simplifies testing whether two sets could be identical.
 *
 * This will get trickier for multicharacter collating elements.  As
 * preliminary hooks for dealing with such things, we also carry along
 * a string of multi-character elements, and decide the size of the
 * vectors at run time.
 */
typedef struct {
	uch *ptr;		/* -> uch [csetsize] */
	uch mask;		/* bit within array */
	uch hash;		/* hash code */
	size_t smultis;
	char *multis;		/* -> char[smulti]  ab\0cd\0ef\0\0 */
} cset;
/* note that CHadd and CHsub are unsafe, and CHIN doesn't yield 0/1 */
#define	CHadd(cs, c)	((cs)->ptr[(uch)(c)] |= (cs)->mask, (cs)->hash += (c))
#define	CHsub(cs, c)	((cs)->ptr[(uch)(c)] &= ~(cs)->mask, (cs)->hash -= (c))
#define	CHIN(cs, c)	((cs)->ptr[(uch)(c)] & (cs)->mask)
#define	MCadd(p, cs, cp)	mcadd(p, cs, cp)	/* regcomp() internal fns */
#define	MCsub(p, cs, cp)	mcsub(p, cs, cp)
#define	MCin(p, cs, cp)	mcin(p, cs, cp)

/* stuff for character categories */
typedef unsigned char cat_t;

/*
 * main compiled-expression structure
 */
struct re_guts {
	int magic;
#		define	MAGIC2	((('R'^0200)<<8)|'E')
	sop *strip;		/* malloced area for strip */
	int csetsize;		/* number of bits in a cset vector */
	int ncsets;		/* number of csets in use */
	cset *sets;		/* -> cset [ncsets] */
	uch *setbits;		/* -> uch[csetsize][ncsets/CHAR_BIT] */
	int cflags;		/* copy of regcomp() cflags argument */
	sopno nstates;		/* = number of sops */
	sopno firststate;	/* the initial OEND (normally 0) */
	sopno laststate;	/* the final OEND */
	int iflags;		/* internal flags */
#		define	USEBOL	01	/* used ^ */
#		define	USEEOL	02	/* used $ */
#		define	BAD	04	/* something wrong */
	int nbol;		/* number of ^ used */
	int neol;		/* number of $ used */
	int ncategories;	/* how many character categories */
	cat_t *categories;	/* ->catspace[-CHAR_MIN] */
	char *must;		/* match must contain this string */
	int mlen;		/* length of must */
	size_t nsub;		/* copy of re_nsub */
	int backrefs;		/* does it use back references? */
	sopno nplus;		/* how deep does it nest +s? */
	/* catspace must be last */
	cat_t catspace[1];	/* actually [NC] */
};

/* misc utilities */
#define	OUT	(CHAR_MAX+1)	/* a non-character value */
#define	ISWORD(c)	(isalnum(c) || (c) == '_')
