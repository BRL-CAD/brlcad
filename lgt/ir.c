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
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) ir.c	2.1
		Modified: 	12/10/86 at 16:03:25	G S M
		Retrieved: 	2/4/87 at 08:53:25
		SCCS archive:	/vld/moss/src/lgt/s.ir.c
*/

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./vecmath.h"
#include "./lgt.h"
#include "./tree.h"
#include "./extern.h"
#define IR_DATA_WID	512
#define Avg_Fah(sum)	((sum)/(sample_sz))
#define Kelvin2Fah( f )	(9.0/5.0)*((f)-273.15) + 32.0
#define S_BINS		10
#define S_INTERVAL	(length/(S_BINS+2))
#define S_XMAX		(xmin+(S_INTERVAL*S_BINS))
#define S_XRANGE	(S_XMAX-xmin)
#define S_XPOS		(x-xmin)
#define S_YMAX		(ymin+S_INTERVAL)

static RGBpixel	black = { 0, 0, 0 };
static int	ir_max_index = -1;
RGBpixel	*ir_table = RGBPIXEL_NULL;

_LOCAL_ void	temp_To_RGB();

_LOCAL_ int
adjust_Page( y )
int	y;
	{	int	scans_per_page = 32;/* (_pagesz / sizeof(RGBpixel)) / IR_DATA_WID;*/
	return	y + scans_per_page - (y % scans_per_page);
	}

void
display_Temps( xmin, ymin, length )
register int	xmin, ymin, length;
	{	register int	x, y;
	/* Avoid page thrashing of frame buffer.			*/
	ymin = adjust_Page( ymin );

	if( ir_table == RGBPIXEL_NULL )
		{
		rt_log( "IR table not initialized.\n" );
		return;
		}
	if( ! get_Font( (char *) NULL ) )
		{
		rt_log( "Could not load font.\n" );
		return;
		}
	for( y = ymin; y <= S_YMAX; y++ )
		{
		x = xmin;
		if( fb_seek( fbiop, x, y ) == -1 )
			{
			rt_log( "\"%s\"(%d) fb_seek to pixel <%d,%d> failed.\n",
				__FILE__, __LINE__, x, y
				);
			return;
			}
		for( ; x <= S_XMAX + S_INTERVAL; x++ )
			{	fastf_t	percent;
				static RGBpixel	*pixel;
			percent = S_XPOS / (fastf_t)(S_XRANGE);
			if( S_XPOS % S_INTERVAL == 0 )
				{	int	temp = AMBIENT+percent*RANGE;
					register int	index = temp - ir_min;
				pixel = (RGBpixel *) ir_table[Min(index,ir_max_index)];
				(void) fb_wpixel( fbiop, black );
				}
			else
				(void) fb_wpixel( fbiop, pixel );

			}
		}
	for( x = xmin; x <= S_XMAX; x += S_INTERVAL )
		{	char	tempstr[4];
			fastf_t	percent = S_XPOS / (fastf_t)(S_XRANGE);
			int	temp = AMBIENT+percent*RANGE;
			int	shrinkfactor = fb_getwidth( fbiop )/grid_sz;
		(void) sprintf( tempstr, "%3d", temp );
		do_line(	x+2,
				y-(S_INTERVAL-(12/shrinkfactor))/2,
				tempstr,
				shrinkfactor
				);
		}
	fb_flush( fbiop );
	return;
	}

_LOCAL_ int
get_IR( x, y, fahp, fp )
int	x, y;
int	*fahp;
FILE	*fp;
	{
	if( fseek( fp, (long)((y*IR_DATA_WID + x) * sizeof(int)), 0 ) != 0 )
		return	0;
	else
	if( fread( (char *) fahp, (int) sizeof(int), 1, fp ) != 1 )
		return	0;
	else
		return	1;
	}

read_IR( fp )
FILE	*fp;
	{	register int	fy;
		register int	rx, ry;
		int		min, max;
	if( ! fb_Setup( fb_file, IR_DATA_WID ) )
		return	0;
	fb_Zoom_Window();
	if(	fread( (char *) &min, (int) sizeof(int), 1, fp ) != 1
	     ||	fread( (char *) &max, (int) sizeof(int), 1, fp ) != 1
		)
		{
		rt_log( "Can't read minimum and maximum temperatures.\n" );
		return	0;
		}
	else
		{
		rt_log(	"IR data temperature range is %d to %d\n",
			min, max
			);
		if( ir_min == ABSOLUTE_ZERO )
			{ /* Temperature range not set.			*/
			ir_min = min;
			ir_max = max;
			}
		else 
			{ /* Merge with existing range.			*/
			ir_min = Min( ir_min, min );
			ir_max = Max( ir_max, max );
			rt_log(	"Global temperature range is %d to %d\n",
				ir_min, ir_max
				);
			}
		(void) fflush( stdout );
		}
	if( ! init_Temp_To_RGB() )
		return	0;
	for( ry = 0, fy = 0; ; ry += aperture_sz, fy++ )
		{
		if( fb_seek( fbiop, 0, fy ) == -1 )
			{
			rt_log( "\"%s\"(%d) fb_seek to pixel <%d,%d> failed.\n",
				__FILE__, __LINE__, 0, fy
				);
			return	0;
			}
		for( rx = 0 ; rx < IR_DATA_WID; rx += aperture_sz )
			{	int	fah;
				int	sum = 0;
				register int	i;
				register int	index;
				RGBpixel	*pixel;
			for( i = 0; i < aperture_sz; i++ )
				{	register int	j;
				for( j = 0; j < aperture_sz; j++ )
					{
					if( get_IR( rx+j, ry+i, &fah, fp ) )
						sum += fah < ir_min ? ir_min : fah;
					else	/* EOF */
						{
						if( ir_octree.o_temp == ABSOLUTE_ZERO )
							ir_octree.o_temp = AMBIENT - 1;
						display_Temps(	grid_sz/8,
								fy + 10,
								(grid_sz*3+2)/4
								);
						close_Output_Device();
						return	1;
						}
					}
				}
			fah = Avg_Fah( sum );
			if( (index = fah-ir_min) > ir_max_index || index < 0 )
				{
				rt_log( "temperature out of range (%d)\n",
					fah
					);
				return	0;
				}
			pixel = (RGBpixel *) ir_table[index];
			(void) fb_wpixel( fbiop, pixel );
			}
		}
	}

/*	t e m p _ T o _ R G B ( )
	Map temperatures to spectrum of colors.
	This routine is extracted from the "mandel" program written by
	Douglas A. Gwyn here at BRL, and has been modified slightly
	to suit the input data.
 */
_LOCAL_ void
temp_To_RGB( rgb, temp )
RGBpixel	rgb;
int		temp;
	{	fastf_t		scale = 4.0 / RANGE;
		fastf_t		t = temp;
		fastf_t		hue = 4.0 - ((t < AMBIENT ? AMBIENT :
					      t > HOTTEST ? HOTTEST :
					      t) - AMBIENT) * scale;
		register int	h = (int) hue;	/* integral part	*/
		register int	f = (int)(256.0 * (hue - (fastf_t)h));
					/* fractional part * 256	*/
	if( t == ABSOLUTE_ZERO )
		rgb[RED] = rgb[GRN] = rgb[BLU] = 0;
	else
	switch ( h )
		{
	default:	/* 0 */
		rgb[RED] = 255;
		rgb[GRN] = f;
		rgb[BLU] = 0;
		break;
	case 1:
		rgb[RED] = 255 - f;
		rgb[GRN] = 255;
		rgb[BLU] = 0;
		break;
	case 2:
		rgb[RED] = 0;
		rgb[GRN] = 255;
		rgb[BLU] = f;
		break;
	case 3:
		rgb[RED] = 0;
		rgb[GRN] = 255 - f;
		rgb[BLU] = 255;
		break;
	case 4:
		rgb[RED] = f;
		rgb[GRN] = 0;
		rgb[BLU] = 255;
		break;
/*	case 5:
		rgb[RED] = 255;
		rgb[GRN] = 0;
		rgb[BLU] = 255 - f;
		break;
 */
		}
/*	rt_log( "temp=%d rgb=(%d %d %d)\n", temp, rgb[RED], rgb[GRN], rgb[BLU] );
 */
	return;
	}

/*	i n i t _ T e m p _ T o _ R G B ( )
	Initialize pseudo-color mapping table for the current view.  This
	color assignment will vary with each set of IR data read so as to
	map the full range of data to the full spectrum of colors.  This
	means that a given color will not necessarily have the same
	temperature mapping for different views of the vehicle, but is only
	valid for display of the current view.
 */
init_Temp_To_RGB()
	{	register int	temp, i;
		RGBpixel	rgb;
	if( (aperture_sz = fb_getwidth( fbiop )/grid_sz) < 1 )
		{
		rt_log( "Grid too large for IR application, max. is %d.\n",
			IR_DATA_WID
			);
		return	0;
		}
	sample_sz = Sqr( aperture_sz );
	if( ir_table != RGBPIXEL_NULL )
		/* Table already initialized presumably from another view,
			since range may differ we must create a different
			table of color assignment, so free storage and re-
			initialize.
		 */
		free( (char *) ir_table );
	ir_table = (RGBpixel *) malloc( sizeof(RGBpixel)*((ir_max-ir_min)+1) );
	if( ir_table == RGBPIXEL_NULL )
		{
		Malloc_Bomb(sizeof(RGBpixel)*((ir_max-ir_min)+1));
		fatal_error = TRUE;
		return	0;
		}
	for( temp = ir_min, i = 0; temp <= ir_max; temp++, i++ )
		{
		temp_To_RGB( rgb, temp );
		COPYRGB( ir_table[i], rgb );
		}
	ir_max_index = i - 1;
	return	1;
	}

pixel_To_Temp( pixel )
register RGBpixel	*pixel;
	{	register RGBpixel	*p, *q = (RGBpixel *) ir_table[ir_max-ir_min];
		register int	temp = ir_min;
	for( p = (RGBpixel *) ir_table[0]; p <= q; p++, temp++ )
		{
		if(	(int) p[RED] == (int) pixel[RED]
		    &&	(int) p[GRN] == (int) pixel[GRN]
		    &&	(int) p[BLU] == (int) pixel[BLU]
			)
			return	temp;
		}
	return	ABSOLUTE_ZERO;
	}

do_IR_Model( ap, op )
register struct application	*ap;
Octree				*op;
	{	fastf_t		octant_min[3], octant_max[3];
		fastf_t		delta = modl_radius / pow_Of_2( ap->a_level );
		fastf_t		point[3]; /* Intersection point.	*/
		fastf_t		norml[3]; /* Unit normal at point.	*/
	/* Push ray origin along ray direction to intersection point.	*/
	VJOIN1( point, ap->a_ray.r_pt, ap->a_uvec[0], ap->a_ray.r_dir );

	/* Compute octant RPP.						*/
	octant_min[X] = op->o_points->c_point[X] - delta;
	octant_min[Y] = op->o_points->c_point[Y] - delta;
	octant_min[Z] = op->o_points->c_point[Z] - delta;
	octant_max[X] = op->o_points->c_point[X] + delta;
	octant_max[Y] = op->o_points->c_point[Y] + delta;
	octant_max[Z] = op->o_points->c_point[Z] + delta;

	if( AproxEq( point[X], octant_min[X], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative X-axis.
		 */
		{
		norml[X] = -1.0;
		norml[Y] =  0.0;
		norml[Z] =  0.0;
		}
	else
	if( AproxEq( point[X], octant_max[X], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive X-axis.
		 */
		{
		norml[X] = 1.0;
		norml[Y] = 0.0;
		norml[Z] = 0.0;
		}
	else
	if( AproxEq( point[Y], octant_min[Y], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative Y-axis.
		 */
		{
		norml[X] =  0.0;
		norml[Y] = -1.0;
		norml[Z] =  0.0;
		}
	else
	if( AproxEq( point[Y], octant_max[Y], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive Y-axis.
		 */
		{
		norml[X] = 0.0;
		norml[Y] = 1.0;
		norml[Z] = 0.0;
		}
	else
	if( AproxEq( point[Z], octant_min[Z], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			negative Z-axis.
		 */
		{
		norml[X] =  0.0;
		norml[Y] =  0.0;
		norml[Z] = -1.0;
		}
	else
	if( AproxEq( point[Z], octant_max[Z], EPSILON ) )
		/* Intersection point lies on plane whose normal is the
			positive Z-axis.
		 */
		{
		norml[X] = 0.0;
		norml[Y] = 0.0;
		norml[Z] = 1.0;
		}

	{	/* Factor in reflectance from "ambient" light source.	*/
		fastf_t	intensity = Dot( norml, lgts[0].dir );
		/* Calculate index into false-color table.		*/
		register int	index = op->o_temp - ir_min;
	if( index > ir_max_index )
		{
		rt_log( "Temperature (%d) above range of data.\n", op->o_temp );
		return	-1;
		}
	if( index < 0 )
		/* Un-assigned octants get colored grey.		*/
		ap->a_color[0] = ap->a_color[1] = ap->a_color[2] = intensity;
	else	/* Lookup false-coloring for octant's temperature.	*/
		{
		intensity *= RGB_INVERSE;
		ap->a_color[0] = (fastf_t) (ir_table[index][RED]) * intensity;
		ap->a_color[1] = (fastf_t) (ir_table[index][GRN]) * intensity;
		ap->a_color[2] = (fastf_t) (ir_table[index][BLU]) * intensity;
		}
	}
	return	1;
	}

do_IR_Backgr( ap )
register struct application *ap;
	{
	VMOVE( ap->a_color, bg_coefs );
	return	0;
	}
