/*
 *			J O V E _ I N S E R T . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.2  1995/06/21  03:41:33  gwyn
 * Eliminated trailing blanks.
 *
 * Revision 11.1  95/01/04  10:35:15  mike
 * Release_4.4
 *
 * Revision 10.2  93/10/26  03:48:09  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:53:59  mike
 * Release_4.0
 *
 * Revision 2.1  91/08/30  17:54:34  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.0  84/12/26  16:46:25  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:08:22  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5/25/83

   Insert routines: the routine to Yank from the kill buffer
   and to insert lines, and characters into the buffer.  */

#include "./jove.h"

void		c_indent();

/* Make a newline after `after' or course in `buf' */

LINE *
listput(buf, after)
BUFFER	*buf;
LINE	*after;
{
	LINE	*newline = nbufline();

	if (after == 0) {	/* First line in this list */
		buf->b_zero = buf->b_dol = buf->b_dot = newline;
		newline->l_next = newline->l_prev = 0;
		return newline;
	}
	newline->l_prev = after;
	newline->l_next = after->l_next;
	after->l_next = newline;
	if (newline->l_next)
		newline->l_next->l_prev = newline;
	else
		if (buf)
			buf->b_dol = newline;
	return newline;
}

/* Global variables aren't that bad.  There is a point where one
 * can go too far in trying to eliminate global variables on a
 * principle.  After all this isn't LISP or anything like that.
 */

void
LineInsert()
{
	register int	num = exp;
	char	newline[LBSIZE];
	LINE	*newdot,
		*olddot = curline;
	int	oldchar = curchar;
	int	atbegin = (!firstp(curline) && bolp());

	exp = 1;
	if (atbegin)	/* This is mostly to make redisplay seem smart
			   but it also decreases the amount of copying
			   from one buffer to another */
		BackChar();
	strcpy(newline, &linebuf[curchar]);

	newdot = curline;
	while (num--) {
		newdot = listput(curbuf, newdot);	/* Put after newdot */
		newdot->l_dline = putline("") | DIRTY;
	}
	linebuf[curchar] = '\0';	/* Shorten this line */
	SavLine(curline, linebuf);
	makedirty(curline);
	curline = newdot;
	curchar = 0;
	strcpy(linebuf, newline);
	makedirty(curline);
	if (atbegin)
		ForChar();
	SetModified(curbuf);
	IFixMarks(olddot, oldchar, curline, curchar);
}

void
LineAI()
{
	LineInsert();
	whitesp(getline(curline->l_prev->l_dline, genbuf), linebuf);
}

void
whitesp(from, to)
register char	*from,
		*to;
{
	char	c;
	int	oldchar = curchar;

	while ((c = *from++) && (c == ' ' || c == '\t'))
		insert(c, to, curchar++, 1, LBSIZE);
	SetModified(curbuf);
	IFixMarks(curline, oldchar, curline, curchar);
}

void
len_error(flag)
{
	char	*mesg = "line too long";

	(flag == COMPLAIN) ? complain(mesg) : error(mesg);
}

/* Insert num number of c's at offset atchar in a linebuf of LBSIZE */

void
insert(c, buf, atchar, num, max)
char	c, *buf;
{
	register char	*pp, *pp1;
	register int	len = atchar + strlen(&buf[atchar]);
	int	numchars;	/* Number of characters to copy forward */

	if (len + num >= max)
		len_error(COMPLAIN);
	pp = &buf[len];
	pp1 = &buf[len + num];
	numchars = len - atchar;
	while (numchars-- >= 0)
		*pp1-- = *pp--;
	pp = &buf[atchar];
	while (num--)
		*pp++ = c;
}

/*
 * Three situations:
 *
 * (1) We are at the end of the line in which case we just insert the char.
 * (2) We are in the middle of the line on a normal character in which case
 *     we replace that character with 'c'.
 * (3) We are in the middle of a tab.  Here we insert the character unless
 *     we are at the end of the tab stop in which case cover the tab with
 *     'c'.
 */

void
OverWrite()
{
	int	i, num;

	for (i = 0, num = exp, exp = 1; i < num; i++) {
		if (!eolp())
			DelNChar();
		Insert(LastKeyStruck);
	}
}

void
SelfInsert()
{
	Insert(LastKeyStruck);
}

void
Insert(c)
{
	SetModified(curbuf);
	makedirty(curline);
	insert(c, linebuf, curchar, exp, LBSIZE);
	IFixMarks(curline, curchar, curline, curchar + exp);
	curchar += exp;
}

/*
 * Tab in to the right place for c mode
 */

void
CTab()
{
	if (strlen(linebuf) == 0)
		c_indent();
	Insert('\t');
}

void
QuotChar()
{
	int	c;

	if (c = (*Getchar)())
		Insert(c);
}

blankp(line)
char	*line;
{
	register char	c;

	while (c = *line++)
		if (c != ' ' && c != '\t')
			return 0;
	return 1;
}

/* Insert the paren.  If in C mode and c is a '}' then insert the
 * '}' in the "right" place for C indentation; that is indented
 * the same amount as the matching '{' is indented.
 */

void
DoParen()
{
	BUFLOC	*bp;
	int	nx,
		c = LastKeyStruck;

	if (c != ')' && c != '}')
		complain((char *) 0);
	if (c == '}' && IsFlagSet(globflags, CMODE))
		if (blankp(linebuf)) {
			Bol();		/* Beginning of line and */
			DelWtSpace();	/* Delete white space */
			c_indent();	/* insert the white space */
		}

	if(IsFlagSet(globflags, OVERWRITE))
		if (!eolp())
			DelNChar();
	Insert(c);
	if (IsFlagSet(globflags, MATCHING)) {
		redisplay();
		GotoDot();
		BackChar();
		if (NotInQuotes(linebuf, curchar)) {
			if (bp = m_paren(c, curwind->w_top)) {
				nx = in_window(curwind, bp->p_line);
				if (nx != -1) {
					BUFLOC	b;

					DOTsave(&b);
					SetDot(bp);
					redisplay();
					sleep(1);
					SetDot(&b);
				}
			}
		}
		ForChar();
	}
}

void
c_indent()
{
	BUFLOC	*bp;

	bp = m_paren('}', curbuf->b_zero);
	if (!bp)
		return;
	ignore(getright(bp->p_line, genbuf));
	whitesp(genbuf, linebuf);
}

void
AtMargin()
{
	int	open_kludge = 0;

	exp = 1;

	if (curline->l_next == 0) {
		OpenLine();
		open_kludge++;
	}
	DoJustify(curline, curline->l_next, 1);
	if (open_kludge)
		DelNChar();
}

TwoBlank()
{
	return (curline->l_next &&
			(getline(curline->l_next->l_dline,
			genbuf)[0] == '\0') && curline->l_next->l_next &&
			(getline(curline->l_next->l_next->l_dline,
			genbuf)[0] == '\0'));
}

void
Newline()
{
	/* If there is more than 2 blank lines in a row then don't make
	   a newline, just move down one. */

	if (exp == 1 && eolp() && TwoBlank()) {
		SetLine(curline->l_next);
		return;
	}
	LineInsert();
}

void
TextInsert()
{
	Insert(LastKeyStruck);
	if ((curchar > RMargin) && LastKeyStruck != ' ')
		AtMargin();
}

void
ins_str(str)
register char	*str;
{
	register char	c;
	int	pos = curchar;

	while (c = *str++) {
		if (c == '\n')
			continue;
		insert(c, linebuf, curchar++, 1, LBSIZE);
	}
	IFixMarks(curline, pos, curline, curchar);
	makedirty(curline);
}

void
linecopy(onto, atchar, from)
register char	*onto, *from;
register int	atchar;
{
	char	*base = onto;

	onto += atchar;

	while (*onto = *from++)
		if (onto++ >= &base[LBSIZE - 2])
			len_error(ERROR);
}

void
OpenLine()
{
	int	num = exp;

	LineInsert();	/* Open the lines... */
	DoTimes(BackChar, num);
}

BUFLOC *
DoYank(fline, fchar, tline, tchar, atline, atchar, whatbuf)
LINE	*fline, *tline, *atline;
BUFFER	*whatbuf;
{
	register LINE	*newline;
	static BUFLOC	bp;
	char	save[LBSIZE],
		buf[LBSIZE];
	LINE	*startline = atline;
	int	startchar = atchar;

	SetModified(curbuf);
	lsave();
	ignore(getright(atline, genbuf));
	strcpy(save, &genbuf[atchar]);

	ignore(getright(fline, buf));
	if (fline == tline)
		buf[tchar] = '\0';

	linecopy(genbuf, atchar, &buf[fchar]);
	atline->l_dline = putline(genbuf);
	makedirty(atline);

	fline = fline->l_next;
	while (fline != tline->l_next) {
		newline = listput(whatbuf, atline);
		newline->l_dline = fline->l_dline;
		makedirty(newline);
		fline = fline->l_next;
		atline = newline;
		atchar = 0;
	}

	ignore(getline(atline->l_dline, genbuf));
	atchar += tchar;
	linecopy(genbuf, atchar, save);
	atline->l_dline = putline(genbuf);
	makedirty(atline);
	IFixMarks(startline, startchar, atline, atchar);
	bp.p_line = atline;
	bp.p_char = atchar;
	this_cmd = YANKCMD;
	getDOT();			/* Whatever used to be in linebuf */
	return &bp;
}

/* This is an attempt to reduce the amount of memory taken up by each line.
   Without this each malloc of a line uses sizeof (LINE) + sizeof(HEADER)
   where LINE is 3 words and HEADER is 1 word.
   This is going to allocate memory in chucks of CHUNKSIZE * sizeof (LINE)
   and divide each chuck into LINES.  A LINE is free in a chunk when its
   line->l_dline == 0, so linefree sets dline to 0. */

#define CHUNKSIZE	40

struct chunk {
	int	c_nlines;	/* Number of lines in this chunk
				   (so they don't all have to be CHUNKSIZE long) */
	LINE	*c_block;	/* Chunk of memory */
	struct chunk	*c_nextfree;	/* Next chunk of lines */
};

struct chunk	*fchunk = 0;

LINE	*ffline = 0;	/* First free line */

void
freeline(line)
register LINE	*line;
{
	line->l_dline = 0;
	line->l_next = ffline;
	if (ffline)
		ffline->l_prev = line;
	line->l_prev = 0;
	ffline = line;
}

void
lfreelist(first)
register LINE	*first;
{
	if (first)
		lfreereg(first, lastline(first));
}

/* Append region from line1 to line2 onto the free list of lines */

void
lfreereg(line1, line2)
register LINE	*line1,
		*line2;
{
	register LINE	*next,
			*last = line2->l_next;

	while (line1 != last) {
		next = line1->l_next;
		freeline(line1);
		line1 = next;
	}
}

newchunk()
{
	register LINE	*newline;
	register int	i;
	struct chunk	*f;
	int	nlines = CHUNKSIZE;

	f = (struct chunk *) malloc((unsigned) sizeof (struct chunk));
	if (f == 0)
		return 0;
	while (nlines > 0) {
		f->c_block = (LINE *) malloc((unsigned) (sizeof (LINE) * nlines));
		if (f->c_block != 0)
			break;
		nlines /= 2;
	}
	if (nlines <= 0)
		return 0;

	f->c_nlines = nlines;
	for (i = 0, newline = f->c_block; i < nlines; newline++, i++)
		freeline(newline);
	f->c_nextfree = fchunk;
	fchunk = f;
	return 1;
}

LINE *
nbufline()
{
	register LINE	*newline;

	if (ffline == 0) {	/* No free list */
		if (newchunk() == 0)
			complain("out of lines");
	}
	newline = ffline;
	ffline = ffline->l_next;
	if (ffline)
		ffline->l_prev = 0;
	return newline;
}

/* Remove the free lines in chunk c from the free list because they are
   no longer free. */

void
remfreelines(c)
register struct chunk	*c;
{
	register LINE	*lp;
	register int	i;

	for (lp = c->c_block, i = 0; i < c->c_nlines; i++, lp++) {
		if (lp->l_prev)
			lp->l_prev->l_next = lp->l_next;
		else
			ffline = lp->l_next;
		if (lp->l_next)
			lp->l_next->l_prev = lp->l_prev;
	}
}

/* This is/{should be} used to garbage collect the chunks of lines when
   malloc fails and we are NOT looking for a new buffer line.  This goes
   through each chunk, and if every line in a given chunk is not allocated,
   the entire chunk is `free'd by "free()"  */

void
GCchunks()
{
	register struct chunk	*cp;
	struct chunk	*prev = 0,
			*next = 0;
	register int	i;
	register LINE	*newline;

	message("Garbage collecting ...");
	UpdateMesg();
	sleep(1);
 	for (cp = fchunk; cp; cp = next) {
		for (i = 0, newline = cp->c_block; i < cp->c_nlines; newline++, i++)
			if (newline->l_dline != 0)
				break;

 		next = cp->c_nextfree;

		if (i == cp->c_nlines) {		/* Unlink it!!! */
			if (prev)
				prev->c_nextfree = cp->c_nextfree;
			else
				fchunk = cp->c_nextfree;
			remfreelines(cp);
			free((char *) cp->c_block);
			free((char *) cp);
		} else
			prev = cp;
	}
}
