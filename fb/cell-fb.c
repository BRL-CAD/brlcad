/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "fb.h"

#ifndef STATIC
#define STATIC static
#endif

#ifndef true
#define false	0
#define true	1
#endif

typedef int bool;

#define XFB(_x) (int)(xorigin+(((_x)-xmin)/cell_size)*(wid+grid_flag))
#define YFB(_y) (int)(yorigin+(((_y)-ymin)/cell_size+2)*(hgt+grid_flag))

#define LORES	512
#define HIRES	1024

#define MAX_LINE	133

#define PI	3.14159265358979323846264338327950288419716939937511
					/* ratio of circumf. to diam. */
#define HFPI	(PI/2.0)

#define Abs( _a )	((_a) < 0.0 ? -(_a) : (_a))

#ifndef Min
#define Min( a, b )		((a) < (b) ? (a) : (b))
#define Max( a, b )		((a) > (b) ? (a) : (b))
#define MinMax( m, M, a )    { m = Min( m, a ); M = Max( M, a ); }
#endif

#define NEG_INFINITY	-10000000.0
#define POS_INFINITY	 10000000.0

typedef struct cell	Cell;

static struct cell
	{
	double	c_x;
	double	c_y;
	double	c_val;
	}
*grid;


static char	*usage[] = {
"",
"cell-fb ($Revision$)",
"",
"Usage: cell-fb [-F dev][-eghik][-bcfv n][-m \"n r g b\"][-p \"x y\"][-s \"w h\"]",
"",
"  option -F dev        Specify frame buffer device to use.",
"         -b n          Ignore values not equal to `n'.",
"         -c n          Specify cell size as `n' (default is 4 units).",
"         -e            Erase frame buffer before displaying picture.",
"         -f n          Display field `n' of cell data.",
"         -g            Leave space between cells.",
"         -h            Force high-resolution display (rather than best fit).",
"         -i            Round values (default is to interpolate colors).",
"         -k            Display color key.",
"         -m \"n r g b\"  Map value `n' to color ``r g b''.",
"         -p \"x y\"      Offset picture from bottom-left corner of display.",
"         -s \"w h\"      Specify width and height, in pixels, of each cell.",
"         -v n          Specify view number to use (default is all views).",
0
};

static char fbfile[MAX_LINE] = {0};

static double bool_val;
static double cell_size = 4.0;

static double xmin;
static double ymin;
static double xmax;
static double ymax;

static bool boolean_flag = false;
static bool debug_flag = false;
static bool erase_flag = false;
static bool grid_flag = false;
static bool hires_flag = false;
static bool interp_flag = true;
static bool key_flag = false;

static int field = 1;
static int wid = 10, hgt = 10;
static int xorigin = 0, yorigin = 0;
static int view_flag = 0;

static long maxcells = 10000;

static FBIO *fbiop = FBIO_NULL;

static RGBpixel	colortbl[12] =
			{
			{ 255, 255, 255 }, /* White */
			{ 100, 100, 140 }, /* Blue-grey */
			{   0,   0, 255 }, /* Blue */
			{   0, 120, 255 }, /* Light blue */
			{ 100, 200, 140 }, /* Turquoise */
			{   0, 150,   0 }, /* Dark green */
			{   0, 225,   0 }, /* Green */
			{ 255, 255,   0 }, /* Yellow */
			{ 255, 160,   0 }, /* Tangerine */
			{ 255, 100, 100 }, /* Pink */
			{ 255,   0,   0 }, /* Red */
			{   0,   0,   0 }  /* Black */
			};
#define MAX_COLORTBL	11
#define WHITE		colortbl[0]
#define BACKGROUND	colortbl[MAX_COLORTBL]

STATIC bool get_OK();
STATIC bool pars_Argv();
STATIC long read_Cell_Data();
STATIC void init_Globs();
STATIC void prnt_Usage();
STATIC void val_To_RGB();

main( argc, argv )
char	*argv[];
	{	static long ncells;
	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}
	if( (grid = (Cell *) malloc( sizeof(Cell)*maxcells )) == NULL )
		{
		fb_log( "%s: couldn't allocate space for %d cells.\n",
			argv[0], maxcells );
		return	1;
		}
	do
		{
		init_Globs();
		if( (ncells = read_Cell_Data()) == 0 )
			{
			fb_log( "%s: failed to read view.\n", argv[0] );
			return	1;
			}
		fb_log(	"Displaying %ld cells.\n", ncells );
		if( ! display_Cells( ncells ) )
			{
			fb_log( "%s: failed to display %ld cells.\n",
				argv[0], ncells );
			return	1;
			}
		}
	while( view_flag == 0 && ! feof( stdin ) && get_OK()  );
	return	0;
	}

STATIC long
read_Cell_Data()
	{	static char linebuf[MAX_LINE];
		static char format[MAX_LINE];
		register int past_data = false;
		static int past_header = false;
		int i;
		register Cell *gp = grid;
		int view_ct = 1;

	(void) strcpy( format, "%lf %lf" );
	for( i = 1; i < field; i++ )
		(void) strcat( format, " %*lf" );
	(void) strcat( format, " %lf" );
	if( ! past_header && fgets( linebuf, MAX_LINE, stdin ) == NULL )
		return	0;
	do
		{	double x, y, value;
			int items;
		if( (gp - grid) >= maxcells )
			{	long ncells = gp - grid;
			maxcells *= 2;
			if( (grid =
				(Cell *)realloc( grid, sizeof(Cell)*maxcells ))
				 == NULL )
				{
				fb_log( "Can't allocate space for %d cells.\n",
					maxcells );
				return	0; /* failure */
				}
			gp = grid + ncells;
#ifdef DEBUG
			fb_log( "maxcells increased to %ld.\n", maxcells );
#endif
			}
		while( (items = sscanf( linebuf, format, &x, &y, &value ))
			!= 3 )
			{
			if( past_header )
				past_data = true;
			/* Check for EOF condition. */
			if(	feof( stdin )
			   ||	fgets( linebuf, MAX_LINE, stdin ) == NULL
				)
				return	gp - grid;
			}
		if( past_data )
			{
			if( view_flag == 0 || view_flag == view_ct++ )
				return	gp - grid;
			else /* Not the selected view, read the next one. */
				{
				past_data = false;
				continue;
				}
			}
		past_header = true;

		/* If user has selected a view, only store values for that
			view. */
		if( view_flag == 0 || view_flag == view_ct )
			{
			MinMax( xmin, xmax, x );
			MinMax( ymin, ymax, y );
			gp->c_x = x;
			gp->c_y = y;
			gp->c_val = value;
			gp++;
			}
		}
	while( fgets( linebuf, MAX_LINE, stdin ) != NULL );
	return	gp - grid;
	}

STATIC bool
get_OK()
	{	int c;
		FILE *infp;
	if( (infp = fopen( "/dev/tty", "r" )) == NULL )
		{
		fb_log( "Can't open /dev/tty for reading.\n" );
		return	false;
		}
	(void) printf( "Another view follows.  Display ? [y/n](y) " );
	(void) fflush( stdout );
	switch( (c = getc( infp )) )
		{
	case '\n' :
		break;
	default :
		while( getc( infp ) != '\n' )
			; /* Read until user hits <RETURN>.		*/
		break;
		}
	(void) fclose( infp );
	if( c == 'n' )
		return	false;
	return	true;
	}

STATIC void
init_Globs()
	{
	xmin = POS_INFINITY;
	ymin = POS_INFINITY;
	xmax = NEG_INFINITY;
	ymax = NEG_INFINITY;
	return;
	}


STATIC bool
display_Cells( ncells )
long ncells;
	{	register Cell *gp, *ep = &grid[ncells];
		static int fbsize;
		static int zoom;
		static RGBpixel	buf[HIRES];
		static RGBpixel	pixel;
		double lasty = NEG_INFINITY;
		double dx, dy;
		register int y0 = 0, y1;
	dx = (xmax - xmin + 1.0)/cell_size * wid;
	dy = (ymax - ymin + 1.0)/cell_size * hgt;
	if( dx > LORES || dy > LORES )
		hires_flag = true;
	fbsize = hires_flag ? HIRES : LORES;
	zoom = 1;
	if( (fbiop = fb_open( fbfile[0] != '\0' ? fbfile : NULL, fbsize, fbsize ))
		== FBIO_NULL )
		return	false;
	if( fb_wmap( fbiop, COLORMAP_NULL ) == -1 )
		fb_log( "Can not initialize color map.\n" );
	if( fb_zoom( fbiop, zoom, zoom ) == -1 )
		fb_log( "Can not set zoom <%d,%d>.\n", zoom, zoom );
	if( erase_flag && fb_clear( fbiop, BACKGROUND ) == -1 )
		fb_log( "Can not clear frame buffer.\n" );
	for( gp = grid; gp < ep; gp++ )
		{	register int x0, x1;
		/* Whenever Y changes, write out row of cells.		*/
		if( lasty != gp->c_y )
			{
			/* If first time, nothing to write out.		*/
			if( lasty != NEG_INFINITY )
				{
				for(	y0 = YFB( lasty ), y1 = y0 + hgt;
					y0 < y1;
					y0++ )
					{
					if( fb_write( fbiop, 0, y0,
							buf, fbsize ) == -1 )
						{
						fb_log( "Couldn't write to <%d,%d>\n",
							0, y0 );
						(void) fb_close( fbiop );
						return	false;
						}
					}
				}

			/* Clear buffer. */
			for( x0 = 0; x0 < fbsize; x0++ )
				{
				COPYRGB( buf[x0], BACKGROUND );
				}


			/* Draw grid line between rows of cells. */
			if( grid_flag )
				{
				if( fb_write( fbiop, 0, y0, buf, fbsize ) == -1 )
					{
					fb_log( "Couldn't write to <%d,%d>\n",
						0, y0 );
					(void) fb_close( fbiop );
					return	false;
					}
				}
			lasty = gp->c_y;
			}
		val_To_RGB( gp->c_val, pixel );
		for( x0 = XFB( gp->c_x ), x1 = x0 + wid; x0 < x1;  x0++ )
			{
			COPYRGB( buf[x0], pixel );
			}
		}
	/* Write out last row of cells. */
	for( y0 = YFB( lasty ), y1 = y0 + hgt; y0 < y1;  y0++ )
		if( fb_write( fbiop, 0, y0, buf, fbsize ) == -1 )
			{
			fb_log( "Couldn't write to <%d,%d>\n", 0, y0 );
			(void) fb_close( fbiop );
			return	false;
			}
	/* Draw color key. */
	if( key_flag )
		{	register int i, j;
			double base;
		/* Clear buffer. */
		for( i = 0; i < fbsize; i++ )
			{
			COPYRGB( buf[i], BACKGROUND );
			}
		base = (xmin+xmax)/2-5*cell_size;
		base = XFB( base );
		for( i = 0; i <= 10; i++ )
			{	double val = i/10.0;
			val_To_RGB( val, pixel );
			for( j = 0; j < wid; j++ )
				{	int offset = i * (wid+grid_flag);
					register int index = base + offset + j;
				COPYRGB( buf[index], pixel );
				}
			}
		for( i = yorigin; i < yorigin+hgt; i++ )
			if( fb_write( fbiop, 0, i, buf, fbsize ) == -1 )
				{
				fb_log( "Couldn't write to <%d,%d>\n", 0, i );
				(void) fb_close( fbiop );
				return	false;
				}
		}
	(void) fb_close( fbiop );
	return	true;
	}

STATIC void
val_To_RGB( val, rgb )
double val;
RGBpixel rgb;
	{
	if(	boolean_flag && val != bool_val
	    ||	val < 0.0 || val > 1.0
		)
		{
		COPYRGB( rgb, BACKGROUND );
		}
	else
	if( val == 0.0 )
		{
		COPYRGB( rgb, WHITE );
		}
	else
		{	int index;
			double rem;
			double res;
		val *= 10.0; /* convert to range [0.0 to 10.0] */
		if( interp_flag )
			{
			index = val + 0.01; /* convert to range [0 to 10] */
			if( (rem = val - (double) index) < 0.0 ) /* remainder */
				rem = 0.0;
			res = 1.0 - rem;
			rgb[RED] = res*colortbl[index][RED] +
						rem*colortbl[index+1][RED];
			rgb[GRN] = res*colortbl[index][GRN] +
						rem*colortbl[index+1][GRN];
			rgb[BLU] = res*colortbl[index][BLU] +
						rem*colortbl[index+1][BLU];
			}
		else
			{
			index = val + 0.51;
			COPYRGB( rgb, colortbl[index] );
			}
		}
	return;
	}

STATIC bool
pars_Argv( argc, argv )
register char **argv;
	{	register int c;
		extern int optind;
		extern char *optarg;
	/* Parse options.						*/
	while( (c = getopt( argc, argv, "F:b:c:ef:ghikm:p:s:v:" )) != EOF )
		{
		switch( c )
			{
		case 'F' :
			(void) strncpy( fbfile, optarg, MAX_LINE );
			break;
		case 'b' :
			if( sscanf( optarg, "%lf", &bool_val ) != 1 )
				{
				fb_log( "Can't read boolean value.\n" );
				return	false;
				}
			boolean_flag = true;
			break;
		case 'c' :
			if( sscanf( optarg, "%lf", &cell_size ) != 1 )
				{
				fb_log( "Can't read cell size.\n" );
				return	false;
				}
			break;
		case 'e' :
			erase_flag = true;
			break;
		case 'f' :
			if( sscanf( optarg, "%d", &field ) != 1 )
				{
				fb_log( "Can't read field specifier.\n" );
				return	false;
				}
			break;
		case 'g' :
			grid_flag = true;
			break;
		case 'h' :
			hires_flag = true;
			break;
		case 'i' :
			interp_flag = false;
			break;
		case 'k' :
			key_flag = true;
			break;

		case 'm' :
			{	double		value;
				RGBpixel	rgb;
				int		red, grn, blu;
				int		index;
			if( sscanf( optarg, "%lf %d %d %d", &value, &red, &grn, &blu ) < 4 )
				{
				fb_log( "Can't read arguments to -m option.\n" );
				return	false;
				}
			value *= 10.0;
			index = value + 0.01;
			if( index < 0 || index > MAX_COLORTBL )
				{
				fb_log( "Value out of range (%s).\n", optarg );
				return	false;
				}
			rgb[RED] = red;
			rgb[GRN] = grn;
			rgb[BLU] = blu;
			COPYRGB( colortbl[index], rgb );
			break;
			}
		case 'p' :
			if( sscanf( optarg, "%d %d", &xorigin, &yorigin ) < 2 )
				{
				fb_log( "Can't read X and Y coordinates.\n" );
				return	false;
				}
			break;
		case 's' :
			if( sscanf( optarg, "%d %d", &wid, &hgt ) < 2 )
				{
				fb_log( "Can't read width and height.\n" );
				return	false;
				}
			break;
		case 'v' :
			if( sscanf( optarg, "%d", &view_flag ) < 1 )
				{
				fb_log( "Can't read view number.\n" );
				return	false;
				}
			break;
		case '?' :
			return	false;
			}
		}
	if( argc != optind )
		{
		fb_log( "Too many arguments!\n" );
		return	false;
		}
	return	true;
	}


/*	prnt_Usage() --	Print usage message.				*/
STATIC void
prnt_Usage()
	{	register char **p = usage;
	while( *p )
		fb_log( "%s\n", *p++ );
	return;
	}
