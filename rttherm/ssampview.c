/*
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

int	width = 64;
int	height = 64;

char	*basename = "mtherm";
char	spectrum_name[100];

FBIO	*fbp;

struct rt_spectrum	*spectrum;

struct rt_spect_sample	*ss;

char	*pixels;

fastf_t	maxval, minval;

main( argc, argv )
{
	int	len;
	int	fd;
	int	i;

	if( (fbp = fb_open( NULL, width, height )) == FBIO_NULL )  {
		rt_bomb("Unable to open fb\n");
	}
	fb_view( fbp, width/2, height/2, fb_getwidth(fbp)/width, fb_getheight(fbp)/height );

	/* Read spectrum definition */
	sprintf( spectrum_name, "%s.spect", basename );
	spectrum = (struct rt_spectrum *)rt_read_spectrum( spectrum_name );
	if( spectrum == NULL )  {
		rt_bomb("Unable to read spectrum\n");
	}

	len = width * height * RT_SIZEOF_SPECT_SAMPLE(spectrum);
	ss = (struct rt_spect_sample *)rt_malloc( len, "rt_spect_sample" );
	pixels = rt_malloc( width * height * 3, "pixels[]" );

	if( (fd = open(basename, 0)) <= 0 )  {
		perror(basename);
		rt_bomb("Unable to open spectral samples\n");
	}
	if( read( fd, (char *)ss, len ) != len )  {
		rt_bomb("Read of spectral samples failed\n");
	}
	close(fd);

	find_minmax();
	rt_log("min = %g, max=%g Watts\n", minval, maxval );

	for( i = 0; i < spectrum->nwave; i++ )  {
		rt_log("%g um to %g um\n", spectrum->wavel[i] * 0.001,
			spectrum->wavel[i+1] * 0.001 );
		rescale(i);
		fb_writerect( fbp, 0, 0, width, height, pixels );
		fb_poll(fbp);
	}
	return 0;
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

	cp = (char *)ss;
	nbytes = RT_SIZEOF_SPECT_SAMPLE(spectrum);

	max = -INFINITY;
	min =  INFINITY;

	for( todo = width * height; todo > 0; todo--, cp += nbytes )  {
		struct rt_spect_sample	*sp;
		sp = (struct rt_spect_sample *)cp;
		RT_CK_SPECT_SAMPLE(sp);
		for( j = 0; j < spectrum->nwave; j++ )  {
			register fastf_t	v;

			if( (v = sp->val[j]) > max )  max = v;
			if( v < min )  min = v;
		}
	}
	maxval = max;
	minval = min;
}

/*
 */
rescale(wav)
int	wav;
{
	char		*cp;
	char		*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;

	cp = (char *)ss;
	nbytes = RT_SIZEOF_SPECT_SAMPLE(spectrum);

	pp = pixels;

	/* Hack the scaling */
	minval = maxval * 0.001;
	maxval = maxval * 0.9;

	scale = 255 / (maxval - minval);
#if 1
	rt_log(" scale = %g, 255 =? %g\n", scale, maxval * scale);
#endif

	for( todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3 )  {
		struct rt_spect_sample	*sp;
		register int		val;

		sp = (struct rt_spect_sample *)cp;
		RT_CK_SPECT_SAMPLE(sp);

		val = (sp->val[wav] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[0] = pp[1] = pp[2] = val;
	}
}
