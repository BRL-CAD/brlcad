/*
 *			J O V E _ M A R K S . C
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
 * Revision 11.3  1997/01/03  17:42:17  jra
 * Mods for Irix 6.2
 *
 * Revision 11.2  1995/06/21  03:43:22  gwyn
 * Eliminated trailing blanks.
 *
 * Revision 11.1  95/01/04  10:35:18  mike
 * Release_4.4
 *
 * Revision 10.2  93/10/26  06:07:55  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:54:01  mike
 * Release_4.0
 *
 * Revision 2.1  91/08/30  17:54:36  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.0  84/12/26  16:46:58  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:08:53  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_marks.c

   Creation of marks, and routines to adjust the marks after insertion
   or deletion.  */

#include "./jove.h"

void	PopMark();
void	ToMark();
void	DFixMarks();

MARK *
DoMakeMark(buf, line, column)
BUFFER	*buf;
register LINE	*line;
{
	register MARK	*newmark = (MARK *) emalloc(sizeof *newmark);

	MarkSet(newmark, line, column);
	newmark->m_next = buf->b_marks;
	buf->b_marks = newmark;
	return newmark;
}

MARK *
MakeMark(line, col)
LINE	*line;
{
	return DoMakeMark(curbuf, line, col);
}

void
DelMark(m)
MARK	*m;
{
	DoDelMark(curbuf, m);
}

void
DoDelMark(b, m)
BUFFER	*b;
register MARK	*m;
{
	register MARK	*mp = b->b_marks;

	if (m == mp)
		b->b_marks = m->m_next;
	else {
		while (mp != 0 && mp->m_next != m)
			mp = mp->m_next;
		if (mp == 0)
			complain("Trying to delete unknown mark!");
		mp->m_next = m->m_next;
	}
	free((char *) m);
}

void
AllMarkSet(b, line, col)
BUFFER	*b;
register LINE	*line;
{
	register MARK	*mp;

	for (mp = b->b_marks; mp; mp = mp->m_next)
		MarkSet(mp, line, col);
}

void
MarkSet(m, line, column)
MARK	*m;
LINE	*line;
{
	m->m_line = line;
	m->m_char = column;
}

void
PopMark()
{
	int	pmark;

	if (curmark == 0)
		return;
	if (curbuf->b_markring[(curbuf->b_themark + 1) % NMARKS] == 0) {
		pmark = curbuf->b_themark;
		do {
			if (--pmark < 0)
				pmark = NMARKS - 1;
		} while (curbuf->b_markring[pmark] != 0);

		curbuf->b_markring[pmark] = MakeMark(curline, curchar);
		ToMark(curmark);
		DelMark(curmark);
		curmark = 0;
	} else
		PtToMark();

	pmark = curbuf->b_themark - 1;
	if (pmark < 0)
		pmark = NMARKS - 1;
	curbuf->b_themark = pmark;
}

void
SetMark()
{
	if (exp_p)
		PopMark();
	else {
		curbuf->b_themark = (curbuf->b_themark + 1) % NMARKS;
		if (curmark == 0)
			curmark = MakeMark(curline, curchar);
		else
			MarkSet(curmark, curline, curchar);
		s_mess("Point pushed");
	}
}

/* Move point to mark */

void
ToMark(m)
MARK	*m;
{
	if (m == 0)
		return;
	DotTo(m->m_line, m->m_char);
}

MARK *
CurMark()
{
	if (curmark == 0)
		complain("No mark");
	return curmark;
}

void
PtToMark()
{
	LINE	*mline;
	int	mchar;
	MARK	*m = CurMark();

	mline = curline;
	mchar = curchar;

	ToMark(m);
	MarkSet(m, mline, mchar);
}

/* Fix marks for after a deletion */
void
DFixMarks(line1, char1, line2, char2)
register LINE	*line1,
		*line2;
{
	register MARK	*m;
	LINE	*lp = line1;

	if (curbuf->b_marks == 0)
		return;
	while (lp != line2->l_next) {
		for (m = curbuf->b_marks; m; m = m->m_next)
			if (m->m_line == lp)
				m->m_char |= (1 << 15);
		lp = lp->l_next;
	}
	for (m = curbuf->b_marks; m; m = m->m_next) {
		if ((m->m_char & (1 << 15)) == 0)
			continue;	/* Not effected */
		m->m_char &= ~(1 << 15);
		if (m->m_line == line1 && m->m_char < char1)
			continue;	/* This mark is not affected */
		if (line1 == line2) {
			if (m->m_char >= char1 && m->m_char <= char2)
				m->m_char = char1;
			else if (m->m_char > char2)
				m->m_char -= (char2 - char1);
			/* Same line move the mark backward */
		} else if (m->m_line == line2) {
			if (m->m_char > char2)
				m->m_char = char1 + (m->m_char - char2);
			else
				m->m_char = char1;
			m->m_line = line1;
		} else {
			m->m_char = char1;
			m->m_line = line1;
		}
	}
}

void
IFixMarks(line1, char1, line2, char2)
register LINE	*line1,
		*line2;
{
	register MARK	*m;

	for (m = curbuf->b_marks; m; m = m->m_next) {
		if (m->m_line == line1) {
			if (m->m_char > char1) {
				m->m_line = line2;
				if (line1 == line2)
					m->m_char += (char2 - char1);
				else
					m->m_char = char2 + (m->m_char - char1);
			}
		}
	}
}
