/*
 *			J O V E _ M I S C . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.2  1995/06/21  03:43:41  gwyn
 * Eliminated trailing blanks.
 * RunEdit() now returns null pointer upon ^G abort; was nonportable (char*)-1.
 * Fixed varargs functions, using VA_* macros.
 * Use LBSIZE instead of hard-wired 100.
 *
 * Revision 11.1  95/01/04  10:35:18  mike
 * Release_4.4
 *
 * Revision 10.4  95/01/04  08:43:58  mike
 * "../jove/jove_misc.c", line 234: value of void expression used
 * on VAX
 *
 * Revision 10.3  93/10/26  06:05:57  mike
 * ANSI C
 *
 * Revision 10.2  93/10/26  06:01:52  mike
 * Changed getchar() to jgetchar() to prevent stdio.h conflict
 *
 * Revision 10.1  91/10/12  06:54:02  mike
 * Release_4.0
 *
 * Revision 2.6  91/08/30  18:59:49  mike
 * Modifications for clean compilation on the XMP
 *
 * Revision 2.5  91/08/30  17:54:37  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.4  91/08/30  17:49:12  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.3  87/04/14  22:46:43  dpk
 * Fixed bug in paragraph justification.
 *
 * Revision 2.2  87/04/14  20:19:12  dpk
 * Fixed casting on RunEdit call
 *
 * Revision 2.1  84/12/31  16:43:09  dpk
 * Make CTL(Y)==CTL(R) in RunEdit() (command line editor)
 *
 * Revision 2.0  84/12/26  16:47:08  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:09:03  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_meta.c

   Various commands that are (by default) invoked by ESC-key.
   Not a very good name since the ESC key can be rebound...  */

#include "./jove.h"

#include <ctype.h>
#include <signal.h>

void	EscPrefix();
void	CopyRegion();
void	GoLine();
void	CtlxPrefix();
void	DelBlnkLines();

void	Eos();
void	to_word();
void	ForWord();
void	BackWord();
void	Eow();
void	Bow();
void	DOTsave();
void	CapChar();

void
PrefAbort(str)
char	*str;
{
	s_mess("%s aborted", str);
	rbell();
}

void
Unknown(pref, c)
char	pref,
	c;
{
	s_mess("%c-%c unbound", pref, c);
	rbell();
}

Upper(c)
register int	c;
{
	return (islower(c) ? toupper(c) : c);
}

void
FourTime()
{
	exp_p = 1;
	exp *= 4;
	this_cmd = ARG_CMD;
}

int	FastPrompt = 0;
static char	*StrToShow;
static int	siged = 0;
void
slowpoke(num)
int	num;
{
	s_mess("%s", StrToShow);
	UpdateMesg();
	siged++;
}

waitfor(sec, str)
char	*str;
{
	int	c;

	siged = 0;
	StrToShow = str;
	signal(SIGALRM, slowpoke);
	ignore(alarm((unsigned) sec));
	c = (*Getchar)();	/* The read will be restarted
				   with the new signal mechanism */
	if (!siged)		/* If we didn't get signaled ... */
		alarm(0);	/* we have to turn it off ouselves */
	s_mess("%s%c", str, c);
	return c;
}

void
EscPrefix()
{
	register int	c, i, sign;
	struct function	*mp;
	int	invokingchar = LastKeyStruck;

	if (FastPrompt)
	{
		s_mess("M-");
		c = (*Getchar)();
		s_mess("M-%c", c);
	} else
		c = waitfor(2, "M-");

	if (c == CTL('G')) {
		PrefAbort("Prefix-1");
		return;
	}

	if (c == '-' || (c >= '0' && c <= '9')) {
		s_mess("M-%c", c);
		sign = c == '-' ? -1 : 1;
		if (sign == 1)
			i = c - '0';
		else
			i = 0;
		for (;;) {
			c = (*Getchar)();
			if (c >= '0' && c <= '9')
				i = i * 10 + (c - '0');
			else {
				exp_p = 1;
				exp = i * sign;
				peekc = c;
				this_cmd = ARG_CMD;
				return;
			}
			s_mess("M-%d", i);
		}
	}

	mp = pref1map[c];
	if (mp == 0)
		Unknown(invokingchar, c);
	else
		ExecFunc(mp, 0);
}

/*
 * Save a region.  A pretend kill.
 */
void
CopyRegion()
{
	LINE	*nl;
	MARK	*mp;
	int	status,
		mod = IsModified(curbuf);

	mp = CurMark();

	if (mp->m_line == curline && mp->m_char == curchar)
		complain((char *) 0);

	killptr = ((killptr + 1) % NUMKILLS);
	if (killbuf[killptr])
		lfreelist(killbuf[killptr]);
	nl = killbuf[killptr] = nbufline();
	nl->l_dline = putline("");
	nl->l_next = nl->l_prev = 0;

	status = inorder(mp->m_line, mp->m_char, curline, curchar);
	if (status == -1)
		return;

	if (status)
		ignore(DoYank(mp->m_line, mp->m_char, curline, curchar,
				nl, 0, (BUFFER *) 0));
	else
		ignore(DoYank(curline, curchar, mp->m_line, mp->m_char,
				nl, 0, (BUFFER *) 0));
	if (mod)
		curbuf->b_status |= B_MODIFIED;
	else
		curbuf->b_status &= ~B_MODIFIED;
}

void
DelNWord()
{
	dword(1);
}

void
DelPWord()
{
	dword(0);
}

void
dword(forward)
{
	BUFLOC	savedot;

	DOTsave(&savedot);
	if(forward) ForWord();
	else	BackWord();
	reg_kill(savedot.p_line, savedot.p_char, curline, curchar, !forward);
}

void
UppWord()
{
	case_word(1);
}

void
LowWord()
{
	case_word(0);
}

void
ToIndent()
{
	register char	*cp, c;

	for (cp = linebuf; c = *cp; cp++)
		if (c != ' ' && c != '\t')
			break;
	curchar = cp - linebuf;
}

void
GoLine()
{
	if (exp_p == 0)
		return;
	SetLine(next_line(curbuf->b_zero, exp - 1));
}

int	RMargin = RMARGIN;

void
VtKeys()
{
	int	c = getch();

	switch (c) {
	case 'A':
		PrevLine();
		break;

	case 'B':
		NextLine();
		break;

	case 'D':
		BackChar();
		break;

	case 'C':
		ForChar();
		break;

	default:
		complain("Unknown command ESC-[-%c", c);
	}
}

void
Justify()
{
	BUFLOC	*bp;
	LINE	*start,
		*end;
	char	*ParaStr = "^\\.\\|^$";

	bp = dosearch(ParaStr, -1, 1);	/* Beginning of paragraph */
	if (bp)		/* Not the tab case */
		start = bp->p_line->l_next;
	else
		start = curbuf->b_zero;
	bp = dosearch(ParaStr, 1, 1);		/* End of paragraph */
	if (bp)
		end = bp->p_line;
	else
		end = curbuf->b_dol;
	DoJustify(start, end, 0);
}

extern int	diffnum;

void
DoJustify(l1, l2, scrunch)		/* Justify text */
LINE	*l1,
	*l2;
{
	int	d1 = 0,
		d2 = 1,
		goal,
		nlines,			/* Number of lines to justify
					 * so we know when we are done.
					 */
		curcol;
	char	*cp;
	MARK	*savedot = MakeMark(curline, curchar);

	fixorder(&l1, &d1, &l2, &d2);	/* l1/c1 will be before l2/c2 */
	nlines = diffnum;
	SetLine(l1);

	if (lastp(l2) && !blnkp(getline(l2->l_dline, genbuf)))
		nlines++;

	while (nlines) {	/* One line at a time */
		goal = RMargin - (RMargin / 10);
		Eol();	/* End of line */
		if ((curcol = calc_pos(linebuf, curchar)) == 0) {
			if (curline->l_next == 0)
				break;
			SetLine(curline->l_next);	/* Skip blank lines */
			nlines--;
		} else if (curcol < goal) {
			if (nlines == 1)
				break;
			DelWtSpace();	/* No white space at the end of the line */
			DelNChar();	/* Delete carraige return */
			nlines--;
			Insert(' ');
		} else if (curcol > RMargin) {
			do {
				/* curchar - 2 because curchar is the null,
				 * curchar - 1 is the space (if there is
				 * one there.
				 */
				cp = StrIndex(-1, linebuf, curchar - 2, ' ');
				if (cp == 0)
					break;
				curchar = cp - linebuf;
				curcol = calc_pos(linebuf, curchar);
			} while (curcol > RMargin);
			if (cp) {
				DelWtSpace();
				LineInsert();
				if (scrunch && TwoBlank()) {
					Eol();
					DelNChar();
				}
			} else {
				if (curline->l_next == 0)
					break;
				SetLine(curline->l_next);
				nlines--;
			}
		} else {
			if (curline->l_next == 0)
				break;
			SetLine(curline->l_next);
			nlines--;
		}
	}
	ToMark(savedot);	/* Back to where we were */
	DelMark(savedot);	/* Free up the mark */
	this_cmd = 0;		/* So everything is under control */
}

void
ChrToOct()
{
	int	c = getch();

	ins_str(sprint("\\%03o", c));
}

char *
StrIndex(dir, buf, charpos, what)
char	*buf,
	what;
{
	char	*cp = &buf[charpos],
		c = '\0';

	if (dir > 0) {
		while (c = *cp++)
			if (c == what)
				return (cp - 1);
	} else {
		while (cp >= buf && (c = *cp--))
			if (c == what)
				return (cp + 1);
	}
	return 0;
}

void
StrLength()
{
	static char	inquotes[] = "Where are the quotes?";
	char	*first = StrIndex(-1, linebuf, curchar, '"');
	char	*last = StrIndex(1, linebuf, curchar, '"');
	char	c;
	int	numchars = 0;

	if (first == 0 || last == 0)
		complain(inquotes);
	first++;
	while (first < last) {
		c = *first++;
		if (c == '\\') {
			int	num;

			if (*first < '0' || *first > '9')
				++first;
			else {
				num = 3;
				while (num-- && (c = *first++) >= '0' &&
						c <= '9' && first < last)
					;
			}
		}
		numchars++;
	}
	s_mess("%d characters", numchars);
}

void
SplitWind()
{
	curwind = div_wind(curwind);
}

LINE *
lastline(lp)
register LINE	*lp;
{
	while (lp->l_next)
		lp = lp->l_next;
	return lp;
}

/* Transpos cur_char with cur_char - 1 */

void
TransChar()
{
	char	c;

	if (curchar == 0)
		complain((char *) 0);	/* BEEP */
	c = linebuf[curchar - 1];
	exp = 1;
	DelPChar();
	if (eolp()) {
		BackChar();
		Insert(c);
		ForChar();
	} else {
		ForChar();
		Insert(c);
		BackChar();
	}
}

void
SetLine(line)
register LINE	*line;
{
	DotTo(line, 0);
}

void
UpScroll()
{
	SetTop(curwind, next_line(curwind->w_top, exp));
	if (in_window(curwind, curline) == -1)
		SetLine(next_line(curwind->w_top, HALF(curwind)));
}

void
DownScroll()
{
	SetTop(curwind, prev_line(curwind->w_top, exp));
	if (in_window(curwind, curline) == -1)
		SetLine(next_line(curwind->w_top, HALF(curwind)));
}

void
Leave()
{
	BUFFER	*bp;

	if (RecDepth == 0)
		for (bp = world; bp; bp = bp->b_next)
			if (bp->b_zero && IsModified(bp) && !IsScratch(bp)) {
				confirm("Modified buffers exist.  Leave anyway? ");
				break;
			}
	longjmp(mainjmp, QUIT);
}

void
KillEOL()
{
	LINE	*line2;
	int	char2;

	if (exp_p) {
		line2 = next_line(curline, exp);
		if (line2 == curline)
			char2 = length(curline);
		else
			char2 = 0;
	} else if (linebuf[curchar] == '\0') {
		line2 = next_line(curline, 1);
		if (line2 == curline)
			char2 = length(curline);
		else
			char2 = 0;
	} else {
		line2 = curline;
		char2 = length(curline);
	}
	reg_kill(curline, curchar, line2, char2, 0);
}

void
CtlxPrefix()
{
	struct function	*fp;
	int	c,
		invokingchar = LastKeyStruck;

	if (FastPrompt)
	{
		s_mess("C-X ");
		c = (*Getchar)();
		s_mess("C-X %c", c);
	} else
		c = waitfor(2, "C-X ");

	if (c == CTL('G')) {
		PrefAbort("Prefix-2");
		return;
	}
	fp = pref2map[c];
	if (fp == 0)
		Unknown(invokingchar, c);
	else
		ExecFunc(fp, 0);
}

void
Yank()
{
	LINE	*line,
		*lp;
	BUFLOC	*dot;

	if (killbuf[killptr] == 0)
		complain("Nothing to yank!");
	lsave();
	this_cmd = YANKCMD;
	line = killbuf[killptr];
	lp = lastline(line);
	dot = DoYank(line, 0, lp, length(lp), curline, curchar, curbuf);
	SetMark();
	SetDot(dot);
}

static int	NumArg = 1;

GetFour(InputFunc)
int	(*InputFunc)();
{
	register int	c;

	do {
		NumArg *= 4;
	} while ((c = (*InputFunc)()) == CTL('U'));
	return c;
}

GetArg(InputFunc)
int	(*InputFunc)();
{
	register int	c,
			i = 0;

	if (!isdigit(c = (*InputFunc)()))
		return (c | 0200);

	do
		i = i * 10 + (c - '0');
	while ((c = getch()) >= '0' && c <= '9');
	NumArg = i;
	return c;
}

char *
jumpup(cp)
char	*cp;
{
	while (*cp && !isword(*cp))
		cp++;
	while (*cp && isword(*cp))
		cp++;
	return cp;
}

char *
backup(string, cp)
char	*string, *cp;
{
	while (cp > string && !isword(*(cp - 1)))
		cp--;
	while (cp > string && isword(*(cp - 1)))
		cp--;
	return cp;
}

#define ASKSIZE	132

char *
RunEdit(c, begin, cp, def, HowToRead)
register int	c;
register char	*begin;
register char	*cp;
char	*def;
int	(*HowToRead)();
{
	char	*tcp;

	switch (c) {
	case CTL('@'):
		break;

	case CTL('A'):
		cp = begin;
		break;

	case CTL('B'):
		if (cp > begin)
			--cp;
		break;

	case CTL('D'):
		if (!*cp)
			break;
		cp++;
		goto delchar;

	case CTL('E'):
		while (*cp)
			cp++;
		break;

	case CTL('F'):
		if (*cp)
			cp++;
		break;

	case CTL('G'):
		return (char *) 0;

	case CTL('K'):
		*cp = '\0';
		break;

	case CTL('N'):
	case CTL('P'):
		ArgIns(begin, c == CTL('N'));
		cp = begin + strlen(begin);
		break;

	case '\177':
	case CTL('H'):
delchar:
		if (cp == begin)
			break;
		strcpy(cp - 1, cp);
		cp--;
		break;

	case META('\177'):	/* Delete previous word */
	case CTL('W'):
	    {
		char	*bp = backup(begin, cp);

		strcpy(bp, cp);
		cp = bp;
	    }
		break;

	case META('D'):
	case META('d'):
		strcpy(cp, jumpup(cp));
		break;		/* Pretty neat huh */

	case META('B'):
	case META('b'):
		cp = backup(begin, cp);
		break;

	case META('F'):
	case META('f'):
		cp = jumpup(cp);
		break;

	case CTL('Y'):
	case CTL('R'):
		if (!def || !*def) {
			rbell();
			break;
		}
		tcp = def;
		while (c = *tcp++) {
			insert(c, begin, cp - begin, 1, LBSIZE);
			cp++;
		}
		break;

	case CTL('^'):
	case CTL('Q'):
		c = (*HowToRead)();
		/* Fall into... */

	default:
		if (c & 0200)
			rbell();
		else {
			insert(c, begin, cp - begin, 1, ASKSIZE);
			cp++;
		}
	}
	return cp;
}

NoMacGetc()
{
	int	c;

	redisplay();
	if ((c = jgetchar()) == EOF)
		finish(SIGHUP);
	return c & 0177;
}

extern int	Interactive;

/* VARARGS */

char *
ask(VA_T(char *def) VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char	*def)
	VA_D(char	*fmt)
	VA_LIST(ap)
	static char	string[ASKSIZE];
	char	*cp,
		*begin;	/* Beginning of real text */
	int	c;
	extern int	(*Getchar)();
	int	(*HowToRead)() = Interactive ? NoMacGetc : Getchar;

	VA_START(ap, fmt)
	VA_I(ap, char *, def)
	VA_I(ap, char *, fmt)
	vsprintf(string, fmt, ap);
	VA_END(ap)
	message(string);
	Asking = strlen(string);	/* Entirely for redisplay */
	begin = string + Asking;
	cp = begin;
	*cp = '\0';

	while ((c = (*HowToRead)()) != '\r' && c != '\n') {
		NumArg = 1;	/* If not otherwise specified */
		if (c == '\033')	/* Maybe a number */
			c = GetArg(HowToRead);
		else if (c == CTL('U'))
			c = GetFour(HowToRead);
		if (c == '\033')	/* Again */
			c = (*HowToRead)() | 0200;
		while (NumArg-- > 0)
			if ((cp = RunEdit(c, begin, cp, def, HowToRead)) == (char *)0)
				complain("Aborted");

		/* Show the change */
		message(string);
		Asking = cp - string;
	}
	Asking = 0;
	if (cp == begin && *cp == '\0')	/* Nothing was typed */
		if (def == 0)
			complain("No default");
		else
			return def;
	return begin;
}

void
YankPop()
{
	LINE	*line,
		*last;
	MARK	*mp = CurMark();
	BUFLOC	*dot;

	if (last_cmd != YANKCMD)
		complain("Yank something first!");

	lfreelist(reg_delete(mp->m_line, mp->m_char, curline, curchar));

	/* Now must find a recently killed region. */

	killptr--;
	for (;;) {
		if (killptr < 0)
			killptr = NUMKILLS - 1;
		if (killbuf[killptr])
			break;
		killptr--;
	}

	this_cmd = YANKCMD;

	line = killbuf[killptr];
	last = lastline(line);
	dot = DoYank(line, 0, last, length(last), curline, curchar, curbuf);
	MarkSet(CurMark(), curline, curchar);
	SetDot(dot);
}

void
WtModBuf()
{
	int	askp = exp_p;
	BUFFER	*oldb = curbuf,
		*bp;
	char	*yorn,
		name[LBSIZE];

	for (bp = world; bp; bp = bp->b_next) {
		if (bp->b_zero == 0 || !IsModified(bp) || IsScratch(bp))
			continue;
		SetBuf(bp);	/* Make this current buffer */
		if (Crashing) {
			strcpy(name, curbuf->b_name);
			insert('_', name, 0, 1, LBSIZE);
		} else {
			if (curbuf->b_fname == 0)
				setfname(curbuf, ask((char *) 0, "No file.  File to use: "));
			strcpy(name, curbuf->b_fname);
		}
		if (askp) {
			yorn = ask((char *) 0, "Write %s? ", curbuf->b_fname);
			if (*yorn != 'Y' && *yorn != 'y')
				continue;
		}
		file_write(name, 0);
		SetUnmodified(curbuf);
	}
	SetBuf(oldb);
}

void
NotModified()
{
	SetUnmodified(curbuf);
}

void
ArgIns(at, next)
char	*at;
{
	int	inc = next ? 1 : -1;

	if (*(argvp + inc) == 0)
		inc = 0;
	argvp += inc;
	strcpy(at, *argvp);
}

blnkp(buf)
register char	*buf;
{
	register char	c;

	while ((c = *buf++) && (c == ' ' || c == '\t'))
		;
	return c == 0;	/* It's zero if we got to the end of the line */
}

void
DelBlnkLines()
{
	exp = 1;
	if (!blnkp(linebuf) && !eolp())
		return;
	while (blnkp(linebuf) && curline->l_prev)
		SetLine(curline->l_prev);

	Eol();
	DelWtSpace();
	NextLine();
	while (blnkp(linebuf) && !eobp()) {
		DelWtSpace();
		DelNChar();
	}
	DelWtSpace();
}

int line_pos;

LINE *
next_line(line, num)
register LINE	*line;
register int	num;
{
	register int	i;

	if (line)
		for (i = 0; i < num && line->l_next; i++, line = line->l_next)
			;

	return line;
}

LINE *
prev_line(line, num)
register LINE	*line;
register int	num;
{
	register int	i;

	if (line)
		for (i = 0; i < num && line->l_prev; i++, line = line->l_prev)
			;

	return line;
}

void
ForChar()
{
	register int	num = exp;

	exp = 1;
	while (num--) {
		if (eolp()) {			/* Go to the next line */
			if (curline->l_next == 0)
				break;
			SetLine(curline->l_next);
		} else
			curchar++;
	}
}

void
BackChar()
{
	register int	num = exp;

	exp = 1;
	while (num--) {
		if (bolp()) {
			if (curline->l_prev == 0)
				break;
			SetLine(curline->l_prev);
			Eol();
		} else
			--curchar;
	}
}

void
NextLine()
{
	if (lastp(curline))
		OutOfBounds();
	down_line(1);
}

void
PrevLine()
{
	if (firstp(curline))
		OutOfBounds();
	down_line(0);
}

void
OutOfBounds()
{
	complain("");
}

void
down_line(down)
{
	LINE	*(*func)() = down ? next_line : prev_line;
	LINE	*line;

	line = (*func)(curline, exp);

	this_cmd = LINE_CMD;

	if (last_cmd != LINE_CMD)
		line_pos = calc_pos(linebuf, curchar);

	SetLine(line);		/* Curline is in linebuf now */
	curchar = how_far(curline, line_pos);
}

/* Returns what cur_char should be for that pos. */

how_far(line, ypos)
LINE	*line;
{
	register char	*pp;
	char	*base;
	register int	cur_char;
	char	c;
	register int	y;

	base = pp = getcptr(line, genbuf);

	cur_char = 0;
	y = 0;

	while (c = *pp++) {
		if (y >= ypos)
			return cur_char;
		if (c == 0177)
			y++;
		else if (c < ' ') {
			if (c == 011)
				y += ((tabstop - y % tabstop) - 1);
			else
				y++;
		}
		y++;
		cur_char++;
	}

	return pp - base - 1;
}

void
Bol()
{
	curchar = 0;
}

void
Eol()
{
	curchar += strlen(&linebuf[curchar]);
}

void
DotTo(line, col)
LINE	*line;
{
	BUFLOC	bp;

	bp.p_line = line;
	bp.p_char = col;
	SetDot(&bp);
}

/* If bp->p_line is != current line, then save current line.  Then set dot
   to bp->p_line, and if they weren't equal get that line into linebuf  */

void
SetDot(bp)
register BUFLOC	*bp;
{
	register int	notequal = bp->p_line != curline;

	if (notequal)
		lsave();
	curline = bp->p_line;
	curchar = bp->p_char;
	if (notequal)
		getDOT();
}

void
Eof()
{
	SetMark();
	SetLine(curbuf->b_dol);
	Eol();
}

void
Bof()
{
	SetMark();
	SetLine(curbuf->b_zero);
}

char	REsent[] = "[?.!]\"  *\\|[.?!]  *\\|[.?!][\"]*$";

void
Bos()
{
	int	num = exp;
	BUFLOC	*bp,
		save;

	exp = 1;
	while (num--) {
onceagain:
		bp = dosearch(REsent, -1, 1);
		DOTsave(&save);
		if (bp == 0) {
			Bof();
			break;
		}
		SetDot(bp);
		to_word(1);
		if (curline == save.p_line && curchar == save.p_char) {
			SetDot(bp);
			goto onceagain;
		}
	}
}

void
Eos()
{
	int	num = exp;
	BUFLOC	*bp;

	exp = 1;
	while (num-- && (bp = dosearch(REsent, 1, 1)))
		SetDot(bp);
	if (bp == 0)
		Eof();
	else
		to_word(1);
}

int
length(line)
register LINE	*line;
{
	register char	*base,
			*cp;

	base = cp = getcptr(line, genbuf);

	while (*cp++)
		;
	return cp - base - 1;
}

int
isword(c)
register char	c;
{
	return isalpha(c) || isdigit(c);
}

void
to_word(dir)
{
	register char	c;

	if (dir > 0) {
		while ((c = linebuf[curchar]) != 0 && !isword(c))
			curchar++;
		if (eolp()) {
			if (curline->l_next == 0)
				return;
			SetLine(curline->l_next);
			to_word(dir);
			return;
		}
	} else {
		while (!bolp() && (c = linebuf[curchar - 1], !isword(c)))
			--curchar;
		if (bolp()) {
			if (curline->l_prev == 0)
				return;
			SetLine(curline->l_prev);
			Eol();
			to_word(dir);
		}
	}
}

void
ForWord()
{
	register char	c;
	register int	num = exp;

	exp = 1;
	while (--num >= 0) {
		to_word(1);
		while ((c = linebuf[curchar]) != 0 && isword(c))
			curchar++;
		if (eobp())
			break;
	}
	this_cmd = 0;	/* Semi kludge to stop some unfavorable behavior */
}

void
BackWord()
{
	register int	num = exp;
	register char	c;

	exp = 1;
	while (--num >= 0) {
		to_word(-1);
		while (!bolp() && (c = linebuf[curchar - 1], isword(c)))
			--curchar;
		if (bobp())
			break;
	}
	this_cmd = 0;
}

/* End of window */

void
Eow()
{
	SetLine(next_line(curwind->w_top, SIZE(curwind) - 1 -
			min(SIZE(curwind) - 1, exp - 1)));
	if (!exp_p)
		Eol();
}

/* Beginning of window */

void
Bow()
{
	SetLine(next_line(curwind->w_top, min(SIZE(curwind) - 1, exp - 1)));
}

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_case.c

   contains case case-region and case-word functions.  */

void
DOTsave(buf)
BUFLOC *buf;
{
	buf->p_line = curline;
	buf->p_char = curchar;
}

void
CapChar()
{
	register int	num = exp;

	exp = 1;
	while (num--) {
		upper(&linebuf[curchar]);	/* Cap this word. */
		SetModified(curbuf);	/* Or else lsave won't do anything */
		makedirty(curline);
		if (eolp()) {			/* Go to the next line */
			if (curline->l_next == 0)
				break;
			SetLine(curline->l_next);
		} else
			curchar++;
	}
}

void
CapWord()
{
	int num = exp;

	exp = 1;	/* So all the commands are done once */

	while (num--) {
		to_word(1);	/* Go to the beginning of the next word. */
		if (eobp())
			break;
		upper(&linebuf[curchar]);	/* Cap this word. */
		SetModified(curbuf);	/* Or else lsave won't do anything */
		makedirty(curline);
		curchar++;
		while (!eolp() && isword(linebuf[curchar])) {
			lower(&linebuf[curchar]);
			curchar++;
		}
	}
}

void
case_word(up)
{
	BUFLOC bp;

	DOTsave(&bp);
	ForWord();	/* Go to end of the region */
	case_reg(bp.p_line, bp.p_char, curline, curchar, up);
}

void
upper(c)
register char	*c;
{
	if (*c >= 'a' && *c <= 'z')
		*c -= 040;
}

void
lower(c)
register char	*c;
{
	if (*c >= 'A' && *c <= 'Z')
		*c += 040;
}

void
case_reg(line1, char1, line2, char2, up)
LINE	*line1,
	*line2;
{
	char lbuf[LBSIZE];

	SetModified(curbuf);
	fixorder(&line1, &char1, &line2, &char2);
	lsave();
	ignore(getline(line1->l_dline, lbuf));
	for (;;) {
		if (line1 == line2 && char1 == char2)
			break;
		if (lbuf[char1] == '\0') {
			char1 = 0;
			line1->l_dline = putline(lbuf);
			makedirty(line1);
			if (lastp(line1))
				break;
			line1 = line1->l_next;
			ignore(getline(line1->l_dline, lbuf));
			continue;
		}
		if (up)
			upper(&lbuf[char1]);
		else
			lower(&lbuf[char1]);
		char1++;
	}
	line1->l_dline = putline(lbuf);
	makedirty(line1);
	getDOT();
}

void
CasRegLower()
{
	CaseReg(0);
}

void
CasRegUpper()
{
	CaseReg(1);
}

void
CaseReg(up)
{
	register MARK	*mp = CurMark();

	case_reg(curline, curchar, mp->m_line, mp->m_char, up);
}
