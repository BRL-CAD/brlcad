/*
 *			J O V E _ B U F . C
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
 * Revision 11.3  1997/01/03  13:32:22  jra
 * Mods for Irix 6.2.
 *
 * Revision 11.2  1995/06/21  03:38:49  gwyn
 * Eliminated trailing blanks.
 * Changed memcpy calls back to bcopy.
 *
 * Revision 11.1  95/01/04  10:35:08  mike
 * Release_4.4
 *
 * Revision 10.3  94/09/17  04:57:35  butler
 * changed all calls to bcopy to be memcpy instead.  Useful for Solaris 5.2
 *
 * Revision 10.2  1993/10/26  03:40:29  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:53:54  mike
 * Release_4.0
 *
 * Revision 2.3  91/08/30  18:59:33  mike
 * Modifications for clean compilation on the XMP
 *
 * Revision 2.2  91/08/30  17:54:09  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.1  91/08/30  17:48:53  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.0  84/12/26  16:45:00  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:07:12  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/* Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_buf.c

   Contains commands that deal with creating, selecting, killing and
   listing buffers.  (And find-file) */

#include "./jove.h"

#include <sys/types.h>
#include <sys/stat.h>

char	*Mainbuf = "Main";

int	flen,
	blen;
BUFFER	*lastbuf = 0;	/* Last buffer we were in so we have a default
			 * buffer during a select buffer.
			 */
void	SetBuf();

char *
filename(bp)
BUFFER	*bp;
{
	return bp->b_fname ? bp->b_fname : "[No file]";
}

char *
itoa(num)
{
	static char	line[10];

	sprintf(line, "%d", num);
	return line;
}

int
AllInts(str)
register char	*str;
{
	register char	c;

	while ((c = *str++) >= '0' && c <= '9')
		;
	return c == 0;
}

BUFFER *
buf_exists(name)
char	*name;
{
	BUFFER	*bp;
	int	n = 0;

	if (name == 0)
		return 0;

	for (bp = world; bp; bp = bp->b_next)
		if (bp->b_zero != 0 && bp->b_name != 0 &&
					strcmp(bp->b_name, name) == 0)
			return bp;

	/* Doesn't match any names.  Try for a buffer number */

	if (AllInts(name) && (n = atoi(name)) > 0) {
		for (bp = world; bp; bp = bp->b_next) {
			if (bp == 0)
				break;
			if (bp->b_zero == 0)
				continue;
			if (--n == 0)
				return (bp);
		}
		return bp;
	}

	return 0;
}

BUFFER *
file_exists(fname)
char	*fname;
{
	struct stat	stbuf;
	BUFFER	*bp;

	if (fname) {
		if (stat(fname, &stbuf) == -1)
			stbuf.st_ino = -1;
		for (bp = world; bp; bp = bp->b_next) {
			if (bp->b_zero == 0)
				continue;
			if ((bp->b_ino != -1)
			  && (bp->b_ino == stbuf.st_ino)
			  && (bp->b_dev == stbuf.st_dev))
				return bp;
			if (bp->b_fname == 0)
				continue;
			if (strcmp(bp->b_fname, fname) == 0)
				return bp;
		}
	}
	return 0;
}

int
max(a, b)
int a, b;
{
	return a > b ? a : b;
}

BUFFER *
findbuf()
{
	BUFFER	*bp,
		*lastbp;

	lastbp = 0;
	for (bp = world; bp; lastbp = bp, bp = bp->b_next)
		if (bp->b_zero == 0)
			break;
	if (bp == 0) {
		bp = (BUFFER *) emalloc(sizeof (BUFFER));
		if (lastbp)
			lastbp->b_next = bp;
		else
			world = bp;
		bp->b_zero = 0;
		bp->b_next = 0;
	}
	return bp;
}

extern int
	OverWrite(),
	TextInsert(),
	SelfInsert(),
	DoParen(),
	CTab(),
	LineAI(),
	Newline();

void
setfuncs(flags)
int	*flags;
{
	UpdModLine++;	/* Kludge ... but speeds things up considerably */
	bcopy(flags, curbuf->b_flags, NFLAGS*sizeof(int));

	if (IsFlagSet(flags, OVERWRITE))
		BindInserts(OverWrite);
	else if (IsFlagSet(flags, TEXTFILL))
		BindInserts(TextInsert);
	else
		BindInserts(SelfInsert);
	if (IsFlagSet(flags, CMODE) || IsFlagSet(flags, MATCHING)) {
		BindFunc(mainmap, '}', DoParen);
		BindFunc(mainmap, ')', DoParen);
	} else {
		BindFunc(mainmap, '}', SelfInsert);
		BindFunc(mainmap, ')', SelfInsert);
	}
	if (IsFlagSet(flags, CMODE))
		BindFunc(mainmap, CTL('I'), CTab);
	else
		BindFunc(mainmap, CTL('I'), SelfInsert);
	if (IsFlagSet(flags, AUTOIND))
		BindFunc(mainmap, CTL('M'), LineAI);
	else
		BindFunc(mainmap, CTL('M'), Newline);
}

void
noflags(f)
register int	*f;
{
	register int	i = NFLAGS;

	while (i--)
		*f++ = 0;
}

void
setflags(buf)
BUFFER	*buf;
{
	bcopy(origflags, buf->b_flags, NFLAGS*sizeof(int));
	SetUnmodified(buf);
	ClrScratch(buf);	/* Normal until proven SCRATCHBUF */
}

BUFFER *
mak_buf(fname, bname)
char	*fname,
	*bname;
{
	register BUFFER	*freebuf;
	register int	i;

	freebuf = buf_exists(bname);
	if (!freebuf) {
		freebuf = findbuf();
		freebuf->b_fname = freebuf->b_name = 0;
		setbname(freebuf, bname);
		setfname(freebuf, fname);
		set_ino(freebuf);
		freebuf->b_marks = 0;
		freebuf->b_themark = 0;		/* Index into markring */
		freebuf->b_status = 0;
		for (i = 0; i < NMARKS; i++)
			freebuf->b_markring[i] = 0;
		/* No marks yet */
		setflags(freebuf);
		freebuf->b_zero = 0;
		initlist(freebuf);
	}
	return freebuf;
}

char *
ralloc(obj, size)
char	*obj;
size_t size;
{
	char	*new;
	if (obj)
		new = realloc(obj, (unsigned) size);
	else
		new = 0;
	if (new == 0 || !obj)
		new = emalloc(size);
	if (new == 0)
		error("No memory in ralloc");
	return new;
}

void
setbname(bp, name)
BUFFER	*bp;
char	*name;
{
	UpdModLine++;	/* Kludge ... but speeds things up considerably */
	if (name) {
		bp->b_name = ralloc(bp->b_name, strlen(name) + 1);
		strcpy(bp->b_name, name);
	} else
		bp->b_name = 0;
}

void
setfname(bp, name)
BUFFER	*bp;
char	*name;
{
	UpdModLine++;	/* Kludge ... but speeds things up considerably */
	if (name) {
		bp->b_fname = ralloc(bp->b_fname, strlen(name) + 1);
		strcpy(bp->b_fname, name);
	} else
		bp->b_fname = 0;
}

void
set_ino(bp)
BUFFER	*bp;
{
	struct stat	stbuf;

	if (bp->b_fname && stat(bp->b_fname, &stbuf) == -1)
		bp->b_ino = -1;
	else {
		bp->b_ino = stbuf.st_ino;
		bp->b_dev = stbuf.st_dev;
	}
}

/* Find the file `fname' into buf and put in in window `wp' */

BUFFER *
do_find(wp, fname)
WINDOW	*wp;
char	*fname;
{
	BUFFER	*oldb = curbuf,
		*bp;

	oldb = curbuf;
	bp = file_exists(fname);
	if (bp == 0) {
		bp = mak_buf(fname, (char *) 0);
		bufname(bp);
		SetBuf(bp);
		read_file(bp->b_fname);
		SetBuf(oldb);
	}
	if (wp)
		tiewind(wp, bp);
	return bp;
}

void
tiewind(wp, bp)
WINDOW	*wp;
BUFFER	*bp;
{
	wp->w_line = bp->b_dot;
	wp->w_char = bp->b_char;
	if (wp->w_bufp != bp) {
		wp->w_bufp = bp;
		CalcTop(wp);
	}
}

void
FindFile()
{
	char	*name;

	name = ask(curbuf->b_fname, FuncName());
	lastbuf = curbuf;
	SetBuf(do_find(curwind, name));
}

void
SetBuf(newbuf)
BUFFER	*newbuf;
{
	if (newbuf == curbuf)
		return;
	lastbuf = curbuf;
	bcopy(globflags, curbuf->b_flags, NFLAGS*sizeof(int));
	lsave();
	curbuf = newbuf;
	getDOT();
	bcopy(curbuf->b_flags, globflags, NFLAGS*sizeof(int));
	setfuncs(curbuf->b_flags);
}

void
SelBuf()
{
	char	*bname;

	bname = ask(lastbuf ? lastbuf->b_name : 0, FuncName());
	SetBuf(do_select(curwind, bname));
}

BUFFER *
do_select(wp, name)
WINDOW	*wp;
char	*name;
{
	BUFFER	*new;

	new = mak_buf((char *) 0, name);
	if (wp)
		tiewind(wp, new);
	return new;
}

void
defb_wind(bp)
BUFFER *bp;
{
	WINDOW	*wp = fwind;

	do {
		if (wp->w_bufp == bp) {
			if (bp == curbuf)
				ignore(do_select(wp, Mainbuf));
			else {
				WINDOW	*save = wp->w_next;

				del_wind(wp);
				wp = save->w_prev;
			}
		}
		wp = wp->w_next;
	} while (wp != fwind);
}

BUFFER *
AskBuf(prompt)
char	*prompt;
{
	register BUFFER	*delbuf;
	register char	*bname;

	bname = ask(curbuf->b_name, prompt);
	delbuf = buf_exists(bname);
	if (delbuf == 0) {
		complain("%s: no such buffer", bname);
		return delbuf;
	}

	/* You cannot delete "Main" */
	if (strcmp(delbuf->b_name, Mainbuf) == 0) {
		complain("You may not delete %s", Mainbuf);
		return 0;
	}
	if (IsModified(delbuf))
		confirm("%s modified, are you sure? ", bname);
	return delbuf;
}

void
BufErase()
{
	BUFFER	*delbuf;

	if (delbuf = AskBuf(FuncName()))
		initlist(delbuf);
	SetUnmodified(delbuf);
}

void
BufKill()
{
	BUFFER	*delbuf;

	if ((delbuf = AskBuf(FuncName())) == 0)
		return;
	defb_wind(delbuf);			/* Erase the windows */
	if (curbuf == delbuf)
		SetBuf(curwind->w_bufp);
	lfreelist(delbuf->b_zero);
	delbuf->b_zero = 0;
	delbuf->b_name = (char *)(-1);
	delbuf->b_fname = (char *)(-1);
	delbuf->b_ino = (-1);
	if (delbuf == lastbuf)
		lastbuf = curbuf;
	UpdModLine++;				/* Update buffer numbers */
}

void
BufList()
{
	char	format[40];
	int	bcount = 1;		/* To give each buffer a number */
	BUFFER	*bp;
	int	what;

	b_format(format);

	if (UseBuffers) {
		TellWBuffers("Buffer list", 0);
		SetScratch(curwind->w_bufp);
	} else
		TellWScreen(0);

	ignore(DoTell(sprint(format, "NO", "Buffer-type", "File-name", "Buffer-name", "")));
	ignore(DoTell(sprint(format, "--", "-----------", "---------", "-----------", "")));
	for (bp = world; bp; bp = bp->b_next) {
		if (bp->b_zero == 0)
			continue;
		what = DoTell(sprint(format, itoa(bcount++),
					IsScratch(bp) ?	"SCRATCH" : "FILE",
					filename(bp),
					bp->b_name,
					bufmod(bp)));
		if (what == ABORT || what == STOP)
			break;
	}
	if (UseBuffers) {
		Bof();		/* Go to the beginning of the file */
		NotModified();
	}
	StopTelling();
}

void
b_format(fmt)
char	*fmt;
{
	BUFFER	*bp;

	flen = 9;
	blen = 11;

	for (bp = world; bp; bp = bp->b_next) {
		if (bp->b_zero == 0)
			continue;
		flen = max(flen, (int)(bp->b_fname ? strlen(bp->b_fname) : 0));
		blen = max(blen, (int)strlen(bp->b_name));
	}
	ignore(sprintf(fmt, " %%-4s %%-11s  %%-%ds   %%-%ds  %%s", flen, blen));
}
