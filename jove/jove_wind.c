/*
 *			J O V E _ W I N D . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.2  1995/06/21  03:46:08  gwyn
 * Eliminated trailing blanks.
 *
 * Revision 11.1  95/01/04  10:35:25  mike
 * Release_4.4
 *
 * Revision 10.2  93/10/26  05:25:22  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:54:07  mike
 * Release_4.0
 *
 * Revision 2.4  91/09/23  03:15:30  mike
 * two  return / return(expr) warnings
 *
 * Revision 2.3  91/08/30  19:17:49  mike
 * Stardent ANSI lint
 *
 * Revision 2.2  91/08/30  18:11:09  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.1  91/08/30  17:54:41  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.0  84/12/26  16:49:18  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:09:58  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_wind.c

   This creates/deletes/divides/grows/shrinks windows.  */

#include "./jove.h"
#include "./termcap.h"

char	onlyone[] = "You only have one window!";
char	toosmall[] = "too small";

void	winit();
void	PrevWindow();
void	SetWind();

/* First line in a window */
int
FLine(w)
WINDOW	*w;
{
	WINDOW	*wp = fwind;
	int	lineno = -1;

	do {
		if (wp == w)
			return lineno + 1;
		lineno += wp->w_height;
		wp = wp->w_next;
	} while (wp != fwind);
	complain("WINDOW?");
	/* NOTREACHED */
	return 0;
}

void
initwinds(b)
BUFFER	*b;
{
	WINDOW	*wp = fwind;

	do {
		if (wp->w_bufp == b) {
			SetTop(wp, (wp->w_line = b->b_dot));
			zero_wind(wp);
		}
		wp = wp->w_next;
	} while (wp != fwind);
}

/* Return last window on the screen */

WINDOW *
lastwind()
{
	WINDOW *wp = fwind;

	do {
		if (wp->w_next == fwind)
			return wp;
		wp = wp->w_next;
	} while (wp != fwind);
	return fwind;
}

WINDOW *

getwind()
{
	WINDOW	*wp;

	wp = (WINDOW *)emalloc(sizeof (WINDOW));
	return wp;
}

/* Delete `wp' from the screen.  If it is the only window left
 * on the screen, then complain via error.  It gives its body
 * to the next window if there is one, otherwise the previous
 * window gets the body.  Resets link list and fwind if necessary.
 */

void
del_wind(wp)
WINDOW	*wp;
{
	WINDOW	*last = lastwind(),
		*prev = wp->w_prev;

	if (wp->w_next == wp)
		complain(onlyone);

	wp->w_prev->w_next = wp->w_next;
	wp->w_next->w_prev = wp->w_prev;

	if (fwind == wp) {
		fwind = wp->w_next;
		fwind->w_height += wp->w_height;
	} else if (wp == last)
		last->w_prev->w_height += wp->w_height;
	else
		prev->w_height += wp->w_height;
	if (curwind == wp)
		SetWind(prev);
	free((char *) wp);
}

/* Divide the `wp'.  Complains if `wp' is too small to be split.
 * It returns the new window
 */

WINDOW *
div_wind(wp)
WINDOW	*wp;
{
	WINDOW	*new;

	if (wp->w_height < 4)
		complain(toosmall);

	new = getwind();
	new->w_offset = 0;
	new->w_numlines = 0;
	/* Reset the window bounds */
	new->w_height = (wp->w_height / 2);
	wp->w_height -= new->w_height;

	/* Set the lines such that w_line is the center in each window */
	new->w_line = wp->w_line;
	new->w_bufp = wp->w_bufp;
	new->w_top = prev_line(new->w_line, HALF(new));

	/* Link the new window into the list */
	new->w_prev = wp;
	new->w_next = wp->w_next;
	new->w_next->w_prev = new;
	wp->w_next = new;
	return new;
}

/* Return one window previous to `wp'.  If at the first window
 * on screen, then go to the last window
 */

WINDOW *
prev_wind(wp)
WINDOW	*wp;
{
	return wp->w_prev;
}

/* Next window from `wp' */

WINDOW *
next_wind(wp)
WINDOW	*wp;
{
	return wp->w_next;
}

/* Initialze the first window setting the bounds to the size of the
 * screen.  There is no buffer with this window.  See parse for the
 * setting of this window.
 */
void
winit()
{
	curwind = fwind = getwind();

	curwind->w_line = curwind->w_top = (LINE *) 0;
	curwind->w_char = 0;
	curwind->w_next = curwind->w_prev = fwind;
	curwind->w_height = LI - 1;
}

/* Change window into the previous window.  curwind becomes the new
 * window
 */
void
PrevWindow()
{
	WINDOW	*new = prev_wind(curwind);

	if (new == curwind)
		complain(onlyone);
	SetWind(new);
}

/* Make new the current window */
void
SetWind(new)
WINDOW	*new;
{
	if (new == curwind)
		return;
	curwind->w_line = curline;
	curwind->w_char = curchar;
	curwind->w_bufp = curbuf;
	SetBuf(new->w_bufp);
	if (!inlist(new->w_bufp->b_zero, new->w_line)) {
		new->w_line = curline;
		new->w_char = curchar;
	}
	DotTo(new->w_line, new->w_char);
	if (curchar > (int)strlen(linebuf))
		new->w_char = curchar = strlen(linebuf);
	curwind = new;
}

/* Delete the current window if it isn't the only one left */

void
DelCurWindow()
{
	del_wind(curwind);
}

/* Return the number of windows being displayed right now */

numwindows()
{
	WINDOW	*wp = fwind;
	int	num = 0;

	do {
		num++;
		wp = wp->w_next;
	} while (wp != fwind);
	return num;
}

void
WindFind()
{
	char	*fname = ask((char *) 0, FuncName());
	BUFFER	*buf;

	if (buf = file_exists(fname))
		pop_wind(buf->b_name, 0);
	else {
		if (numwindows() == 1)
			curwind = div_wind(curwind);
		else
			curwind = next_wind(curwind);
		SetBuf(do_find(curwind, fname));
	}
}

/* Go into one window mode by deleting all the other windows */

void
OneWindow()
{
	while (curwind->w_next != curwind)
		del_wind(curwind->w_next);
}

/* Look for a window containing a buffer whose name is `name' */

WINDOW *
windlook(name)
char	*name;
{
	BUFFER	*bp = (BUFFER *) buf_exists(name);
	WINDOW	*wp = fwind;

	if (bp == 0)
		return 0;
	do {
		if (wp->w_bufp == bp)
			return wp;
		wp = wp->w_next;
	} while (wp != fwind);
	return 0;
}

/* Change window into the next window.  curwind becomes the new
 * window
 */

void
NextWindow()
{
	WINDOW	*new = next_wind(curwind);

	if (new == curwind)
		complain(onlyone);
	SetWind(new);
}

/* Scroll the next window */

void
PageNWind()
{
	if (numwindows() == 1)
		complain(onlyone);
	NextWindow();
	NextPage();
	PrevWindow();
}

/* Put a window with the buffer `name' in it.  Erase the buffer if
 * `clobber' is non-zero.
 */

void
pop_wind(name, clobber)
char	*name;
{
	WINDOW	*wp;
	BUFFER	*newb;

	if ((wp = windlook(name)) == 0) {
		if (numwindows() == 1)
			SplitWind();
		else
			PrevWindow();
	} else
		SetWind(wp);

	newb = do_select((WINDOW *) 0, name);
	if (clobber)
		initlist(newb);
	tiewind(curwind, newb);
	SetBuf(newb);
}

void
GrowWindow()
{
	WindSize(curwind, abs(exp));
}

void
ShrWindow()
{
	WindSize(curwind, -abs(exp));
}

/* Change the size of the window by inc.  First arg is the window,
 * second is the increment.
 */

void
WindSize(w, inc)
register WINDOW	*w;
{
	if (numwindows() == 1)
		complain(onlyone);

	if (inc < 0) {	/* Shrinking this window */
		if (w->w_height + inc < 2)
			complain(toosmall);
		w->w_height += inc;
		w->w_prev->w_height -= inc;
	} else
		WindSize(w->w_next, -inc);
}

/* Set the topline of the window, calculating its number in the buffer.
 * This is for numbering the lines only.
 */

void
SetTop(w, line)
WINDOW	*w;
register LINE	*line;
{
	register LINE	*lp = w->w_bufp->b_zero;
	register int	num = 0;

	w->w_top = line;
	if (w->w_numlines)
		while (lp) {
			num++;
			if (line == lp)
				break;
			lp = lp->l_next;
		}
	w->w_topnum = num;
}

void
WNumLines()
{
	zero_wind(curwind);
	/* So the redisplay will know to update the screen even if it
	 * looks like there are no differences.
	 */

	curwind->w_numlines = !curwind->w_numlines;
	SetTop(curwind, curwind->w_top);
}

void
zero_wind(wp)
register WINDOW	*wp;
{
	register int	i,
			upper;

	upper = FLine(wp);
	for (i = 0; i < wp->w_height; i++)
		oimage[upper + i].Line = (LINE *) -1;
}

/* Return the line number that `line' occupies in `windes' */

in_window(windes, line)
register WINDOW	*windes;
register LINE	*line;
{
	register int	i;
	LINE	*top = windes->w_top;

	for (i = 0; top && i < windes->w_height - 1; i++, top = top->l_next)
		if (top == line)
			return FLine(windes) + i;
	return -1;
}
