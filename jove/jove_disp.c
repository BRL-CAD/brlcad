/*
 *			J O V E _ D I S P . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.2  1995/06/21  03:40:01  gwyn
 * Eliminated trailing blanks.
 *
 * Revision 11.1  95/01/04  10:35:11  mike
 * Release_4.4
 *
 * Revision 10.4  93/10/26  06:32:47  mike
 * Changed printf() to jprintf() so that all modules could safely
 * use stdio.h
 *
 * Revision 10.3  93/10/26  03:43:10  mike
 * ANSI C
 *
 * Revision 10.2  93/08/11  20:25:34  mike
 * Removed 132 column restriction on output displays.
 *
 * Revision 10.1  91/10/12  06:53:56  mike
 * Release_4.0
 *
 * Revision 2.3  91/08/30  18:10:44  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.2  91/08/30  17:54:31  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.1  85/01/17  23:57:57  dpk
 * Added minimal blit support, more to come...
 *
 * Revision 2.0  84/12/26  16:45:33  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:07:36  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/* Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_disp.c

   This code figures out the best way to update the screen.
   It contains the procedure "redisplay()" that should be called
   whenever the screen needs updating.  This optimizes interline
   movement and intraline movement, taking advantage of insert/delete
   line/character features of the terminal (if they exist).  */

#include "./jove.h"
#include "./termcap.h"

#include <signal.h>

#define	TOPGONE	01
#define	CURGONE	02	/* Topline (curline) of window has been deleted
			   since the last time a redisplay was called. */

void	DoIDline();
void	UpdateWindow();
void	GotoDot();

extern char	mesgbuf[];

/* Kludge windows gets called by the routines that delete lines from the
   buffer.  If the w->w_line or w->w_top are deleted and this procedure
   is not called, the redisplay routine will barf. */

void
KludgeWindows(line1, line2)
LINE	*line1;
register LINE	*line2;
{
	register WINDOW	*w = fwind;
	register LINE	*lp;

	do {
		for (lp = line1->l_next; lp != line2->l_next; lp = lp->l_next) {
			if (lp == w->w_top)
				w->w_flags |= TOPGONE;
			if (lp == w->w_line)
				w->w_flags |= CURGONE;
		}
		w = w->w_next;
	} while (w != fwind);
}

inlist(first, what)
register LINE	*first,
		*what;
{
	while (first) {
		if (first == what)
			return 1;
		first = first->l_next;
	}
	return 0;
}

int	UpdWCalls,	/* Number of times we called UpdateWindow for this window */
	IDstart,	/* First different line */
	NumDirty;	/* Number of dirty lines in this screen update */

int	VisBell = 0;	/* If set, use visible bell if available */
int	RingBell = 0;	/* So if we have a lot of errors ...
			   ring the bell only ONCE */

/* The redisplay algorithm:

   Jove remembers where each buffer lines is on the screen in the array
   `oimage' (old image).  UpdateWindow() makes a new image, in `nimage',
   by started from w->w_top and working to the bottom line of the window.
   A line by line comparison starts from nimage[0] and oimage[0], and if
   there are no differences, no insert/delete lines is done.  When there
   is a difference, there are two possibilities:

	Some lines were deleted in the buffer.  This is detected by looking
     further down in the old image, for the line where the difference occurred
     in the new image.  So where we used to have 1-2-3-4 (oimage), we now have
     1-4, in which case two lines were deleted.

        Some lines were inserted in the buffer.  This is detected by looking
     further down in the new image, for the line where the difference occurred
     in the old image.  So where we used to have 1-2-3, we now have 1-2-4-5-3,
     in which case two lines were inserted (lines 4 and 5).

   UpdateWindow has a few optimizations in it, e.g. it checks for
   mismatches AS it builds `nimage', and sets a variable to the line number
   at which the difference occurred.  It also keeps a count of the number
   of lines that need updating i.e. the line was changed by an editing
   operation, or the start print column is different (the line just scrolled
   left or right).  Imagine that a single character was inserted on the top
   line of the screen.  The number of lines that are dirty = 1, so the loop
   that checks to see that all the lines are up-to-date can terminate after
   updating the first line WITHOUT checking the rest of the lines. */
void
redisplay()
{
	register WINDOW	*w = fwind;
	int	lineno,
		i;
	register struct scrimage	*np,
					*op;

	curwind->w_bufp = curbuf;
	if (curwind->w_line != curline)
		curwind->w_offset = 0;
	curwind->w_line = curline;
	curwind->w_char = curchar;

	if (InputPending = charp())
		return;
	if (RingBell) {
		if (VisBell && VB)
			putstr(VB);
		else
			outchar('\007');
		RingBell = 0;
	}

	/* So messages that aren't error messages don't hang around forever */
	if (!UpdMesg && !Asking) {	/* Don't erase if we are asking */
		if (mesgbuf[0] && !errormsg)
			message("");
	}
	if (UpdMesg)
		UpdateMesg();

	NumDirty = 0;
	IDstart = -1;

	for (lineno = 0, w = fwind; lineno < LI - 1; w = w->w_next) {
		UpdWCalls = 0;
		UpdateWindow(w, lineno);
		lineno += w->w_height;
	}

	if (IDstart != -1) {		/* Shucks this is gonna be slower */
		DoIDline(IDstart);
		NumDirty = LI - 1;
	}

	np = nimage;
	op = oimage;
	for (i = 0; i < LI - 1 && NumDirty > 0; i++, np++, op++) {
		if ((np->Sflags & DIRTY) ||
				op->Line != np->Line ||
				op->StartCol != np->StartCol ||
				(UpdModLine && np->Sflags & MODELINE)) {
			UpdateLine(np->Window, i);
			NumDirty--;
		}
		if (InputPending)
			return;
	}
	if (InputPending)
		return;
	UpdModLine = 0;

	if (Asking) {
		Placur(LI - 1, calc_pos(mesgbuf, Asking));
			/* Nice kludge */
		flusho();
	} else
		GotoDot();
}

int	UpdModLine = 0,
	UpdMesg = 0;

void
DoIDline(start)
{
	register struct scrimage	*np = &nimage[start],
					*op = &oimage[start];
	register int	i;
	int	j;

	/* Some changes have been made.  Try for insert or delete lines.
	   If either case has happened, Addlines and/or DelLines will do
	   necessary scrolling, also CONVERTING oimage to account for the
	   physical changes.  The comparison continues from where the
	   insertion/deletion takes place; this doesn't happen very often,
	   usually it happens with more than one window with the same
	   buffer. */

	if (!CanScroll)
		return;		/* We should never have been called! */

	for (i = 0; i < LI - 1; i++, np++, op++)
		if (np->Line != op->Line)
			break;
	for (np = nimage, op = oimage; i < LI - 1; i++) {
		for (j = i + 1; j < LI - 1; j++) {
			if (np[j].Line != 0 && np[j].Line == op[j].Line)
				break;
			if (np[j].Line == op[i].Line) {
				if (np[j].Line == 0)
					continue;
				if (AddLines(i, j - i)) {
					DoIDline(j);
					return;
				}
				break;
			}
			if (np[i].Line == op[j].Line) {
				if (np[i].Line == 0)
					continue;
				if (DelLines(i, j - i)) {
					DoIDline(i);
					return;
				}
				break;
			}
		}
	}
}

/* Make nimage reflect what the screen should look like when we are done
   with the redisplay.  This deals with horizontal scrolling.  Also makes
   sure the current line of the window is in the window. */
void
UpdateWindow(w, start)
register WINDOW	*w;
{
	LINE	*lp;
	int	i,
		DotIsHere = 0,
		upper,		/* Top of window */
		lower;		/* Bottom of window */
	register struct scrimage	*np,
					*op;
	int	savestart = IDstart;

	if (w->w_flags & CURGONE) {
		w->w_line = w->w_bufp->b_dot;
		w->w_char = w->w_bufp->b_char;
	}
	if (w->w_flags & TOPGONE)
		CalcTop(w);	/* Reset topline of screen */
	w->w_flags = 0;

	upper = start;
	lower = upper + w->w_height - 1;	/* Don't include modeline */
	np = &nimage[upper];
	op = &oimage[upper];
	for (i = upper, lp = w->w_top; lp != 0 && i < lower;
					i++, np++, op++, lp = lp->l_next) {
		if (lp == w->w_line) {
			w->w_dotcol = find_pos(lp, w->w_char);
			w->w_dotline = i;

			if (w->w_numlines)
				w->w_dotcol += 8;
			DotIsHere++;
			if (w->w_dotcol < np->StartCol ||
				(w->w_dotcol + 2) >= (np->StartCol + CO)) {
				if (w->w_dotcol + 2 < CO)
					w->w_offset = 0;
				else
					w->w_offset = w->w_dotcol - (CO / 2);
			}
			np->StartCol = w->w_offset;
		} else
			np->StartCol = 0;

		if ((np->Sflags = (lp->l_dline & DIRTY)) ||
					np->StartCol != op->StartCol)
			NumDirty++;
		if (((np->Line = lp) != op->Line) && IDstart == -1)
			IDstart = i;
		np->Window = w;
	}
	if (!DotIsHere) {	/* Current line not in window */
		if (UpdWCalls != 0) {
			jprintf("\rCalled UpdateWindow too many times");
			finish(SIGHUP);
		}
		IDstart = savestart;
		UpdWCalls++;
		CalcScroll(w);
		UpdateWindow(w, start);	/* This time for sure */
		return;
	}

	/* Is structure assignment faster than copy each field seperately */
	if (i < lower) {
		static struct scrimage	dirty_plate = { 0, DIRTY, 0, 0 };
		static struct scrimage	clean_plate = { 0, 0, 0, 0 };

		for (; i < lower; i++, np++, op++) {
			if (np->Sflags = ((op->Line) ? DIRTY : 0)) {
				if (IDstart < 0)
					IDstart = i;
				NumDirty++;
				*np = dirty_plate;
			} else
				*np = clean_plate;
		}
	}

	if (((np->Line = (LINE *) w->w_bufp) != op->Line) || UpdModLine) {
		if (IDstart < 0)
			IDstart = i;
		NumDirty++;
	}

	np->Sflags = MODELINE;
	np->Window = w;
}

/* Make `buf' modified and tell the redisplay code to update the modeline
   if it will need to be changed. */

void
SetModified(buf)
BUFFER	*buf;
{
	extern int	DOLsave;

	if (! IsModified(buf))
		UpdModLine++;
	buf->b_status |= B_MODIFIED;
	DOLsave++;
}

void
SetUnmodified(buf)
BUFFER	*buf;
{
	if (IsModified(buf))
		UpdModLine++;
	buf->b_status &= ~B_MODIFIED;
}

/* Write whatever is in mesgbuf (maybe we are Asking,  or just printed
   a message.  Turns off the Update Mesg line flag */

void
UpdateMesg()
{
	i_set(LI - 1, 0);
	if (swrite(mesgbuf)) {
		cl_eol();
		UpdMesg = 0;
	}
	flusho();
}

/* Goto the current position in the current window.  Presumably redisplay()
   has already been called, and curwind->{w_dotline,w_dotcol} have been set
   correctly. */
void
GotoDot()
{
	if (InputPending)
		return;
	Placur(curwind->w_dotline, curwind->w_dotcol -
				oimage[curwind->w_dotline].StartCol);
	flusho();
}

/* Put the current line of `w' in the middle of the window */

void
CalcTop(w)
WINDOW	*w;
{
	SetTop(w, prev_line(w->w_line, HALF(w)));
}

int	ScrollStep = 0;		/* Full scrolling */

/* Calculate the new topline of the screen; different when in single-scroll
   mode */

void
CalcScroll(w)
register WINDOW	*w;
{
	extern int	diffnum;
	register int	up;

	if (ScrollStep == 0)	/* Means just center it */
		CalcTop(w);
	else {
		up = inorder(w->w_line, 0, w->w_top, 0);
		if (up)		/* Dot is above the screen */
			SetTop(w, prev_line(w->w_line, min(ScrollStep - 1, HALF(w))));
		else
			SetTop(w, prev_line(w->w_line,
					(SIZE(w) - 1) -
					min(ScrollStep - 1, HALF(w))));
	}
}

UntilEqual(start)
register int	start;
{
	register struct scrimage	*np = &nimage[start],
					*op = &oimage[start];

	while ((start < LI - 1) && (np->Line != op->Line)) {
		np++;
		op++;
		start++;
	}

	return start;
}

/* Calls the routine to do the physical changes, and changes oimage to
   reflect those changes. */

AddLines(at, num)
register int	at,
		num;
{
	register  int	i;
	int	bottom = UntilEqual(at + num);

	if (num == 0 || num >= ((bottom - 1) - at))
		return 0;	/* We did nothing */
	v_ins_line(num, at, bottom - 1);

	/* Now change oimage to account for the physical change */

	for (i = bottom - 1; i - num >= at; i--)
		oimage[i] = oimage[i - num];
	for (i = 0; i < num; i++)
		oimage[at + i].Line = 0;
	return 1;	/* We did something */
}

DelLines(at, num)
register int	at,
		num;
{
	register int	i;
	int	bottom = UntilEqual(at + num);

	if (num == 0 || num >= ((bottom - 1) - at))
		return 0;
	v_del_line(num, at, bottom - 1);

	for (i = at; num + i < bottom; i++)
		oimage[i] = oimage[num + i];
	for (i = bottom - num; i < bottom; i++)
		oimage[i].Line = 0;
	return 1;
}

void
DeTab(StartCol, buf, outbuf, limit)
register char	*buf;
char	*outbuf;
int limit;
{
	register char	*op = outbuf,
			c;
	register int	pos = 0;

#define OkayOut(ch)	if ((pos++ >= StartCol) && (op < &outbuf[limit]))\
				*op++ = ch;\
			else

	while (c = *buf++) {
		if (c == '\t') {
			int	nchars = (tabstop - (pos % tabstop));

			while (nchars--)
				OkayOut(' ');

		} else if (c < ' ' || c == 0177) {
			OkayOut('^');
			OkayOut(c == 0177 ? '?' : c + '@');
		} else
			OkayOut(c);
		if (pos - StartCol >= CO) {
			op = &outbuf[CO - 1];
			*op++ = '!';
			break;
		}
	}
	*op = 0;
}

/* Update line linenum in window w.  Only set oimage to nimage if
 * the swrite or cl_eol works, that is nothing is interrupted by
 * characters typed
 */

void
UpdateLine(w, linenum)
register WINDOW	*w;
register int	linenum;
{
	int	hasIC = (IC || IM || M_IC);
	register struct scrimage	*np = &nimage[linenum];

	if (np->Sflags == MODELINE)
		ModeLine(w);
	else if (np->Line) {
		np->Line->l_dline &= ~DIRTY;
		np->Sflags &= ~DIRTY;
		i_set(linenum, 0);
		if (!hasIC && w->w_numlines)
			ignore(swrite(sprint("%6d  ", (linenum - FLine(w) +
					w->w_topnum))));
		if (hasIC) {
			char	outbuf[LBSIZE],
				buff[LBSIZE],
				*bptr;
			int	fromcol = w->w_numlines ? 8 : 0;

			if (w->w_numlines) {
				ignore(sprintf(buff, "%6d  ", (linenum - FLine(w) +
						w->w_topnum)));
				ignore(getright(np->Line, buff + fromcol));
				bptr = buff;
			} else
				bptr = getcptr(np->Line, buff);
			DeTab(np->StartCol, bptr,
				outbuf, (int)(sizeof outbuf) - 1);
			if (IDchar(outbuf, linenum, 0))
				oimage[linenum] = *np;
			else if (i_set(linenum, 0), swrite(outbuf))
				do_cl_eol(linenum);
			else
				oimage[linenum].Line = (LINE *) -1;
		} else if (BufSwrite(linenum))
			do_cl_eol(linenum);
		else
			oimage[linenum].Line = (LINE *) -1;
	} else if (oimage[linenum].Line) {	/* Not the same ... make sure */
		i_set(linenum, 0);
		do_cl_eol(linenum);
	}
}

void
do_cl_eol(linenum)
register int	linenum;
{
	cl_eol();
	oimage[linenum] = nimage[linenum];
}


/* ID character routines full of special cases and other fun stuff like that.
   It actually works thougth ... */

extern struct screenline	*Screen;
int	InMode = 0;
int	DClen = 0;
int	MDClen = 0;
int	IClen = 0;
int	MIClen = 0;
int	IMlen = 0;
int	CElen = 0;

/*
 *  Initialization routine.  Called from jove_term.c.
 */
void
disp_opt_init()
{
	if (DC) DClen = strlen(DC);
	MDClen = (M_DC ? strlen(M_DC) : 9999);
	if (IC) IClen = strlen(IC);
	MIClen = (M_IC ? strlen(M_IC) : 9999);
	if (IM) IMlen = strlen(IM);
	if (CE) CElen = strlen(CE);
}

IDchar(new, lineno, col)
register char	*new;
{
	int	i,
		j,
		oldlen,
		NumSaved;
	register struct screenline	*sline = &Screen[lineno];

	oldlen = sline->s_length - sline->s_line;

	for (i = col; i < oldlen && new[i] != 0; i++)
		if (sline->s_line[i] != new[i])
			break;
	if (new[i] == 0 || i == oldlen)
		return (new[i] == 0 && i == oldlen);

	for (j = i + 1; j < oldlen && new[j]; j++) {
		if (new[j] == sline->s_line[i]) {
			NumSaved = IDcomp(new + j, sline->s_line + i,
					strlen(new)) + NumSimilar(new + i,
						sline->s_line + i, j - i);
			if (OkayInsert(NumSaved, j - i)) {
				InsChar(lineno, i, j - i, new);
				return(IDchar(new, lineno, j));
			}
		}
	}

	for (j = i + 1; j < oldlen && new[i]; j++) {
		if (new[i] == sline->s_line[j]) {
			NumSaved = IDcomp(new + i, sline->s_line + j,
					oldlen - j);
			if (OkayDelete(NumSaved, j - i, new[oldlen] == 0)) {
				DelChar(lineno, i, j - i);
				return(IDchar(new, lineno, j));
			}
		}
	}
	return 0;
}

NumSimilar(s, t, n)
register char	*s,
		*t;
{
	register int	num = 0;

	while (n--)
		if (*s++ == *t++)
			num++;
	return num;
}

IDcomp(s, t, len)
register char	*s,
		*t;
{
	register int	i;
	int	num = 0,
		nonspace = 0;
	char	c;

	for (i = 0; i < len; i++) {
		if ((c = *s++) != *t++)
			break;
		if (c != ' ')
			nonspace++;
		if (nonspace)
			num++;
	}

	return num;
}

OkayDelete(Saved, num, samelength)
{
	/* If the old and the new are the same length, then we don't
	 * have to clear to end of line.  We take that into consideration.
	 */
	return ((Saved + (!samelength ? CElen : 0))
		> min(MDClen, DClen * num));
}

OkayInsert(Saved, num)
{
	register int	n = 0;

	/*
	 *  Total cost of inserting characters is the cost of
	 *  of any per-character leadin plus initial cost to
	 *  enter insert mode if necessary.  We ignore the cost
	 *  cost of exiting insert mode.    (DPK@BRL)
	 */
	if (IC)		/* Per character prefixes */
		n = min (num * IClen, MIClen);

	if (IM && !InMode)
		n += IMlen;	/* Must go into insert mode */

	n += num;		/* Account for the chars themselves */

	return (Saved > n);
}

extern int	CapCol;
extern char	*cursend;
extern struct screenline	*Curline;

void
DelChar(lineno, col, num)
{
	register int	i;
	register char	*from, *to;
	struct screenline *sp = (&Screen[lineno]);

	Placur(lineno, col);
	if (M_DC && num > 1) {
		char	minibuf[16];

		sprintf (minibuf, M_DC, num);
		putpad (minibuf, num);
	} else {
		for (i = 0; i < num; i++)
			putpad(DC, 1);
	}

	to = sp->s_line + col;
	from = to + num;

	i = sp->s_length - from + 1;
	while (i--)
		*to++ = *from++;
	clrline(sp->s_length - num, sp->s_length);
	sp->s_length -= num;
}

void
InsChar(lineno, col, num, new)
char	*new;
{
	register char	*sp1,
			*sp2,	/* To push over the array */
			*sp3;	/* Last character to push over */

	/*
	 *  If there is an insert character string, then
	 *  it is expected to insert spaces which we then
	 *  write the chars into.  The screen must be
	 *  updated appropriately.
	 */
	int	i;

	i_set(lineno, 0);
	sp2 = Curline->s_length + num;
	if (sp2 >= cursend) {
		i_set(lineno, CO - (sp2 - cursend) - 1);
		cl_eol();
		sp2 = cursend - 1;
	}
	Curline->s_length = sp2;
	sp1 = sp2 - num;
	sp3 = Curline->s_line + col;

	while (sp1 >= sp3)
		*sp2-- = *sp1--;
	sp1 = Curline->s_line + col;
	new += col;
	for (i = 0; i < num; i++)
		*sp1++ = new[i];
	/* The internal screen is correct, and now we have to do
	 * the physical stuff
	 */

	Placur(lineno, col);
	if (!InMode && IM) {
		putpad(IM, 1);
		InMode++;
	}
	if (M_IC && num > 1) {
		char	minibuf[16];

		sprintf (minibuf, M_IC, num);
		putpad (minibuf, num);
	}
	else if (IC) {
		for (i = 0; i < num; i++)
			putpad(IC, 1);
	}
	for (i = 0; i < num; i++) {
		outchar(new[i]);
		CapCol++;
	}
}
