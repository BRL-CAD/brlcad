/*
 *			J O V E _ D E L E T E . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 1.1  2004/05/20 14:49:59  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.4  2000/08/24 23:12:22  mike
 *
 * lint, RCSid
 *
 * Revision 11.3  1997/01/03  17:42:17  jra
 * Mods for Irix 6.2
 *
 * Revision 11.2  1995/06/21  03:39:34  gwyn
 * Eliminated trailing blanks.
 *
 * Revision 11.1  95/01/04  10:35:10  mike
 * Release_4.4
 *
 * Revision 10.2  93/10/26  03:41:22  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:53:55  mike
 * Release_4.0
 *
 * Revision 2.1  91/08/30  17:54:30  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.0  84/12/26  16:45:23  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:07:27  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_delete.c

   Routines to perform deletion.  The word delete and line delete
   use the same routine as "delete-to-killbuffer".  Character delete
   uses some of the same stuff, but doesn't save the text in the
   kill-buffer.  */

#include "./jove.h"

int diffnum = 0;
void	fixorder();

/* Returns whether line1 char1 is before line2 char2 in the buffer.
   Also sets `diffnum' to the number of lines between the two for
   redisplay purposes. */

int
inorder(line1, char1, line2, char2)
LINE	*line1,
	*line2;
{
	register int	i = 1;
	register LINE	*lp = curbuf->b_zero;
	int	f1,
		f2;

	diffnum = f1 = f2 = 0;

	if (line1 == line2)
		return char1 < char2;

	while (lp && (!f1 || !f2)) {
		if (lp == line1)
			f1 = i;
		else if (lp == line2)
			f2 = i;
		lp = lp->l_next;
		++i;
	}
	diffnum = abs(f1 - f2);
	if (f1 == 0 || f2 == 0)
		return -1;
	return f1 < f2;
}

/* Use this when you just want to look at a line without
   changing anything.  It'll return linebuf if it is the
   current line of the current buffer (no copying). */

char *
getcptr(line, buf)
LINE	*line;
char	*buf;
{
	if (line == curline)
		return linebuf;
	else {
		ignore(getline(line->l_dline, buf));
		return buf;
	}
}

/* Use this when ggetcptr is not appropiate */

char *
getright(line, buf)
LINE	*line;
char	*buf;
{
	if (line == curline) {
		if (buf != linebuf)
			strcpy(buf, linebuf);
	} else
		ignore(getline(line->l_dline, buf));
	return buf;
}

/* Assumes that either line1 or line2 is actual the current line, so it can
   put its result into linebuf. */

void
patchup(line1, char1, line2, char2)
register LINE	*line1,
		*line2;
{
	char	line2buf[LBSIZE];
	register char	*twoptr;

	if (line1 != line2)
		KludgeWindows(line1, line2);
	SetModified(curbuf);
	if (line2 == curline) {		/* So we don't need to getline it */
		if (line1 == curline)
			twoptr = linebuf;
		else {
			strcpy(line2buf, linebuf);
			twoptr = line2buf;
		}
		SetLine(line1);		/* Line1 now in linebuf */
	} else
		twoptr = getright(line2, line2buf);

	curchar = char1;
	linecopy(linebuf, curchar, twoptr + char2);

	DFixMarks(line1, char1, line2, char2);
	makedirty(curline);
}

/* Deletes the region by unlinking the lines in the middle,
   and patching things up.  The unlinked lines are still in
   order.  */

LINE *
reg_delete(line1, char1, line2, char2)
LINE	*line1, *line2;
{
	LINE	*retline;

	if ((line1 == line2 && char1 == char2) || line2 == 0)
		complain((char *) 0);
	fixorder(&line1, &char1, &line2, &char2);

	retline = nbufline();	/* New buffer line */

	ignore(getright(line1, genbuf));
	if (line1 == line2)
		genbuf[char2] = '\0';

	retline->l_prev = 0;
	retline->l_dline = putline(&genbuf[char1]);
	patchup(line1, char1, line2, char2);

	if (line1 == line2)
		retline->l_next = 0;
	else {
		retline->l_next = line1->l_next;
		ignore(getright(line2, genbuf));
		genbuf[char2] = '\0';
		line2->l_dline = putline(genbuf);	/* Shorten this line */
	}

	if (line1 != line2) {
		line1->l_next = line2->l_next;
		if (line1->l_next)
			line1->l_next->l_prev = line1;
		else
			curbuf->b_dol = line1;
		line2->l_next = 0;
	}

	return retline;
}

void
lremove(line1, line2)
register LINE	*line1,
		*line2;
{
	LINE	*next = line1->l_next;

	if (line1 == line2)
		return;
	line1->l_next = line2->l_next;
	if (line1->l_next)
		line1->l_next->l_prev = line1;
	else
		curbuf->b_dol = line1;
	lfreereg(next, line2);	/* Put region at end of free line list */
}

/* Delete character forward */

void
DelNChar()
{
	del_char(1);
}

/* Delete character backward */

void
DelPChar()
{
	del_char(0);
}

/* Delete some characters.  If deleting `forward' then call for_char
 * to the final position otherwise call back_char.  Then delete the
 * region between the two with patchup().
 */

void
del_char(forward)
{
	BUFLOC	newdot,
		pt;

	if (forward) {
		DOTsave(&newdot);
		ForChar();
		DOTsave(&pt);
	} else {
		DOTsave(&pt);
		BackChar();
		DOTsave(&newdot);	/* New dot will be back here */
	}

	/* Patchup puts the result in linebuf, which is fine with us */
	patchup(newdot.p_line, newdot.p_char, pt.p_line, pt.p_char);
	lremove(newdot.p_line, pt.p_line);
}

/* This kills a region and puts it on the kill-ring.  If the last command
   was one of the kill commands, the region is appended (prepended if
   backwards) to the last entry. */


void
reg_kill(line1, char1, line2, char2, backwards)
LINE	*line1, *line2;
{
	LINE *nl, *ol;
	char buf[LBSIZE];

	fixorder(&line1, &char1, &line2, &char2);
	DotTo(line1, char1);

	nl = reg_delete(line1, char1, line2, char2);

	if (last_cmd != KILLCMD) {
		killptr = ((killptr + 1) % NUMKILLS);
		lfreelist(killbuf[killptr]);
		killbuf[killptr] = nl;
	} else {
		/* Last cmd was a KILLCMD so merge the kills */
		if (backwards) {
			LINE *fl, *lastln;

			fl = killbuf[killptr];
			lastln = lastline(nl);
			ignore(getright(lastln, buf));
			ignore(getright(fl, genbuf));
			linecopy(buf, strlen(buf), genbuf);
			lastln->l_dline = putline(buf);
			killbuf[killptr] = nl;
			lastln->l_next = fl->l_next;
		} else {
			ol = lastline(killbuf[killptr]);
			ignore(getright(ol, buf));
			ignore(getright(nl, genbuf));
			linecopy(buf, strlen(buf), genbuf);
			ol->l_dline = putline(buf);
			makedirty(ol);
			ol->l_next = nl->l_next;
		}
	}
	this_cmd = KILLCMD;
}

void
fixorder(line1, char1, line2, char2)
register LINE	**line1,
		**line2;
register int	*char1,
		*char2;
{
	LINE	*tline;
	int	tchar;

	if (inorder(*line1, *char1, *line2, *char2))
		return;

	tline = *line1;
	tchar = *char1;
	*line1 = *line2;
	*char1 = *char2;
	*line2 = tline;
	*char2 = tchar;
}

void
DelReg()
{
	register MARK	*mp = CurMark();

	reg_kill(mp->m_line, mp->m_char, curline, curchar, 0);
}

void
DelWtSpace()
{
	register char	c;

	while (!eolp() && ((c = linebuf[curchar]) == ' ' || c == '\t'))
		DelNChar();
	while (curchar > 0 && ((c = linebuf[curchar - 1]) == ' ' || c == '\t'))
		DelPChar();
}
