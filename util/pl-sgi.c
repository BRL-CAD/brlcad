/*
 *			P L - S G I . C
 *
 *  SGI Iris 3-D Unix Plot display program.
 *
 *  Authors -
 *	Paul R. Stay
 *	Gary S. Moss
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

#ifdef sgi
# include "gl.h"
# include "device.h"
#ifdef mips
# include "gl/addrs.h"
# include "gl/cg2vme.h"
#endif

#define	HUGE	1.0e10	/* for near/far clipping */

#define Min( x1, x2 )	((x1) < (x2) ? (x1) : (x2))
#define Max( x1, x2 )	((x1) > (x2) ? (x1) : (x2))

extern int	getopt();
extern char	*optarg;
extern int	optind;

Matrix	*viewmat;	/* current viewing projection */
Matrix	viewortho;	/* ortho viewing projection */
Matrix	viewpersp;	/* perspective viewing projection */
Matrix	identmat;	/* identity */
Matrix	centermat;	/* center screen matrix */
Coord	viewsize;

int	axis = 0;	/* display coord axis */
int	info = 0;	/* output orientation info */
int	fullscreen = 0;	/* use a full screen window (if mex) */
short	thickness = 0;	/* line thickness */
int	file_input = 0;	/* !0 if input came from a disk file */
int	minobj = 1;	/* lowest active object number */
int	maxobj = 1;	/* next available object number */
long	menu;
char	*menustring;

/*
 *  Color Map:
 *  In doublebuffered mode, you only have 12 bits per pixel.
 *  These get mapped via a 4096 entry colormap.
 *  MEX however steals:
 *    0 - 15	on the 3030 (first 8 are "known" colors)
 *    top 256	on the 4D
 *  Note: as of Release 3.1, 4Sight on the 4D's uses the bottom 32 colors.
 *    To quote makemap, the lowest eight colors are the eight standard
 *    Graphics Library colors (black, red, green, blue, magenta, cyan,
 *    and white).  The next 23 (8 to 30) are a black to white gray ramp.
 *    The remaining 225 colors (31 to 255) are mapped as a 5*9*5 color cube.
 */
/* Map RGB onto 10x10x10 color cube, giving index in range 0..999 */
#ifdef mips
#define MAP_RESERVED	32		/* # slots reserved by MAX */
#else
#define MAP_RESERVED	16		/* # slots reserved by MEX */
#endif
#define COLOR_APPROX(r,g,b)	\
	((r/26)+ (g/26)*10 + (b/26)*100 + MAP_RESERVED)

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "aft:" )) != EOF )  {
		switch( c )  {
		case 'a':
			axis++;
			break;
		case 'f':
			fullscreen++;
			break;
		case 't':
			thickness = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
	}

	return(1);		/* OK */
}

static char usage[] = "\
Usage: pl-sgi [-a -f] [-t thickness] [file.plot]\n";
#endif /* sgi */

main( argc, argv )
int	argc;
char	**argv;
{
#ifdef sgi
	Coord	max[3], min[3];
	char	*file;
	FILE	*fp;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* In case if there is no space command */
	min[0] = min[1] = min[2] = -32768.0;
	max[0] = max[1] = max[2] = 32768.0;

	init_display();

	if( optind >= argc ) {
		makeobj( maxobj );
		uplot( stdin, max, min );
		closeobj( maxobj++ );
	}
	while( optind < argc ) {
		file = argv[optind];
		if( (fp = fopen(file,"r")) == NULL ) {
			fprintf(stderr,"pl-sgi: can't open \"%s\"\n", file);
			exit( 3 );
		}
		file_input = 1;

		makeobj( maxobj );
		uplot( fp, max, min );
		closeobj( maxobj++ );

		fclose( fp );
		optind++;
	}

	/* scale to the largest X, Y, or Z interval */
	viewsize = max[0] - min[0];
	viewsize = Max( viewsize, max[1] - min[1] );
	viewsize = Max( viewsize, max[2] - min[2] );
	viewsize /= 2.0;

	/* set up and save the viewing projection matrix */
	if( ismex() ) {
		if( fullscreen ) {
			/* Compensate for the rectangular display surface */
#ifdef mips
			ortho( -1.25*viewsize, 1.25*viewsize,
				-viewsize, viewsize, -HUGE, HUGE );
#else
			ortho( -1.33*viewsize, 1.33*viewsize,
				-viewsize, viewsize, -HUGE, HUGE );
#endif
		} else {
			ortho( -viewsize, viewsize, -viewsize, viewsize,
				-HUGE, HUGE );
		}
		getmatrix( viewortho );
		perspective( 900, 1.0, 0.01, 1.0e10 );
		/*polarview( 1.414, 0, 0, 0 );*/
		getmatrix( viewpersp );
	} else {
		/* Compensate for the rectangular display surface */
		ortho( -1.33*viewsize, 1.33*viewsize,
			-viewsize, viewsize, -HUGE, HUGE );
		getmatrix( viewortho );
		perspective( 900, 1.0, 0.01, 1.0e10 );
		/*polarview( 1.414, 0, 0, 0 );*/
		getmatrix( viewpersp );
	}
	viewmat = (Matrix *)viewortho;

	/* Make an identity matrix */
	identmat[0][0] = identmat[1][1] = identmat[2][2] = identmat[3][3] = 1.0;
	identmat[1][0] = identmat[1][2] = identmat[1][3] = 0.0;
	identmat[2][0] = identmat[0][1] = identmat[2][3] = 0.0;
	identmat[3][0] = identmat[3][1] = identmat[3][2] = 0.0;

	/* set up and save the initial rot/trans/scale matrix */
	loadmatrix( identmat );
	translate( -(max[0]+min[0])/2.0, -(max[1]+min[1])/2.0,
		-(max[2]+min[2])/2.0 );
	getmatrix( centermat );

	/* set up the command menu */
	menu = defpup( menustring );

	/* set up line thickness/styles */
	if( thickness > 0 )
		linewidth( thickness );
	deflinestyle( 1, 0x8888 );	/* dotted */
	deflinestyle( 2, 0xF8F8 );	/* longdashed */
	deflinestyle( 3, 0xE0E0 );	/* shortdashed */
	deflinestyle( 4, 0x4F4F );	/* dotdashed */

	view_loop();
	exit( 0 );
#else
	printf( "pl-sgi: this is an SGI Iris specific program\n" );
	exit( 1 );
#endif
}

#ifdef sgi
/* 
 *	V I E W _ L O O P
 */

#define	ROTX	DIAL6
#define	ROTY	DIAL4
#define	ROTZ	DIAL2
#define	TRANX	DIAL7
#define	TRANY	DIAL5
#define	TRANZ	DIAL3
#define	ZOOM	DIAL1

#define	ORTHO	SW0
#define	PERSP	SW1
#undef RESET
#define	RESET	SW2

#define	BOTTOM	SW23
#define	TOP	SW24
#define	REAR	SW25
#define	V4545	SW26
#define	RIGHT	SW28
#define	FRONT	SW29
#define	LEFT	SW30
#define	V3525	SW31

#define	MENU_CENTER	1
#define	MENU_AXIS	2
#define	MENU_INFO	3
#define	MENU_SNAP	4
#define	MENU_EXIT	5
char *menustring = "Center|Axis|Info|DunnSnap|Exit";

/*XXX*/
float	tran[3];	/* xyz screen space translate */

view_loop()
{
	Matrix	m;		/* our overall composite orientation matrix */
	Device	event;
	short	val;
	int	end_it = 0;
	int	o = 1;
	long	menuval;
	Matrix	rot;		/* rotations for this pass thru the loop */
	float	scal[3];	/* scale */


	/* Initial translate/rotate/scale matrix */
	loadmatrix( centermat );
	getmatrix( m );
	tran[0] = tran[1] = tran[2] = 0;
	scal[0] = scal[1] = scal[2] = 1;

	/*depthcue(1);*/
	/*cursoff();XXX*/

	/*
	 *  Each time through this loop, m holds the current
	 *  orientation matrix.  An identity matrix is placed
	 *  on the stack and acted on by device inputs.
	 *  After inputs, m = oldm * stack.
	 *  The stack is then replaced by m*viewmat for drawing.
	 */
	while( 1 ) {
		Coord		fval;

		if( qtest() )
			event = qread( &val );
		fval = val;

		loadmatrix ( identmat );
		tran[0] = tran[1] = tran[2] = 0;
		scal[0] = scal[1] = scal[2] = 1;

		switch (event) {
		case ROTX:
			fval *= 5.0;
			rotate( (Angle) fval, 'x' );
			setvaluator(ROTX, 0, -360, 360);
			break;
		case ROTY:
			fval *= 5.0;
			rotate( (Angle) fval, 'y' );
			setvaluator(ROTY, 0, -360, 360);
			break;
		case ROTZ:
			fval *= 5.0;
			rotate( (Angle) fval, 'z' );
			setvaluator(ROTZ, 0, -360, 360);
			break;
		case TRANX:
			fval *= viewsize/200.0;
			tran[0] += fval;
			/*translate( fval, 0.0, 0.0 );*/
			setvaluator(TRANX, 0, -10, 10);
			break;
		case TRANY:
			fval *= viewsize/200.0;
			tran[1] += fval;
			/*translate( 0.0, fval, 0.0 );*/
			setvaluator(TRANY, 0, -10, 10);
			break;
		case TRANZ:
			fval *= viewsize/200.0;
			tran[2] += fval;
			/*translate( 0.0, 0.0, fval );*/
			setvaluator(TRANZ, 0, -10, 10);
			break;
		case ZOOM:
			fval = 1.0 + fval / 1100.0;
			scal[0] *= fval;
			scal[1] *= fval;
			scal[2] *= fval;
			/*scale( fval, fval, fval );*/
			setvaluator(ZOOM, 1, -1000, 1000);
			break;
		case LEFTMOUSE:
			if( val == 0 )
				break;
			fval = 0.5;
			scal[0] *= fval;
			scal[1] *= fval;
			scal[2] *= fval;
			/*scale( fval, fval, fval );*/
			break;
		case RIGHTMOUSE:
		/*case MIDDLEMOUSE:*/
			if( val == 0 )
				break;
			menuval = dopup( menu );
			if( menuval == MENU_EXIT )
				end_it = 1;
			else
				domenu( menuval );
			break;
		case MIDDLEMOUSE:
		/*case RIGHTMOUSE:*/
			if( val == 0 )
				break;
			fval = 2.0;
			scal[0] *= fval;
			scal[1] *= fval;
			scal[2] *= fval;
			/*scale( fval, fval, fval );*/
			break;
		case ORTHO:
			if( val == 0 )
				break;
			viewmat = (Matrix *)viewortho;
			break;
		case PERSP:
			if( val == 0 )
				break;
			viewmat = (Matrix *)viewpersp;
			break;
		case RESET:
			if( val == 0 )
				break;
			/* reset */
			loadmatrix( centermat );
			getmatrix( m );
			loadmatrix( identmat );
			break;
		case BOTTOM:
			if( val != 0 )
			setview( m, 180, 0, 0 );
			break;
		case TOP:
			if( val != 0 )
			setview( m, 0, 0, 0 );
			break;
		case REAR:
			if( val != 0 )
			setview( m, 270, 0, 90 );
			break;
		case V4545:
			if( val != 0 )
			setview( m, 270+45, 0, 270-45 );
			break;
		case RIGHT:
			if( val != 0 )
			setview( m, 270, 0, 0 );
			break;
		case FRONT:
			if( val != 0 )
			setview( m, 270, 0, 270 );
			break;
		case LEFT:
			if( val != 0 )
			setview( m, 270, 0, 180 );
			break;
		case V3525:
			if( val != 0 )
			setview( m, 270+25, 0, 270-35 );
			break;
		}
		if (end_it == 1)
			break;

		if( event ) {
			event = 0;
			qreset();

			getmatrix( rot );
			newview( m, rot, tran, scal, viewmat );

			/* draw the object(s) */
			cursoff();
			color(BLACK);
			clear();
			if( axis )
				draw_axis();
			/* draw all objects */
			for( o = minobj; o < maxobj; o++ ) {
				/* set the default drawing color to white */
				if( ismex() )
					color( COLOR_APPROX(255,255,255) );
				else
					color( (255&0xf0)<<4 | (255&0xf0) | (255>>4) );
				callobj( o );
			}
			curson();
			swapbuffers();
		}

		/* Check for more objects to be read */
		if( !file_input && !feof(stdin) /* && select()*/ ) {
			double	max[3], min[3];
			makeobj( maxobj );
			uplot( stdin, max, min );
			closeobj( maxobj++ );
		}
	}
	/*depthcue( 0 );*/
	curson();
	greset();
	tpon();
	gexit();
}

/* Window Location */
#ifdef mips
#define WINDIM	1024
#define WIN_R	(1279-MARGIN)
#else
#define WINDIM	768
#define WIN_R	(1023-MARGIN)
#endif
#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	20
#define WIN_L	(WIN_R-WINDIM)
#define WIN_B	MARGIN
#define WIN_T	(WINDIM-BANNER-MARGIN)

init_display()
{
	int	i;
	short	r, g, b;
	int map_size;		/* # of color map slots available */

	if( ismex() ) {
		if( fullscreen )
			prefposition( 0, XMAXSCREEN, 0, YMAXSCREEN );
		else
			prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();
		if( winopen( "UNIX plot display" ) == -1 ) {
			printf( "No more graphics ports available.\n" );
			return	1;
		}
		wintitle( "UNIX plot display" );
		/* Free window of position constraint.			*/
		winconstraints();
		tpoff();
		doublebuffer();
		gconfig();	/* Must be called after singlebuffer().	*/
		winattach();

		/*
		 * Deal with the SGI hardware color map
		 * Note: on a 3030, MEX will make getplanes() return
		 *  10 on a 12 bit system (or double buffered 24).
		 *  On the 4D, it still returns 12.
		 */
		map_size = 1<<getplanes(); /* 10 or 12, depending on ismex() */
		map_size -= MAP_RESERVED;	/* MEX's share */
		if( map_size > 1000 )
			map_size = 1000;	/* all we are asking for */

		/* The first 8 entries of the colormap are "known" colors */
		mapcolor( 0, 000, 000, 000 );	/* BLACK */
		mapcolor( 1, 255, 000, 000 );	/* RED */
		mapcolor( 2, 000, 255, 000 );	/* GREEN */
		mapcolor( 3, 255, 255, 000 );	/* YELLOW */
		mapcolor( 4, 000, 000, 255 );	/* BLUE */
		mapcolor( 5, 255, 000, 255 );	/* MAGENTA */
		mapcolor( 6, 000, 255, 000 );	/* CYAN */
		mapcolor( 7, 255, 255, 255 );	/* WHITE */

		/* Use fixed color map with 10x10x10 color cube */
		for( i = 0; i < map_size; i++ ) {
			mapcolor( i+MAP_RESERVED,
				  (short)((i % 10) + 1) * 25,
				  (short)(((i / 10) % 10) + 1) * 25,
				  (short)((i / 100) + 1) * 25 );
		}
	} else {
		/* not mex => 3030 with 12 planes/buffer */
		ginit();
		tpoff();
		doublebuffer();
		onemap();
		gconfig();

		for( b = 0; b < 16; b++ ) {
			for( g = 0; g < 16; g++ ) {
				for( r = 0; r < 16; r++ ) {
					mapcolor( b*256+g*16+r, r<<4, g<<4, b<<4 );
				}
			}
		}
		/* XXX - Hack for the cursor until the above code gets
		 * fixed to avoid the first 8 magic entries! */
		mapcolor( 1, 255, 000, 000 );	/* RED */
	}

	/* enable the mouse */
	qdevice(LEFTMOUSE);
	qdevice(MIDDLEMOUSE);
	qdevice(RIGHTMOUSE);

	/* enable all buttons */
	for( i = 0; i < 32; i++ )
		qdevice(i+SWBASE);

	/* enable all dials */
	for (i = DIAL0; i < DIAL8; i++) {
		qdevice(i);
	}

	/*
	 * SGI Dials: 1024 steps per rev
	 *   -32768 .. 32767
	 */
	setvaluator(ROTX, 0, -360, 360);
	setvaluator(ROTY, 0, -360, 360);
	setvaluator(ROTZ, 0, -360, 360);
	setvaluator(TRANX, 0, -10, 10);
	setvaluator(TRANY, 0, -10, 10);
	setvaluator(TRANZ, 0, -10, 10);
	setvaluator(ZOOM, 1, -1000, 1000);
	noise( ROTX, 5 );
	noise( ROTY, 5 );
	noise( ROTZ, 5 );
	noise( TRANX, 5 );
	noise( TRANY, 5 );
	noise( TRANZ, 5 );
	noise( ZOOM, 5 );

	blanktime( 60 * 60 * 5L );	/* 5 minute blanking */

	return	0;
}


/*
 *  Iris 3-D Unix plot reader
 *
 *  UNIX-Plot integers are 16bit VAX order (little-endian) 2's complement.
 */
#define	geti(fp,x)	{ (x) = getc(fp); (x) |= (short)(getc(fp)<<8); }
#define	getb(fp)	(getc(fp))

uplot( fp, max, min )
FILE	*fp;
Coord	max[3], min[3];
{
	register int	c;
	int	x, y, z, x1, y1, z1, x2, y2, z2;
	int	r, g, b;
	long	l;
	char	str[180];
	double	d[8];
	int 	o;

	/* We have to keep the "current position" ourselves
	 * for the silly labels, since the SGI can't give
	 * us the graphics position inside an object!
	 */
	double	xp, yp, zp;

	xp = yp = zp = 0;
	while( (c = getc(fp)) != EOF ) {
		switch( c ) {
		/* One of a kind functions */
		case 'e':
			if( !file_input ) {
				/* remove any objects, start a new one */
				closeobj( maxobj );
				for( o = minobj; o <= maxobj; o++ )
					delobj( o );
				minobj = maxobj;
				makeobj( maxobj );
			}
			break;
		case 'F':
			/* display everything up to here */
			if( !file_input )
				return;
			break;
		case 'f':
			get_string( fp, str );
			if( strcmp(str, "solid") == 0 )
				setlinestyle( 0 );
			else if( strcmp(str, "dotted") == 0 )
				setlinestyle( 1 );
			else if( strcmp(str, "longdashed") == 0 )
				setlinestyle( 2 );
			else if( strcmp(str, "shortdashed") == 0 )
				setlinestyle( 3 );
			else if( strcmp(str, "dotdashed") == 0 )
				setlinestyle( 4 );
			else {
				fprintf(stderr, "pl-sgi: unknown linestyle \"%s\"\n", str);
				setlinestyle( 0 );
			}
			break;
		case 't':
			get_string( fp, str );
			cmov( xp, yp, zp );	/* all that for this... */
			charstr( str );
			break;
		/* 2D integer */
		case 's':
			geti(fp,x1);
			geti(fp,y1);
			geti(fp,x2);
			geti(fp,y2);
			min[0] = x1; min[1] = y1;
			max[0] = x2; max[1] = y2;
			min[2] = -1.0; max[2] = 1.0;
			break;
		case 'p':
			geti(fp,x);
			geti(fp,y);
			pnti( x, y, 0 );
			xp = x; yp = y; zp = 0;
			break;
		case 'm':
			geti(fp,x);
			geti(fp,y);
			movei( x, y, 0 );
			xp = x; yp = y; zp = 0;
			break;
		case 'n':
			geti(fp,x);
			geti(fp,y);
			drawi( x, y, 0 );
			xp = x; yp = y; zp = 0;
			break;
		case 'l':
			geti(fp,x1);
			geti(fp,y1);
			geti(fp,x2);
			geti(fp,y2);
			movei( x1, y1, 0 );
			drawi( x2, y2, 0 );
			xp = x2; yp = y2; zp = 0;
			break;
		case 'c':
			geti(fp,x);
			geti(fp,y);
			geti(fp,r);
			circ( (double)x, (double)y, (double)r );
			xp = x; yp = y; zp = 0;
			break;
		case 'a':
			geti(fp,x);
			geti(fp,y);
			geti(fp,x1);
			geti(fp,y1);
			geti(fp,x2);
			geti(fp,y2);
			/* ARC XXX */
			break;
		/* 3D integer */
		case 'S':
			geti(fp,x1);
			geti(fp,y1);
			geti(fp,z1);
			geti(fp,x2);
			geti(fp,y2);
			geti(fp,z2);
			min[0] = x1; min[1] = y1; min[2] = z1;
			max[0] = x2; max[1] = y2; max[2] = z2;
			break;
		case 'P':
			geti(fp,x);
			geti(fp,y);
			geti(fp,z);
			pnti( x, y, z );
			xp = x; yp = y; zp = z;
			break;
		case 'M':
			geti(fp,x);
			geti(fp,y);
			geti(fp,z);
			movei( x, y, z );
			xp = x; yp = y; zp = z;
			break;
		case 'N':
			geti(fp,x);
			geti(fp,y);
			geti(fp,z);
			drawi( x, y, z );
			xp = x; yp = y; zp = z;
			break;
		case 'L':
			geti(fp,x1);
			geti(fp,y1);
			geti(fp,z1);
			geti(fp,x2);
			geti(fp,y2);
			geti(fp,z2);
			movei( x1, y1, z1 );
			drawi( x2, y2, z2 );
			xp = x2; yp = y2; zp = z2;
			break;
		case 'C':
			r = getb(fp);
			g = getb(fp);
			b = getb(fp);
			if( ismex() )
				color( COLOR_APPROX(r,g,b) );
			else
				color( (b&0xf0)<<4 | (g&0xf0) | (r>>4) );
			break;
		/* 2D and 3D IEEE */
		case 'w':
			getieee( fp, d, 4 );
			min[0] = d[0]; min[1] = d[1]; min[2] = -1.0;
			max[0] = d[2]; max[1] = d[3]; max[2] = 1.0;
			break;
		case 'W':
			getieee( fp, d, 6 );
			min[0] = d[0]; min[1] = d[1]; min[2] = d[2];
			max[0] = d[3]; max[1] = d[4]; max[2] = d[5];
			break;
		case 'o':
			getieee( fp, d, 2 );
			xp = d[0]; yp = d[1]; zp = 0;
			move( xp, yp, 0.0 );
			break;
		case 'O':
			getieee( fp, d, 3 );
			xp = d[0]; yp = d[1]; zp = d[2];
			move( xp, yp, zp );
			break;
		case 'q':
			getieee( fp, d, 2 );
			xp = d[0]; yp = d[1]; zp = 0;
			draw( xp, yp, 0.0 );
			break;
		case 'Q':
			getieee( fp, d, 3 );
			xp = d[0]; yp = d[1]; zp = d[2];
			draw( xp, yp, zp );
			break;
		case 'x':
			getieee( fp, d, 2 );
			xp = d[0]; yp = d[1]; zp = 0;
			pnt( xp, yp, 0.0 );
			break;
		case 'X':
			getieee( fp, d, 3 );
			pnt( d[0], d[1], d[2] );
			break;
		case 'v':
			getieee( fp, d, 4 );
			move( d[0], d[1], 0.0 );
			draw( d[2], d[3], 0.0 );
			xp = d[2]; yp = d[3]; zp = 0;
			break;
		case 'V':
			getieee( fp, d, 6 );
			move( d[0], d[1], d[2] );
			draw( d[3], d[4], d[5] );
			xp = d[3]; yp = d[4]; zp = d[5];
			break;
		case 'r':
			getieee( fp, d, 6 );
			/*XXX*/
			break;
		case 'i':
			getieee( fp, d, 3 );
			circ( d[0], d[1], d[2] );
			xp = d[0]; yp = d[1]; zp = d[2];
			break;
		default:
			fprintf( stderr, "pl-sgi: bad command '%c' (0x%02x)\n", c, c );
			break;
		}
	}
}

get_string( fp, s )
FILE	*fp;
char	*s;
{
	int	c;

	while( (c = getc(fp)) != '\n' && c != EOF )
		*s++ = c;
	*s = NULL;
}

getieee( fp, out, n )
FILE	*fp;
double	out[];
int	n;
{
	char	in[8*16];
	fread( in, 8, n, fp );
	ntohd( out, in, n );
}

setview( m, rx, ry, rz )
Matrix	m;
int	rx, ry, rz;
{
	/* Hmm... save translation and scale? */
	loadmatrix( centermat );
	getmatrix( m );
	loadmatrix( identmat );

	rotate( (Angle) rx*10, 'x' );
	rotate( (Angle) ry*10, 'y' );
	rotate( (Angle) rz*10, 'z' );
#ifdef never
	calpha = cos( alpha );
	cbeta = cos( beta );    
	cgamma = cos( ggamma ); 

	salpha = sin( alpha );
	sbeta = sin( beta ); 
	sgamma = sin( ggamma );
            
	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = sbeta;

	mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;

	mat[8] = -calpha * sbeta * cgamma + salpha * sgamma;
	mat[9] = calpha * sbeta * sgamma + salpha * cgamma;
	mat[10] = calpha * cbeta;
#endif
}

newview( orient, rot, tran, scal, viewmat )
Matrix	orient, rot, viewmat;
float	tran[3], scal[3];
{
	/*
	 * combine new operations with old
	 *  orient = orient * scal * trans * rot * view
	 */
	loadmatrix( rot );
	translate( tran[0], tran[1], tran[2] );
	scale( scal[0], scal[1], scal[2] );
	multmatrix( orient );
	getmatrix( orient );

	/* set up total viewing transformation */
	loadmatrix( viewmat );
	multmatrix( orient );

	if( info ) {
		printf( "(%f %f %f) scale %f\n",
			orient[3][0], orient[3][1], orient[3][2],
			orient[3][3] );
	}
}

domenu( n )
int	n;
{
	long	left, bottom, winx_size, winy_size;
	long	x, y;
	double	fx, fy;
	int	ret;
#ifdef mips
	long	video;
#endif

	switch( n ) {
	case MENU_CENTER:
		x = getvaluator(CURSORX);
		y = getvaluator(CURSORY);
		getsize( &winx_size, &winy_size);
		getorigin( &left, &bottom );
		fx = 0.5 - (x - left) / (double)winx_size;
		fy = 0.5 - (y - bottom) / (double)winy_size;
		tran[0] += fx * 2.0 * viewsize;
		tran[1] += fy * 2.0 * viewsize;
		break;
	case MENU_AXIS:
		if( axis == 0 )
			axis = 1;
		else
			axis = 0;
		break;
	case MENU_INFO:
		if( info == 0 )
			info = 1;
		else
			info = 0;
		break;
	case MENU_SNAP:
		cursoff();
#ifdef mips
		video = getvideo(DE_R1);
		setvideo( DE_R1, DER1_30HZ|DER1_UNBLANK );
#else
		system("Set30");
#endif
		ret = system("dunnsnap");
#ifdef mips
		setvideo( DE_R1, video );
#else
		system("Set60");
#endif
		curson();
		if( ret ) {
			fprintf( stderr, "pl-sgi: Snap failed. Out of film?\n" );
			ringbell();
		}
		break;
	case MENU_EXIT:
		break;
	}
}

draw_axis()
{
	color( MAGENTA );
	movei( 0, 0, 0 );
	drawi( 10, 0, 0 );
	cmovi( 12, 0, 0 );
	charstr( "x" );
	movei( 0, 0, 0 );
	drawi( 0, 10, 0 );
	cmovi( 0, 12, 0 );
	charstr( "y" );
	movei( 0, 0, 0 );
	drawi( 0, 0, 10 );
	cmovi( 0, 0, 12 );
	charstr( "z" );
	movei( 0, 0, 0 );	/* back to origin */
}
#endif /* sgi */
