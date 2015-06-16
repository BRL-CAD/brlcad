/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/*
 * fant.c - Perform spacial transforms on images. (should be "remap")
 *
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jun 25 1986
 * Copyright (c) 1986 John W. Peterson
 *
 * Last Modified by:
 *              James S. Painter
 * 		Computer Science Dept.
 * 		University of Utah
 *
 * Date:	Fri Jun 15 1990
 * Purpose:   Add -b option; speed up inner loop through fixed point arithmetic
 *
 * Date:        Fri Nov 23 1990
 * Purpose:  Add -S option; change coordinate system convention so that
 *           scale factors are more intuitive;  Redo interp_row (sigh, again);
 *           added blur_factor to -b argument
 *
 * 		Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Feb 19, 1991
 * Purpose: Add -c option: scale image so that it fits in -S
 * 	    rectangle, but keep its aspect ratio constant, and center
 * 	    the result if it doesn't exactly fit the rectangle.
 *
 * $Id$
 */

/*
 * This program performs spatial transforms on images.  For full
 * details, consult the paper:
 *      Fant, Karl M. "A Nonaliasing, Real-Time, Spatial Transform
 *                     Technique", IEEE CG&A, January 1986, p. 71
 *
 * Editorial note:  This is not a particularly elegant example of toolkit
 * programming.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

#define MAXCHAN 256

#ifndef PI
#define PI 3.14159265358979323846
#endif


#define H_PASS 0
#define V_PASS 1

/* Conversion macros */

#define FRAC(x) ((x) - ((int) floor(x)))
#define ROUND(x) ((int)((x) + 0.5))
#define DEG(x) ((x) * PI / 180.0)
#define EPSILON .0001

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))



typedef struct point
{
    double x,y;
} point;

/*
 * Each channel is stored in its own raster (an array_lines * array_width
 * array of pixels).  This allows each channel to be transformed separately.
 * A single copy is used and updated in place.
 */
rle_pixel *** rast;
rle_pixel *bufin;		/* A single channel scanline */

int const_ind;			/* Constant index */
int cur_chan;			/* Which channel we're munging */
int pass;			/* Which pass we're on (h or v) */
rle_hdr in_hdr, out_hdr;
int nchan;
int array_width;		/* Width of getrow line (0..xmax) */
int outlinewidth, array_lines;
int verboseflag;		/* Be chatty (Fant can be slow) */
int originflag;			/* Center picture on given orig instead of center */
int X_origin, Y_origin;
int vpassonlyflag;		/* If true, we only need the vertical pass */


/* External and Forward declarations */
extern void *mallocNd();
void interp_row(), fast_interp_row(), general_interp_row();
void getraster(), xform_image(), putraster(), clear_raster(), xform_points();


int
main(argc,argv)
int argc;
char *argv[];
{
    char 	 *infilename = NULL,
                 *out_fname = NULL;
    int		 oflag = 0;
    int		 rle_cnt;
    int 	 scaleflag = 0,
                 sizeflag = 0,
    		 centerflag = 0,
                 angleflag = 0,
    		 translateflag = 0,
                 blurflag = 0;
    double 	 xscale = 1.0,
                 yscale = 1.0,
                 xsize,
                 ysize,
                 blur = 1.0,
    		 angle = 0.0,
    		 xtrans = 0.0,
    		 ytrans = 0.0;
    FILE 	 *outfile = stdout;
    int          rle_err;
    int          i, i1, i2;
    point        p[5];		/* "5" so we can use 1-4 indices Fant does. */
    int          dims[3];


    X_origin = 0;
    Y_origin = 0;
    verboseflag = 0;
    originflag = 0;
    vpassonlyflag = 0;

    if (scanargs( argc, argv,
"% s%-xscale!Fyscale!F S%-xsize!Fysize!F c%- a%-angle!F b%-blur!F v%- \
p%-xorg!dyorg!d t%-xoff!Fyoff!F \to%-outfile!s infile%s",
                 &scaleflag, &xscale, &yscale,
		 &sizeflag, &xsize, &ysize, &centerflag,
		 &angleflag, &angle,
		 &blurflag,  &blur,
                 &verboseflag, &originflag, &X_origin, &Y_origin,
		 &translateflag, &xtrans, &ytrans,
		 &oflag, &out_fname, &infilename ) == 0)
      exit(-1);

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infilename, "r");
    rle_names( &in_hdr, cmd_name( argv ), infilename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    if (fabs(angle) > 45.0)
      fprintf(stderr,
	      "fant: Warning: angle larger than 45 degrees, image will blur.\n");

    if (sizeflag && (angleflag || scaleflag || originflag || translateflag))
    {
	fprintf( stderr,
  "%s: size option (-S) is incompatible with the angle (-a), scale (-s),\n\
   \ttranslate (-t) and origin (-p) options\n",
		 cmd_name( argv ) );
	exit(-1);
    }
    if ( !sizeflag && centerflag )
	fprintf( stderr,
		 "%s: center option (-c) ignored without size option (-S)\n",
		 cmd_name( argv ) );

    if (blur < 0)
    {
	fprintf( stderr, "fant: blur factor must be positive\n" );
	exit(-1);
    }

    if ((scaleflag && xscale == 1.0) && (angle == 0.0))
    {
	vpassonlyflag = 1;
	if (verboseflag)
	  fprintf(stderr, "fant: Only performing vertical pass\n");
    }

    for ( rle_cnt = 0;
	 (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	 rle_cnt++ )
    {
	nchan = in_hdr.ncolors + in_hdr.alpha;

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	  outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/*
	 * To define the output rectangle, we start with a set of points
	 * defined by the original image, and then rotate and scale them
	 * as desired.  Note that we use a continuous coordinate system:
         * min to max+1  with pixel centers at the midpoints of each unit
         * square.
         *
         *  Mapping between coordinate systems:
	 * if c = continuous coord and d = discrete coord, then
	 *	c = d+.5
	 *	d = floor(c)
	 */
	p[1].x = in_hdr.xmin;		p[1].y = in_hdr.ymin;
	p[2].x = in_hdr.xmax+1; 	p[2].y = in_hdr.ymin;
	p[3].x = in_hdr.xmax+1;         p[3].y = in_hdr.ymax+1;
	p[4].x = in_hdr.xmin;		p[4].y = in_hdr.ymax+1;

	if (sizeflag)
	{
  	      /* Compute the scale factors from the desired destination size*/
	    xscale = xsize / (p[2].x - p[1].x);
	    yscale = ysize / (p[3].y - p[2].y);
	    X_origin = p[1].x;
	    Y_origin = p[1].y;
	    /* If centering, adjust scale factors and origin. */
	    if ( centerflag )
	    {
		if ( xscale < yscale )
		{
		    yscale = xscale;
		    xtrans = 0;
		    ytrans = (ysize - (p[3].y - p[2].y) * yscale) / 2;
		}
		else
		{
		    xscale = yscale;
		    xtrans = (xsize - (p[2].x - p[1].x) * xscale) / 2;
		    ytrans = 0;
		}
	    }
	    originflag = 1;
	}
	xform_points(p, xscale, yscale, angle, xtrans, ytrans );



	/* Map the continous coordinates back to discrete coordinates to
         * determine the output image size.  We only output a pixel if it's
         * a least partially covered.  Partial, for us, means at least
         * EPSILON coverage.  This is a little arbitrary but ensures that
         * extra pixels aren't included on the ends.
         */


	i1 = (int) (p[1].x + EPSILON);
	i2 = (int) (p[4].x + EPSILON);
	out_hdr.xmin = MAX(0,MIN(i1,i2));

	i1 = (int) (p[1].y + EPSILON);
	i2 = (int) (p[2].y + EPSILON);
	out_hdr.ymin = MAX(0,MIN(i1,i2));

	i1 = (int) (p[2].x - EPSILON);
	i2 = (int) (p[3].x - EPSILON);
	out_hdr.xmax = MAX(i1,i2);

	i1 = (int) (p[3].y - EPSILON);
	i2 = (int) (p[4].y - EPSILON);
	out_hdr.ymax = MAX(i1,i2);


	/*
	 * Need to grab the largest dimensions so the buffers will hold the
	 * picture.  The arrays for storing the pictures extend from 0
         * to xmax in width and from ymin to ymax in height.
	 */
	array_width = MAX( out_hdr.xmax, in_hdr.xmax) +1;
	array_lines = MAX(out_hdr.ymax,in_hdr.ymax) -
	              MIN(out_hdr.ymin,in_hdr.ymin) + 1;
	outlinewidth = out_hdr.xmax - out_hdr.xmin + 1;

	/*
	 * Since the array begins at ymin, the four output corner points must be
	 * translated to this coordinate system.
	 */
	for (i = 1; i <= 4; i++)
	  p[i].y -= MIN(out_hdr.ymin,in_hdr.ymin);

	/* Oink. */

	dims[0] = nchan;
	dims[1] = array_lines;
	dims[2] = array_width;
	rast = (rle_pixel ***) mallocNd(3,dims,sizeof(rle_pixel));
	bufin = (rle_pixel *)
	  malloc(MAX(array_lines,array_width)*sizeof(rle_pixel));
	RLE_CHECK_ALLOC( cmd_name( argv ), rast && bufin, "raster" );

	getraster(rast);

	/* Transform each channel */
	cur_chan = 0;
	for (cur_chan = 0; cur_chan < nchan; cur_chan++)
	  xform_image(p,blur);

	putraster(rast);

	rle_puteof( &out_hdr );
	free(rast);
	free(bufin);
    }

    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
      rle_get_error( rle_err, cmd_name( argv ), infilename );
    exit( 0 );
}

/*
 * This transforms one channel (defined by cur_chan) of the image.
 * The result image is based on the points p, using linear
 * interpolation per scanline.
 */
void
xform_image(p,blur)
point *p;
double blur;
{
    double real_outpos, sizefac, delta;
    int i1, i2, ystart, yend;
    int xinlen = in_hdr.xmax - in_hdr.xmin + 1;
    int yinlen = in_hdr.ymax - in_hdr.ymin + 1;

    if (verboseflag)
      fprintf(stderr, "transforming channel %d...\n",
	      cur_chan - in_hdr.alpha);

    /* Vertical pass */
    pass = V_PASS;
/*     clear_raster( rast ); */

    real_outpos = p[1].y;

    sizefac = (p[4].y-p[1].y) / (yinlen);
    delta = (p[2].y - p[1].y) / (xinlen);

    for ( const_ind = in_hdr.xmin;
	 const_ind <= in_hdr.xmax; const_ind++ )
    {
	interp_row( sizefac, blur, 0, real_outpos,
		      in_hdr.ymax - in_hdr.ymin ,
		      array_lines-1  );
	real_outpos += delta;
    }

    if (! vpassonlyflag )
    {
	/* Horizontal pass */
	pass = H_PASS;

	real_outpos = ( ((p[2].y - p[4].y) * (p[1].x - p[4].x))
		       / (p[1].y - p[4].y) ) + p[4].x;
	sizefac = (p[2].x - real_outpos) / (xinlen);
	delta = (p[4].x - real_outpos)
	  / ((double) ((int) p[4].y) - ((int) p[2].y));

	/* If we're moving backwards, start at p1 (needs more thought...) */
	if (delta < 0)
	  real_outpos = p[1].x;

	i1 = (int) (p[1].y + EPSILON);
	i2 = (int) (p[2].y + EPSILON);
	ystart = MIN(i1,i2);

	i1 = (int) (p[3].y - EPSILON);
	i2 = (int) (p[4].y - EPSILON);
	yend = MAX(i1,i2);

	if (ystart < 0)		/* Ensure start isn't negative */
	{
	    real_outpos += delta * abs(ystart);
	    ystart = 0;
	}

	for ( const_ind = ystart; const_ind <= yend; const_ind++ )
	{
	    interp_row( sizefac, blur, in_hdr.xmin, real_outpos,
			  in_hdr.xmax,
			  out_hdr.xmax );
	    real_outpos += delta;
	}
    }
}

/*
 * Transform the points p according to xscale, yscale and angle.
 * Rotation is done first, this allows the separate scaling factors to
 * be used to adjust aspect ratios.  Note the image quality of the
 * resulting transform degrades sharply if the angle is > 45 degrees.
 */
void
xform_points(p, xscale, yscale, angle, xtrans, ytrans)
point *p;
double xscale, yscale, angle, xtrans, ytrans;
{
    double s, c, xoff, yoff;
    double tmp;
    int i;

    /* Sleazy - should build real matrix */

    c = cos(DEG(angle));
    s = sin(DEG(angle));
    if (!originflag)
    {
	xoff =  ((double) (in_hdr.xmax - in_hdr.xmin + 1) / 2.0
		 + in_hdr.xmin);
	yoff =  ((double) (in_hdr.ymax - in_hdr.ymin + 1) / 2.0
		 + in_hdr.ymin);
    }
    else
    {
	xoff = X_origin;
	yoff = Y_origin;
    }
    if (verboseflag)
      fprintf(stderr, "Output rectangle:\n");

    for ( i = 1; i <= 4; i++ )
    {
	p[i].x -= xoff;		/* translate to origin */
	p[i].y -= yoff;

	tmp = p[i].x * c + p[i].y * s; /* Rotate... */
	p[i].y = -p[i].x * s + p[i].y * c;
	p[i].x = tmp;

	p[i].x *= xscale;	/* Scale */
	p[i].y *= yscale;

	p[i].x += ( xoff + xtrans );	/* translate back from origin */
	p[i].y += ( yoff + ytrans );

	if (verboseflag)
	  fprintf(stderr, "  %4.1f\t%4.1f\n", p[i].x, p[i].y);
    }
}




/* Constants used for storing floats as scaled integers */
#define SHIFT 16
#define SCALE (1 << SHIFT)

void
interp_row( sizefac, blur, inmin, real_outpos, inmax, outmax)
double sizefac;			/* scale factor (maps input to output) */
double blur;			/* blur factor 1.0 normal; <1 jaggy; > 1 blury */
int inmin;			/* minimum index in input image */
double real_outpos;		/* output coordinate of input 0 */
int inmax, outmax;		/* upper bounds on image indices */
{
    double pos,			/* input coordinate of first output point */
           delta,		/* delta (in input coords) between output points */
           radius;		/* filter radius (in input coords) */
    int    xmin, xmax,          /* index bounds in output array */
           stride,		/* stride between output array positions */
           inlen;		/* length of input buffer; */
    register rle_pixel *pin, *pout, *pend, *p;


    delta  = 1/sizefac;

    radius = (sizefac > 1.0) ? 1.0 : delta; /* filter radius */
    radius *= blur;
    /* clamp the filter radius on the bottom end to make sure we get complete
       coverage.  */
    if (radius < 0.5+EPSILON) radius = 0.5+EPSILON;


    /*
     *  Find the bounds in the output array
     */
  				/* min in output coords */
    xmin = (int) floor((-radius)*sizefac + real_outpos);
    if (xmin < 0)      xmin = 0;
    if (xmin > outmax) xmin = outmax;
    xmax = floor(inmax - inmin + 1.0 + radius); /* max in input coordinates */
    xmax = floor((xmax + 0.5)*sizefac + real_outpos); /* -> output coords */
    if (xmax < 0)      xmax = 0;
    if (xmax > outmax) xmax = outmax;

    inlen = inmax - inmin + 1;
    if (pass == V_PASS)
    {
	pin  = rast[cur_chan][inmin]+const_ind;
	pout = rast[cur_chan][0]+const_ind;
	stride = array_width;
    }
    else
    {
	pin  = rast[cur_chan][const_ind] + inmin;
	pout = rast[cur_chan][const_ind] + 0;
	stride = 1;
    }


    /*
     *  Copy the input into a row buffer. This saves us some memory so
     *  we don't need a separate output array for the whole image.
     */
    p = bufin;
    pend = bufin + inlen;
    while (p < pend)
      {
	*p++ = *pin;
	pin += stride;
      }
    pin = bufin;

    /* zero out the part of the output array we aren't going to touch */
    p = pout;
    pend = p + stride*xmin;
    while (p < pend)
      {	*p = 0; p += stride; }
    p = pout + stride*(xmax+1);
    pend = pout + stride*(outmax+1);
    while (p < pend)
      {	*p = 0; p += stride; }
    pout += stride*xmin;

    /* input coordinate of first output pixel center */
    pos = (xmin + 0.5 - real_outpos)*delta;

    if (radius != 1.0)
      general_interp_row(pin,inlen,pout,stride,xmin,xmax,pos,radius,delta);
    else
      fast_interp_row(pin,inlen,pout,stride,xmin,xmax, pos,delta);
}


/*
 * Do the actual work of interp row.  This version handles the general
 * case of any filter radius.
 */
void general_interp_row(pin,inlen, pout, stride, xmin, xmax, pos, radius, delta)
register rle_pixel *pin;	/* pointer to input buffer */
int inlen;			/* length of input */
register rle_pixel *pout;	/* pointer to array buffer */
int stride;			/* stride between output values */
int xmin, xmax;			/* range of output values */
double pos;			/* input coord position of first output pixel */
double radius;			/* triangle filter radius */
double delta;			/* input coord delta between output pixels */
{
    register long pxl, fpos, weight, sum, accum;
    register int i, i0, i1, x;
    long rpos, rradius, rdelta ;


    /* This is the general case.  We walk through all the output points.
     * for each output point we map back into the input coordinate system
     * to figure out which pixels affect it.  We sum over all input pixels
     * that overlap the filter support.  A triangle filter is hardwired at
     * present for speed.
     */

    rpos = (long) (pos*SCALE + 0.5); /* fixed point forms */
    rradius = (long) (radius*SCALE + 0.5);
    rdelta = (long) (delta*SCALE + 0.5);


    /* For each output pixel */
    for (x=xmin; x<=xmax; x++)
    {
	/* find bounds in input array */

	i0 = (rpos - rradius + SCALE/2) >>SHIFT; /* floor(pos -radius +0.5); */
	i1 = (rpos + rradius - SCALE/2) >>SHIFT; /* floor(pos +radius -0.5); */

	/* sum over each input pixel which effects this output pixel
         * weights are taken from a triangle filter of width radius.
         */
	sum = accum = 0;
	for(i=i0; i<=i1; i++)
	{
	    /* lookup input pixel (zero's at boundaries) */
	    pxl = (i < 0 || i >= inlen) ? 0 :  pin[i];

	    /* map to filter coordinate system */
	    fpos = (i<<SHIFT) + SCALE/2 - rpos; /* fabs(i+0.5-pos); */
	    if (fpos < 0) fpos = -fpos;

	    weight = rradius - fpos;
	    accum += pxl * weight;
	    sum += weight;
	}
	if (sum > 0)
	  *pout = accum/sum;

	/* advance to next input pixel (in src coordinates) */
	rpos +=  rdelta;		/* pos += delta */
	pout += stride;
    }
}


/*
 *  This is a special case of general_interp_row for use when the filter
 *  radius is exactly 1, a common case.  It amounts to linear interpolation
 *  between the input pixels.
 */
void fast_interp_row(pin,inlen, pout, stride, xmin, xmax, pos, delta)
register rle_pixel *pin;	/* pointer to input buffer */
int inlen;			/* length of input */
register rle_pixel *pout;	/* pointer to output buffer */
int stride;			/* stride between array values */
int xmin, xmax;			/* range of output values */
double pos;			/* input coord of first output pixel center */
double delta;			/* input coord delta between output pixels */
{
    register long pv, nv, inseg;
    register long rdelta;
    register rle_pixel *pend;
    long rpos;
    int i;

    rpos = (long) (pos*SCALE + 0.5); /* fixed point forms */
    rdelta = (long) (delta*SCALE + 0.5);

    /* find starting point in input array */
    rpos -= SCALE/2;
    i  = (rpos  >> SHIFT);
    inseg = (rpos - (i << SHIFT));
    if (inseg < 0)  {  i++;   inseg += SCALE;	}
    if (i > 0)  pin+= i;

    /* Lookup initial pixel values */
    if (i < 0 || i >= inlen)  pv = 0;  else { pv = *pin; pin++; }
    i++;
    if (i < 0 || i >= inlen)  nv = 0;  else { nv = *pin; pin++; }

    /* Loop over output pixels */
    pend = pout + (xmax-xmin)*stride + 1;
    while (pout < pend)
    {
	/* Simple linear interpolation */
	*pout = (pv*(SCALE-inseg) + nv*inseg + SCALE/2) >> SHIFT;

	/* advance to next input pixel (in src coordinates) */
	inseg += rdelta;
	if (inseg > SCALE)
	{
	    pv = nv;
	    i++;
	    if (i < 0 || i >= inlen)
	      nv = 0;
	    else
	      { nv = *pin; pin++; }
	    inseg -= SCALE;
	}
	pout += stride;
    }
}


/*
 * Read all channels of the picture in, placing each channel directly
 * into a separate array.
 */
void
getraster(ras_ptrs)
rle_pixel ***ras_ptrs;
{
    int i, chan;
    rle_pixel *ptrs[MAXCHAN];
    rle_pixel *rows[MAXCHAN];	/* Pointers for getrow/putrow */

    for ( chan = 0; chan < nchan; chan++ )
      ptrs[chan] = ras_ptrs[chan][0];

    for (i = in_hdr.ymin; i <= in_hdr.ymax; i++)
    {
	for ( chan = 0; chan < nchan; chan++ )
	  rows[chan] = ptrs[chan];
	rle_getrow( &in_hdr, &(rows[in_hdr.alpha]) );


	/* Bump pointers */
	for ( chan = 0; chan < nchan; chan++ )
	  ptrs[chan] += array_width;
    }
}

/* Write out the rasters */
void
putraster(ras_ptrs)
rle_pixel ***ras_ptrs;
{
    int i, chan;
    rle_pixel *ptrs[MAXCHAN];
    rle_pixel *rows[MAXCHAN];	/* Pointers for getrow/putrow */

    rle_put_setup( &out_hdr );

    /*
     * If the output image is smaller than the input, we must offset
     * into the pixel array by the difference between the two.
     */
    if (in_hdr.ymin < out_hdr.ymin)
      for ( chan = 0; chan < nchan; chan++ )
	ptrs[chan] = ras_ptrs[chan][(out_hdr.ymin - in_hdr.ymin)];
    else
      for ( chan = 0; chan < nchan; chan++ )
	ptrs[chan] = ras_ptrs[chan][0];

    for (i = out_hdr.ymin; i <= out_hdr.ymax; i++)
    {
	for ( chan = 0; chan < nchan; chan++ )
	  rows[chan] = &((ptrs[chan])[out_hdr.xmin]);
	rle_putrow( &(rows[out_hdr.alpha]), outlinewidth, &out_hdr );

	/* Bump pointers */
	for ( chan = 0; chan < nchan; chan++ )
	  ptrs[chan] += array_width;
    }
}

/* Clear the raster's cur_chan channel */
void
clear_raster(ras_ptr)
rle_pixel ***ras_ptr;
{
    bzero( &(ras_ptr[cur_chan][0][0]), array_width * array_lines );
}

#ifdef DEBUG
/*
 * Dump out a raster (used for debugging).
 */
void
dumpraster(ras_ptrs)
rle_pixel ***ras_ptrs;
{
    int x, y, xmin, ymin, xmax, ymax, chan;

    ymin = MIN(out_hdr.ymin,in_hdr.ymin);
    ymax = MAX(out_hdr.ymax,in_hdr.ymax) - ymin;
    xmin = MIN(out_hdr.xmin,in_hdr.xmin);
    xmax = MAX(out_hdr.xmax,in_hdr.xmax);

    for ( chan = 0; chan < nchan; chan++ )
      for (y = ymax; y >=0 ; y--)
      {
        for (x=xmin; x<=xmax; x++)
	{
	  fprintf(stderr, "%2x ", ras_ptrs[chan][y][x]);
	}
	fprintf(stderr,"\n");
      }

}
#endif


