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
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
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

char	*pixels;		/* en-route to framebuffer */

fastf_t	maxval, minval;				/* Linked with TCL */

Tcl_Interp	*interp;
Tk_Window	tkwin;

int	doit(), doit1();

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

	/* Source the TCL part of this lashup */
	tcl_RcFileName = "./disp.tcl";

	return TCL_OK;
}

main( argc, argv )
char	**argv;
{

	rt_g.debug = 1;

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
	rt_table_scale( atmosphere_orig->table, 1000.0 );
	atmosphere = rt_tabdata_resample( spectrum, atmosphere_orig );

	/* Allocate and read 2-D spectrum array */
	data = rt_tabdata_binary_read( basename, width*height, spectrum );

	/* Allocate framebuffer image buffer */
	pixels = rt_malloc( width * height * 3, "pixels[]" );

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
rescale(wav)
int	wav;
{
	char		*cp;
	char		*pp;
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
show_color(off)
int	off;
{
	char		*cp;
	char		*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;
	struct rt_tabdata	*xyzsamp;

	cp = (char *)data;
	nbytes = RT_SIZEOF_TABDATA(spectrum);

	pp = pixels;
	RT_GET_TABDATA(xyzsamp, spectrum);

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

		rt_tabdata_mul( xyzsamp, sp, cie_x );
		xyz[X] = rt_tabdata_area1( xyzsamp );

		rt_tabdata_mul( xyzsamp, sp, cie_y );
		xyz[Y] = rt_tabdata_area1( xyzsamp );

		rt_tabdata_mul( xyzsamp, sp, cie_z );
		xyz[Z] = rt_tabdata_area1( xyzsamp );

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
	rt_free( (char *)xyzsamp, "xyz sample");
}
