/*                            H M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file Hm.c
 *
 * This code is derived in part from menuhit(9.3) in AT&T 9th Edition
 * UNIX, Version 1 Programmer's Manual.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "bio.h"
#include "bu.h"

#include "./Sc.h"
#include "./Hm.h"
#include "./Mm.h"
#include "./extern.h"


#define ErLog brst_log

#define HmDEBUG 0

#ifndef Max
#  define Max(_a, _b)	((_a)<(_b)?(_b):(_a))
#  define Min(_a, _b)	((_a)>(_b)?(_b):(_a))
#endif

#define HmRingbell()	(void) putchar('\07'),  (void) fflush(stdout)

/* Keys for manipulating menus. */
#define Ctrl(c_)	((c_)&037)
#define M_DEBUG		Ctrl('?')
#define M_DOWN		'd'
#define M_HELP		'h'
#define M_MYXMOUSE	Ctrl('X')
#define M_NOSELECT	'q'
#define M_REDRAW	Ctrl('L')
#define M_SELECT	' '
#define M_UP		'u'


/* Alternate keys for conformance to standard conventions. */
#define A_UP		Ctrl('P')
#define A_DOWN		Ctrl('N')
#define A_HELP		'?'

#define P_OFF		(0)
#define P_ON		(1)
#define P_FORCE		(1<<1)

#define PutMenuChar(_c, _co, _ro, _map, _bit) {\
	static int lro = -1, lco = -1;\
	if ((_map) & (_bit) || (_bit) == 0) {\
		if (lco++ != (_co)-1 || lro != (_ro)) {\
			(void) ScMvCursor(_co, _ro);\
			lco = _co;\
		}\
		(void) putchar((_c));\
		}\
	(_bit) <<= 1;\
	(_co)++;\
}


static int HmDirty = 0;
static int HmMyxflag = 0;
static int HmPkgInit = 0;

static HmWindow *windows = NULL;

#define HmENTRY (itemp-win->menup->item)
#define HmHEIGHT Min(win->height, HmMaxVis)
typedef struct nmllist HmLList;
struct nmllist
{
    HmItem *itemp;
    HmLList *next;
};


/*
  void HmBanner(char *pgmname, int borderchr)

  Print program name and row of border characters to delimit the top
  of the scrolling region.
*/
void
HmBanner(const char *pgmname, int borderchr)
{
    int column;
    char *p;
#define HmBUFLEN 81
    static char HmPgmName[HmBUFLEN] = "No name";
    static int HmBorderChr = '_';
    if (pgmname != NULL) {
	bu_strlcpy(HmPgmName, pgmname, sizeof(HmPgmName));
	HmBorderChr = borderchr;
    }
    (void) ScMvCursor(HmLftMenu, HmYBORDER);
    for (column = 1; column <= 3; column++)
	(void) putc(HmBorderChr, stdout);
    for (p = HmPgmName; column <= ScCO && *p != '\0'; column++, p++)
	(void) putc((int)(*p), stdout);
    for (; column <= ScCO; column++)
	(void) putc(HmBorderChr, stdout);
    return;
}


/*
  void HmPrntItem(HmItem *itemp)	(DEBUG)

  Print contents of itemp.
*/
static void
HmPrntItem(HmItem *itemp)
{
    (void) ErLog("\t\tHmPrntItem(0x%x)\n", itemp);
    for (; itemp->text != (char *) NULL; itemp++) {
	(void) ErLog("\t\t\ttext=\"%s\"\n", itemp->text);
	(void) ErLog("\t\t\thelp=\"%s\"\n", itemp->help == (char *) NULL ? "(null)" : itemp->help);
	(void) ErLog("\t\t\tnext=0x%x\n", itemp->next);
	(void) ErLog("\t\t\tdfn=0x%x\n", itemp->dfn);
	(void) ErLog("\t\t\tbfn=0x%x\n", itemp->bfn);
	(void) ErLog("\t\t\thfn=0x%x\n", itemp->hfn);
	(void) ErLog("\t\t\tdata=%d\n", itemp->data);
	(void) ErLog("\t\t\t----\n");
    }
    (void) ErLog("\t\t\ttext=0x%x\n", itemp->text);
    return;
}


/*
  void HmPrntMenu(menup)	(DEBUG)

  Print "windows" stack.
*/
static void
HmPrntMenu(HmMenu *menup)
{
    (void) ErLog("\tHmPrntMenu(0x%x)\n", menup);
    (void) ErLog("\t\tgenerator=0x%x\n", menup->generator);
    (void) ErLog("\t\tprevtop=%d\n", menup->prevtop);
    (void) ErLog("\t\tprevhit=%d\n", menup->prevhit);
    (void) ErLog("\t\tsticky=%s\n", menup->sticky ? "true" : "false");
    HmPrntItem(menup->item);
    return;
}


/*
  void HmPrntWindows(void)	(DEBUG)

  Print "windows" stack.
*/
static void
HmPrntWindows(void)
{
    HmWindow *win;
    (void) ErLog("HmPrntWindows()\n");
    for (win = windows; win != (HmWindow *) NULL; win = win->next) {
	(void) ErLog("\twin=0x%x\n", win);
	(void) ErLog("\tmenup=0x%x\n", win->menup);
	(void) ErLog("\tmenux=%d\n", win->menux);
	(void) ErLog("\tmenuy=%d\n", win->menuy);
	(void) ErLog("\twidth=%d\n", win->width);
	(void) ErLog("\theight=%d\n", win->height);
	(void) ErLog("\tdirty=0x%x\n", win->dirty);
	(void) ErLog("\tnext=0x%x\n", win->next);
	HmPrntMenu(win->menup);
    }
    return;
}


/*
  void HmPrntLList(HmLList *listp)	(DEBUG)

  Print all HmItem's in listp.
*/
static void
HmPrntLList(HmLList *listp)
{
    if (listp == (HmLList *) NULL)
	return;
    HmPrntItem(listp->itemp);
    HmPrntLList(listp->next);
    return;
}


/*
  void HmFreeItems(Hmitem *itemp)

  Free storage (allocated with malloc) for an array of HmItem's.
*/
static void
HmFreeItems(HmItem *itemp)
{
    HmItem *citemp;
    int count;
    for (citemp = itemp, count = 1;
	 citemp->text != (char *) NULL;
	 citemp++, count++
	) {
	MmStrFree(citemp->text);
	if (citemp->help != (char *) NULL)
	    MmStrFree(citemp->help);
    }
    MmVFree(count, HmItem, itemp);
    return;
}


/*
  void HmFreeLList(HmLList *listp)

  Free storage (allocated with malloc) for a linked-list of
  HmItem's.
*/
static void
HmFreeLList(HmLList *listp)
{
    HmLList *tp;
    for (; listp != (HmLList *) NULL; listp = tp) {
	MmFree(HmItem, listp->itemp);
	tp = listp->next;
	MmFree(HmLList, listp);
    }
    return;
}


/*
  void HmPutItem(HmWindow *win, HmItem *itemp, int flag)

  Display menu entry itemp.

  Win is the menu control structure for itemp.

  Flag is a bit flag and the following bits are meaningful:

  P_FORCE means draw the entire entry regardless of the
  value of the dirty bitmap.
  P_ON means this entry is current so highlight it.
*/
static void
HmPutItem(HmWindow *win, HmItem *itemp, int flag)
{
    int label_len = strlen(itemp->text);
    static char buf[HmMAXLINE];
    char *p = buf;
    int col = win->menux;
    int row = win->menuy+
	(HmENTRY-win->menup->prevtop)+1;
    int width = win->width;
    int bitmap = flag & P_FORCE ?
	~0 : win->dirty[row-win->menuy];
    /*	~0 : win->dirty[HmENTRY+1];*/
    int bit = 1;
    int writemask = 0;
    if (bitmap == 0)
	return;
    if (itemp->text[0] & 0200) {
	/* right-justified */
	int i;
	label_len--;
	for (i = 0; i < width - label_len; i++)
	    *p++ = itemp->text[0] & 0177;
	for (i = 1; itemp->text[i] != '\0'; i++)
	    *p++ = itemp->text[i];
    } else				/* left-justified */
	if (itemp->text[label_len-1] & 0200) {
	    int i;
	    label_len--;
	    for (i = 0; !(itemp->text[i] & 0200); i++)
		*p++ = itemp->text[i];
	    for (; i < width; i++)
		*p++ = itemp->text[label_len] & 0177;
	} else {
	    /* centered */
	    int i, j;
	    for (i = 0; i < (width - label_len)/2; i++)
		*p++ = ' ';
	    for (j = 0; itemp->text[j] != '\0'; j++)
		*p++ = itemp->text[j];
	    for (i += j; i < width; i++)
		*p++ = ' ';
	}
    *p = '\0';

    PutMenuChar('|', col, row, bitmap, bit);
    if (flag & P_ON)
	(void) ScSetStandout();
    else
	(void) ScClrStandout();

    /* Optimized printing of entry. */
    if (bitmap == ~0) {
	(void) fputs(buf, stdout);
	col += p-buf;
	bit <<= p-buf;
    } else {
	int i;
	for (i = 0; i < p-buf; i++)
	    writemask |= 1<<(i+1);
	for (i = 0; i < p-buf; i++) {
	    if ((bitmap & writemask) == writemask)
		break;
	    writemask &= ~bit;
	    PutMenuChar(buf[i], col, row, bitmap, bit);
	}
	if (i < p-buf) {
	    (void) ScMvCursor(col, row);
	    (void) fputs(&buf[i], stdout);
	    col += (p-buf) - i;
	    bit <<= (p-buf) - i;
	}
    }

    if (flag & P_ON)
	(void) ScClrStandout();
    PutMenuChar('|', col, row, bitmap, bit);
    return;
}


/*
  void HmPutBorder(HmWindow *win, row, char mark)

  Draw the horizontal border for row of win->menup using mark
  for the corner characters.
*/
static void
HmPutBorder(HmWindow *win, int row, char mark)
{
    int i;
    int bit = 1;
    int col = win->menux;
    int bitmap = win->dirty[row - win->menuy];
    static char buf[HmMAXLINE];
    char *p = buf;
    if (bitmap == 0)
	return; /* No dirty bits. */
    *p++ = mark;
    for (i = 0; i < win->width; i++)
	*p++ = '-';
    *p++ = mark;
    *p = '\0';
    if (bitmap == ~0) {
	/* All bits dirty. */
	(void) ScMvCursor(col, row);
	(void) fputs(buf, stdout);
    } else {
	for (i = 0; i < p - buf; i++)
	    PutMenuChar(buf[i], col, row, bitmap, bit);
    }
    return;
}


/*
  void HmSetbit(HmWindow *win, int col, int row)

  Mark as dirty, the bit in win->dirty that corresponds to
  col and row of the screen.
*/
static void
HmSetbit(HmWindow *win, int col, int row)
{
    int bit = col - win->menux;
#if HmDEBUG && 0
    (void) ErLog("HmSetbit:menu{<%d, %d>, <%d, %d>}col=%d, row=%d\n",
		 win->menux, win->menux+win->width+1,
		 win->menuy, win->menuy+HmHEIGHT+1,
		 col, row
	);
#endif
    win->dirty[row-win->menuy] |= bit == 0 ? 1 : 1 << bit;
#if HmDEBUG && 0
    (void) ErLog("\tdirty[%d]=0x%x\r\n",
		 row-win->menuy, win->dirty[row-win->menuy]
	);
#endif
    return;
}


/*
  void HmClrmap(HmWindow *win)

  Mark as clean, the entire dirty bitmap for win.
*/
static void
HmClrmap(HmWindow *win)
{
    int row;
    int height = HmHEIGHT;
    for (row = 0; row <= height+1; row++)
	win->dirty[row] = 0;
    return;
}


/*
  void HmSetmap(HmWindow *win)

  Mark as dirty the entire dirty bitmap for win.
*/
static void
HmSetmap(HmWindow *win)
{
    int row;
    int height = HmHEIGHT;
    for (row = 0; row <= height+1; row++)
	win->dirty[row] = ~0; /* 0xffff... */
    return;
}


/*
  HmWindow *HmInWin(x, y, HmWindow *win)

  Return pointer to top window in stack, starting with win whose
  boundaries contain the screen coordinate <x, y>.  If the point
  is outside of all these windows, return 0.
*/
static HmWindow *
HmInWin(int x, int y, HmWindow *win)
{
#if HmDEBUG && 0
    if (win != (HmWindow *) NULL)
	(void) ErLog("HmInWin:x=%d y=%d win{<%d, %d>, <%d, %d>}\r\n",
		     x, y,
		     win->menux, win->menux+win->width+1,
		     win->menuy, win->menuy+HmHEIGHT+1
	    );
#endif
    for (; win != (HmWindow *) NULL; win = win->next) {
	int height = HmHEIGHT;
	if (! (x < win->menux || x > win->menux + win->width + 1 ||
	       y < win->menuy || y > win->menuy + height + 1)
	    )
	    return win;
    }
    return (HmWindow *) NULL;
}


/*
  void HmDrawWin(HmWindow *win)

  Draw win->menup on screen.  Actually, only characters flagged as
  dirty are drawn.
*/
static void
HmDrawWin(HmWindow *win)
{
    HmItem *itemp;
    int height;

#if HmDEBUG && 1
    (void) ErLog("HmDrawWin:win{<%d, %d>, <%d, %d>}\r\n",
		 win->menux, win->menux+win->width+1,
		 win->menuy, win->menuy+HmHEIGHT+1
	); {
	int i;
	for (i = 0; i <= HmHEIGHT+1; i++)
	    (void) ErLog("\tdirty[%d]=0x%x\r\n", i, win->dirty[i]);
    }
#endif
    HmPutBorder(win, win->menuy, win->menup->prevtop > 0 ? '^' : '+');
    for (itemp = win->menup->item + win->menup->prevtop;
	 HmENTRY-win->menup->prevtop < HmMaxVis && itemp->text != (char *) NULL;
	 itemp++
	)
	HmPutItem(win, itemp,
		  HmENTRY == win->menup->prevhit ? P_ON : P_OFF
	    );
    height = HmHEIGHT;
    HmPutBorder(win, win->menuy+height+1, HmENTRY < win->height ? 'v' : '+');
    HmClrmap(win);
    (void) fflush(stdout);
    return;
}


/*
  void HmHelp(HmWindow *win, int entry)

  Display help message for item indexed by entry in win->menup
  on line HmYCOMMO.  This message will be erased when the user
  strikes a key (or uses the mouse).
*/
static void
HmHelp(HmWindow *win, int entry)
{
    (void) ScMvCursor(HmLftMenu, HmYCOMMO);
    (void) ScClrEOL();
    (void) ScSetStandout();
    (void) printf("%s", win->menup->item[entry].help);
    (void) ScClrStandout();
    (void) fflush(stdout);
    return;
}


/*
  void HmError(const char *str)

  Display str on line HmYCOMMO.
*/
void
HmError(const char *str)
{
    (void) ScMvCursor(HmLftMenu, HmYCOMMO);
    (void) ScClrEOL();
    (void) ScSetStandout();
    (void) fputs(str, stdout);
    (void) ScClrStandout();
    (void) fflush(stdout);
    return;
}


/*
  void HmLiftWin(HmWindow *win)

  Remove win->menup from screen, marking any occluded portions
  of other menus as dirty so that they will be redrawn by HmHit().
*/
static void
HmLiftWin(HmWindow *win)
{
    int row, col;
    int lastcol = -1, lastrow = -1;
    int endcol = win->menux + win->width + 2;
    int endrow = win->menuy +
	HmHEIGHT + HmHGTBORDER;
#if HmDEBUG && 1
    (void) ErLog("HmLiftWin:win{<%d, %d>, <%d, %d>}\r\n",
		 win->menux, win->menux+win->width+1,
		 win->menuy, win->menuy+HmHEIGHT+1
	);
#endif
    for (row = win->menuy; row < endrow; row++) {
	for (col = win->menux; col < endcol; col++) {
	    HmWindow *olwin;
	    if ((olwin = HmInWin(col, row, win->next))
		!= (HmWindow *) NULL
		) {
		HmSetbit(olwin, col, row);
		HmDirty = 1;
	    } else {
		if (lastcol != col-1 || lastrow != row)
		    (void) ScMvCursor(col, row);
		lastcol = col; lastrow = row;
		(void) putchar(' ');
	    }
	}
    }
    (void) fflush(stdout);
    return;
}


/*
  void HmPushWin(HmWindow *win)

  Add window to top of "windows" stack.
*/
static void
HmPushWin(HmWindow *win)
{
    win->next = windows;
    windows = win;
    return;
}


/*
  void HmPopWin(HmWindow *win)

  Delete window from top of "windows" stack.
*/
static void
HmPopWin(HmWindow *win)
{
    windows = win->next;
    return;
}


/*
  void HmRefreshWin(HmWindow *win)

  Draw any dirty portions of all windows in stack starting at win.
*/
static void
HmRefreshWin(HmWindow *win)
{
    if (win == (HmWindow *) NULL) {
	HmDirty = 0;
	return;
    }
    HmRefreshWin(win->next);
    HmDrawWin(win);
    return;
}
/*
  void HmRedraw(void)

  Force a redraw of all active menus.
*/
void
HmRedraw(void)
{
    HmWindow *win;
    int reset = 0;

#if HmDEBUG && 1
    HmPrntWindows();
#endif
    (void) ScClrText();	/* clear entire screen */

    /* See if we changed the maximum items displayed parameter. */
    if (HmMaxVis != HmLastMaxVis)
	reset = 1;
    for (win = windows; win != (HmWindow *) NULL; win = win->next) {
	if (reset) {
	    /* Correct window to reflect new maximum. */
	    /* Reset scrolling state-info in each window. */
	    if (win->menup->prevhit >= HmMaxVis)
		win->menup->prevtop = win->menup->prevhit -
		    HmMaxVis + 1;
	    else
		win->menup->prevtop = 0;
	    /* Must reallocate "dirty" bit map to fit new size. */
	    MmVFree(Min(win->height, HmLastMaxVis)+HmHGTBORDER,
		    int, win->dirty);
	    if ((win->dirty =
		 MmVAllo(HmHEIGHT+HmHGTBORDER, int)
		    ) == NULL) {
		return;
	    }
	}
	HmSetmap(win); /* force all bits on in "dirty" bitmap */
    }
    HmLastMaxVis = HmMaxVis;
    HmRefreshWin(windows);	/* redisplay all windows */
    HmBanner((char *) NULL, 0);	/* redraw banner */
    return;
}


/*
  void HmTtySet(void)

  Set up terminal handler and MYX-menu options for menu interaction.
*/
void
HmTtySet(void)
{
    set_Cbreak(HmTtyFd);
    clr_Echo(HmTtyFd);
    clr_Tabs(HmTtyFd);
    if (HmMyxflag) {
	/* Send escape codes to push current toggle settings,
	   and turn on "editor ptr". */
	(void) fputs("\033[?1;2Z", stdout);
	(void) fputs("\033[?1;00000010000Z", stdout);
	(void) fflush(stdout);
    }
    return;
}


/*
  void HmTtyReset(void)

  Reset terminal handler and MYX-menu options to user settings.
*/
void
HmTtyReset(void)
{
    if (HmMyxflag) {
	/* Send escape codes to pop old toggle settings. */
	(void) fputs("\033[?1;3Z", stdout);
	(void) fflush(stdout);
    }
    reset_Tty(HmTtyFd);
    return;
}


/*
  void HmInit(int x, int y, int maxvis)

  Initialize position of top-level menu.  Specify maximum
  number of menu items visable at once.  Place these values
  in global variables.  Determine as best we can whether MYX
  is available and place int result in HmMyxflag.  Return
  true for success and false for failure to open "/dev/tty".
*/
int
HmInit(int x, int y, int maxvis)
{
    if ((HmTtyFd = open("/dev/tty", O_RDONLY)) == (-1)
	||	(HmTtyFp = fdopen(HmTtyFd, "r")) == NULL
	) {
	HmError("Can't open /dev/tty for reading.");
	return 0;
    }
    save_Tty(HmTtyFd);
    HmPkgInit = 1;
    HmLftMenu = x;
    HmTopMenu = y;
    HmMaxVis = maxvis;

    return 1;
}


/*
  void HmWidHgtMenu(HmWindow *win)

  Determine width and height of win->menup, and store in win.
*/
static void
HmWidHgtMenu(HmWindow *win)
{
    HmItem *itemp;
    
    /* Determine width of menu, allowing for border.		*/
    for (itemp = win->menup->item; itemp->text != (char *) NULL; itemp++) {
	int len = 0;
	int i;
	for (i = 0; itemp->text[i] != '\0'; i++) {
	    if (! (itemp->text[i] & 0200))
		len++;
	}
	win->width = Max(win->width, len);
    }
    win->height = HmENTRY;
    return;
}


/*
  int HmFitMenu(HmWindow *nwin, HmWindow *cwin)

  If nwin->menup will fit below cwin->menup on screen, store
  position in nwin, and return 1.  Otherwise, return 0.
*/
static int
HmFitMenu(HmWindow *nwin, HmWindow *cwin)
{
    if (cwin == (HmWindow *) NULL)
	return 0;
    else
	if (HmFitMenu(nwin, cwin->next))
	    return 1;
	else
	    /* Look for space underneath this menu.				*/
	    if (cwin->menux + nwin->width + 1 <= ScCO
		&&	cwin->menuy + cwin->height + nwin->height + HmHGTBORDER
		< HmMaxVis + HmTopMenu
		) {
		nwin->menux = cwin->menux;
		nwin->menuy = cwin->menuy + cwin->height + HmHGTBORDER - 1;
		return 1;
	    } else {
		return 0;
	    }
}


/*
  void HmPosMenu(HmWindow *win)

  Find best screen position for win->menup.
*/
static void
HmPosMenu(HmWindow *win)
{
    /* Determine origin (top-left corner) of menu.			*/
    if (win->next != (HmWindow *) NULL) {
	win->menux = win->next->menux + win->next->width + 1;
	win->menuy = win->next->menuy;
	if (win->menux + win->width + 2 > ScCO) {
	    if (! HmFitMenu(win, win->next)) {
		/* No space, so overlap top-level menu.	*/
		win->menux = HmLftMenu;
		win->menuy = HmTopMenu;
	    }
	}
    } else {
	/* Top-level menu. */
	win->menux = HmLftMenu;
	win->menuy = HmTopMenu;
    }
    return;
}


/*
  void HmMyxMouse(int *x, int *y)

  Read and decode screen coordinates from MYX "editor ptr".
  Implicit return in x and y.
*/
static void
HmMyxMouse(int *x, int *y)
{
    int c;

    c = HmGetchar();
    switch (c) {
	case Ctrl('A') :
	    *x = HmGetchar() - ' ' + 96;
	    break;
	case Ctrl('B') :
	    *x = HmGetchar() - ' ' + 192;
	    break;
	default :
	    *x = c - ' ';
	    break;
    }
    c = HmGetchar();
    switch (c) {
	case Ctrl('A') :
	    *y = HmGetchar() - ' ' + 96;
	    break;
	case Ctrl('B') :
	    *y = HmGetchar() - ' ' + 192;
	    break;
	default :
	    *y = c - ' ';
	    break;
    }
    (*x)++;
    (*y)++;
    return;
}


/*
  HmItem *HmHit(HmMenu *menup)

  Present menup to the user and return a pointer to the selected
  item, or 0 if there was no selection made.  For more details,
  see "Hm.h".
*/
HmItem *
HmHit(HmMenu *menup)
{
    HmItem *itemp;
    HmItem *retitemp = NULL;
    HmWindow *win;
    int done = 0;
    int dynamic = 0;
    static int HmLevel = 0;

#if HmDEBUG
    ErLog("HmHit(0x%x)\n", menup);
#endif
    if (HmPkgInit == 0) {
	HmInit(HmLftMenu, HmTopMenu, HmMaxVis);
	HmPkgInit = 1;
    }
    if (++HmLevel == 1)
	HmTtySet();

    /* If generator function is provided, dynamically allocate the
       menu items.
    */
    if ((dynamic = (menup->item == (HmItem *) NULL))) {
	int i;
	HmItem *gitemp;
	HmLList llhead, **listp;
	for (i = 0, listp = &llhead.next;
	     ;
	     i++,   listp = &(*listp)->next
	    ) {
	    if ((*listp = MmAllo(HmLList)) == NULL
		||	((*listp)->itemp = MmAllo(HmItem)) == NULL
		) {
		goto clean_exit;
	    }
	    itemp = (*listp)->itemp;
	    if ((gitemp = (*menup->generator)(i)) == (HmItem *) NULL) {
		itemp->text = (char *) NULL;
		(*listp)->next = (HmLList *) NULL;
		break;
	    }
	    if (gitemp->text != (char *) NULL) {
		if ((itemp->text = MmStrDup(gitemp->text))
		    == (char *) NULL
		    ) {
		    goto clean_exit;
		}
	    } else
		itemp->text = (char *) NULL;
	    if (gitemp->help != (char *) NULL) {
		if ((itemp->help = MmStrDup(gitemp->help))
		    == (char *) NULL
		    ) {
		    goto clean_exit;
		}
	    } else
		itemp->help = (char *) NULL;
	    itemp->next = gitemp->next;
	    itemp->dfn = gitemp->dfn;
	    itemp->bfn = gitemp->bfn;
	    itemp->hfn = gitemp->hfn;
	    itemp->data = gitemp->data;
#if HmDEBUG && 0
	    HmPrntItem(itemp);
#endif
	}
#if HmDEBUG && 0
	HmPrntLList(llhead.next);
#endif
	/* Steal the field that the user isn't using temporarily to
	   emulate the static allocation of menu items.
	*/
	if (i > 0) {
	    int ii;
	    HmLList *lp;
	    if ((menup->item = MmVAllo(i+1, HmItem)) == NULL) {
		goto clean_exit;
	    }
	    for (ii = 0, lp = llhead.next;
		 lp != (HmLList *) NULL;
		 ii++,   lp = lp->next
		)
		menup->item[ii] = *lp->itemp;
	}
	HmFreeLList(llhead.next);
	if (i == 0) /* Zero items, so return NULL */
	    goto clean_exit;
    }
    if ((win = MmAllo(HmWindow)) == NULL) {
#if HmDEBUG
	ErLog("HmHit, BUG: memory pool possibly corrupted.\n");
#endif
	goto clean_exit;
    }
    win->menup = menup;
    win->width = 0;
    HmPushWin(win);
    HmWidHgtMenu(win);
    HmPosMenu(win);

    if (menup->prevhit < 0 || menup->prevhit >= win->height)
	menup->prevhit = 0;
    itemp = &menup->item[menup->prevhit];

    if ((win->dirty = MmVAllo(HmHEIGHT+HmHGTBORDER, int)) == NULL) {
	goto clean_exit;
    }
    if (HmDirty)
	HmRefreshWin(windows);
    HmSetmap(win);
    HmDrawWin(win);
    while (! done) {
	int c;
	if (HmDirty)
	    HmRefreshWin(windows);
	(void) ScMvCursor(HmXPROMPT, HmYPROMPT);
	(void) ScClrEOL();
	(void) ScMvCursor(win->menux+win->width+2,
			  win->menuy+(HmENTRY-win->menup->prevtop)+1);
	(void) fflush(stdout);
	c = HmGetchar();
	(void) ScMvCursor(HmLftMenu, HmYCOMMO);
	(void) ScClrEOL();
	switch (c) {
	    case M_UP :
	    case A_UP :
		if (HmENTRY == 0)
		    HmRingbell();
		else {
		    HmPutItem(win, itemp, P_OFF | P_FORCE);
		    itemp--;
		    menup->prevhit = HmENTRY;
		    if (HmENTRY < win->menup->prevtop) {
			win->menup->prevtop -=
			    HmENTRY > HmMaxVis/2 ?
			    HmMaxVis/2+1 : HmENTRY+1;
			HmSetmap(win);
			HmDrawWin(win);
		    } else
			HmPutItem(win, itemp, P_ON | P_FORCE);
		}
		break;
	    case M_DOWN :
	    case A_DOWN :
		if (HmENTRY >= win->height-1)
		    HmRingbell();
		else {
		    HmPutItem(win, itemp, P_OFF | P_FORCE);
		    itemp++;
		    menup->prevhit = HmENTRY;
		    if (HmENTRY - win->menup->prevtop >= HmMaxVis) {
			win->menup->prevtop +=
			    win->height-HmENTRY > HmMaxVis/2 ?
			    HmMaxVis/2 : win->height-HmENTRY;
			HmSetmap(win);
			HmDrawWin(win);
		    } else
			HmPutItem(win, itemp, P_ON | P_FORCE);
		}
		break;
	    case M_MYXMOUSE : {
		static int mousex, mousey;
		HmItem *lastitemp;
		if (HmGetchar() != Ctrl('_') || HmGetchar() != '1')
		    goto m_badinput;
		HmMyxMouse(&mousex, &mousey);
		if (HmInWin(mousex, mousey, win) != win) {
		    /* Mouse cursor outside of menu. */
		    HmRingbell();
		    break;
		}
		if (mousey == win->menuy && win->menup->prevtop == 0) {
		    /* Top border of menu and can't scroll. */
		    goto m_noselect;
		}
		if (mousey == win->menuy + HmHEIGHT + 1
		    &&	win->height <= HmMaxVis + win->menup->prevtop
		    ) {
		    /* Bottom border of menu and can't scroll. */
		    HmRingbell();
		    break;
		}
		lastitemp = itemp;
		itemp = win->menup->item +
		    win->menup->prevtop +
		    (mousey - (win->menuy + 1));
		if (itemp == lastitemp)
		    /* User hit item twice in a row, so select it. */
		    goto m_select;
		HmPutItem(win, lastitemp, P_OFF | P_FORCE);
		menup->prevhit = HmENTRY;
		if (HmENTRY - win->menup->prevtop >= HmMaxVis) {
		    win->menup->prevtop +=
			win->height-HmENTRY > HmMaxVis/2 ?
			HmMaxVis/2 : win->height-HmENTRY;
		    HmSetmap(win);
		    HmDrawWin(win);
		} else
		    if (HmENTRY < win->menup->prevtop) {
			win->menup->prevtop -=
			    HmENTRY > HmMaxVis/2 ?
			    HmMaxVis/2+1 : HmENTRY+1;
			HmSetmap(win);
			HmDrawWin(win);
		    } else {
			HmPutItem(win, itemp, P_ON | P_FORCE);
		    }
		break;
	    }
	    case M_HELP :
	    case A_HELP :
		HmHelp(win, HmENTRY);
		break;
		m_select :
	    case M_SELECT :
		    if (itemp->next != (HmMenu *) NULL) {
			HmItem *subitemp;
			if (itemp->dfn != (void (*)()) NULL) {
			    int level = HmLevel;
			    HmTtyReset();
			    HmLevel = 0;
			    (*itemp->dfn)(itemp);
			    HmLevel = level;
			    HmTtySet();
			}
			subitemp = HmHit(itemp->next);
			if (itemp->bfn != (void (*)()) NULL) {
			    int level = HmLevel;
			    HmTtyReset();
			    HmLevel = 0;
			    (*itemp->bfn)(itemp);
			    HmLevel = level;
			    HmTtySet();
			}
			if (subitemp != (HmItem *) NULL) {
			    retitemp = subitemp;
			    done = ! menup->sticky;
			}
		    } else {
			retitemp = itemp;
			if (itemp->hfn != (void (*)()) NULL) {
			    int level = HmLevel;
			    HmTtyReset();
			    HmLevel = 0;
			    (*itemp->hfn)(itemp);
			    HmLevel = level;
			    HmTtySet();
			}
			done = ! menup->sticky;
		    }
		break;
		m_noselect :
	    case M_NOSELECT :
		    done = 1;
		break;
	    case M_REDRAW :
		HmRedraw();
		break;
	    case M_DEBUG :
		HmPrntWindows();
		break;
		m_badinput :
	    default :
		    HmError("Type 'd' down, 'u' up, 'h' help, <space> to select, 'q' no selection.");
		break;
	}
	(void) fflush(stdout);
    }
    /* Free storage of dynamic menu.				*/
    if (dynamic) {
	if (retitemp != (HmItem *) NULL) {
	    /* Must make copy of item we are returning. */
	    static HmItem dynitem;
	    dynitem = *retitemp;
	    retitemp = &dynitem;
	}
	HmFreeItems(menup->item);
	menup->item = 0;
    }

    HmLiftWin(win);
    HmPopWin(win);
    MmVFree(HmHEIGHT+HmHGTBORDER, int, win->dirty);
    MmFree(HmWindow, win);
    clean_exit :
	if (HmLevel-- == 1)
	    HmTtyReset();
    return retitemp;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
