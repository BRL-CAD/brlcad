/*
 *			J O V E _ D R A W . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 1.1  2004/05/20 14:49:59  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.5  2000/08/25 03:19:49  mike
 *
 * Lint
 *
 * Revision 11.4  2000/08/24 23:12:22  mike
 *
 * lint, RCSid
 *
 * Revision 11.3  1997/01/03  17:42:17  jra
 * Mods for Irix 6.2
 *
 * Revision 11.2  1995/06/21  03:40:22  gwyn
 * Eliminated trailing blanks.
 * Eliminated blank line at EOF.
 * Use LBSIZE instead of hard-wired 100.
 * Fixed varargs functions, using VA_* macros.
 *
 * Revision 11.1  95/01/04  10:35:12  mike
 * Release_4.4
 *
 * Revision 10.4  93/10/26  06:33:04  mike
 * Changed printf() to jprintf() so that all modules could safely
 * use stdio.h
 *
 * Revision 10.3  93/10/26  06:01:39  mike
 * Changed getchar() to jgetchar() to prevent stdio.h conflict
 *
 * Revision 10.2  93/10/26  03:44:16  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:53:57  mike
 * Release_4.0
 *
 * Revision 2.4  91/08/30  18:59:46  mike
 * Modifications for clean compilation on the XMP
 *
 * Revision 2.3  91/08/30  18:11:00  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.2  91/08/30  17:54:31  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.1  91/08/30  17:49:04  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.0  84/12/26  16:45:54  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:07:48  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 4-19-83

   jove_draw.c

   This contains, among other things, the modeline formatting, and the
   message routines (for prompting).  */

#include "./jove.h"

#include "./termcap.h"

void	message();

char mesgbuf[LBSIZE];

char *
bufmod(bp)
BUFFER	*bp;
{
	if (IsModified(bp))
		return "* ";
	return "";
}

/*
 * Find_pos returns the position on the line, that c_char represents
 * in line.
 */

find_pos(line, c_char)
LINE	*line;
{
	register char	*lp;
	char	buf[LBSIZE];

	if (line == curline)
		lp = linebuf;
	else
		lp = getcptr(line, buf);
	return calc_pos(lp, c_char);
}

calc_pos(lp, c_char)
register char	*lp;
register int	c_char;
{
	register int	pos = 0;
	register char	c = 0;

	while ((--c_char >= 0) && (c = *lp++)) {
		if (c == '\t')
			pos += (tabstop - (pos % tabstop));
		else if (c == '\177' || c < ' ')
			pos += 2;
		else
			pos++;
 	}
	return pos;
}

static char	sp_line[LBSIZE];

/* VARARGS */

char *
sprint(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)

	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	vsprintf(sp_line, fmt, ap);
	VA_END(ap)
	return sp_line;
}

/* VARARGS */

void
s_mess(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)

	if (Input)
		return;
	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	vsprintf(mesgbuf, fmt, ap);
	VA_END(ap)
	message(mesgbuf);
}

/* VARARGS */

void
jprintf(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)

	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	vsprintf(sp_line, fmt, ap);
	putstr (sp_line);
	VA_END(ap)
}

/*
 * message prints the null terminated string onto the bottom line of the
 * terminal.
 */
void
message(str)
char	*str;
{
	if (Input)
		return;
	UpdMesg++;
	errormsg = 0;
	if (str != mesgbuf)
		strncpy(mesgbuf, str, sizeof mesgbuf);
}

/*
 * This prints all the information about the current mode, and the
 * current filename.
 */

#define strcop(lp,str)		for (--lp, pp = str; (*lp++ = *pp++); )

void
ModeLine(w)
WINDOW	*w;
{
	int	lineno = FLine(w) + SIZE(w),
		i;
	char	line[512];

	int	*flags = w->w_bufp->b_flags;

	register char	*lp = line + 1,		/* Pay attention to this! */
			*pp;
	static char	*modenames[] = {
		"TE-",
		"OV-",
		"C-",
		"RE-",
		"CIS-",
		"SM-",
		"AI-"
	};

	strcop(lp, "--- JOVE (");

	for (i = 0; i < NFLAGS; i++)
		if (IsFlagSet(flags, i))
			strcop(lp, modenames[i]);

	if (*(lp - 2) == '(')
		strcop(lp, "NORMAL) ");
	else {
		lp--;	/* Back over the '-' */
		strcop(lp, ") ");
	}
	strcop(lp, sprint("%d: %s", bufnumber(w->w_bufp), w->w_bufp->b_name));
	pp = w->w_bufp->b_fname ? sprint(" \"%s\" ", w->w_bufp->b_fname) : " [No file] ";
	strcop(lp, pp);
	if (IsModified(w->w_bufp))
		strcop(lp, "* ");
	if (IsReadOnly(w->w_bufp))
		strcop(lp, "[readonly] ");
	--lp;			/* strcop leaves lp after the null */
	i = CO - 2 - (2 * SG);	/* 2 space pad plus padding for magic cookies */
	while (lp < &line[i])
		*lp++ = '-';
	*lp = 0;

	i_set(lineno, 0);
	Placur(lineno, 0);
	if( SO ) putpad( SO, 1 );	/* Put mode line in standout */
	if (swrite(line))
		do_cl_eol(lineno);
	if( SO )
		putpad( SE, 1 );
}

bufnumber( cb )
BUFFER	*cb;		/* Current Buffer.	*/
{
    int		i;	/* Buffer Count.	*/
    BUFFER	*bp;	/* Pointer into buffer list..	*/

    for (i=1, bp=world;	bp; bp=bp->b_next) {
    	if (bp->b_zero == 0)
    	    continue;
	if (bp == cb)
	    return i;
	++i;
    }
    return -1;
}

void
RedrawDisplay()
{
	LINE	*newtop = prev_line(curwind->w_line, exp_p ?
				exp : HALF(curwind));

	if (newtop == curwind->w_top)
		v_clear(FLine(curwind), FLine(curwind) + SIZE(curwind));
	else
		SetTop(curwind, newtop);
}

void
v_clear(line1, line2)
register int	line1,
		line2;
{
	while (line1 <= line2) {
		i_set(line1, 0);
		cl_eol();
		oimage[line1].Line = nimage[line1].Line = 0;
		line1++;
	}
}

void
ClAndRedraw()
{
	cl_scr();
}

void
NextPage()
{
	LINE	*newline;

	if (exp_p)
		UpScroll();
	else {
		newline = next_line(curwind->w_top, max(1, SIZE(curwind) - 1));
		DotTo(newline, 0);
		if (in_window(curwind, curwind->w_bufp->b_dol) == -1)
			SetTop(curwind, newline);
		curwind->w_line = newline;
	}
}

void
PrevPage()
{
	LINE	*newline;

	if (exp_p)
		DownScroll();
	else {
		newline = prev_line(curwind->w_top, max(1, SIZE(curwind) - 1));
		DotTo(newline, 0);
		SetTop(curwind, curline);
		curwind->w_line = curwind->w_top;
	}
}


/* Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   interface.c

   contains the procedures to HELP the user by creating buffers with
   information in them, or temporarily writing over the user's text. */


#include "./termcap.h"

static WINDOW	*LastW;		/* Save old window here so we can return */

static int	Gobble,
		Wrapped,
		StartNo,
		LineNo;		/* Line number we have reached (if we are NOT
				   using buffers.  This way is MUCH easier since
				   all we have to do is zero out the oimage line
				   pointer and let redisplay() notice the change
				   and fix it. */

static int	WhichKind;	/* Buffers or screen? */

int	UseBuffers = 0;		/* Don't create buffers by default. It may
				   be useful sometimes to making listings */

#define WITHSCREEN	1
#define WITHBUFFER	2

/* Tell With Buffers sets everything up so that we can clean up after
   ourselves when we are told to. */

void
TellWBuffers(bname, clobber)
char	*bname;
{
	WhichKind = WITHBUFFER;		/* So we know how to clean up */
	LastW = curwind;
	pop_wind(bname, clobber);	/* This creates the window and
					   makes it the current window. */
}

/* Tell With Screen.  If gobble is non-zero we DON'T ungetc characters as
   they are typed  e.g. --more-- or at the end of the list. */

void
TellWScreen(gobble)
{
	WhichKind = WITHSCREEN;
	StartNo = LineNo = FLine(curwind);	/* Much easier, see what I mean! */
	Wrapped = 0;
	Gobble = gobble;
}

/* DoTell ... don't keep the user in suspense!

   Takes a string as an argument and displays it correctly, i.e. if we are
   using buffers simply insert the string into the buffer adding a newline.
   Otherwise we swrite the line and change oimage */

DoTell(string)
char	*string;
{
	if (WhichKind == WITHBUFFER) {
		exp = 1;
		ins_str(string);
		LineInsert();
		return OKAY;
	}

	if (LineNo == StartNo + curwind->w_height - 1) {
		int	c;

		Wrapped++;		/* We wrapped ... see StopTelling */
		LineNo = StartNo;
		message("--more--");
		UpdateMesg();
		switch (c = jgetchar()) {
		case ' ':
			break;

		case CTL('G'):
		case '\177':
			if (!Gobble)
				ignore(Ungetc(c));
			return ABORT;

		default:
			if (Gobble == 0)
				ignore(Ungetc(c));
			return STOP;
		}
		message("");
		UpdateMesg();
	}
	i_set(LineNo, 0);
	ignore(swrite(string));
	cl_eol();
	oimage[LineNo].Line = (LINE *) -1;
	LineNo++;
	flusho();
	return OKAY;
}

void
StopTelling()
{
	if (WhichKind == WITHBUFFER) {
		NotModified();
		SetWind(LastW);
		LastW = 0;
	} else {
		int	c;

		ignore(DoTell("----------"));
		c = jgetchar();
		if (c != ' ' && Gobble == 0)
			ignore(Ungetc(c));
	}
}
