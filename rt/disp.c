/*
 *			D I S P . C
 *
 *  Quickie program to display spectral curves on the framebuffer.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "tabdata.h"
#include "spectrum.h"
#include "fb.h"
#include "tcl.h"
#include "tk.h"

int	width = 64;				/* Linked with TCL */
int	height = 64;				/* Linked with TCL */

char	*basename = "mtherm";
char	spectrum_name[100];

FBIO	*fbp;

struct rt_table		*spectrum;

struct rt_tabdata	*data;

struct rt_tabdata	*atmosphere_orig;
struct rt_tabdata	*atmosphere;
int			use_atmosphere = 0;	/* Linked with TCL */

struct rt_tabdata	*cie_x;
struct rt_tabdata	*cie_y;
struct rt_tabdata	*cie_z;
int			use_cie_xyz = 0;	/* Linked with TCL */
mat_t			xyz2rgb;

struct rt_tabdata	*ntsc_r;
struct rt_tabdata	*ntsc_g;
struct rt_tabdata	*ntsc_b;

unsigned char	*pixels;		/* en-route to framebuffer */

fastf_t	maxval, minval;				/* Linked with TCL */

Tcl_Interp	*interp;
Tk_Window	tkwin;

int	doit(), doit1();
void	find_minmax();
void	rescale(BU_ARGS(int wav));
void	show_color(BU_ARGS(int off));

/*
 *
 *  With no args, returns the number of wavelengths.
 *  With an integer arg, returns the i-th wavelength.
 *
 *  spectrum pointer should be an arg, not implicit.
 */
int
getspectrum( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	int	wl;

	RT_CK_TABLE(spectrum);

	if( argc <= 1 )  {
		sprintf( interp->result, "%d", spectrum->nx );
		return TCL_OK;
	}
	if( argc != 2 )  {
		interp->result = "Usage: getspectrum [wl]";
		return TCL_ERROR;
	}
	wl = atoi(argv[2]);

	if( wl < 0 || wl > spectrum->nx )  {
		sprintf( interp->result, "getspectrum: wavelength %d out of range 0..%d",
			wl, spectrum->nx);
		return TCL_ERROR;
	}
	sprintf( interp->result, "%g", spectrum->x[wl] );
	return TCL_OK;
}

int
getspectval( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	struct rt_tabdata	*sp;
	int	x, y, wl;
	char	*cp;
	fastf_t	val;

	if( argc != 4 )  {
		interp->result = "Usage: getspect x y wl";
		return TCL_ERROR;
	}
	x = atoi(argv[1]);
	y = atoi(argv[2]);
	wl = atoi(argv[3]);

	RT_CK_TABLE(spectrum);

	if( x < 0 || x > width || y < 0 || y > height )  {
		interp->result = "x or y out of range";
		return TCL_ERROR;
	}
	if( wl < 0 || wl >= spectrum->nx )  {
		interp->result = "wavelength out of range";
		return TCL_ERROR;
	}
	cp = (char *)data;
	cp = cp + (y * width + x) * RT_SIZEOF_TABDATA(spectrum);
	sp = (struct rt_tabdata *)cp;
	RT_CK_TABDATA(sp);
	val = sp->y[wl];
	if( use_atmosphere )
		val *= atmosphere->y[wl];
	sprintf( interp->result, "%g", val );
	return TCL_OK;
}

/*
 *  TCL interface to LIBFB.
 *  Points at lower left corner of selected pixel.
 */
int
tcl_fb_cursor( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	FBIO	*ifp;
	int	mode, x, y;

	if( argc != 5 )  {
		interp->result = "Usage: fb_cursor fbp mode x y";
		return TCL_ERROR;
	}
	ifp = (FBIO *)atoi(argv[1]);
	mode = atoi(argv[2]);
	x = atoi(argv[3]);
	y = atoi(argv[4]);

	ifp = fbp;	/* XXX hack, ignore tcl arg. */

	FB_CK_FBIO(ifp);
	if( fb_cursor( ifp, mode, x, y ) < 0 )  {
		interp->result = "fb_cursor got error from library";
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *  Return value of one pixel as RGB tripple, in decimal
 */
int
tcl_fb_readpixel( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	FBIO	*ifp;
	int	mode, x, y;
	unsigned char	pixel[4];

	if( argc != 4 )  {
		interp->result = "Usage: fb_readpixel fbp x y";
		return TCL_ERROR;
	}
	ifp = (FBIO *)atoi(argv[1]);
	x = atoi(argv[2]);
	y = atoi(argv[3]);

	ifp = fbp;	/* XXX hack, ignore tcl arg. */

	FB_CK_FBIO(ifp);
	if( fb_read( ifp, x, y, pixel, 1 ) < 0 )  {
		interp->result = "fb_readpixel got error from library";
		return TCL_ERROR;
	}
	sprintf(interp->result, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU] );
	return TCL_OK;
}

int
tcl_appinit(inter)
Tcl_Interp	*inter;
{
	interp = inter;	/* set global var */
	if( Tcl_Init(interp) == TCL_ERROR )  {
		return TCL_ERROR;
	}

	/* Run tk.tcl script */
	if( Tk_Init(interp) == TCL_ERROR )  return TCL_ERROR;

	Tcl_CreateCommand(interp, "fb_cursor", tcl_fb_cursor, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "fb_readpixel", tcl_fb_readpixel, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateCommand(interp, "doit", doit, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "doit1", doit1, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateCommand(interp, "getspectval", getspectval, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "getspectrum", getspectrum, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_LinkVar( interp, "minval", (char *)&minval, TCL_LINK_DOUBLE );
	Tcl_LinkVar( interp, "maxval", (char *)&maxval, TCL_LINK_DOUBLE );

	Tcl_LinkVar( interp, "width", (char *)&width, TCL_LINK_INT );
	Tcl_LinkVar( interp, "height", (char *)&height, TCL_LINK_INT );
	Tcl_LinkVar( interp, "use_atmosphere", (char *)&use_atmosphere, TCL_LINK_INT );
	Tcl_LinkVar( interp, "use_cie_xyz", (char *)&use_cie_xyz, TCL_LINK_INT );

	/* Specify startup file to invoke when run interactively */
	/* Source the TCL part of this lashup */
	/* Tcl7 way:  tcl_RcFileName = "./disp.tcl"; */
	Tcl_SetVar(interp, "tcl_rcFileName", "./disp.tcl", TCL_GLOBAL_ONLY);

	return TCL_OK;
}

void check( double x, double y, double z);

/* Check XYZ value to spectrum and back */
void
check( x, y, z )
double x, y, z;
{
	point_t	xyz;
	point_t	new;
	struct rt_tabdata	*tabp;
	FAST fastf_t	tab_area;

	RT_GET_TABDATA( tabp, spectrum );
	xyz[X] = x;
	xyz[Y] = y;
	xyz[Z] = z;
	VPRINT( "\nstarting xyz", xyz );

	rt_spect_xyz_to_curve( tabp, xyz, cie_x, cie_y, cie_z );
	rt_pr_table_and_tabdata( "/dev/tty", tabp );
	tab_area = rt_tabdata_area2( tabp );
	bu_log(" tab_area = %g\n", tab_area);

	rt_spect_curve_to_xyz( new, tabp, cie_x, cie_y, cie_z );

	VPRINT( "new xyz", new );
	rt_free( (char *)tabp, "struct rt_tabdata" );
exit(2);
}

int
main( argc, argv )
char	**argv;
{

	rt_g.debug = 1;

#if 1
{
struct rt_tabdata	*flat;
vect_t			xyz;
/* Code for testing library routines */
spectrum = rt_table_make_uniform( 10, 380.0, 770.0 );
rt_spect_make_CIE_XYZ( &cie_x, &cie_y, &cie_z, spectrum );
bu_log("X:\n");rt_pr_table_and_tabdata( "/dev/tty", cie_x );
bu_log("Y:\n");rt_pr_table_and_tabdata( "/dev/tty", cie_y );
bu_log("Z:\n");rt_pr_table_and_tabdata( "/dev/tty", cie_z );

rt_spect_make_NTSC_RGB( &ntsc_r, &ntsc_g, &ntsc_b, spectrum );
bu_log("R:\n");rt_pr_table_and_tabdata( "/dev/tty", ntsc_r );
bu_log("G:\n");rt_pr_table_and_tabdata( "/dev/tty", ntsc_g );
bu_log("B:\n");rt_pr_table_and_tabdata( "/dev/tty", ntsc_b );

/* "A flat spectral curve is represente by equal XYZ values".  Hall pg 52 */
flat = rt_tabdata_get_constval( 42.0, spectrum );
bu_log("flat:\n");rt_pr_table_and_tabdata( "/dev/tty", flat );
rt_spect_curve_to_xyz(xyz, flat, cie_x, cie_y, cie_z );
VPRINT("flat xyz?", xyz);

/* Check identity of XYZ->spectrum->XYZ */
check( 1, 0, 0 );
check( 0, 1, 0 );
check( 0, 0, 1 );
check( 1, 1, 1 );
check( 1, 1, 0 );
check( 1, 0, 1 );
check( 0, 1, 1 );
check( .5, .5, .5 );
exit(1);
}
#endif

	rt_make_ntsc_xyz2rgb( xyz2rgb );

	if( (fbp = fb_open( NULL, width, height )) == FBIO_NULL )  {
		rt_bomb("Unable to open fb\n");
	}
	fb_view( fbp, width/2, height/2, fb_getwidth(fbp)/width, fb_getheight(fbp)/height );

	/* Read spectrum definition */
	sprintf( spectrum_name, "%s.spect", basename );
	spectrum = (struct rt_table *)rt_table_read( spectrum_name );
	if( spectrum == NULL )  {
		rt_bomb("Unable to read spectrum\n");
	}

	/* Read atmosphere curve -- input is in microns, not nm */
	atmosphere_orig = rt_read_table_and_tabdata( "std_day_1km.dat" );
	rt_table_scale( (struct rt_table *)(atmosphere_orig->table), 1000.0 );
	atmosphere = rt_tabdata_resample( spectrum, atmosphere_orig );

	/* Allocate and read 2-D spectrum array */
	data = rt_tabdata_binary_read( basename, width*height, spectrum );

	/* Allocate framebuffer image buffer */
	pixels = (unsigned char *)bu_malloc( width * height * 3, "pixels[]" );

	find_minmax();
	rt_log("min = %g, max=%g Watts\n", minval, maxval );

	Tk_Main( argc, argv, tcl_appinit );
	/* NOTREACHED */

	return 0;
}

int
doit( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	int	wl;
	char	cmd[96];

	for( wl = 0; wl < spectrum->nx; wl++ )  {
		sprintf( cmd, "doit1 %d", wl );
		Tcl_Eval( interp, cmd );
	}
	return TCL_OK;
}

int
doit1( cd, interp, argc, argv )
ClientData	cd;
Tcl_Interp	*interp;
int		argc;
char		*argv[];
{
	int	wl;
	char	buf[32];

	if( argc != 2 )  {
		interp->result = "Usage: doit1 wavel#";
		return TCL_ERROR;
	}
	wl = atoi(argv[1]);
	if( wl < 0 || wl >= spectrum->nx )  {
		interp->result = "Wavelength number out of range";
		return TCL_ERROR;
	}

	rt_log("%d: %g um to %g um\n",
		wl,
		spectrum->x[wl] * 0.001,
		spectrum->x[wl+1] * 0.001 );
	if( use_cie_xyz )
		show_color(wl);
	else
		rescale(wl);
	fb_writerect( fbp, 0, 0, width, height, pixels );
	fb_poll(fbp);

	/* export C variables to TCL, one-way */
	/* These are being traced by Tk, this will cause update */
	sprintf(buf, "%d", wl);
	Tcl_SetVar(interp, "x", buf, TCL_GLOBAL_ONLY);
	sprintf(buf, "%g", spectrum->x[wl] * 0.001);
	Tcl_SetVar(interp, "lambda", buf, TCL_GLOBAL_ONLY);

	return TCL_OK;
}

/*
 */
void
find_minmax()
{
	char			*cp;
	int			todo;
	register fastf_t	max, min;
	int		nbytes;
	int		j;

	cp = (char *)data;
	nbytes = RT_SIZEOF_TABDATA(spectrum);

	max = -INFINITY;
	min =  INFINITY;

	for( todo = width * height; todo > 0; todo--, cp += nbytes )  {
		struct rt_tabdata	*sp;
		sp = (struct rt_tabdata *)cp;
		RT_CK_TABDATA(sp);
		for( j = 0; j < spectrum->nx; j++ )  {
			register fastf_t	v;

			if( (v = sp->y[j]) > max )  max = v;
			if( v < min )  min = v;
		}
	}
	maxval = max;
	minval = min;
}

/*
 *			R E S C A L E
 *
 *  Create monochrome image from the spectral data, at wavelength 'wav',
 *  given current min & max values.
 */
void
rescale(wav)
int	wav;
{
	char		*cp;
	unsigned char	*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;
	fastf_t		atmos_scale;

	cp = (char *)data;
	nbytes = RT_SIZEOF_TABDATA(spectrum);

	pp = pixels;

	scale = 255 / (maxval - minval);

	if( use_atmosphere )
		atmos_scale = atmosphere->y[wav];
	else
		atmos_scale = 1;

	for( todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3 )  {
		struct rt_tabdata	*sp;
		register int		val;

		sp = (struct rt_tabdata *)cp;
		RT_CK_TABDATA(sp);

		val = (sp->y[wav] * atmos_scale - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[0] = pp[1] = pp[2] = val;
	}
}


/*
 *			S H O W _ C O L O R
 *
 *  Create color image from spectral curve,
 *  given current min & max values, and frequency offset (in nm).
 *  Go via CIE XYZ space.
 */
void
show_color(off)
int	off;
{
	char		*cp;
	unsigned char	*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;

	cp = (char *)data;
	nbytes = RT_SIZEOF_TABDATA(spectrum);

	pp = pixels;

	scale = 255 / (maxval - minval);

	/* Build CIE curves */
	rt_spect_make_CIE_XYZ( &cie_x, &cie_y, &cie_z, spectrum );

	if( use_atmosphere )  {
		rt_tabdata_mul( cie_x, cie_x, atmosphere );
		rt_tabdata_mul( cie_y, cie_y, atmosphere );
		rt_tabdata_mul( cie_z, cie_z, atmosphere );
	}

	for( todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3 )  {
		struct rt_tabdata	*sp;
		point_t			xyz;
		point_t			rgb;
		register int		val;

		sp = (struct rt_tabdata *)cp;
		RT_CK_TABDATA(sp);

		/* rt_spect_curve_to_xyz( xyz, sp, cie_x, cie_y, cie_z ); */
		xyz[X] = rt_tabdata_mul_area1( sp, cie_x );
		xyz[Y] = rt_tabdata_mul_area1( sp, cie_y );
		xyz[Z] = rt_tabdata_mul_area1( sp, cie_z );

		MAT3X3VEC( rgb, xyz2rgb, xyz );

		val = (rgb[RED] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[RED] = val;

		val = (rgb[GRN] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[GRN] = val;

		val = (rgb[BLU] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[BLU] = val;
	}
}
