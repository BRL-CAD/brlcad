/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef sgi
#include <stdio.h>
#include <gl.h>
#include <device.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "./lgt.h"
#include "./vecmath.h"
#undef RED
#include "./extern.h"
extern char	*get_Input();
extern void	sgi_Pt_Select();
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
long		movie_fps_menu;
long		two_digit_menu;
static long	popup_gid = -1;

_LOCAL_ void	sgi_Read_Keyboard();

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
					return;
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
				qenter( KEYBD, val );
				sgi_Read_Keyboard( args );
				if( ! user_Cmd( args ) )
					return;
				prnt_Event( "" );
				prnt_Prompt( "" );
				(void) fflush( stdout );
				break;
			case INPUTCHANGE :
				break;
			case REDRAW :
				break;
			default :
				fb_log( "dev=%d val=%d\n", (int) dev, (int) val );
				break;
				}
			prnt_Status();
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
#define MARGIN	4
#define PUPSZ	(140+MARGIN)
	prefposition( 1024-PUPSZ, 1023-MARGIN, 768-PUPSZ, 768-MARGIN-20 );
	foreground();
	if( (popup_gid = winopen( "pop up menus" )) == -1 )
		{
		fb_log( "No more graphics ports available.\n" );
		return	-1;
		}
	wintitle( "pop up menus" );
	winconstraints(); /* Free window of constraints.		*/
	winattach();
	buffering_menu = defpup( "buffering %t|unbuffered %x0|paged buffering %x1|scan line buffered%x2" );
	cursorect_menu = defpup( "cursor input %t|tag pixel %x0|sweep rectangle %x1|window in %x2|window out %x3|query region %x4" );
	debugging_menu = defpup( "debugging %t|reset all flags %x0|all rays %x1|shoot %x2|db %x16|solids %x32|regions %x64|arb8 %x128|spline %x256|roots %x4096|partitioning %x8192|cut %x16384|boxing %x32768|memory allocation %x65536|testing %x131072|fdiff %x262144|RGB %x524288|refraction %x1048576|normals %x2097152|shadows %x4194304|cell size %x8388608|octree %x16777216" );
	irpaint_menu = defpup( "temperature painting from cursor module %t|ON %x1|OFF %x0" );
	irflags_menu = defpup( "infrared module flags %t|read only %x1|edit %x2|octree rendering %x4|reset all flags %x0" );
	grid_cntl_menu = defpup( "gridding parameters %t|resolution %x71|distance to model centroid %x102|perspective %x112|roll %x97|field of view %x103|image translation %x68|grid translation %x116|over sampling factor %x65|key frame input %x106|movie setup %x74|animate %x70" );
	grid_size_menu = defpup( "resolution %t|16-by-16 %x16|32-by-32 %x32|64-by-64 %x64|128-by-128 %x128|256-by-256 %x256|512-by-512 %x512|1024-by-1024 %x1024" );
	file_name_menu = defpup( "files %t|frame buffer %x111|error/debug log %x79|write script %x83|save image %x72|read image %x104|texture map %x84" );
	light_src_menu = defpup( "light sources %t|print entry %x108|modify entry %x76|read database %x118|write database %x86" );
	lgts_edit_menu = defpup( "light index %t|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9" );
	lgts_prnt_menu = defpup( "light index %t|all %x-1|eye (ambient) %x0|1 %x1|2 %x2|3 %x3|4 %x4|5 %x5|6 %x6|7 %x7|8 %x8|9 %x9" );
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
	movie_fps_menu = defpup( "fps %t|0..9 %m %x-1|10..19 %m %x-1|20..29 %m %x-1|30..39 %m %x-1|40..49 %m %x-1|50..59 %m %x-1|60..64 %m %x-1",
				one_digit_menu,
				tens_menu,
				twenties_menu,
				thirties_menu,
				forties_menu,
				fifties_menu,
				sixties_partial_menu
				);
	infrared_menu = defpup( "infrared module %t|set flags %x115|read real IR data %x73|read IR data base %x117|write IR data base %x85|automatic mapping offsets %x100|specify noise threshold %x105|set temperature %x78|assign temperature by region %x81|print temperatures by region %x80|display color assignment legend %x90" );
	raytracer_menu = defpup( "raytrace %t|go %x82|submit batch run %x66" );
	materials_menu = defpup( "material attributes %t|print entry %x109|modify entry %x77|read database %x119|write database %x87" );
	special_menu = defpup( "special applications %t|infrared modeling %m %x35|hidden-line drawing %x107",
				infrared_menu
				);
	main_menu = defpup( "main menu %t|raytrace %m %x35|gridding parameters %m %x35|buffering %x46|debugging %x101|shell escape %x33|background color %x98|maximum ray bounces %x75|tracking cursor (on/off) %x99|cursor input %x67|clear frame buffer %x69|redraw text %x114|light sources %m %x35|material attributes %m %x35|files %m %x35|special applications %m %x35|quit %x113",
				raytracer_menu,
				grid_cntl_menu,
				light_src_menu,
				materials_menu,
				file_name_menu,
				special_menu
				);
	qdevice(MENUBUTTON);
	qdevice(KEYBD);
	qdevice(INPUTCHANGE);
	return	1;
	}

int
sgi_Sweep_Rect( origin, x, y, x0, y0 )
int	origin, x, y, x0, y0;
	{	short	val;
		long	xwin, ywin;
	getorigin( &xwin, &ywin );
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
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
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			qreset();
			(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
			(void) fb_cursor( fbiop, tracking_cursor, grid_sz/2, grid_sz/2 );
			return	1;
		case MIDDLEMOUSE :
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			if( origin )
				/* Done sweeping.	*/
				(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
			else
				{ /* Sweeping a rectangle.*/
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				clear();
				endpupmode();
				curson();
				(void) fb_setcursor( fbiop, sweeportrack, 16, 16, 0, 15 );
				}
			break;
		case MOUSEX :
			if( ! origin )
				{
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				recti( x0-xwin, y0-ywin, x-xwin, y-ywin );
				x = val;
				pupcolor( PUP_WHITE );
				recti( x0-xwin, y0-ywin, x-xwin, y-ywin );
				endpupmode();
				curson();
				}
			else
				x = val;
			break;
		case MOUSEY :
			if( ! origin )
				{
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				recti( x0-xwin, y0-ywin, x-xwin, y-ywin );
				y = val;
				pupcolor( PUP_WHITE );
				recti( x0-xwin, y0-ywin, x-xwin, y-ywin );
				endpupmode();
				curson();
				}
			else
				y = val;
			break;
		case KEYBD :
			qenter( KEYBD, val );
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			return	0;
		case INPUTCHANGE :
			break;
			}
		}
	}


int
sgi_Tag_Pixel( origin, x, y, x0, y0 )
int	origin, x, y, x0, y0;
	{	short	val;
		long	xwin, ywin;
		int	flag = tracking_cursor;
	tracking_cursor = FALSE; /* Disable tracking cursor.		*/
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
	getorigin( &xwin, &ywin );
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
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
			(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
			(void) fb_cursor( fbiop, tracking_cursor, grid_sz/2, grid_sz/2 );
			return	1;
		case MIDDLEMOUSE :
			/* Wait for user to let go.			*/
			for( ; ! qtest() || qread( &val ) != MIDDLEMOUSE; )
				;
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			sgi_Pt_Select( x, y, &x0, &y0, &origin );
			user_interrupt = FALSE;
			render_Model();
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
		case INPUTCHANGE :
			break;
			}
		}
	}

int
sgi_Window_In( origin, x, y, x0, y0, out_flag )
int	origin, x, y, x0, y0, out_flag;
	{	short		val;
		register int	dx = 0, dy = 0, dw = 0;
		double		scale;
		double		x_translate, y_translate;
		long		xwin, ywin;
	getorigin( &xwin, &ywin );
	qdevice( MOUSEX );
	qdevice( MOUSEY );
	qdevice( MIDDLEMOUSE );
	(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
	(void) fb_cursor( fbiop, 1, x, y );
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
#define Pixel2Grid(x_) ((x_)/((double)fbiop->if_width/grid_sz))
#define Grid2Model(x_) ((x_)*cell_sz)
			scale = Pixel2Grid( dw*2.0 )/ (double)(grid_sz);
			if( out_flag )
				scale = 1.0 / scale;
			x_translate = (x0-xwin) - (fbiop->if_width/2);
			x_translate = Pixel2Grid( x_translate );
			x_translate = Grid2Model( x_translate );
			y_translate = (y0-ywin) - (fbiop->if_height/2);
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
			grid_scale *= scale;	/* Scale down grid.	*/
			cursoff();
			pupmode();
			pupcolor( PUP_CLEAR );
			clear();
			endpupmode();
			curson();
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			qreset();
			(void) fb_setcursor( fbiop, arrowcursor, 16, 16, 0, 0 );
			(void) fb_cursor( fbiop, tracking_cursor, grid_sz/2, grid_sz/2 );
			return	1;
		case MIDDLEMOUSE :
			Toggle( origin );
			if( origin )
				{ /* Done framing window.	*/
				(void) fb_setcursor( fbiop, target1, 16, 16, 8, 8 );
				}
			else
				{ /* Framing a window.		*/
				x0 = x;
				y0 = y;
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				clear();
				endpupmode();
				curson();
				(void) fb_setcursor( fbiop, sweeportrack, 16, 16, 0, 16 );
				}
			break;
		case MOUSEX :
			if( ! origin )
				{
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				recti( x0-dw-xwin, y0-dw-ywin, x0+dw-xwin, y0+dw-ywin );
				x = val;
				dx = x - x0;
				dx = Abs( dx );
				dw = Max( dx, dy );
				pupcolor( PUP_WHITE );
				recti( x0-dw-xwin, y0-dw-ywin, x0+dw-xwin, y0+dw-ywin );
				endpupmode();
				curson();
				}
			else
				x = val;
			break;
		case MOUSEY :
			if( ! origin )
				{
				cursoff();
				pupmode();
				pupcolor( PUP_CLEAR );
				recti( x0-dw-xwin, y0-dw-ywin, x0+dw-xwin, y0+dw-ywin );
				y = val;
				dy = y - y0;
				dy = Abs( dy );
				dw = Max( dx, dy );
				pupcolor( PUP_WHITE );
				recti( x0-dw-xwin, y0-dw-ywin, x0+dw-xwin, y0+dw-ywin );
				endpupmode();
				curson();
				}
			else
				y = val;
			break;
		case KEYBD :
			qenter( KEYBD, val );
			unqdevice( MOUSEX );
			unqdevice( MOUSEY );
			unqdevice( MIDDLEMOUSE );
			return	0;
		case INPUTCHANGE :
			break;
			}
		}
	}

#include <machine/param.h>
void
sgi_Animate( fps )
int	fps;
	{	register int	i, j;
		register int	wid;
		static long	movie_gid = -1;
		long		xwin, ywin;
		long		movie_xwin, movie_ywin;
	if( fps < 1 )
		fps = 1;
	if( fps > HZ )
		fps = HZ;
	wid = 512 / grid_sz;

	/* Get origin of frame buffer window (source).			*/
	getorigin( &xwin, &ywin );

	/* Create destination window for movie, with user positioning.	*/
	prefsize( grid_sz, grid_sz );
	if( (movie_gid = winopen( "movie" )) == -1 )
		{
		fb_log( "No more graphics ports available.\n" );
		return;
		}
	/* Adjust window position optimally for fast "rectcopy()".	*/
	getorigin( &movie_xwin, &movie_ywin );
	if( ((xwin - movie_xwin) % 16) != 0 )
		movie_xwin += (xwin - movie_xwin) % 16;
	while( movie_xwin > XMAXSCREEN - grid_sz )
		movie_xwin -= 16;
	winmove( movie_xwin, movie_ywin );

	fullscrn();
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
			rectcopy((Screencoord)(xwin+j*grid_sz),
				 (Screencoord)(ywin+i*grid_sz),
				 (Screencoord)(xwin+(j+1)*grid_sz)-1,
				 (Screencoord)(ywin+(i+1)*grid_sz)-1,
				 (Screencoord) movie_xwin,
				 (Screencoord) movie_ywin
				 );
			}
		}
	}

_LOCAL_ void
sgi_Read_Keyboard( args )
char	**args;
	{	char		input_ln[BUFSIZ];
		register int	i;
		register char	*eof_flag;
	get_Input( input_ln, BUFSIZ, ": " );
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
	winattach();
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
