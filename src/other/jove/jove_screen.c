/*
 *			J O V E _ S C R E E N . C
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
 * Revision 11.2  1995/06/21  03:44:53  gwyn
 * Eliminated trailing blanks.
 * Eliminated blank line at EOF.
 *
 * Revision 11.1  95/01/04  10:35:21  mike
 * Release_4.4
 *
 * Revision 10.3  93/10/26  06:33:01  mike
 * Changed printf() to jprintf() so that all modules could safely
 * use stdio.h
 *
 * Revision 10.2  93/10/26  05:30:25  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:54:04  mike
 * Release_4.0
 *
 * Revision 2.5  91/08/30  18:11:07  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.4  91/08/30  17:54:39  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.3  88/03/10  05:25:54  phil
 * ignore ll if li != winsize
 *
 * Revision 2.2  85/05/14  01:43:59  dpk
 * Added changes to support System V (conditional on SYS5)
 *
 * Revision 2.1  85/01/17  23:58:32  dpk
 *
 *
 * Revision 2.0  84/12/26  16:47:55  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.4  84/02/06  20:43:17  dpk
 * Fixed handling of clear to end of line (ADM3A bug).  Thanks
 * go to Terry Slatterly for finding this bug and providing the fix.
 *
 * Revision 1.3  84/02/06  20:41:23  dpk
 * Made screen handling more conservative (i.e. fixed)
 *
 * Revision 1.2  83/12/16  00:09:41  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_screen.c

   Deals with writing output to the screen optimally i.e. it doesn't
   write what is already there.  It keeps an exact image of the screen
   in the Screen array.  */

#include "./jove.h"
#include "./jove_temp.h"
#include "./termcap.h"

extern int	BufSize;

int	CheckTime,
	tabstop = 8;

struct scrimage
	*nimage,	/* What lines should be where after redisplay */
	*oimage;	/* What line are after redisplay */

struct screenline	*Screen,	/* The screen, a bunch of screenline */
			*Savelines,	/* Another bunch (LI of them) */
			*Curline;	/* Current line */
char	*cursor,			/* Offset into current line */
	*cursend;			/* Pointer to last char in line */

int	CapCol,
	CapLine,

	i_line,
	i_col;

void	DoPlacur();
void	cl_scr();
int	dosputc();

#define sputc(c)	((*cursor != c) ? dosputc(c) : (cursor++, i_col++))
#define soutputc(c)	if (--n > 0) sputc(c); else { sputc('!'); goto outahere;}

void
make_scr()
{
	register int	i;
	register struct screenline	*ns;
	register char	*nsp;

	if (Savelines)
		free (Savelines);
	Savelines = (struct screenline *)emalloc(LI*sizeof (struct screenline));

	if (nimage)
		free (nimage);
	if (oimage)
		free (oimage);
	nimage = (struct scrimage *) emalloc(LI * sizeof (struct scrimage));
	oimage = (struct scrimage *) emalloc(LI * sizeof (struct scrimage));

	if (Screen)
		free (Screen);
	/* XXX - We should also clean up the char storage */
	ns = Screen = (struct screenline *)
			emalloc(LI * sizeof(struct screenline));

	nsp = (char *) emalloc(CO * LI);

	for (i = 0; i < LI; i++) {
		ns->s_line = nsp;
		nsp += CO;
		ns->s_length = nsp - 1;		/* End of line */
		ns++;
	}
	cl_scr();
}

void
clrline(cp1, cp2)
register char	*cp1,
		*cp2;
{
	while (cp1 <= cp2)
		*cp1++ = ' ';
}

void
cl_eol()
{
	if (InputPending || cursor > cursend)
		return;

	if (cursor < Curline->s_length) {
		Placur(i_line, i_col);
		if (CE) {
			clrline(cursor, Curline->s_length);
			putpad(CE, 1);
		} else {
			/* Ugh.  The slow way */
			register char *savecp = cursor;

			while (cursor <= Curline->s_length)
				sputc (' ');
			cursor = savecp;
		}
		Curline->s_length = cursor;	/* Update end pointer */
	}
}

void
cl_scr()
{
	register int	i;
	register struct screenline	*sp = Screen;

	for (i = 0; i < LI; i++, sp++) {
		clrline(sp->s_line, sp->s_length);
		sp->s_length = sp->s_line;
		oimage[i].Line = 0;
	}
	putpad(CL, LI);
	Placur(0, 0);
	UpdMesg++;
}

/* Output one character (if necessary) at the current position */
int
dosputc(c)
register char	c;
{
	if (*cursor != c) {
		Placur(i_line, i_col);
		outchar(c);
		CapCol++;
		*cursor++ = c;
		i_col++;
	} else {
		cursor++;
		i_col++;
	}
	return 0;		/* Called by ?: operation */
}

/* Write `line' at the current position of `cursor'.  Stop when we
   reach the end of the screen.  Aborts if there is a character
   waiting.  */
int
swrite(line)
register char	*line;
{
	register char	c;
	int	col = 0,
		aborted = 0;
	register int	n = cursend - cursor;

	while (c = *line++) {
		if (CheckTime) {
			flusho();
			CheckTime = 0;
			if (InputPending = charp()) {
				aborted = 1;
				break;
			}
		}
		if (c == '\t') {
			int	nchars;

			nchars = (tabstop - (col % tabstop));
			col += nchars;

			while (nchars--)
				soutputc(' ')
		} else if (c < 040 || c == '\177') {
			soutputc('^')
			soutputc(c == '\177' ? '?' : (c + '@'))
			col += 2;
		} else {
			soutputc(c)
			col++;
		}
	}
outahere:
	if (cursor > Curline->s_length)
		Curline->s_length = cursor;
	return !aborted;
}

/* This is for writing a buffer line to the screen.  This is to
 * minimize the amount of copying from one buffer to another buffer.
 * This gets the info directly from ibuff[12].
 */
int
BufSwrite(linenum)
{
	register char	c,
			*bp;
	LINE	*lp = nimage[linenum].Line;
	register int	n = cursend - cursor;
	int	tl = lp->l_dline,
		nl,
		col = 0,
		StartCol = nimage[linenum].StartCol,
		aborted = 0;

#define OkayOut(c)	if (col++ >= StartCol) soutputc(c) else

	if (lp == curline) {
		bp = linebuf;
		nl = BSIZ;
	} else {
		bp = getblock(tl, READ);
		nl = nleft;
		tl &= ~OFFMSK;
	}

	while (c = *bp++) {
		if (CheckTime) {
			flusho();
			CheckTime = 0;
			if (InputPending = charp()) {
				aborted = 1;
				break;
			}
		}
		if (c == '\t') {
			int	nchars;

			nchars = (tabstop - (col % tabstop));

			while (nchars--)
				OkayOut(' ');

		} else if (c < 040 || c == '\177') {
			OkayOut('^');
			OkayOut(c == '\177' ? '?' : (c + '@'));
		} else
			OkayOut(c);

		if (--nl == 0) {
			bp = getblock(tl += INCRMT, READ);
			nl = nleft;
		}
	}
outahere:
	if (cursor > Curline->s_length)
		Curline->s_length = cursor;
	return !aborted;		/* Didn't abort */
}

void
putstr(str)
register char	*str;
{
	register char	c;

	while (c = *str++)
		outchar(c);
}

void
i_set(nline, ncol)
register int	nline,
		ncol;
{
	Curline = &Screen[nline];
	cursor = Curline->s_line + ncol;
	cursend = &Curline->s_line[CO - 1];
	i_line = nline;
	i_col = ncol;
}

extern int	diffnum;
#define PrintHo()	putpad(HO, 1), CapLine = CapCol = 0

/* Insert `num' lines at `top', but leave all the lines BELOW `bottom'
 * alone (at least they won't look any different when we are done).
 * This changes the screen array AND does the physical changes.
 */
void
v_ins_line(num, top, bottom)
{
	register int	i;

	/* Save the screen pointers. */

	for(i = 0; i < num && top + i <= bottom; i++)
		Savelines[i] = Screen[bottom - i];

	/* Num number of bottom lines will be lost.
	 * Copy everything down num number of times.
	 */

	for (i = bottom; i > top && i-num >= 0; i--)
		Screen[i] = Screen[i - num];

	/* Restore the saved ones, making them blank. */

	for (i = 0; i < num; i++) {
		Screen[top + i] = Savelines[i];
		clrline(Screen[top + i].s_line, Screen[top + i].s_length);
	}

        if (BG) {	/* But it makes such a big difference built in */
		jprintf("\033[%d;%dr\033[%dL\033[r", top + 1, bottom + 1, num);
		CapCol = CapLine = 0;
	} else if (CS) {
		putpad(tgoto(CS, bottom, top), 0);
		PrintHo();			/* Conservative */
		DoPlacur(top, 0);		/* Conservative */
		for (i = 0; i < num; i++)
			putpad(SR, 0);
		putpad(tgoto(CS, LI - 1, 0), 0);
		PrintHo();			/* Conservative */
	} else {
		Placur(bottom - num + 1, 0);
		if (M_DL && (num > 1)) {
			char	minibuf[16];

			sprintf (minibuf, M_DL, num);
			putpad (minibuf, num);
		} else {
			for (i = 0; i < num; i++)
				putpad(DL, LI - CapLine);
		}
		Placur(top, 0);
		if (M_AL && (num > 1)) {
			char	minibuf[16];

			sprintf (minibuf, M_AL, num);
			putpad (minibuf, num);
		} else {
			for (i = 0; i < num; i++)
				putpad(AL, LI - CapLine);
		}
	}
}

/* Delete `num' lines starting at `top' leaving the lines below `bottom'
   alone.  This updates the internal image as well as the physical image.  */
void
v_del_line(num, top, bottom)
{
	register int	i,
			bot;

	bot = bottom;

	/* Save the lost lines. */

	for (i = 0; i < num && top + i <= bottom; i++)
		Savelines[i] = Screen[top + i];

	/* Copy everything up num number of lines. */

	for (i = top; num + i <= bottom; i++)
		Screen[i] = Screen[i + num];

	/* Restore the lost ones, clearing them. */

	for (i = 0; i < num; i++) {
		Screen[bottom - i] = Savelines[i];
		clrline(Screen[bot].s_line, Screen[bot].s_length);
		bot--;
	}

	if (BG) {	/* It makes such a big difference when built in!!! */
		putstr(sprint("\033[%d;%dr\033[%dM\033[r",
				top + 1, bottom + 1, num));
		CapCol = CapLine = 0;
	} else if (CS) {
		putpad(tgoto(CS, bottom, top), 0);
		PrintHo();			/* Conservative */
		DoPlacur(bottom, 0);		/* Conservative */
		for (i = 0; i < num; i++)
			putpad (SF, 0);
		putpad(tgoto(CS, LI - 1, 0), 0);
		PrintHo();			/* Conservative */
	} else {
		Placur(top, 0);
		if (M_DL && (num > 1)) {
			char	minibuf[16];

			sprintf (minibuf, M_DL, num);
			putpad (minibuf, LI - CapLine);
		} else {
			for (i = 0; i < num; i++)
				putpad(DL, LI - CapLine);
		}
		Placur(bottom + 1 - num, 0);
		if (M_AL && (num > 1)) {
			char	minibuf[16];

			sprintf (minibuf, M_AL, num);
			putpad (minibuf, LI - CapLine);
		} else {
			for (i = 0; i < num; i++)
				putpad(AL, LI - CapLine);
		}
	}
}

/* The cursor optimization happens here.  You may decide that this
   is going too far with cursor optimization, or perhaps it should
   limit the amount of checking to when the output speed is slow.
   What ever turns you on ...   */

extern int	CapCol,
		CapLine;

struct cursaddr {
	int	c_numchars;
	void	(*c_func)();
};

char	*Cmstr;
struct cursaddr	*HorMin,
		*VertMin,
		*DirectMin;

#define	FORWARD		0	/* Move forward */
#define FORTAB		1	/* Forward using tabs */
#define	BACKWARD	2	/* Move backward */
#define RETFORWARD	3	/* Beginning of line and then tabs */
#define NUMHOR		4

#define DOWN		0	/* Move down */
#define UPMOVE		1	/* Move up */
#define NUMVERT		2

#define DIRECT		0	/* Using CM */
#define HOME		1	/* HOME	*/
#define LOWER		2	/* Lower line */
#define NUMDIRECT	3

#define	home()		Placur(0, 0)
#define LowLine()	putpad(LL, 1), CapLine = LI - 1, CapCol = 0

struct cursaddr	WarpHor[NUMHOR],
		WarpVert[NUMVERT],
		WarpDirect[NUMDIRECT];

int	phystab = 8;

void
GoDirect(line, col)
register int	line,
		col;
{
	putpad(Cmstr, 1), CapLine = line, CapCol = col;
}

void
RetTab(col)
register int	col;
{
	outchar('\r');
	CapCol = 0;
	ForTab(col);
}

void
HomeGo(line, col)
{
	PrintHo(), DownMotion(line), ForTab(col);
}

void
BottomUp(line, col)
register int	line,
		col;
{
	LowLine(), UpMotion(line), ForTab(col);
}

void
ForTab(col)
register int	col;
{
	register int	where,
			ntabs;

	if (TABS) {
		where = col - (col % phystab);	/* Round down. */
		if ((where + phystab) - col < col - where)
			where += phystab; /* Go past and come back. */
		if (where >= CO)
			where -= phystab;	/* Don't tab to last place
						 * or it is likely to screw
						 * up
						 */
		ntabs = (where / phystab) - (CapCol / phystab);
		while (--ntabs >= 0)
			outchar('\t');
		CapCol = where;
	}
	if (CapCol > col)
		BackMotion(col);
	else if (CapCol < col)
		ForMotion(col);
}

extern struct screenline	*Screen;

void
ForMotion(col)
register int	col;
{
	register int	length = Screen[CapLine].s_length -
					Screen[CapLine].s_line;
	register char	*cp = &Screen[CapLine].s_line[CapCol];

	while (CapCol < col) {

		if (CapCol >= length)
			*cp = ' ';
		outchar(*cp);
		cp++;
		CapCol++;
	}
}

void
BackMotion(col)
register int	col;
{
	while (CapCol > col) {	/* Go to the left. */
		if (BC)
			putpad(BC, 1);
		else
			outchar('\b');
		CapCol--;
	}
}

void
DownMotion(line)
register int	line;
{
	while (CapLine < line) {	/* Go down. */
		outchar('\n');
		CapLine++;
	}
}

void
UpMotion(line)
register int	line;
{
	while (CapLine > line) {	/* Go up. */
		putpad(UP, 1);
		CapLine--;
	}
}

void
InitCM()
{
	WarpHor[FORWARD].c_func = ForMotion;
	WarpHor[BACKWARD].c_func = BackMotion;
	WarpHor[FORTAB].c_func = ForTab;
	WarpHor[RETFORWARD].c_func = RetTab;

	WarpVert[DOWN].c_func = DownMotion;
	WarpVert[UPMOVE].c_func = UpMotion;

	WarpDirect[DIRECT].c_func = GoDirect;
	WarpDirect[HOME].c_func = HomeGo;
	WarpDirect[LOWER].c_func = BottomUp;

	HomeLen = HO ? strlen(HO) : 1000;
	LowerLen = (LL && (LI == LI_termcap)) ? strlen(LL) : 1000;
	UpLen = UP ? strlen(UP) : 1000;
}

extern int	InMode;

void
DoPlacur(line, col)
{
	int	dline,		/* Number of lines to move */
		dcol;		/* Number of columns to move */
	register int	best,
			i;
	register struct cursaddr	*cp;

#define CursMin(which,addrs,max) \
	for (best = 0, cp = &addrs[1], i = 1; i < max; i++, cp++) \
		if (cp->c_numchars < addrs[best].c_numchars) \
			best = i; \
	which = &addrs[best]; \

	if (line == CapLine && col == CapCol)
		return;		/* We are already there. */

	if (InMode)
		putstr(EI), InMode = 0;
	dline = line - CapLine;
	dcol = col - CapCol;

	/* Number of characters to move horizontally for each case.
	 * 1: Just move forward by typing the right character on the screen
	 * 2: Print the correct number of back spaces
	 * 3: Try tabbing to the correct place
	 * 4: Try going to the beginning of the line, and then tab
	 */

	if (dcol == 1 || dcol == 0) {		/* Most common case */
		HorMin = &WarpHor[FORWARD];
		HorMin->c_numchars = dcol;
	} else {
		WarpHor[FORWARD].c_numchars = dcol >= 0 ? dcol : 1000;
		WarpHor[BACKWARD].c_numchars = dcol < 0 ? -dcol : 1000;
		WarpHor[FORTAB].c_numchars = dcol >= 0 && TABS ?
				ForNum(CapCol, col) : 1000;
		WarpHor[RETFORWARD].c_numchars = (1 + (TABS ? ForNum(0, col) : col));

		/* Which is the shortest of the bunch */

		CursMin(HorMin, WarpHor, NUMHOR);
	}

	/* Moving vertically is more simple. */

	WarpVert[DOWN].c_numchars = dline >= 0 ? dline : 1000;
	WarpVert[UPMOVE].c_numchars = dline < 0 ? ((-dline) * UpLen) : 1000;

	/* Which of these is simpler */
	CursMin(VertMin, WarpVert, NUMVERT);

	/* Homing first and lowering first are considered
	   direct motions.
	   Homing first's total is the sum of the cost of homing
	   and the sum of tabbing (if possible) to the right. */

	if (VertMin->c_numchars + HorMin->c_numchars <= 3) {
		DirectMin = &WarpDirect[DIRECT];	/* A dummy ... */
		DirectMin->c_numchars = 100;
	}
	WarpDirect[DIRECT].c_numchars = CM ?
				strlen(Cmstr = tgoto(CM, col, line)) : 1000;
	WarpDirect[HOME].c_numchars = HomeLen + line +
				WarpHor[RETFORWARD].c_numchars;
	WarpDirect[LOWER].c_numchars = LowerLen + ((LI - line - 1) * UpLen) +
				WarpHor[RETFORWARD].c_numchars;

	CursMin(DirectMin, WarpDirect, NUMDIRECT);

	if (HorMin->c_numchars + VertMin->c_numchars < DirectMin->c_numchars) {
		if (line != CapLine)
			(*VertMin->c_func)(line);
		if (col != CapCol)
			(*HorMin->c_func)(col);
	} else
		(*DirectMin->c_func)(line, col);
}

#define abs(x)	((x) >= 0 ? (x) : -(x))

ForNum(from, to)
register int	from,
		to;
{
	register int	where;
	int		numchars = 0;

	if (from > to)
		return from - to;
	if (TABS) {
		where = to - (to % phystab);   /* Round down. */
		if ((where + phystab) - to < to - where)
			where += phystab;
		if (where >= CO)
			where -= phystab;	/* Don't tab to last place
						 * or it is likely to screw
						 * up
						 */
		numchars = (where / phystab) - (from / phystab);
		from = where;
	}
	return numchars + abs(from - to);
}
