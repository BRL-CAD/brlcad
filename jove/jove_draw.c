/*
 *			J O V E _ D R A W . C 
 *
 * $Revision$
 *
 * $Log$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 4-19-83
  
   jove_draw.c

   This contains, among other things, the modeline formatting, and the
   message routines (for prompting).  */

#include "jove.h"

#include "termcap.h"

char mesgbuf[100];

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
	register char	c;

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

/*
 * message prints the null terminated string onto the bottom line of the
 * terminal.
 */

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

ModeLine(w)
WINDOW	*w;
{
	int	lineno = FLine(w) + SIZE(w),
		i;
	char	line[132];

	int	*flags = w->w_bufp->b_flags;

	char	*printf();			/* Forward reference */
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

	i_set(lineno, 0);

#ifdef INCONSISTENT
	if (w->w_next != fwind)
		strcop(lp, "--- ");
	else
		strcop(lp, "    ");
#endif
	strcop(lp, "--- ");

	strcop(lp, "JOVE (");

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
	--lp;	/* strcop leaves lp after the null */
#ifdef INCONSISTENT
	if (w->w_next != fwind)
#endif
		while (lp < &line[CO - 2])
			*lp++ = '-';
	*lp = 0;
	if( SO ) putpad( SO, 1 );	/* Put mode line in standout */
	if (swrite(line))
		do_cl_eol(lineno);
	if( SO ) putpad( SE, 1 );
}

bufnumber( cb )
BUFFER	*cb;		/* Current Buffer.	*/
{
    int		i;	/* Buffer Count.	*/
    BUFFER	*bp;	/* Pointer into buffer list..	*/

    for (i=1, bp=world;	bp; bp=bp->b_next) {
	if (bp == cb)
	    return i;
	if (bp->b_zero != 0)
	    ++i;
    }
    return -1;
}

RedrawDisplay()
{
	LINE	*newtop = prev_line(curwind->w_line, exp_p ?
				exp : HALF(curwind));

	if (newtop == curwind->w_top)
		v_clear(FLine(curwind), FLine(curwind) + SIZE(curwind));
	else
		SetTop(curwind, newtop);
}

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

ClAndRedraw()
{
	cl_scr();
}

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

int	VisBell = 0;	/* If set, use visible bell if available */
int	RingBell = 0;	/* So if we have a lot of errors ...
			   ring the bell only ONCE */

rbell()
{
	RingBell++;
}

/* Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   format.c

   contains procedures that call _doprnt */

/* VARARGS2 */

format(buf, fmt, args)
char	*buf,
	*fmt;
int	*args;
{
	IOBUF	strbuf;

	strbuf.io_flag = 0;
	strbuf.io_base = strbuf.io_ptr = buf;
	strbuf.io_cnt = 32767;
	_doprnt(fmt, args, &strbuf);
	Putc('\0', &strbuf);
}

/* VARARGS1 */

char *
sprint(fmt, args)
char	*fmt;
{
	static char line[100];

	format(line, fmt, &args);
	return line;
}

/* VARARGS1 */

char *
printf(fmt, args)
char	*fmt;
{
	_doprnt(fmt, &args, &termout);
}

/* VARARGS2 */

char *
sprintf(str, fmt, args)
char	*str,
	*fmt;
{
	format(str, fmt, &args);
	return str;
}

/* VARARGS1 */

s_mess(fmt, args)
char	*fmt;
{
	if (Input)
		return;
	format(mesgbuf, fmt, &args);
	message(mesgbuf);
}

_strout(string, count, adjust, file, fillch)
register char *string;
register int count;
int adjust;
register IOBUF	*file;
{

	while (adjust < 0) {
		if (*string=='-' && fillch=='0') {
			Putc(*string++, file);
			count--;
		}
		Putc(fillch, file);
		adjust++;
	}
	while (--count >= 0)
		Putc(*string++, file);
	while (adjust) {
		Putc(fillch, file);
		adjust--;
	}
}

/* Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   interface.c

   contains the procedures to HELP the user by creating buffers with
   information in them, or temporarily writing over the user's text. */


#include "termcap.h"

static char	*BufToUse;	/* Buffer to pop to if we are using buffers */
static WINDOW	*LastW;		/* Save old window here so we can return */
static BUFFER	*LastB;		/* Buffer that used to be in LastW (in case it
				   isn't when we're done. */

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

TellWBuffers(bname, clobber)
char	*bname;
{
	WhichKind = WITHBUFFER;		/* So we know how to clean up */
	BufToUse = bname;
	LastW = curwind;
	LastB = curbuf;
	pop_wind(bname, clobber);	/* This creates the window and
					   makes it the current window. */
}

/* Tell With Screen.  If gobble is non-zero we DON'T ungetc characters as
   they are typed  e.g. --more-- or at the end of the list. */

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
		switch (c = getchar()) {
		case ' ':
			break;

		case CTL(G):
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

StopTelling()
{
	if (WhichKind == WITHBUFFER) {
		NotModified();
		SetWind(LastW);
		LastW = 0;
	} else {
		int	c;

		ignore(DoTell("----------"));
		c = getchar();
		if (c != ' ' && Gobble == 0)
			ignore(Ungetc(c));
	}
}

