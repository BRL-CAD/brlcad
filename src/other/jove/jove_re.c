/*
 *			J O V E _ R E . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 1.1  2004/05/20 14:50:00  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.4  2000/08/24 23:12:24  mike
 *
 * lint, RCSid
 *
 * Revision 11.3  1997/01/03  17:42:48  jra
 * Mods for Irix 6.2
 *
 * Revision 11.2  1995/06/21  03:44:28  gwyn
 * Eliminated trailing blanks.
 * Changed memcpy calls back to bcopy.
 * Use LBSIZE instead of hard-wired 100.
 *
 * Revision 11.1  95/01/04  10:35:20  mike
 * Release_4.4
 *
 * Revision 10.6  94/09/17  04:57:35  butler
 * changed all calls to bcopy to be memcpy instead.  Useful for Solaris 5.2
 *
 * Revision 10.5  1993/12/10  05:37:01  mike
 * ANSI lint
 *
 * Revision 10.4  93/12/10  04:25:35  mike
 * Added FindCursorTag(), bound to M-t.
 *
 * Revision 10.3  93/12/10  03:47:46  mike
 * Fixed the "FindTag" support to be able to search "tags" files
 * that include ANSI function declarations.
 * This means not treating ".*[" as regular expression "magic".
 *
 * Revision 10.2  93/10/26  05:56:57  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:54:03  mike
 * Release_4.0
 *
 * Revision 2.4  91/08/30  19:47:04  mike
 * Stardent ANSI C
 *
 * Revision 2.3  91/08/30  18:46:09  mike
 * Changed from BSD index/rindex nomenclature to SYSV strchr/strrchr.
 *
 * Revision 2.2  91/08/30  17:54:38  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.1  91/08/30  17:49:14  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.0  84/12/26  16:47:42  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:09:27  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_re.c

   Much of this code was taken from /usr/src/cmd/ed.c.  It has been
   modified a lot and features have been added, but the general algorithm
   is the same. */

#include "./jove.h"

#define	CBRA		1	/* \( */
#define	CCHR		2	/* Normal comparisn */
#define	CDOT		4	/* Matches any character */
#define	CCL		6	/* [0-9] matches 0123456789 */
#define	NCCL		8	/* a '^' inside a [...] */
#define	CDOL		10	/* Match at end of the line */
#define	CEOF		12	/* End of line */
#define	CKET		14	/* \) */
#define	CBACK		16	/* \1 \2 \3 ... */
#define CIRC		18	/* At beginning of line */

#define CWORD		20	/* A word character	\w */
#define CNWORD		22	/* Not a word character \W */
#define CBOUND		24	/* At a word boundary   \b */
#define CNBOUND		25	/* Not at a word boundary \B */

#define	STAR	01

char	*loc1,		/* Beginning of found search */
	*loc2,		/* End of found search */
	*locs;		/* Where to start comparison */

#define	NBRA	5
#define NALTS	10

char	*braslist[NBRA],
	*braelist[NBRA],
	*alternates[NALTS];
int	nbra;

void	compile();
void	isearch();
void	find_tag();

#define SEARCHSIZE LBSIZE
#define EOL	-1

char	unmatched[] = "unmatched parens?";
char	toolong[] = "too long";
char	syntax[] = "syntax?";

char	*reptr,		/* Pointer to the expression to search for */
	rhsbuf[LBSIZE/2],
	expbuf[ESIZE+4],
	searchbuf[SEARCHSIZE];
int	dir,
	REpeekc;

#define CharCom(a, b) (IsFlagSet(globflags, CASEIND) ? \
	(Upper(a) == Upper(b)) : (a == b))

void
REerror(str)
char	*str;
{
	complain("RE error: %s", str);
}

nextc()
{
	int	c;

	if (c = REpeekc)
		REpeekc = 0;
	else if (*reptr == 0)
		c = EOL;
	else
		c = *reptr++;
	return c;
}

void
QRepSearch()
{
	replace(1);
}

void
RepSearch()
{
	replace(0);
}

/* Prompt for search and replacement string and do the substitution.
   The point is saved and restored at the end. */

void
replace(query)
{
	MARK	*save = MakeMark(curline, curchar);

	REpeekc = 0;
	reptr = ask(searchbuf[0] ? searchbuf : (char *) 0,
			": %sreplace-search ", query ? "query-" : "");
	strcpy(searchbuf, reptr);
	compile(EOL, IsFlagSet(globflags, MAGIC));
			/* Compile search string complaining
			 * about errors before asking for
			 * the replacement string.
			 */
	reptr = ask("", "Replace \"%s\" with: ", searchbuf);
	compsub(EOL);
		/* Compile replacement string */
	substitute(query);
	ToMark(save);
	DelMark(save);
}

/* Return a buffer location of where the search string in searchbuf is.
   NOTE: this procedure used to munge all over linebuf, but doesn't
   anymore (It uses genbuf instead!). */

BUFLOC *
dosearch(str, direction, magic)
char	*str;
int	direction;
int	magic;		/* 0=> don't process "*[." regular expression chars */
{
	static BUFLOC	ret;
	LINE	*a1;
	int	fromchar;

	dir = direction;
	reptr = str;		/* Read from this string */
	REpeekc = 0;
	compile(EOL, magic);

	a1 = curline;
	fromchar = curchar;
	if (dir < 0)
		fromchar--;

	if (fromchar >= (int)strlen(linebuf) && dir > 0)
		a1 = a1->l_next, fromchar = 0;
	else if (fromchar < 0 && dir < 0)
		a1 = a1->l_prev, fromchar = a1 ? length(a1) : 0;
	for (;;) {
		if (a1 == 0)
			break;
		if (execute(fromchar, a1))
			break;
		a1 = dir > 0 ? a1->l_next : a1->l_prev;
		if (dir < 0 && !a1)	/* don't do the length if == 0 */
			break;
		fromchar = dir > 0 ? 0 : length(a1);
	}

	if (a1) {
		ret.p_line = a1;
		ret.p_char = dir > 0 ? loc2 - genbuf : loc1 - genbuf;
		return &ret;
	}
	return 0;
}

void
setsearch(str)
char	*str;
{
	strcpy(searchbuf, str);
}

void
ForSearch()
{
	search(1);
}

void
RevSearch()
{
	search(0);
}

/* Prompts for a search string.  Complains if the string cannot be found.
   Searches don't wrap line in ed/vi */

void
search(forward)
{
	BUFLOC	*newdot;

	dir = forward ? 1 : -1;
	reptr = ask(searchbuf[0] ? searchbuf : 0, FuncName());

	setsearch(reptr);
	message(searchbuf);
	UpdateMesg();		/* Only message not whole screen! */
	newdot = dosearch(searchbuf, dir, IsFlagSet(globflags, MAGIC));
	if (newdot == 0)
		complain("Search for \"%s\" failed", searchbuf);
	SetMark();
	SetDot(newdot);
}

/* Perform the substitution.  Both the replacement and search strings have
   been compiled already.  This knows how to deal with query replace. */

void
substitute(query)
{
	LINE	*lp;
	int	numdone = 0,
		fromchar = curchar,
		stop = 0;

	dir = 1;	/* Always going forward */
	for (lp = curline; !stop && lp; lp = lp->l_next, fromchar = 0) {
		while (!stop && execute(fromchar, lp)) {
			DotTo(lp, loc2 - genbuf);
			fromchar = curchar;
			if (query) {
 				message("Replace (Type '?' for help)? ");
reswitch:
				switch (Upper((*Getchar)())) {
				case '.':
					stop++;	/* Stop after this */

				case 'Y':	/* To be "user friendly" */
				case ' ':
					break;	/* Do the replace */

				case 'N':	/* To be "user friendly" */
				case '\177':	/* Skip this one */
					continue;

				case 'R':	/* Allow user to move around */
				    {
					BUFFER	*oldbuf = curbuf;
				    	MARK	*m;
				    	char	pushexp[ESIZE + 4];

				    	fromchar = loc1 - genbuf;
				    	m = MakeMark(curline, fromchar);
				    	curchar = loc2 - genbuf;
					message("Recursive edit ...");
				    	bcopy(expbuf, pushexp, ESIZE + 4);
					Recurse();
					bcopy(pushexp, expbuf, ESIZE + 4);
					SetBuf(oldbuf);
				    	ToMark(m);
				    	DelMark(m);
				    	fromchar = curchar;
				    	lp = curline;
					continue;
				    }

				case 'P':		/* Replace all */
					query = 0;
					break;

				case '\n':	/* Stop this replace search */
				case '\r':
					goto done;

				default:
					rbell();
				case '?':
message("<SP> => yes, <DEL> => no, <CR> => stop, 'r' => recursive edit, 'p' => proceed");
					goto reswitch;
				}

			}
			dosub(linebuf);		/* Do the substitution */
			numdone++;
			SetModified(curbuf);
			fromchar = curchar = loc2 - genbuf;
			exp = 1;
			makedirty(curline);
			if (query) {
				message(mesgbuf);	/* No blinking ... */
				redisplay();	/* Show the change */
			}
			if (linebuf[fromchar] == 0)
				break;
		}
	}
done:
	if (numdone == 0)
		complain("None found");
	s_mess("%d substitution%s", numdone, numdone ? "s" : "");
}

/* Compile the substitution string */

void
compsub(seof)
{
	register int	c;
	register char	*p;

	p = rhsbuf;
	for (;;) {
		c = nextc();
		if (c == '\\')
			c = nextc() | 0200;
		if (c == seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			REerror(toolong);
	}
	*p++ = 0;
}

void
dosub(base)
char	*base;
{
	register char	*lp,
			*sp,
			*rp;
	int	c;

	lp = genbuf;
	sp = base;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while (c = *rp++ & 0377) {
		if (c == '&') {
			sp = place(LBSIZE, base, sp, loc1, loc2);
			continue;
		} else if (c & 0200 && (c &= 0177) >= '1' && c < nbra + '1') {
			sp = place(LBSIZE, base, sp, braslist[c - '1'], braelist[c - '1']);
			continue;
		}
		*sp++ = c & 0177;
		if (sp >= &base[LBSIZE])
			REerror(toolong);
	}
	lp = loc2;
	loc2 = genbuf + max(1, sp - base);
			/* At least one character forward after the replace
			   to prevent infinite number of replacements in the
			   same place, e.g. replace "^" with "" */

	while (*sp++ = *lp++)
		if (sp >= &base[LBSIZE])
			len_error(ERROR);
}

char *
place(size, base, sp, l1, l2)
char	*base;
register char	*sp,
		*l1,
		*l2;
{
	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &base[size])
			len_error(ERROR);
	}

	return sp;
}

void
putmatch(which, buf, size)
char	*buf;
{
	*(place(size, buf, buf, braslist[which - 1], braelist[which - 1])) = 0;
}

/* Compile the string, that nextc knows about, into a sequence of
   [TYPE][CHAR] where type says what kind of comparison to make
   and CHAR is the character to compare with.  CHAR is actually
   optional, like in the case of '.' there is no CHAR. */
void
compile(aeof, magic)
int	aeof;
int	magic;		/* 0=> don't process "*[." regular expression chars */
{
	register int	xeof,
			c;
	register char	*ep;
	char	*lastep,
		bracket[NBRA],
		*bracketp,
		**alt = alternates;
	int	cclcnt;

	ep = expbuf;
	*alt++ = ep;
	xeof = aeof;
	bracketp = bracket;

	nbra = 0;
	lastep = 0;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			REerror(toolong);
		c = nextc();
		if (c == xeof) {
			if (bracketp != bracket)
				REerror(unmatched);
			*ep++ = CEOF;
			*alt = 0;
			return;
		}
		if (c != '*')
			lastep = ep;
		if (!magic && strchr("*[.", c))
			goto defchar;
		switch (c) {
		case '\\':
			c = nextc();
			if (!magic && strchr("()0123456789|wWbB", c))
				goto defchar;
			switch (c) {
			case 'w':
				*ep++ = CWORD;
				continue;

			case 'W':
				*ep++ = CNWORD;
				continue;

			case 'b':
				*ep++ = CBOUND;
				continue;

			case 'B':
				*ep++ = CNBOUND;
				continue;

			case '(':
				if (nbra >= NBRA)
					REerror(unmatched);
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;

			case ')':
				if (bracketp <= bracket)
					REerror(unmatched);
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			case '|':	/* Another alternative */
				if (alt >= &alternates[NALTS])
					REerror("Too many alternates");
				*ep++ = CEOF;
				*alt++ = ep;
				continue;

			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
				*ep++ = CBACK;
				*ep++ = c - '1';
				continue;

			}
			goto defchar;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			goto cerror;

		case '*':
			if (lastep == 0 || *lastep == CBRA || *lastep == CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '^':
			if (ep != expbuf && ep[-1] != CEOF)
				goto defchar;
			*ep++ = CIRC;
			continue;

		case '$':
			if (((REpeekc = nextc()) != xeof) && REpeekc != '\\')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c = nextc()) == '^') {
				c = nextc();
				ep[-2] = NCCL;
			}
			do {
				if (c == '\\')
					c = nextc();
				if (c == '\n')
					goto cerror;
				if (c == xeof)
					REerror("Missing ']'");
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					REerror(toolong);
			} while ((c = nextc()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	nbra = 0;
	REerror(syntax);
}

/* Look for a match in `line' starting at `fromchar' from the beginning */

execute(fromchar, line)
LINE	*line;
{
	register char	*p1,
			c;

	for (c = 0; c < NBRA; c++) {
		braslist[c] = 0;
		braelist[c] = 0;
	}
	ignore(getright(line, genbuf));
	locs = p1 = genbuf + fromchar;

	if (dir < 0)
		locs = genbuf;

	/* fast check for first character */

	if (expbuf[0] == CCHR && !alternates[1]) {
		c = expbuf[1];
		do {
			if (!CharCom(*p1, c))
				continue;
			if (advance(p1, expbuf)) {
				loc1 = p1;
				return 1;
			}
		} while (dir > 0 ? *p1++ : p1-- > genbuf);
		return 0;
	}
	/* regular algorithm */
	do {
		register char	**alt = alternates;

		while (*alt)
			if (advance(p1, *alt++)) {
				loc1 = p1;
				return 1;
			}
	} while (dir > 0 ? *p1++ : p1-- > genbuf);

	return 0;
}

/* Does the actual comparison, lp is the buffer line and ep is the
   expression pointer.  Returns 1 if it found a match or 0 if it
   didn't. */

advance(lp, ep)
register char	*ep,
		*lp;
{
	register char	*curlp;
	int	i;

	for (;;) switch (*ep++) {

	case CIRC:	/* If we are not at the beginning
			   of the line there isn't a
			   match. */

		if (lp == genbuf)
			continue;
		return 0;

	case CCHR:	/* Normal comparison, if they're the same, continue */
		if (CharCom(*ep++, *lp++))
			continue;
		return 0;

	case CDOT:	/* Any character matches.  Can only fail if we are
			   at the end of the buffer line already */
		if (*lp++)
			continue;
		return 0;

	case CDOL:	/* Only works if we are at the end of the line */
		if (*lp == 0)
			continue;
		return 0;

	case CEOF:	/* If we get here then we made it! */
		loc2 = lp;
		return 1;

	case CWORD:	/* Is this a word character? */
		if (*lp && isword(*lp)) {
			lp++;
			continue;
		}
		return 0;

	case CNWORD:	/* Is this NOT a word character */
		if (*lp && !isword(*lp)) {
			lp++;
			continue;
		}
		return 0;

	case CBOUND:	/* Is this at a word boundery */
		if (	(*lp == 0) ||		/* end of line */
			(lp == genbuf) ||	/* Beginning of line */
			(isword(*lp) != isword(*(lp - 1)))	)
					/* Characters are of different
					   type i.e one is part of a
					   word and the other isn't. */
			continue;
		return 0;

	case CNBOUND:	/* Is this NOT a word boundery */
		if (	(*lp) &&	/* Not end of the line */
			(isword(*lp) == isword(*(lp - 1)))	)
			continue;
		return 0;

	case CCL:	/* Is this a member of [] */
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return 0;

	case NCCL:	/* Is this NOT a member of [] */
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return 0;

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		if (braelist[i = *ep++] == 0)
			goto badcase;
		if (backref(i, lp)) {
			lp += braelist[i] - braslist[i];
			continue;
		}
		return 0;

	case CWORD | STAR:	/* Any number of word characters */
		curlp = lp;
		while (*lp++ && isword(*(lp - 1)))
			;
		goto star;

	case CNWORD | STAR:
		curlp = lp;
		while (*lp++ && !isword(*(lp - 1)))
			;
		goto star;

	case CBACK | STAR:
		curlp = lp;
		while (backref(i, lp))
			lp += braelist[i] - braslist[i];
		while (lp >= curlp) {
			if (advance(lp, ep))
				return 1;
			lp -= braelist[i] - braslist[i];
		}
		continue;

	case CDOT | STAR:	/* Any number of any character */
		curlp = lp;
		while (*lp++)
			;
		goto star;

	case CCHR | STAR:	/* Any number of the current character */
		curlp = lp;
		while (CharCom(*lp++, *ep))
			;
		ep++;
		goto star;

	case CCL | STAR:
	case NCCL | STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1] == (CCL | STAR)))
			;
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp < locs)
				break;
			if (advance(lp, ep))
				return 1;
		} while (lp > curlp);
		return 0;

	default:
badcase:
		REerror("advance");
	}
	/* NOTREACHED */
}

backref(i, lp)
register int	i;
register char	*lp;
{
	register char	*bp;

	bp = braslist[i];
	while (*bp++ == *lp++)
		if (bp >= braelist[i])
			return 1;
	return 0;
}

cclass(set, c, af)
register char	*set,
		c;
{
	register int	n;
	char	*base;

	if (c == 0)
		return 0;
	n = *set++;
	base = set;
	while (--n) {
		if (*set == '-' && set > base) {
			if (c >= *(set - 1) && c <= *(set + 1))
				return af;
		} else if (*set++ == c)
			return af;
	}
	return !af;
}

#define FOUND	1
#define GOBACK	2

static char	IncBuf[LBSIZE],
		*incp = 0;
int	FirstInc = 0;
jmp_buf	incjmp;

void
IncFSearch()
{
	IncSearch(1);
}

void
IncRSearch()
{
	IncSearch(-1);
}

extern char	searchbuf[];

void
IncSearch(direction)
{
	BUFLOC	bp;

	DOTsave(&bp);
	switch (setjmp(incjmp)) {
	case GOBACK:
		SetDot(&bp);
		break;

	case 0:
		IncBuf[0] = 0;
		incp = IncBuf;
		FirstInc = 1;
		SetMark();
		isearch(direction, 0, 0);
		break;

	case FOUND:
	default:
		break;
	}
	message(IncBuf);	/* Show the search string */
	setsearch(IncBuf);	/* This is now the global string */
}

void
isearch(direction, failing, ctls)
{
	char	c;
	BUFLOC	bp,
		*newdot;
	int	nextfail = failing;	/* Kind of a temp variable */

	DOTsave(&bp);
	for (;;) {
		s_mess("%s%sI-search: %s", failing ? "Failing " : "",
				direction < 0 ? "reverse-" : "", IncBuf);
		c = (*Getchar)();
		switch (c) {
		case '\177':
		case CTL('H'):
			if (!ctls)	/* If we didn't repeated the command */
				if (incp > IncBuf)
					*--incp = 0;
			return;

		case CTL('\\'):
			c = CTL('S');
		case CTL('R'):
		case CTL('S'):
			if (FirstInc && incp) {	/* We have been here before */
				FirstInc = 0;
				strcpy(IncBuf, searchbuf);
				incp = IncBuf + strlen(IncBuf);
				failing = 0;
			}
			/* If we are not failing, OR we are failing BUT we
			   are changing direction, then allow another
			   search */

			if (!failing || ((direction == -1 && c == CTL('S')) ||
					(direction == 1 && c == CTL('R')))) {
				newdot = dosearch(IncBuf, c == CTL('R') ? -1 : 1, 0);
				if (newdot) {
					SetDot(newdot);
					nextfail = 0;
				} else
					nextfail = 1;
				isearch(c == CTL('R') ? -1 : 1, nextfail, 1);
				nextfail = failing;
			} else
				rbell();
			break;

		case CTL('G'):
			longjmp(incjmp, GOBACK);
			/* Easy way out! */

		case CTL('J'):
		case CTL('M'):
			longjmp(incjmp, FOUND);
			/* End gracefully on carriage return or linefeed */

		case CTL('Q'):
		case CTL('^'):
			c = (*Getchar)() | 0200;	/* Tricky! */
		default:
			if (c & 0200)
				c &= 0177;
			else {	/* Check for legality */
				if (c < ' ' && c != '\t') {
					peekc = c;
					longjmp(incjmp, FOUND);
				}
			}
			FirstInc = 0;	/* Not first time anymore! */
			*incp++ = c;	/* Always put in string */
			*incp = 0;
			if (!failing) {
				if (direction > 0 && CharCom(linebuf[curchar], c))
					ForChar();
				else if (direction < 0 && CharCom(linebuf[curchar + strlen(IncBuf) - 1], c))
					;
				else if (!failing) {
					newdot = dosearch(IncBuf, direction, 0);
					if (newdot == 0)
						nextfail++;
					else
						SetDot(newdot);
				}
			}
			isearch(direction, nextfail, 0);
			nextfail = failing;
			/* Reset this to what it was, because if we
			 * are here again, then we must have deleted
			 * back.
			 */
		}
		SetDot(&bp);
	}
}

/* C tags package.  */

/* Find the line with the correct tagname at the beginning of the line.
   then put the data into the struct, and then return a pointer to
   the struct.  0 if an error. */

look_up(sstr, filebuf, name)
register char	*name;
char	*sstr,
	*filebuf;
{
	register int	namlen = strlen(name);
	char	line[LBSIZE];
	register char	*cp;

	if ((io = open("tags", 0)) == -1) {
		message(IOerr("open", "tags"));
		return 0;
	}
	while (getfline(line) != EOF) {
		if (line[0] != *name || strncmp(name, line, namlen) != 0)
			continue;
		if (line[namlen] != '\t')
			continue;
		else {
			char	*endp,
				*newp;

			if (cp = strchr(line, '\t'))
				cp++;
			else
tagerr:				complain("Bad tag file format");
			if (endp = strchr(cp, '\t'))
				*endp = 0;
			else
				goto tagerr;
			strcpy(filebuf, cp);
			if ((newp = strchr(endp + 1, '/')) ||
					(newp = strchr(endp + 1, '?')))
				newp++;
			else
				goto tagerr;
			strcpy(sstr, newp);
			sstr[strlen(sstr) - 1] = 0;
			IOclose();
			return 1;
		}
	}
	IOclose();
	return 0;
}

void
TagError(tag)
char	*tag;
{
	s_mess("tag: %s not found", tag);
}

/*
 * Find_tag searches for the 'tagname' in the file "tags".
 * The "tags" file is in the format generated by "/usr/bin/ctags".
 */

void
find_tag(tagname)
char	*tagname;
{
	char	filebuf[LBSIZE/2],
		sstr[LBSIZE];
	BUFLOC	*bp;

	if (look_up(sstr, filebuf, tagname) == 0) {
		TagError(tagname);
		return;
	}

	if (numwindows() == 1)
		curwind = div_wind(curwind);
	else
		curwind = next_wind(curwind);
	SetBuf(do_find(curwind, filebuf));
	Bof();

	/*
	 * Search forward without treating ".*[" specially.
	 * This is necessary to handle ANSI function declaration style.
	 */
	if ((bp = dosearch(sprint(sstr, tagname), 1, 0)) == 0)
		TagError(tagname);
	else
		SetDot(bp);
}

/* Called from user typing ^X t */

void
FindTag()
{
	char	*tagname;

	tagname = ask((char *) 0, FuncName());
	find_tag(tagname);
}

int
isfuncname(c)
int	c;
{
	if( isalpha(c) || isdigit(c) || c == '_' ) return 1;
	return 0;
}

/* Called from user typing M-t */
int
FindCursorTag()
{
	register int	c;
	int	start;
	int	end;
#define MAX_FUNCTION_NAME_LEN	80
	char	buf[MAX_FUNCTION_NAME_LEN];
	int	len;

	if( !isfuncname(linebuf[curchar]) )  {
		message("Cursor is not over a word, FindCursorTag aborted.");
		return -1;
	}

	/* Find beginning of current word, going backward in line. */
	start = curchar;
	while( start > 0 && (c = linebuf[start-1]) != 0 && isfuncname(c) )
		start--;

	/* Find ending of current word */
	end = curchar;
	while( (c = linebuf[end]) != 0 && isfuncname(c) )
		end++;

	/* Snatch out that string, and null terminate */
	len = end - start;
	if( len > MAX_FUNCTION_NAME_LEN-1 )  len = MAX_FUNCTION_NAME_LEN-1;
	bcopy( linebuf+start, buf, len );
	buf[len] = '\0';

	find_tag(buf);
	return 0;
}
