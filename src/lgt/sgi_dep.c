/*                       S G I _ D E P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 */
/** @file sgi_dep.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651

	SGI specific routines.  NOTE (XXX) getorigin() doesn't work for
		remote frame buffers.  This should be changed to use libfb
		only.
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>

#if HAS_SGIGL
#include <gl.h>
#include <device.h>
#include <string.h>
#undef RED
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./ascii.h"
#include "./vecmath.h"
#include "./lgt.h"

#define WINBORDER	4  /* Thickness of window border. */
#define WINBANNER	16 /* Thickness of window title bar. */
#define MAXSTRS		6
#define CHARHGT		WINBANNER
#define PUPWID		(375+WINBORDER*2)
#define PUPHGT		(WINBANNER*MAXSTRS+WINBORDER*2)
static char	**pupstr;
static char	*menupupstr[] =
			{
			"Pop-up menus are available in this or the",
			"frame buffer window by holding down the",
			"right mouse button.",
			(char *) NULL
			};
static char	*sweepupstr[] =
			{
			"Hold down the middle button to sweep",
			"a rectangle.",
			"Click the right mouse button to quit.",
			(char *) NULL
			};
static char	*tagpupstr[] =
			{
			"Click the middle mouse button to tag",
			"the screen position under the cursor.",
			"Click the right mouse button to quit.",
			(char *) NULL
			};
static char	*winpupstr[] =
			{
			"Hold down the middle button to adjust",
			"the window frame.",
			"Click the right mouse button to quit.",
			(char *) NULL
			};

/*
	bool tryGetOrigin( long *xp, long *yp )

	There is no way to inquire as to the frame buffer window origin
	through the frame buffer library (libfb(3)), so we need to use
	the SGI Graphics Library call getorigin(), but it will dump core
	if winopen() has not been called.  This is indeed the case if a
	remote frame buffer window is being used.  This routine simply
	tests as to whether the current frame buffer is remote.  This
	test could easily break if libfb changes its name for the remote
	frame buffer since it doesn't advertise the values returned by
	fb_gettype().
 */
static bool
tryGetOrigin( xp, yp )
long *xp, *yp;
	{	char *fbtype = fb_gettype( fbiop );
	if( strcmp( fbtype, "Remote Device Interface" ) == 0 )	/* trouble */
		{
		prnt_Scroll( "Can't get window origin from remote device.\n" );
		return false;
		}
	getorigin( xp, yp );
	return true;
	}

static void
sgi_Rect( x0, y0, x1, y1 )
int	x0, y0, x1, y1;
	{
#ifndef mips
	cursoff();
#endif
	recti( (Icoord) x0, (Icoord) y0, (Icoord) x1, (Icoord) y1 );
#ifndef mips
	curson();
#endif
	return;
	}

static void
sgi_OL_Transparent()
	{
#ifdef mips
	color( 0 );
#else
	pupcolor( PUP_CLEAR );
#endif
	return;
	}

static void
sgi_OL_Opaque()
	{
#ifdef mips
	color( 1 );
#else
	pupcolor( PUP_WHITE );
#endif
	return;
	}

static void
sgi_OL_Start()
	{
#ifdef mips
	drawmode( OVERDRAW );
#else
	pupmode();
#endif
	return;
	}

static void
sgi_OL_End()
	{
#ifdef mips
	drawmode( NORMALDRAW );
#else
	endpupmode();
#endif
	return;
	}

static void
sgi_OL_Erase()
	{
#ifndef mips
	cursoff();
#endif
	sgi_OL_Transparent();
	clear();
#ifndef mips
	curson();
#endif
	return;
	}

extern char	*get_Input();
long		main_menu;
long		buffering_menu;
long		cursorect_menu;
long		debugging_menu;
long		irflags_menu;
long		irpaint_menu;
long		grid_size_menu;
long		lgts_edit_menu;
long		lgts_prnt_menu;
long		mat_index_menu;
long		mat_pindex_menu;
long		mat_eindex_menu;
long		max_ray_menu;
long		movie_fps_menu;
long		two_digit_menu;
static long	popup_gid = -1;

void		sgi_Animate();
static void	sgi_Pt_Select();
static void	sgi_Read_Keyboard();

static int
mips_Animate( fps )
int	fps;
	{
	sgi_Animate( fps < 0 ? 1 : (fps > 64 ? 64 : fps) );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_IR_Flags( flag )
int	flag;
	{
	set_IRmapping( flag );
	note_IRmapping();
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_IR_Paint( flag )
int	flag;
	{
	ir_doing_paint = flag;
	if( ir_doing_paint )
		{
		get_Input( input_ln, MAX_LN, "Enter temperature : " );
		if( sscanf( input_ln, "%d", &ir_paint ) != 1 )
			ir_doing_paint = FALSE;
		}
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Mat_Print( flag )
int	flag;
	{
	if( flag != -1 )
		(void) mat_Print_Db( flag );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Max_Ray( flag )
int	flag;
	{
	if( ir_mapping )
		bu_log( "Multiple bounces disallowed during IR mapping.\n" );
	else
		max_bounce = flag;
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Mat_Edit( flag )
int	flag;
	{
	(void) mat_Edit_Db_Entry( flag );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Lgt_Print( flag )
int	flag;
	{
	(void) lgt_Print_Db( flag );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Lgt_Edit( flag )
int	flag;
	{
	(void) lgt_Edit_Db_Entry( flag );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Buffering( flag )
int	flag;
	{
	pix_buffered = flag;
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Debugging( flag )
int	flag;
	{
	if( flag > 0 )
		rt_g.debug |= flag;
	else
	if( flag == 0 )
		rt_g.debug = 0;
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static int
mips_Size_Grid( size )
int	size;
	{
	grid_sz = size;
	setGridSize( grid_sz );
	return	'#'; /* For backward compatibility with SGI menu interface. */
	}

static void
sgi_Pup_Strs()
	{	register int	i, ypos = PUPHGT-WINBANNER-WINBORDER;
		long	gid = winget();
	winset( popup_gid );
	color( CYAN );
	clear();
	color( BLACK );
	for( i = 0; i < MAXSTRS && pupstr[i] != NULL; i++, ypos -= CHARHGT )
		{
		cmov2i( 10, ypos );
		charstr( pupstr[i] );
		}
	winset( gid );
	}

static void
sgi_Pt_Select( x, y, xp, yp, originp )
register int	x, y, *xp, *yp, *originp;
	{	char		*args[3];
		char		buf1[5], buf2[5];
		long		xwin, ywin;
	if( ! tryGetOrigin( &xwin, &ywin ) )
		return; /* XXX */
	args[1] = buf1;
	args[2] = buf2;
	if( *originp )
		{
		*xp = x;
		*yp = y;
		(void) sprintf( args[1], "%d", SGI_XCVT( x ) + x_fb_origin );
		(void) sprintf( args[2], "%d", grid_x_fin );
		(void) f_Grid_X_Pos( (HMitem *) 0, args );
		(void) sprintf( args[1], "%d", SGI_YCVT( y ) + y_fb_origin );
		(void) sprintf( args[2], "%d", grid_y_fin );
		(void) f_Grid_Y_Pos( (HMitem *) 0, args );
		}
	else
		{	int	x_fin, y_fin;
		x_fin = SGI_XCVT( x ) + x_fb_origin;
		y_fin = SGI_YCVT( y ) + y_fb_origin;
		if( x_fin < grid_x_org )
			{
			Swap_Integers( x_fin, grid_x_org );
			}
		if( y_fin < grid_y_org )
			{
			Swap_Integers( y_fin, grid_y_org );
			}
		(void) sprintf( args[1], "%d", grid_x_org );
		(void) sprintf( args[2], "%d", x_fin );
		(void) f_Grid_X_Pos( (HMitem *) 0, args );
		(void) sprintf( args[1], "%d", grid_y_org );
		(void) sprintf( args[2], "%d", y_fin );
		(void) f_Grid_Y_Pos( (HMitem *) 0, args );
		}
	Toggle( *originp );
	return;
	}

static void
sgi_Pup_Redraw()
	{	long	gid = winget();
	winset( popup_gid );
	reshapeviewport();
	sgi_Pup_Strs();
	winset( gid );
	return;
	}

static int
sgi_Tag_Pixel( origin, x, y, x0, y0 )
int	origin, x, y, x0, y0;
	{	short	val;
		long	xwin, ywin;
		int	flag = tracking_cursor;
	tracking_cursor = FALSE; /* Disable tracking cursor.		*/
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
	if( ! tryGetOrigin( &xwin, &ywin ) )
		return -1; /* XXX */
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
	pupstr = tagpupstr;
	sgi_Pup_Strs();
	for( ; ; )
		{
		if( ! qtest() )
			continue;
		switch( qread( &val ) )
			{
		case MENUBUTTON :
			/* Wait for user to let go.			*/
			for( ; ! qtest() || qread( &val ) != MENUBUTTON; )
				;
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			qreset();
			tracking_cursor = flag;
			return	1;
		case MIDDLEMOUSE :
			/* Wait for user to let go.			*/
			for( ; ! qtest() || qread( &val ) != MIDDLEMOUSE; )
				;
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			user_interrupt = FALSE;
			render_Model( 0 );
			prnt_Event( (char *) NULL );
			qreset();
			break;
		case MOUSEX :
			x = val;
			break;
		case MOUSEY :
			y = val;
			break;
		case KEYBD :
			qenter( KEYBD, val );
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			tracking_cursor = flag;
			return	0;
		case REDRAW :
			sgi_Pup_Redraw();
		case INPUTCHANGE :
			break;
			}
		}
	}

static int
sgi_Sweep_Rect( origin, x, y, x0, y0 )
int	origin, x, y, x0, y0;
	{	short	val;
		long	xwin, ywin;
	if( ! tryGetOrigin( &xwin, &ywin ) )
		return -1; /* XXX */
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
#ifdef mips
	overlay( 2 );
	gconfig();
#endif
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
	pupstr = sweepupstr;
	sgi_Pup_Strs();
	sgi_OL_Start();
	for( ; ; )
		{
		if( ! qtest() )
			continue;
		switch( qread( &val ) )
			{
		case MENUBUTTON :
			/* Wait for user to let go.	*/
			for(	;
			      !	qtest()
			    ||	qread( &val ) != MENUBUTTON;
				)
				;
			sgi_OL_End();
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			qreset();
			return	1;
		case MIDDLEMOUSE :
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			if( origin )
				/* Done sweeping.	*/
				(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
			else
				{ /* Sweeping a rectangle.*/
				sgi_OL_Erase();
				(void) fb_setcursor( fbiop, sweeportrack, 16, 16, 0, 15 );
				}
			break;
		case MOUSEX :
			if( ! origin )
				{
				sgi_OL_Erase();
				x = val;
				sgi_OL_Opaque();
				sgi_Rect( x0-xwin, y0-ywin, x-xwin, y-ywin );
				}
			else
				x = val;
			break;
		case MOUSEY :
			if( ! origin )
				{
				sgi_OL_Erase();
				y = val;
				sgi_OL_Opaque();
				sgi_Rect( x0-xwin, y0-ywin, x-xwin, y-ywin );
				}
			else
				y = val;
			break;
		case KEYBD :
			sgi_OL_End();
			qenter( KEYBD, val );
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			return	0;
		case REDRAW :
			sgi_Pup_Redraw();
		case INPUTCHANGE :
			break;
			}
		}
	}

static int
sgi_Window_In( origin, x, y, x0, y0, out_flag )
int	origin, x, y, x0, y0, out_flag;
	{	short		val;
		register long	dx = 0, dy = 0, dw = 0;
		double		relscale;
		double		x_translate, y_translate;
		long		xwin, ywin;
	if( ! tryGetOrigin( &xwin, &ywin ) )
		return -1; /* XXX */
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
#ifdef mips
	overlay( 2 );
	gconfig();
#endif
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
	pupstr = winpupstr;
	sgi_Pup_Strs();
	sgi_OL_Start();
	for( ; ; )
		{
		if( ! qtest() )
			continue;
		switch( qread( &val ) )
			{
		case MENUBUTTON :
			/* Wait for user to let go.	*/
			for(	;
			      !	qtest()
			    ||	qread( &val ) != MENUBUTTON;
				)
				;
#define Pixel2Grid(x_) ((x_)/((double)fb_getwidth(fbiop)/grid_sz))
#define Grid2Model(x_) ((x_)*cell_sz)
			relscale = Pixel2Grid( dw*2.0 )/ (double)(grid_sz);
			if( out_flag )
				relscale = 1.0 / relscale;
			x_translate = (x0-xwin) - (fb_getwidth(fbiop)/2);
			x_translate = Pixel2Grid( x_translate );
			x_translate = Grid2Model( x_translate );
			y_translate = (y0-ywin) - (fb_getheight(fbiop)/2);
			y_translate = Pixel2Grid( y_translate );
			y_translate = Grid2Model( y_translate );
			if( out_flag )
				{
				x_grid_offset -= x_translate;
				y_grid_offset -= y_translate;
				}
			else
				{
				x_grid_offset += x_translate;
				y_grid_offset += y_translate;
				}
			view_size *= relscale;	/* Scale down view size. */
			sgi_OL_Erase();
			sgi_OL_End();
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			qreset();
			return	1;
		case MIDDLEMOUSE :
			Toggle( origin );
			if( origin )
				{ /* Done framing window. */
				(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
				}
			else
				{ /* Framing a window. */
				x0 = x;
				y0 = y;
				sgi_OL_Erase();
				(void) fb_setcursor( fbiop, sweeportrack, 16, 16, 0, 16 );
				}
			break;
		case MOUSEX :
			if( ! origin )
				{
				sgi_OL_Erase();
				x = val;
				dx = x - x0;
				dx = Abs( dx );
				dw = Max( dx, dy );
				sgi_OL_Opaque();
				sgi_Rect( x0+dw-xwin, y0+dw-ywin, x0-dw-xwin, y0-dw-ywin );
				}
			else
				x = val;
			break;
		case MOUSEY :
			if( ! origin )
				{
				sgi_OL_Erase();
				y = val;
				dy = y - y0;
				dy = Abs( dy );
				dw = Max( dx, dy );
				sgi_OL_Opaque();
				sgi_Rect( x0+dw-xwin, y0+dw-ywin, x0-dw-xwin, y0-dw-ywin );
				}
			else
				y = val;
			break;
		case KEYBD :
			sgi_OL_End();
			qenter( KEYBD, val );
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			return	0;
		case REDRAW :
			sgi_Pup_Redraw();
		case INPUTCHANGE :
			break;
			}
		}
	}

int
sgi_User_Input( args )
char	**args;
	{
	prnt_Status();
	for( ; ; )
		{
		if( qtest() )
			{	short	val;
				long	dev = qread( &val );
			switch( dev )
				{
			case MENUBUTTON :
				if( ! user_Pop( dopup( main_menu ) ) )
					return 0;
				prnt_Event( "" );
				prnt_Prompt( "" );
				(void) fflush( stdout );
				qreset();
				break;
			case MOUSEX :
				fb_log( "Mouse x = %d\n", (int) val );
				break;
			case MOUSEY :
				fb_log( "Mouse y = %d\n", (int) val );
				break;
			case KEYBD :
				break;
			case INPUTCHANGE :
				break;
			case REDRAW :
				sgi_Pup_Redraw();
				break;
			default :
				fb_log( "dev=%d val=%d\n", (int) dev, (int) val );
				break;
				}
			prnt_Status();
			}
		}
	}

int
sgi_Cursor_Input( x, y, mxp, myp, xx0, yy0, origin )
register int	x, y, *mxp, *myp;
int	xx0, yy0;
int	origin;
	{
	winset( (long) fbiop->if_fd );
	(void) winattach();
	for( ; ; )
		{
		(void) fb_setcursor( fbiop, menucursor, 16, 16, 0, 0 );
		if( qtest() )
			{	short	val;
				long	dev = qread( &val );
				int	ret;
			switch( dev )
				{
			case MENUBUTTON :
				(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
				switch( dopup( cursorect_menu ) )
					{
				case C_TAGPIXEL : /* Tag pixel. */
					*mxp = XSCR2MEM( x );
					*myp = YSCR2MEM( y );
					if( sgi_Tag_Pixel( origin, *mxp, *myp, xx0, yy0 ) )
						goto	hit_menubutton;
					else
						ret = 2;
					break;
				case C_SWEEPREC : /* Sweep rectangle. */
					*mxp = XSCR2MEM( x );
					*myp = YSCR2MEM( y );
					if( sgi_Sweep_Rect( origin, *mxp, *myp, xx0, yy0 ) )
						goto	hit_menubutton;
					else
						ret = 2;
					break;
				case C_I_WINDOW : /* Window in. */
					*mxp = XSCR2MEM( x );
					*myp = YSCR2MEM( y );
					if( rel_perspective > 0.0 )
						{
						prnt_Scroll( "Windowing does not yet work WITH perspective.\n" );
						prnt_Scroll( "Set perspective to zero or negative.\n" );
						}
					else
					if( sgi_Window_In( origin, *mxp, *myp, xx0, yy0, 0 ) )
						goto	hit_menubutton;
					else
						ret = 2;
					break;
				case C_O_WINDOW : /* Window out. */
					*mxp = XSCR2MEM( x );
					*myp = YSCR2MEM( y );
					if( rel_perspective > 0.0 )
						{
						prnt_Scroll( "Windowing does not yet work WITH perspective.\n" );
						prnt_Scroll( "Set perspective to zero or negative.\n" );
						}
					else
					if( sgi_Window_In( origin, *mxp, *myp, xx0, yy0, 1 ) )
						goto	hit_menubutton;
					else
						ret = 2;
					break;
				case C_QUERYREG : /* Query region. */
					*mxp = XSCR2MEM( x );
					*myp = YSCR2MEM( y );
					Toggle( query_region );
					if( sgi_Tag_Pixel( origin, *mxp, *myp, xx0, yy0 ) )
						goto	hit_menubutton;
					else
						ret = 2;
					break;
				default :
				hit_menubutton :
					ret = 3;
					break;
					}
				pupstr = menupupstr;
				sgi_Pup_Strs();
				qreset();
				(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
				(void) fb_cursor( fbiop, 1, grid_sz/2, grid_sz/2 );
				return	ret;
			case MOUSEX :
				fb_log( "Mouse x = %d\n", (int) val );
				break;
			case MOUSEY :
				fb_log( "Mouse y = %d\n", (int) val );
				break;
			case KEYBD :
				qenter( KEYBD, val );
				return	2;
			case INPUTCHANGE :
				break;
			case REDRAW :
				sgi_Pup_Redraw();
				break;
			default :
				fb_log( "dev=%d val=%d\n", (int) dev, (int) val );
				break;
				}
			}
		}
	}

/*
nul  0	soh  1	stx  2	etx  3	eot  4	enq  5	ack  6	bel  7
bs   8	ht   9	nl  10	vt  11	np  12	cr  13	so  14	si  15
dle 16	dc1 17	dc2 18	dc3 19	dc4 20	nak 21  syn 22	etb 23
can 24	em  25	sub 26	esc 27	fs  28	gs  29	rs  30	us  31
sp  32	!   33	"   34	#   35	$   36	%   37	&   38	'   39
(   40	)   41	*   42  +   43	,   44	-   45	.   46	/   47
0   48	1   49	2   50	3   51	4   52	5   53	6   54	7   55
8   56	9   57	:   58	;   59	<   60	=   61	>   62	?   63
@   64	A   65	B   66	C   67	D   68	E   69	F   70	G   71
H   72	I   73	J   74	K   75	L   76	M   77	N   78	O   79
P   80	Q   81	R   82	S   83	T   84	U   85	V   86	W   87
X   88	Y   89	Z   90  [   91  \   92  ]   93  ^   94  _   95
`   96	a   97	b   98	c   99	d  100	e  101	f  102	g  103
h  104	i  105	j  106	k  107	l  108	m  109	n  110	o  111
p  112	q  113	r  114	s  115	t  116	u  117	v  118	w  119
x  120	y  121	z  122
 */
int
sgi_Init_Popup_Menu()
	{	long	grid_cntl_menu;
		long	file_name_menu;
		long	light_src_menu;
		long	special_menu;
		long	infrared_menu;
		long	materials_menu;
		long	raytracer_menu;
		long	one_digit_menu;
		long	tens_menu;
		long	twenties_menu;
		long	thirties_menu;
		long	forties_menu;
		long	fifties_menu;
		long	sixties_menu;
		long	sixties_partial_menu;
		long	seventies_menu;
		long	eighties_menu;
		long	nineties_menu;
	prefsize( PUPWID, PUPHGT );
	foreground();
	if( (popup_gid = winopen( "pop up menus" )) == -1 )
		{
		fb_log( "No more graphics ports available.\n" );
		return	-1;
		}
	wintitle( "pop up menus" );
	winconstraints(); /* Free window of constraints.		*/
	singlebuffer();
	pupstr = menupupstr;
	sgi_Pup_Strs();
	winattach();
	cursorect_menu = defpup( "cursor input %t|tag pixel %x0|sweep rectangle %x1|window in %x2|window out %x3|query region %x4" );
	one_digit_menu = defpup( "0..9 %t|0 %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9" );
	tens_menu =      defpup( "10..19 %t|10 %x10|11 %x11|12 %x12|13 %x13|14 %x14|15 %x15|16 %x16|17 %x17|18 %x18|19 %x19" );
	twenties_menu =  defpup( "20..29 %t|20 %x20|21 %x21|22 %x22|23 %x23|24 %x24|25 %x25|26 %x26|27 %x27|28 %x28|29 %x29" );
	thirties_menu =  defpup( "30..39 %t|30 %x30|31 %x31|32 %x32|33 %x33|34 %x34|35 %x35|36 %x36|37 %x37|38 %x38|39 %x39" );
	forties_menu =   defpup( "40..49 %t|40 %x40|41 %x41|42 %x42|43 %x43|44 %x44|45 %x45|46 %x46|47 %x47|48 %x48|49 %x49" );
	fifties_menu =   defpup( "50..59 %t|50 %x50|51 %x51|52 %x52|53 %x53|54 %x54|55 %x55|56 %x56|57 %x57|58 %x58|59 %x59" );
	sixties_menu =   defpup( "60..69 %t|60 %x60|61 %x61|62 %x62|63 %x63|64 %x64|65 %x65|66 %x66|67 %x67|68 %x68|69 %x69" );
	sixties_partial_menu =   defpup( "60..64 %t|60 %x60|61 %x61|62 %x62|63 %x63|64 %x64" );
	seventies_menu = defpup( "70..79 %t|70 %x70|71 %x71|72 %x72|73 %x73|74 %x74|75 %x75|76 %x76|77 %x77|78 %x78|79 %x79" );
	eighties_menu =  defpup( "80..89 %t|80 %x80|81 %x81|82 %x82|83 %x83|84 %x84|85 %x85|86 %x86|87 %x87|88 %x88|89 %x89" );
	nineties_menu =  defpup( "90..99 %t|90 %x90|91 %x91|92 %x92|93 %x93|94 %x94|95 %x95|96 %x96|97 %x97|98 %x98|99 %x99" );
#ifdef mips
	movie_fps_menu = defpup( "fps %t %F|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..64 %m %x-1",
				mips_Animate,
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_partial_menu
				);
	irflags_menu = defpup( "infrared module flags %t %F|read only %x1|edit %x2|octree rendering %x4|reset all flags %x0", mips_IR_Flags );
	irpaint_menu = defpup( "temperature painting from cursor module %t %F|ON %x1|OFF %x0", mips_IR_Paint );
	debugging_menu = defpup( "debugging %t %F|reset all flags %x0|all rays %x1|shoot %x2|db %x16|solids %x32|regions %x64|arb8 %x128|spline %x256|roots %x4096|partitioning %x8192|cut %x16384|boxing %x32768|memory allocation %x65536|testing %x131072|fdiff %x262144|RGB %x524288|refraction %x1048576|normals %x2097152|shadows %x4194304|cell size %x8388608|octree %x16777216", mips_Debugging );
	buffering_menu = defpup( "buffering %t %F|unbuffered %x0|paged buffering %x1|scan line buffered%x2", mips_Buffering );
	grid_size_menu = defpup( "resolution %t %F|16-by-16 %x16|32-by-32 %x32|64-by-64 %x64|128-by-128 %x128|256-by-256 %x256|512-by-512 %x512|1024-by-1024 %x1024", mips_Size_Grid );
	grid_cntl_menu = defpup( "gridding parameters %t|resolution %m %x71|distance to model centroid %x102|perspective %x112|roll %x97|field of view %x103|image translation %x68|grid translation %x116|anti-aliasing %x65|key-frame input %x106|movie set up %x74", grid_size_menu );
	lgts_prnt_menu = defpup( "light index %t %F|all %x-1|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9", mips_Lgt_Print );
	lgts_edit_menu = defpup( "light index %t %F|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9", mips_Lgt_Edit );
	light_src_menu = defpup( "light sources %t|print entry %m %x108|modify entry %m %x76|read database %x118|write database %x86", lgts_prnt_menu, lgts_edit_menu );
	mat_eindex_menu = defpup( "0..99 %t %F|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..69 %m %x-1|70..79 %m %x-1|80..89 %m %x-1|90..99 %m %x-1",
				mips_Mat_Edit,
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_menu,
				seventies_menu,
				eighties_menu,
				nineties_menu
				);
	mat_pindex_menu = defpup( "0..99 %t %F|all %x-2|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..69 %m %x-1|70..79 %m %x-1|80..89 %m %x-1|90..99 %m %x-1",
				mips_Mat_Print,
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_menu,
				seventies_menu,
				eighties_menu,
				nineties_menu
				);
	materials_menu = defpup( "material attributes %t|print entry %m %x109|modify entry %m %x77|read database %x119|write database %x87", mat_pindex_menu, mat_eindex_menu );
	max_ray_menu = defpup( "0..99 %t %F|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..69 %m %x-1|70..79 %m %x-1|80..89 %m %x-1|90..99 %m %x-1",
				mips_Max_Ray,
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_menu,
				seventies_menu,
				eighties_menu,
				nineties_menu
				);
	infrared_menu = defpup( "infrared module %t|set flags %m %x115|read real IR data %x73|read IR data base %x117|write IR data base %x85|disable automatic mapping offsets %x100|specify noise threshold %x105|set temperature %m %x78|assign temperature by region %x81|print temperatures by region %x80|display color assignment legend %x90", irflags_menu, irpaint_menu );
#else
	movie_fps_menu = defpup( "fps %t|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..64 %m %x-1",
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_partial_menu
				);
	irflags_menu = defpup( "infrared module flags %t|read only %x1|edit %x2|octree rendering %x4|reset all flags %x0" );
	irpaint_menu = defpup( "temperature painting from cursor module %t|ON %x1|OFF %x0" );
	debugging_menu = defpup( "debugging %t|reset all flags %x0|all rays %x1|shoot %x2|db %x16|solids %x32|regions %x64|arb8 %x128|spline %x256|roots %x4096|partitioning %x8192|cut %x16384|boxing %x32768|memory allocation %x65536|testing %x131072|fdiff %x262144|RGB %x524288|refraction %x1048576|normals %x2097152|shadows %x4194304|cell size %x8388608|octree %x16777216" );
	buffering_menu = defpup( "buffering %t|unbuffered %x0|paged buffering %x1|scan line buffered%x2" );
	grid_size_menu = defpup( "resolution %t|16-by-16 %x16|32-by-32 %x32|64-by-64 %x64|128-by-128 %x128|256-by-256 %x256|512-by-512 %x512|1024-by-1024 %x1024" );
	grid_cntl_menu = defpup( "gridding parameters %t|resolution %x71|distance to model centroid %x102|perspective %x112|roll %x97|field of view %x103|image translation %x68|grid translation %x116|anti-aliasing %x65|key-frame input %x106|movie set up %x74" );
	lgts_prnt_menu = defpup( "light index %t|all %x-1|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9" );
	lgts_edit_menu = defpup( "light index %t|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9" );
	light_src_menu = defpup( "light sources %t|print entry %x108|modify entry %x76|read database %x118|write database %x86" );
	two_digit_menu = defpup( "0..99 %t|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..69 %m %x-1|70..79 %m %x-1|80..89 %m %x-1|90..99 %m %x-1",
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_menu,
				seventies_menu,
				eighties_menu,
				nineties_menu
				);
	mat_index_menu = defpup( "0..99 %t|all %x-2|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..69 %m %x-1|70..79 %m %x-1|80..89 %m %x-1|90..99 %m %x-1",
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_menu,
				seventies_menu,
				eighties_menu,
				nineties_menu
				);
	materials_menu = defpup( "material attributes %t|print entry %x109|modify entry %x77|read database %x119|write database %x87" );
	infrared_menu = defpup( "infrared module %t|set flags %x115|read real IR data %x73|read IR data base %x117|write IR data base %x85|automatic mapping offsets %x100|specify noise threshold %x105|set temperature %x78|assign temperature by region %x81|print temperatures by region %x80|display color assignment legend %x90" );
#endif
	file_name_menu = defpup( "files %t|frame buffer %x111|error/debug log %x79|write script %x83|save image %x72|read image %x104" );
	raytracer_menu = defpup( "raytrace %t|go %x82|submit batch run %x66" );
	special_menu = defpup( "special applications %t|infrared modeling %m %x35|hidden-line drawing %x107",
				infrared_menu
				);
#ifdef mips
	main_menu = defpup( "main menu %t|raytrace %m %x35|animate %x70|gridding parameters %m %x35|buffering %m %x46|debugging %m %x101|shell escape %x33|background color %x98|maximum ray bounces %m %x75|tracking cursor (on/off) %x99|overlap reporting (on/off) %x88|cursor input %x67|clear frame buffer %x69|redraw text %x114|light sources %m %x35|material attributes %m %x35|files %m %x35|special applications %m %x35|quit %x113",
				raytracer_menu,
				grid_cntl_menu,
				buffering_menu,
				debugging_menu,
				max_ray_menu,
				light_src_menu,
				materials_menu,
				file_name_menu,
				special_menu
				);
#else
	main_menu = defpup( "main menu %t|raytrace %m %x35|animate %x70|gridding parameters %m %x35|buffering %x46|debugging %x101|shell escape %x33|background color %x98|maximum ray bounces %x75|tracking cursor (on/off) %x99|overlap reporting (on/off) %x88|cursor input %x67|clear frame buffer %x69|redraw text %x114|light sources %m %x35|material attributes %m %x35|files %m %x35|special applications %m %x35|quit %x113",
				raytracer_menu,
				grid_cntl_menu,
				light_src_menu,
				materials_menu,
				file_name_menu,
				special_menu
				);
#endif
	qdevice(MENUBUTTON);
	qdevice(KEYBD);
	qdevice(INPUTCHANGE);
	return	1;
	}

#ifdef mips
#include <sys/param.h>
#else
#include <machine/param.h>
#endif
void
sgi_Animate( framesz, fps )
int	framesz;
int	fps;
	{	register int	i, j;
		register int	wid;
		register int	xpos, ypos;
		static long	movie_gid = -1;
		long		xwin, ywin, xsiz, ysiz;
		long		movie_xwin, movie_ywin;
	if( fps < 1 )
		fps = 1;
	if( fps > HZ )
		fps = HZ;

	/* Get origin of frame buffer window (source). */
	if( ! tryGetOrigin( &xwin, &ywin ) )
		return; /* XXX */

	/* Get size of frame buffer window (source). */
	/* getsize( &xsiz, &ysiz ); */
	xsiz = fb_getwidth( fbiop );
	ysiz = fb_getheight( fbiop );

	wid = xsiz / framesz;
	xpos = ypos = xsiz / 2;
	zoom = 1;
	if( fb_zoom( fbiop, zoom, zoom ) == -1 )
		bu_log( "Can not set zoom <%d,%d>.\n", zoom, zoom );
	if( fb_viewport( fbiop, 0, 0, xsiz, ysiz ) == -1 )
		bu_log( "Can not set viewport {<%d,%d>,<%d,%d>}.\n",
			0, 0, xsiz, ysiz
			);
	if( fb_window( fbiop, xpos, ypos ) == -1 )
		bu_log( "Can not set window <%d,%d>.\n", xpos, ypos );

	/* Create destination window for movie, with user positioning. */
	prefsize( framesz, framesz );
	if( (movie_gid = winopen( "movie" )) == -1 )
		{
		fb_log( "No more graphics ports available.\n" );
		return;
		}
	/* Adjust window position optimally for fast "rectcopy()". */
	if( ! tryGetOrigin( &movie_xwin, &movie_ywin ) )
		return; /* XXX */
	if( ((xwin - movie_xwin) % 16) != 0 )
		movie_xwin += (xwin - movie_xwin) % 16;
	while( movie_xwin > XMAXSCREEN - framesz )
		movie_xwin -= 16;
	winmove( movie_xwin, movie_ywin );

	fullscrn();
	RGBmode();	/* By pass color table, interpret pixel RGB values. */
	gconfig();

	qdevice( MIDDLEMOUSE );
	for( ; ; )
	for( i = 0; i < wid; i++ )
		{
		for( j = 0; j < wid; j++ )
			{
			sginap( HZ/(long)fps );
			if( qtest() )
				{	short	val;
				switch( qread( &val ) )
					{
				case MENUBUTTON :
					/* Wait for user to let go.	*/
					for(	;
					      !	qtest()
					    ||	qread( &val ) != MENUBUTTON;
						)
						;
					break;
				case MIDDLEMOUSE :
					/* Wait for user to let go.	*/
					for(	;
					      !	qtest()
					    ||	qread( &val ) != MIDDLEMOUSE;
						)
						;
					endfullscrn();
					unqdevice( MIDDLEMOUSE );
					winclose( movie_gid );
					winset( fbiop->if_fd );
					return;
					}
				}
			rectcopy((Screencoord)(xwin+j*framesz),
				 (Screencoord)(ywin+i*framesz),
				 (Screencoord)(xwin+(j+1)*framesz)-1,
				 (Screencoord)(ywin+(i+1)*framesz)-1,
				 (Screencoord) movie_xwin,
				 (Screencoord) movie_ywin
				 );
			}
		}
	}

char *
sgi_GetInput( inbuf, bufsz, msg )
char	 *inbuf;
int	 bufsz;
char	*msg;
	{	static char	buffer[BUFSIZ];
		register char	*p = buffer;
		register int	c;
	prnt_Prompt( msg );
	*p = NUL;
	do
		{		(void) fflush( stdout );
		c = hm_getchar();
		switch( c )
			{
		case Ctrl('A') : /* Cursor to beginning of line.	*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			break;
		case Ctrl('B') :
		case BS : /* Move cursor back one character.		*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			--p;
			break;
		case Ctrl('D') : /* Delete character under cursor.	*/
			{	register char	*q = p;
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			for( ; *q != NUL; ++q )
				{
				*q = *(q+1);
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			break;
			}
		case Ctrl('E') : /* Cursor to end of line.		*/
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			(void) printf( "%s", p );
			p += strlen( p );
			break;
		case Ctrl('F') : /* Cursor forward one character.	*/
			if( *p == NUL || p-buffer >= bufsz-2 )
				{
				ring_Bell();
				break;
				}
			putchar( *p++ );
			break;
		case Ctrl('G') : /* Abort input.			*/
			ring_Bell();
			prnt_Event( "Aborted." );
			prnt_Prompt( "" );
			return	NULL;
		case Ctrl('K') : /* Erase from cursor to end of line.	*/
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			ClrEOL();
			*p = NUL;
			break;
		case Ctrl('P') : /* Yank previous contents of "inbuf".	*/
			{	register int	len = strlen( inbuf );
			if( (p + len) - buffer >= BUFSIZ )
				{
				ring_Bell();
				break;
				}
			(void) strncpy( p, inbuf, bufsz );
			(void) printf( "%s", p );
			p += len;
			break;
			}
		case Ctrl('U') : /* Erase from start of line to cursor.	*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; --p )
				{	register char	*q = p;
				(void) putchar( BS );
				for( ; *(q-1) != NUL; ++q )
					{
					*(q-1) = *q;
					(void) putchar( *q != NUL ? *q : SP );
					}
				for( ; q > p; --q )
					(void) putchar( BS );
				}
			break;
		case Ctrl('R') : /* Print line, cursor doesn't move.	*/
			{	register int	i;
			if( buffer[0] == NUL )
				break;
			for( i = p - buffer; i > 0; i-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			for( i = strlen( buffer ) - (p - buffer); i > 0; i-- )
				(void) putchar( BS );
			break;
			}
		case DEL : /* Delete character behind cursor.		*/
			{	register char	*q = p;
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			for( ; *(q-1) != NUL; ++q )
				{
				*(q-1) = *q;
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			p--;
			break;
			}
		case CR :
		case LF :
		case EOF :
			(void) strncpy( inbuf, buffer, bufsz );
			prnt_Prompt( "" );
			if( inbuf[0] == '\0' )
				return	NULL;
			else
				return	inbuf;
		case Ctrl('V') :
			/* Escape character, do not process next char.	*/
			c = hm_getchar();
			/* Fall through to default case!		*/
		default : /* Insert character at cursor.		*/
			{	register char	*q = p;
				register int	len = strlen( p );
			/* Print control characters as strings.		*/
			if( c >= NUL && c < SP )
				(void) printf( "%s", char_To_String( c ) );
			else
				(void) putchar( c );
			/* Scroll characters forward.			*/
			for( ; len >= 0; len--, q++ )
				(void) putchar( *q == NUL ? SP : *q );
			for( ; q > p; q-- )
				{
				(void) putchar( BS );
				*q = *(q-1);
				}
			*p++ = c;
			break;
			}
			} /* End switch. */
		}
	while( strlen( buffer ) < BUFSIZ );
	(void) strncpy( inbuf, buffer, bufsz );
	ring_Bell();
	prnt_Event( "Buffer full." );
	prnt_Prompt( "" );
	return	inbuf;
	}

static void
sgi_Read_Keyboard( args )
char	**args;
	{	register int	i;
		register char	*eof_flag;
	(void) get_Input( input_ln, BUFSIZ, ": " );
	if( (args[0] = strtok( input_ln, " \t" )) == NULL )
		{
		args[0] = "#";
		args[1] = NULL;
		return;
		}
	for( i = 1; args[i-1] != NULL ; ++i )
		args[i] = strtok( (char *) NULL, " \t" );
	return;
	}

int
sgi_Getchar()
	{	short	val;
	(void) winattach();
	while( ! qtest() || qread( &val ) != KEYBD )
		;
	return	(int) val;
	}

int
sgi_Ungetchar( c )
int	c;
	{	short	val = c;
	qenter( KEYBD, val );
	return c;
	}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
