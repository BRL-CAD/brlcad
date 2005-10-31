/*                            H M . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file Hm.c
	Author:	Gary S. Moss
		U. S. Army Ballistic Research Laboratory
		Aberdeen Proving Ground
		Maryland 21005-5066

	$Header$ (BRL)

	This code is derived in part from menuhit(9.3) in AT&T 9th Edition UNIX,
		Version 1 Programmer's Manual.

*/
/*LINTLIBRARY*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifndef DEBUG
#define NDEBUG
#define STATIC static
#else
#define STATIC
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "./Sc.h"
#include "./Hm.h"
#include "./Mm.h"
#include "./extern.h"
#define ErLog	brst_log
#if __STDC__
extern void	exit( int s );
extern unsigned alarm( unsigned s );
#else
extern void	exit();
extern unsigned alarm();
#endif

#ifndef _POSIX_SOURCE
#if __STDC__
extern FILE    *fdopen(int, const char *);
#else
extern FILE    *fdopen();
#endif
#endif

#define HmDEBUG		false

#ifndef Max
#define Max(_a,_b)	((_a)<(_b)?(_b):(_a))
#define Min(_a,_b)	((_a)>(_b)?(_b):(_a))
#endif
#define HmRingbell()	(void) putchar( '\07' ),  (void) fflush( stdout )

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

#define  PutMenuChar(_c,_co,_ro,_map,_bit)\
		{	static int	lro = -1, lco = -1;\
		if( (_map) & (_bit) || (_bit) == 0 )\
			{\
			if( lco++ != (_co)-1 || lro != (_ro) )\
				{\
				(void) ScMvCursor( _co, _ro );\
				lco = _co;\
				}\
			(void) putchar( (_c) );\
			}\
		(_bit) <<= 1;\
		(_co)++;\
		}\

static bool	HmDirty = false;
static bool	HmMyxflag = false;
static bool	HmPkgInit = false;

static HmWindow	*windows = NULL;

#define HmENTRY		(itemp-win->menup->item)
#define HmHEIGHT	Min(win->height,HmMaxVis)
typedef struct nmllist HmLList;
struct nmllist
	{
	HmItem	*itemp;
	HmLList	*next;
	};

/*
	void    HmBanner( char *pgmname, int borderchr )

	Print program name and row of border characters to delimit the top
		of the scrolling region.
 */
void
#if __STDC__
HmBanner( char *pgmname, int borderchr )
#else
HmBanner( pgmname, borderchr )
char *pgmname;
int borderchr;
#endif
	{       register int    co;
		register char   *p;
#define HmBUFLEN	81
		static char     HmPgmName[HmBUFLEN] = "No name";
		static int      HmBorderChr = '_';
	if( pgmname != NULL )
		{
		(void) strncpy( HmPgmName, pgmname, HmBUFLEN );
		HmBorderChr = borderchr;
		}
	(void) ScMvCursor( HmLftMenu, HmYBORDER );
	for( co = 1; co <= 3; co++ )
		(void) putc( HmBorderChr, stdout );
	for( p = HmPgmName; co <= ScCO && *p != '\0'; co++, p++ )
		(void) putc( (int)(*p), stdout );
	for( ; co <= ScCO; co++ )
		(void) putc( HmBorderChr, stdout );
	return;
	}

/*
	void	HmPrntItem( HmItem *itemp )	(DEBUG)

	Print contents of itemp.
 */
STATIC void
#if __STDC__
HmPrntItem( HmItem *itemp )
#else
HmPrntItem( itemp )
register HmItem	*itemp;
#endif
	{
	(void) ErLog( "\t\tHmPrntItem(0x%x)\n", itemp );
	for( ; itemp->text != (char *) NULL; itemp++ )
		{
		(void) ErLog( "\t\t\ttext=\"%s\"\n", itemp->text );
		(void) ErLog( "\t\t\thelp=\"%s\"\n", itemp->help == (char *) NULL ? "(null)" : itemp->help );
		(void) ErLog( "\t\t\tnext=0x%x\n", itemp->next );
		(void) ErLog( "\t\t\tdfn=0x%x\n", itemp->dfn );
		(void) ErLog( "\t\t\tbfn=0x%x\n", itemp->bfn );
		(void) ErLog( "\t\t\thfn=0x%x\n", itemp->hfn );
		(void) ErLog( "\t\t\tdata=%d\n", itemp->data );
		(void) ErLog( "\t\t\t----\n" );
		}
	(void) ErLog( "\t\t\ttext=0x%x\n", itemp->text );
	return;
	}

/*
	void	HmPrntMenu( menup )	(DEBUG)

	Print "windows" stack.
 */
STATIC void
#if __STDC__
HmPrntMenu( HmMenu *menup )
#else
HmPrntMenu( menup )
HmMenu	*menup;
#endif
	{
	(void) ErLog( "\tHmPrntMenu( 0x%x )\n", menup );
	(void) ErLog( "\t\tgenerator=0x%x\n", menup->generator );
	(void) ErLog( "\t\tprevtop=%d\n", menup->prevtop );
	(void) ErLog( "\t\tprevhit=%d\n", menup->prevhit );
	(void) ErLog( "\t\tsticky=%s\n", menup->sticky ? "true" : "false" );
	HmPrntItem( menup->item );
	return;
	}

/*
	void	HmPrntWindows( void )	(DEBUG)

	Print "windows" stack.
 */
STATIC void
#if __STDC__
HmPrntWindows( void )
#else
HmPrntWindows()
#endif
	{	register HmWindow	*win;
	(void) ErLog( "HmPrntWindows()\n" );
	for( win = windows; win != (HmWindow *) NULL; win = win->next )
		{
		(void) ErLog( "\twin=0x%x\n", win );
		(void) ErLog( "\tmenup=0x%x\n", win->menup );
		(void) ErLog( "\tmenux=%d\n", win->menux );
		(void) ErLog( "\tmenuy=%d\n", win->menuy );
		(void) ErLog( "\twidth=%d\n", win->width );
		(void) ErLog( "\theight=%d\n", win->height );
		(void) ErLog( "\tdirty=0x%x\n", win->dirty );
		(void) ErLog( "\tnext=0x%x\n", win->next );
		HmPrntMenu( win->menup );
		}
	return;
	}

/*
	void	HmPrntLList( HmLList *listp )	(DEBUG)

	Print all HmItem's in listp.
 */
STATIC void
#if __STDC__
HmPrntLList( HmLList *listp )
#else
HmPrntLList( listp )
HmLList	*listp;
#endif
	{
	if( listp == (HmLList *) NULL )
		return;
	HmPrntItem( listp->itemp );
	HmPrntLList( listp->next );
	return;
	}

/*
	void	HmFreeItems( Hmitem *itemp )

	Free storage (allocated with malloc) for an array of HmItem's.
 */
STATIC void
#if __STDC__
HmFreeItems( HmItem *itemp )
#else
HmFreeItems( itemp )
HmItem	*itemp;
#endif
	{	register HmItem	*citemp;
		register int	count;
	for(	citemp = itemp, count = 1;
		citemp->text != (char *) NULL;
		citemp++, count++
		)
		{
		MmStrFree( citemp->text );
		if( citemp->help != (char *) NULL )
			MmStrFree( citemp->help );
		}
	MmVFree( count, HmItem, itemp );
	return;
	}

/*
	void	HmFreeLList( HmLList *listp )

	Free storage (allocated with malloc) for a linked-list of
	HmItem's.
 */
STATIC void
#if __STDC__
HmFreeLList( HmLList *listp )
#else
HmFreeLList( listp )
register HmLList	*listp;
#endif
	{	register HmLList	*tp;
	for( ; listp != (HmLList *) NULL; listp = tp )
		{
		MmFree( HmItem, listp->itemp );
		tp = listp->next;
		MmFree( HmLList, listp );
		}
	return;
	}

/*
	void	HmPutItem( register HmWindow *win, register HmItem *itemp, int flag )

	Display menu entry itemp.

	Win is the menu control structure for itemp.

	Flag is a bit flag and the following bits are meaningful:

		P_FORCE means draw the entire entry regardless of the
			value of the dirty bitmap.
		P_ON	means this entry is current so highlight it.
 */
STATIC void
#if __STDC__
HmPutItem( register HmWindow *win, register HmItem *itemp, int flag )
#else
HmPutItem( win, itemp, flag )
register HmWindow	*win;
register HmItem		*itemp;
int			flag;
#endif
	{	register int	label_len = strlen( itemp->text );
		static char	buf[HmMAXLINE];
		register char	*p = buf;
		register int	col = win->menux;
		register int	row = win->menuy+
					(HmENTRY-win->menup->prevtop)+1;
		register int	width = win->width;
		register int	bitmap = flag & P_FORCE ?
					~0 : win->dirty[row-win->menuy];
				/*	~0 : win->dirty[HmENTRY+1];*/
		register int	bit = 1;
		register int	writemask = 0;
	if( bitmap == 0 )
		return;
	if( itemp->text[0] & 0200 )	/* right-justified */
		{	register int	i;
		label_len--;
		for( i = 0; i < width - label_len; i++ )
			*p++ = itemp->text[0] & 0177;
		for( i = 1; itemp->text[i] != '\0'; i++ )
			*p++ = itemp->text[i];
		}
	else				/* left-justified */
	if( itemp->text[label_len-1] & 0200 )
		{	register int	i;
		label_len--;
		for( i = 0; !(itemp->text[i] & 0200); i++ )
			*p++ = itemp->text[i];
		for( ; i < width; i++ )
			*p++ = itemp->text[label_len] & 0177;
		}
	else				/* centered */
		{	register int	i, j;
		for( i = 0; i < (width - label_len)/2; i++ )
			*p++ = ' ';
		for( j = 0; itemp->text[j] != '\0'; j++ )
			*p++ = itemp->text[j];
		for( i += j; i < width; i++ )
			*p++ = ' ';
		}
	*p = '\0';

	PutMenuChar( '|', col, row, bitmap, bit );
	if( flag & P_ON )
	 	(void) ScSetStandout();
	else
		(void) ScClrStandout();

	/* Optimized printing of entry.				*/
	{
	if( bitmap == ~0 )
		{
		(void) fputs( buf, stdout );
		col += p-buf;
		bit <<= p-buf;
		}
	else
		{	register int	i;
		for( i = 0; i < p-buf; i++ )
			writemask |= 1<<(i+1);
		for( i = 0; i < p-buf; i++ )
			{
			if( (bitmap & writemask) == writemask )
				break;
			writemask &= ~bit;
			PutMenuChar( buf[i], col, row, bitmap, bit );
			}
		if( i < p-buf )
			{
			(void) ScMvCursor( col, row );
			(void) fputs( &buf[i], stdout );
			col += (p-buf) - i;
			bit <<= (p-buf) - i;
			}
		}
	}

	if( flag & P_ON )
		(void) ScClrStandout();
	PutMenuChar( '|', col, row, bitmap, bit );
	return;
	}

/*
	void	HmPutBorder( register HmWindow *win, register row, char mark )

	Draw the horizontal border for row of win->menup using mark
	for the corner characters.
 */
STATIC void
#if __STDC__
HmPutBorder( register HmWindow *win, register int row, char mark )
#else
HmPutBorder( win, row, mark )
register HmWindow	*win;
register int		row;
char			mark;
#endif
	{	register int	i;
		register int	bit = 1;
		register int	col = win->menux;
		register int	bitmap = win->dirty[row - win->menuy];
		static char	buf[HmMAXLINE];
		register char	*p = buf;
	if( bitmap == 0 )
		return; /* No dirty bits. */
	*p++ = mark;
	for( i = 0; i < win->width; i++ )
		*p++ = '-';
	*p++ = mark;
	*p = '\0';
	if( bitmap == ~0 )
		{	/* All bits dirty. */
		(void) ScMvCursor( col, row );
		(void) fputs( buf, stdout );
		}
	else
		{
		for( i = 0; i < p - buf; i++ )
			PutMenuChar( buf[i], col, row, bitmap, bit );
		}
	return;
	}

/*
	void	HmSetbit( register HmWindow *win, int col, int row )

	Mark as dirty, the bit in win->dirty that corresponds to
	col and row of the screen.
*/
STATIC void
#if __STDC__
HmSetbit( register HmWindow *win, int col, int row )
#else
HmSetbit( win, col, row )
register HmWindow	*win;
int			col, row;
#endif
	{	register int	bit = col - win->menux;
#if HmDEBUG && false
	(void) ErLog(	"HmSetbit:menu{<%d,%d>,<%d,%d>}col=%d,row=%d\n",
			win->menux, win->menux+win->width+1,
			win->menuy, win->menuy+HmHEIGHT+1,
			col, row
			);
#endif
	win->dirty[row-win->menuy] |= bit == 0 ? 1 : 1 << bit;
#if HmDEBUG && false
	(void) ErLog( 	"\tdirty[%d]=0x%x\r\n",
			row-win->menuy, win->dirty[row-win->menuy]
			);
#endif
	return;
	}

/*
	void	HmClrmap( HmWindow *win )

	Mark as clean, the entire dirty bitmap for win.
 */
STATIC void
#if __STDC__
HmClrmap( HmWindow *win )
#else
HmClrmap( win )
HmWindow	*win;
#endif
	{	register int	row;
		register int	height = HmHEIGHT;
	for( row = 0; row <= height+1; row++ )
		win->dirty[row] = 0;
	return;
	}

/*
	void	HmSetmap( HmWindow *win )

	Mark as dirty the entire dirty bitmap for win.
 */
STATIC void
#if __STDC__
HmSetmap( HmWindow *win )
#else
HmSetmap( win )
HmWindow	*win;
#endif
	{	register int	row;
		register int	height = HmHEIGHT;
	for( row = 0; row <= height+1; row++ )
		win->dirty[row] = ~0; /* 0xffff... */
	return;
	}

/*
	HmWindow *HmInWin( register  x, register y, register HmWindow *win )

	Return pointer to top window in stack, starting with win whose
	boundaries contain the screen coordinate <x,y>.  If the point
	is outside of all these windows, return 0.
 */
STATIC HmWindow	*
#if __STDC__
HmInWin( register int x, register int y, register HmWindow *win )
#else
HmInWin( x, y, win )
register int		x, y;
register HmWindow	*win;
#endif
	{
#if HmDEBUG && false
	if( win != (HmWindow *) NULL )
		(void) ErLog(	"HmInWin:x=%d y=%d win{<%d,%d>,<%d,%d>}\r\n",
				x, y,
				win->menux, win->menux+win->width+1,
				win->menuy, win->menuy+HmHEIGHT+1
				);
#endif
	for( ; win != (HmWindow *) NULL; win = win->next )
		{	register int	height = HmHEIGHT;
		if( !	(x < win->menux || x > win->menux + win->width + 1 ||
			 y < win->menuy || y > win->menuy + height + 1)
			)
			return	win;
		}
	return	(HmWindow *) NULL;
	}

/*
	void	HmDrawWin( register HmWindow *win )

	Draw win->menup on screen.  Actually, only characters flagged as
	dirty are drawn.
 */
STATIC void
#if __STDC__
HmDrawWin( register HmWindow *win )
#else
HmDrawWin( win )
register HmWindow	*win;
#endif
	{	register HmItem	*itemp;
		int	height;
#if HmDEBUG && true
	(void) ErLog(	"HmDrawWin:win{<%d,%d>,<%d,%d>}\r\n",
			win->menux, win->menux+win->width+1,
			win->menuy, win->menuy+HmHEIGHT+1
			);
	{	register int	i;
	for( i = 0; i <= HmHEIGHT+1; i++ )
		(void) ErLog( "\tdirty[%d]=0x%x\r\n", i, win->dirty[i] );
	}
#endif
	HmPutBorder( win, win->menuy, win->menup->prevtop > 0 ? '^' : '+' );
	for(	itemp = win->menup->item + win->menup->prevtop;
		HmENTRY-win->menup->prevtop < HmMaxVis && itemp->text != (char *) NULL;
		itemp++
		)
		HmPutItem(	win, itemp,
				HmENTRY == win->menup->prevhit ? P_ON : P_OFF
				);
	height = HmHEIGHT;
	HmPutBorder( win, win->menuy+height+1, HmENTRY < win->height ? 'v' : '+' );
	HmClrmap( win );
	(void) fflush( stdout );
	return;
	}

/*
	void	HmHelp( register HmWindow *win, int entry )

	Display help message for item indexed by entry in win->menup
	on line HmYCOMMO.  This message will be erased when the user
	strikes a key (or uses the mouse).
 */
STATIC void
#if __STDC__
HmHelp( register HmWindow *win, int entry )
#else
HmHelp( win, entry )
register HmWindow	*win;
int	entry;
#endif
	{
	(void) ScMvCursor( HmLftMenu, HmYCOMMO );
	(void) ScClrEOL();
	(void) ScSetStandout();
	(void) printf( "%s", win->menup->item[entry].help );
	(void) ScClrStandout();
	(void) fflush( stdout );
	return;
	}

/*
	void	HmError( char *str )

	Display str on line HmYCOMMO.
 */
void
#if __STDC__
HmError( char *str )
#else
HmError( str )
char	*str;
#endif
	{
	(void) ScMvCursor( HmLftMenu, HmYCOMMO );
	(void) ScClrEOL();
	(void) ScSetStandout();
	(void) fputs( str, stdout );
	(void) ScClrStandout();
	(void) fflush( stdout );
	return;
	}

/*
	void	HmLiftWin( register HmWindow *win )

	Remove win->menup from screen, marking any occluded portions
	of other menus as dirty so that they will be redrawn by HmHit().
 */
STATIC void
#if __STDC__
HmLiftWin( register HmWindow *win )
#else
HmLiftWin( win )
register HmWindow	*win;
#endif
	{	register int	row, col;
		register int	lastcol = -1, lastrow = -1;
		register int	endcol = win->menux + win->width + 2;
		register int	endrow = win->menuy +
				  HmHEIGHT + HmHGTBORDER;
#if HmDEBUG && true
	(void) ErLog( "HmLiftWin:win{<%d,%d>,<%d,%d>}\r\n",
			win->menux, win->menux+win->width+1,
			win->menuy, win->menuy+HmHEIGHT+1
			);
#endif
	for( row = win->menuy; row < endrow; row++ )
		{
		for( col = win->menux; col < endcol; col++ )
			{	register HmWindow	*olwin;
			if( (olwin = HmInWin( col, row, win->next ))
				!= (HmWindow *) NULL
				)
				{
				HmSetbit( olwin, col, row );
				HmDirty = true;
				}
			else
				{
				if( lastcol != col-1 || lastrow != row )
					(void) ScMvCursor( col, row );
				lastcol = col; lastrow = row;
				(void) putchar( ' ' );
				}
			}
		}
	(void) fflush( stdout );
	return;
	}

/*
	void	HmPushWin( HmWindow *win )

	Add window to top of "windows" stack.
*/
STATIC void
#if __STDC__
HmPushWin( HmWindow *win )
#else
HmPushWin( win )
HmWindow	*win;
#endif
	{
	win->next = windows;
	windows = win;
	return;
	}

/*
	void	HmPopWin( HmWindow *win )

	Delete window from top of "windows" stack.
*/
STATIC void
#if __STDC__
HmPopWin( HmWindow *win )
#else
HmPopWin( win )
HmWindow	*win;
#endif
	{
	windows = win->next;
	return;
	}

/*
	void	HmRefreshWin( HmWindow *win )

	Draw any dirty portions of all windows in stack starting at win.
*/
STATIC void
#if __STDC__
HmRefreshWin( HmWindow *win )
#else
HmRefreshWin( win )
HmWindow	*win;
#endif
	{
	if( win == (HmWindow *) NULL )
		{
		HmDirty = false;
		return;
		}
	HmRefreshWin( win->next );
	HmDrawWin( win );
	return;
	}
/*
	void	HmRedraw( void )

	Force a redraw of all active menus.
 */
void
#if __STDC__
HmRedraw( void )
#else
HmRedraw()
#endif
	{	register HmWindow	*win;
		register int		reset = false;
#if HmDEBUG && true
	HmPrntWindows();
#endif
	(void) ScClrText();	/* clear entire screen */

	/* See if we changed the maximum items displayed parameter. */
	if( HmMaxVis != HmLastMaxVis )
		reset = true;
	for( win = windows; win != (HmWindow *) NULL; win = win->next )
		{
		if( reset ) /* Correct window to reflect new maximum. */
			{
			/* Reset scrolling state-info in each window. */
			if( win->menup->prevhit >= HmMaxVis )
				win->menup->prevtop = win->menup->prevhit -
							HmMaxVis + 1;
			else
				win->menup->prevtop = 0;
			/* Must reallocate "dirty" bit map to fit new size. */
			MmVFree( Min(win->height,HmLastMaxVis)+HmHGTBORDER,
				 int, win->dirty );
			if(	(win->dirty =
				MmVAllo( HmHEIGHT+HmHGTBORDER, int )
				) == NULL )
				{
				return;
				}
			}
		HmSetmap( win ); /* force all bits on in "dirty" bitmap */
		}
	HmLastMaxVis = HmMaxVis;
	HmRefreshWin( windows );	/* redisplay all windows */
	HmBanner( (char *) NULL, 0 );	/* redraw banner */
	return;
	}

/*
	void	HmTtySet( void )

	Set up terminal handler and MYX-menu options for menu interaction.
 */
void
#if __STDC__
HmTtySet( void )
#else
HmTtySet()
#endif
	{
	set_Cbreak( HmTtyFd );
	clr_Echo( HmTtyFd );
	clr_Tabs( HmTtyFd );
	if( HmMyxflag )
		{
		/* Send escape codes to push current toggle settings,
			and turn on "editor ptr". */
		(void) fputs( "\033[?1;2Z", stdout );
		(void) fputs( "\033[?1;00000010000Z", stdout );
		(void) fflush( stdout );
		}
	return;
	}

/*
	void	HmTtyReset( void )

	Reset terminal handler and MYX-menu options to user settings.
*/
void
#if __STDC__
HmTtyReset( void )
#else
HmTtyReset()
#endif
	{
	if( HmMyxflag )
		{
		/* Send escape codes to pop old toggle settings. */
		(void) fputs( "\033[?1;3Z", stdout );
		(void) fflush( stdout );
		}
	reset_Tty( HmTtyFd );
	return;
	}

/*
	void	HmInit( int x, int y, int maxvis )

	Initialize position of top-level menu.  Specify maximum
	number of menu items visable at once.  Place these values
	in global variables.  Determine as best we can whether MYX
	is available and place boolean result in HmMyxflag.  Return
	true for success and false for failure to open "/dev/tty".
 */
bool
#if __STDC__
HmInit( int x, int y, int maxvis )
#else
HmInit( x, y, maxvis )
int	x, y;
int	maxvis;
#endif
	{
	if(	(HmTtyFd = open( "/dev/tty", O_RDONLY )) == (-1)
	    ||	(HmTtyFp = fdopen( HmTtyFd, "r" )) == NULL
		)
		{
		HmError( "Can't open /dev/tty for reading." );
		return	false;
		}
	save_Tty( HmTtyFd );
	HmPkgInit = true;
	HmLftMenu = x;
	HmTopMenu = y;
	HmMaxVis = maxvis;

	return	true;
	}

/*
	void	HmWidHgtMenu( register HmWindow *win )

	Determine width and height of win->menup, and store in win.
 */
STATIC void
#if __STDC__
HmWidHgtMenu( register HmWindow *win )
#else
HmWidHgtMenu( win )
register HmWindow	*win;
#endif
	{	register HmItem	*itemp;
	/* Determine width of menu, allowing for border.		*/
	for( itemp = win->menup->item; itemp->text != (char *) NULL; itemp++ )
		{	register int	len = 0;
			register int	i;
		for( i = 0; itemp->text[i] != '\0' ; i++ )
			if( ! (itemp->text[i] & 0200) )
				len++;
		win->width = Max( win->width, len );
		}
	win->height = HmENTRY;
	return;
	}

/*
	bool HmFitMenu( register HmWindow *nwin, register HmWindow *cwin )

	If nwin->menup will fit below cwin->menup on screen, store
	position in nwin, and return true.  Otherwise, return false.
 */
STATIC bool
#if __STDC__
HmFitMenu( register HmWindow *nwin, register HmWindow *cwin  )
#else
HmFitMenu( nwin, cwin  )
register HmWindow	*nwin, *cwin;
#endif
	{
	if( cwin == (HmWindow *) NULL )
		return	0;
	else
	if( HmFitMenu( nwin, cwin->next ) )
		return	1;
	else
	/* Look for space underneath this menu.				*/
	if(	cwin->menux + nwin->width + 1 <= ScCO
	    &&	cwin->menuy + cwin->height + nwin->height + HmHGTBORDER
			< HmMaxVis + HmTopMenu
		)
		{
		nwin->menux = cwin->menux;
		nwin->menuy = cwin->menuy + cwin->height + HmHGTBORDER - 1;
		return	1;
		}
	else
		{
		return	0;
		}
	}

/*
	void	HmPosMenu( register HmWindow *win )

	Find best screen position for win->menup.
 */
STATIC void
#if __STDC__
HmPosMenu( register HmWindow *win )
#else
HmPosMenu( win )
register HmWindow	*win;
#endif
	{
	/* Determine origin (top-left corner) of menu.			*/
	if( win->next != (HmWindow *) NULL )
		{
		win->menux = win->next->menux + win->next->width + 1;
		win->menuy = win->next->menuy;
		if( win->menux + win->width + 2 > ScCO )
			{
			if( ! HmFitMenu( win, win->next ) )
				{
				/* No space, so overlap top-level menu.	*/
				win->menux = HmLftMenu;
				win->menuy = HmTopMenu;
				}
			}
		}
	else /* Top-level menu. */
		{
		win->menux = HmLftMenu;
		win->menuy = HmTopMenu;
		}
	return;
	}

/*
	void	HmMyxMouse( register int *x, register int *y )

	Read and decode screen coordinates from MYX "editor ptr".
	Implicit return in x and y.
 */
STATIC void
#if __STDC__
HmMyxMouse( register int *x, register int *y )
#else
HmMyxMouse( x, y )
register int	*x, *y;
#endif
	{	register int	c;
	c = HmGetchar();
	switch( c )
		{
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
	switch( c )
		{
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
	HmItem	*HmHit( HmMenu *menup )

	Present menup to the user and return a pointer to the selected
	item, or 0 if there was no selection made.  For more details,
	see "Hm.h".
 */
HmItem *
#if __STDC__
HmHit( HmMenu *menup )
#else
HmHit( menup )
HmMenu	*menup;			/* -> first HmItem in array.		*/
#endif
	{	register HmItem	*itemp;
		HmItem		*retitemp = NULL;
		HmWindow	*win;
		register int	done = false;
		int		dynamic = false;
		static int	HmLevel = 0;
#if HmDEBUG
	ErLog( "HmHit(0x%x)\n", menup );
#endif
	if( HmPkgInit == false )
		{
		HmInit( HmLftMenu, HmTopMenu, HmMaxVis );
		HmPkgInit = true;
		}
	if( ++HmLevel == 1 )
		HmTtySet();

	/* If generator function is provided, dynamically allocate the
		menu items.
	 */
	if( (dynamic = (menup->item == (HmItem *) NULL) ))
		{	register int	i;
			register HmItem	*gitemp;
			HmLList	llhead, **listp;
		for(	i = 0, listp = &llhead.next;
			;
			i++,   listp = &(*listp)->next
			)
			{
			if(	(*listp = MmAllo( HmLList )) == NULL
			     ||	((*listp)->itemp = MmAllo( HmItem )) == NULL
				)
				{
				goto	clean_exit;
				}
			itemp = (*listp)->itemp;
			if( (gitemp = (*menup->generator)( i )) == (HmItem *) NULL )
				{
				itemp->text = (char *) NULL;
				(*listp)->next = (HmLList *) NULL;
				break;
				}
			if( gitemp->text != (char *) NULL )
				{
				if( (itemp->text = MmStrDup( gitemp->text ))
					== (char *) NULL
					)
					{
					goto	clean_exit;
					}
				}
			else
				itemp->text = (char *) NULL;
			if( gitemp->help != (char *) NULL )
				{
				if( (itemp->help = MmStrDup( gitemp->help ))
					== (char *) NULL
					)
					{
					goto	clean_exit;
					}
				}
			else
				itemp->help = (char *) NULL;
			itemp->next = gitemp->next;
			itemp->dfn = gitemp->dfn;
			itemp->bfn = gitemp->bfn;
			itemp->hfn = gitemp->hfn;
			itemp->data = gitemp->data;
#if HmDEBUG && false
			HmPrntItem( itemp );
#endif
			}
#if HmDEBUG && false
		HmPrntLList( llhead.next );
#endif
		/* Steal the field that the user isn't using temporarily to
			emulate the static allocation of menu items.
		 */
		if( i > 0 )
			{	register int		ii;
				register HmLList	*lp;
			if( (menup->item = MmVAllo( i+1, HmItem )) == NULL )
				{
				goto	clean_exit;
				}
			for(	ii = 0, lp = llhead.next;
				lp != (HmLList *) NULL;
				ii++,   lp = lp->next
				)
				menup->item[ii] = *lp->itemp;
			}
		HmFreeLList( llhead.next );
		if( i == 0 ) /* Zero items, so return NULL */
			goto	clean_exit;
		}
	if( (win = MmAllo( HmWindow )) == NULL )
		{
#if HmDEBUG
		ErLog( "HmHit, BUG: memory pool possibly corrupted.\n" );
#endif
		goto	clean_exit;
		}
	win->menup = menup;
	win->width = 0;
	HmPushWin( win );
	HmWidHgtMenu( win );
	HmPosMenu( win );

	if( menup->prevhit < 0 || menup->prevhit >= win->height )
		menup->prevhit = 0;
	itemp = &menup->item[menup->prevhit];

	if( (win->dirty = MmVAllo( HmHEIGHT+HmHGTBORDER, int )) == NULL )
		{
		goto	clean_exit;
		}
	if( HmDirty )
		HmRefreshWin( windows );
	HmSetmap( win );
	HmDrawWin( win );
	while( ! done )
		{	int	c;
		if( HmDirty )
			HmRefreshWin( windows );
		(void) ScMvCursor( HmXPROMPT, HmYPROMPT );
		(void) ScClrEOL();
		(void) ScMvCursor(win->menux+win->width+2,
				  win->menuy+(HmENTRY-win->menup->prevtop)+1 );
		(void) fflush( stdout );
		c = HmGetchar();
		(void) ScMvCursor( HmLftMenu, HmYCOMMO );
		(void) ScClrEOL();
		switch( c )
			{
		case M_UP :
		case A_UP :
			if( HmENTRY == 0 )
				HmRingbell();
			else
				{
				HmPutItem( win, itemp, P_OFF | P_FORCE );
				itemp--;
				menup->prevhit = HmENTRY;
				if( HmENTRY < win->menup->prevtop )
					{
					win->menup->prevtop -=
						HmENTRY > HmMaxVis/2 ?
						HmMaxVis/2+1 : HmENTRY+1;
					HmSetmap( win );
					HmDrawWin( win );
					}
				else
					HmPutItem( win, itemp, P_ON | P_FORCE );
				}
			break;
		case M_DOWN :
		case A_DOWN :
			if( HmENTRY >= win->height-1 )
				HmRingbell();
			else
				{
				HmPutItem( win, itemp, P_OFF | P_FORCE );
				itemp++;
				menup->prevhit = HmENTRY;
				if( HmENTRY - win->menup->prevtop >= HmMaxVis )
					{
					win->menup->prevtop +=
						win->height-HmENTRY > HmMaxVis/2 ?
						HmMaxVis/2 : win->height-HmENTRY;
					HmSetmap( win );
					HmDrawWin( win );
					}
				else
					HmPutItem( win, itemp, P_ON | P_FORCE );
				}
			break;
		case M_MYXMOUSE :
			{	static int	mousex, mousey;
				register HmItem	*lastitemp;
			if( HmGetchar() != Ctrl('_') || HmGetchar() != '1' )
				goto	m_badinput;
			HmMyxMouse( &mousex, &mousey );
			if( HmInWin( mousex, mousey, win ) != win )
				{ /* Mouse cursor outside of menu. */
				HmRingbell();
				break;
				}
			if( mousey == win->menuy && win->menup->prevtop == 0 )
				{ /* Top border of menu and can't scroll. */
				goto	m_noselect;
				}
			if(	mousey == win->menuy + HmHEIGHT + 1
			    &&	win->height <= HmMaxVis + win->menup->prevtop
				)
				{ /* Bottom border of menu and can't scroll. */
				HmRingbell();
				break;
				}
			lastitemp = itemp;
			itemp = win->menup->item +
					win->menup->prevtop +
					(mousey - (win->menuy + 1));
			if( itemp == lastitemp )
				/* User hit item twice in a row, so select it. */
				goto	m_select;
			HmPutItem( win, lastitemp, P_OFF | P_FORCE );
			menup->prevhit = HmENTRY;
			if( HmENTRY - win->menup->prevtop >= HmMaxVis )
				{
				win->menup->prevtop +=
					win->height-HmENTRY > HmMaxVis/2 ?
					HmMaxVis/2 : win->height-HmENTRY;
				HmSetmap( win );
				HmDrawWin( win );
				}
			else
			if( HmENTRY < win->menup->prevtop )
				{
				win->menup->prevtop -=
					HmENTRY > HmMaxVis/2 ?
					HmMaxVis/2+1 : HmENTRY+1;
				HmSetmap( win );
				HmDrawWin( win );
				}
			else
				{
				HmPutItem( win, itemp, P_ON | P_FORCE );
				}
			break;
			}
		case M_HELP :
		case A_HELP :
			HmHelp( win, HmENTRY );
			break;
		m_select :
		case M_SELECT :
			if( itemp->next != (HmMenu *) NULL )
				{	HmItem	*subitemp;
				if( itemp->dfn != (void (*)()) NULL )
					{	int	level = HmLevel;
					HmTtyReset();
					HmLevel = 0;
					(*itemp->dfn)( itemp );
					HmLevel = level;
					HmTtySet();
					}
				subitemp = HmHit( itemp->next );
				if( itemp->bfn != (void (*)()) NULL )
					{	int	level = HmLevel;
					HmTtyReset();
					HmLevel = 0;
					(*itemp->bfn)( itemp );
					HmLevel = level;
					HmTtySet();
					}
				if( subitemp != (HmItem *) NULL )
					{
					retitemp = subitemp;
					done = ! menup->sticky;
					}
				}
			else
				{
				retitemp = itemp;
				if( itemp->hfn != (void (*)()) NULL )
					{	int	level = HmLevel;
					HmTtyReset();
					HmLevel = 0;
					(*itemp->hfn)( itemp );
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
			HmError( "Type 'd' down, 'u' up, 'h' help, <space> to select, 'q' no selection." );
			break;
			}
		(void) fflush( stdout );
		}
	/* Free storage of dynamic menu.				*/
	if( dynamic )
		{
		if( retitemp != (HmItem *) NULL )
			{ /* Must make copy of item we are returning. */
				static HmItem	dynitem;
			dynitem = *retitemp;
			retitemp = &dynitem;
			}
		HmFreeItems( menup->item );
		menup->item = 0;
		}

	HmLiftWin( win );
	HmPopWin( win );
	MmVFree( HmHEIGHT+HmHGTBORDER, int, win->dirty );
	MmFree( HmWindow, win );
clean_exit :
	if( HmLevel-- == 1 )
		HmTtyReset();
	return	retitemp;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
