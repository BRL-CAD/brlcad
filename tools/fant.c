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
 * Date:	Fri Jun 15 1990
 *
 * Purpose:   Add -b option; speed up inner loop through fixed point arithmetic
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

#include "conf.h"

#include <stdio.h>
#include <math.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "rle.h"

#define MAXCHAN 4
#ifdef M_PI
#define PI M_PI
#endif

#ifndef PI
#define PI 3.141592
#endif

#define and &&
#define or ||				/* C readability */
#define not !

#define H_PASS 0
#define V_PASS 1

/* Conversion macros */

#define FRAC(x) ((x) - ((int) (x)))
#define ROUND(x) ((int)((x) + 0.5))
#define DEG(x) ((x) * PI / 180.0)

#ifdef DEBUG
#define INDEX(ptr,x,y) (*arr_index(ptr,x,y))
#define FINDEX(ptr,x,y) ((double)(*arr_index(ptr,x,y)))
#else
#define INDEX(ptr,x,y) ((ptr)[array_width*(y)+(x)])
#define FINDEX(ptr,x,y) ((double)((ptr)[array_width*(y)+(x)]))
#endif

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

typedef struct point
{
    double x,y;
} point;

/*
 * Each channel is stored in it's own raster (an array_lines * array_width  
 * array of pixels).  This allows each channel to be transformed separately.
 * We need two copies of each set of channels so we can flip
 * between them for the two passes of the algorithm.
 */
rle_pixel * in_rast[MAXCHAN];	/* Assumes channels RGBA */
rle_pixel * out_rast[MAXCHAN];

int const_ind;			/* Constant index */
int cur_chan;			/* Which channel we're munging */
int pass;			/* Which pass we're on (h or v) */
rle_hdr in_hdr, out_hdr;
int nchan;
int array_width;		/* Width of getrow line (0..xmax) */
int outlinewidth, array_lines;
rle_pixel *rows[MAXCHAN];	/* Pointers for getrow/putrow */
int verboseflag;		/* Be chatty (Fant can be slow) */
void (*interp_row)();		/* The interp_row function to use */
int originflag;			/* Center picture on given orig instead of center */
int X_origin, Y_origin;
int vpassonlyflag;		/* If true, we only need the vertical pass */


/* Forward declarations */
void fant_interp_row(double sizefac, int ras_strt, double real_outpos, int inmax, int outmax);
void painter_interp_row(double sizefac, int ras_strt, double real_outpos, int inmax, int outmax);
void getraster(rle_pixel **ras_ptrs), xform_image(point *p), putraster(rle_pixel **ras_ptrs), clear_raster(rle_pixel **ras_ptr), xform_points(point *p, double xscale, double yscale, double angle);

#ifdef DEBUG
rle_pixel * arr_index(ptr,x,y)
rle_pixel * ptr;
{
    if ( x >= array_width )
    {
	fprintf( stderr, "X=%d >= array_width=%d\n", x, array_width );
	abort();
    }
    if ( x < 0 )
    {
	fprintf( stderr, "X=%d < 0\n", x );
	abort();
    }
    if ( y >= array_lines )
    {
	fprintf( stderr, "Y=%d >= array_lines=%d\n", y, array_lines );
	abort();
    }
    if ( y < 0 )
    {
	fprintf( stderr, "Y=%d < 0\n", y );
	abort();
    }
    return &ptr[array_width*(y)+(x)];
}
#endif /* DEBUG */

int
main(int argc, char **argv)
{
    char 	       *infilename = NULL,
    		       *out_fname = NULL;
    int			oflag = 0;
    int			rle_cnt;
    int 		blurflag = 0;	/* Use the "Painter" interp_row */
    int 		scaleflag = 0,
    			angleflag = 0;
    double 		xscale = 1.0,
			yscale = 1.0,
			angle = 0.0;
    FILE 	       *outfile = stdout;
    int rle_err;
    int i;
    point p[5];			/* "5" so we can use 1-4 indices Fant does. */

    /* Need to get around an HP C compiler bug. */
#ifdef hpux
    int tmp_int1, tmp_int2;
#endif /* hpux */

    X_origin = 0;
    Y_origin = 0;
    verboseflag = 0;
    originflag = 0;
    vpassonlyflag = 0;

    if (scanargs( argc, argv, 
		  "% s%-xscale!Fyscale!F a%-angle!F b%- v%- p%-xoff!dyoff!d \
\to%-outfile!s infile%s",
                 &scaleflag, &xscale, &yscale, &angleflag, &angle,
                 &blurflag, &verboseflag, &originflag, &X_origin, &Y_origin,
		 &oflag, &out_fname, &infilename ) == 0)
	exit(-1);

    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infilename, "r");

    if (fabs(angle) > 45.0)
    	fprintf(stderr,
    	  "fant: Warning: angle larger than 45 degrees, image will blur.\n");

    if ((xscale == 1.0) and (angle == 0.0))
    {
	vpassonlyflag = 1;
	if (verboseflag)
	    fprintf(stderr, "fant: Only performing vertical pass\n");
    }
	
    if (blurflag)
      interp_row = painter_interp_row;
    else
      interp_row = fant_interp_row;

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	nchan = in_hdr.ncolors + in_hdr.alpha;

	out_hdr = in_hdr;
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/*
	 * To define the output rectangle, we start with a set of points
	 * defined by the original image, and then rotate and scale them
	 * as desired.
	 */
	p[1].x = in_hdr.xmin;		p[1].y = in_hdr.ymin;
	p[2].x = in_hdr.xmax; 	p[2].y = in_hdr.ymin;
	p[3].x = in_hdr.xmax; 	p[3].y = in_hdr.ymax;
	p[4].x = in_hdr.xmin;		p[4].y = in_hdr.ymax;

	xform_points(p, xscale, yscale, angle );

	/* Make sure the output image is large enough */

#ifdef hpux
	tmp_int1 = p[1].x;
	tmp_int2 = p[4].x;
	out_hdr.xmin = MAX( 0, MIN( tmp_int1, tmp_int2 ) );
	tmp_int1 = p[1].y;
	tmp_int2 = p[2].y;
	out_hdr.ymin = MAX( 0, MIN( tmp_int1, tmp_int2 ) );

	tmp_int1 = ROUND( p[2].x );
	tmp_int2 = ROUND( p[3].x );
	out_hdr.xmax = MAX( tmp_int1, tmp_int2 );
	tmp_int1 = ROUND( p[3].y );
	tmp_int2 = ROUND( p[4].y );
	out_hdr.ymax = MAX( tmp_int1, tmp_int2 );
#else
	out_hdr.xmin = MAX( 0, MIN( (int) p[1].x, (int) p[4].x ) );
	out_hdr.ymin = MAX( 0, MIN( (int) p[1].y, (int) p[2].y ) );

	out_hdr.xmax = MAX( ROUND( p[2].x ), ROUND( p[3].x ) );
	out_hdr.ymax = MAX( ROUND( p[3].y ), ROUND( p[4].y ) );
#endif /* hpux */

	/*
	 * Need to grab the largest dimensions so the buffers will hold the
	 * picture.  The arrays for storing the pictures extend from 0 to
	 * out_hdr.xmax in width and from out_hdr.ymin to ymax in
	 * height.  The reason X goes from 0 to xmax is because rle_getrow
	 * returns the entire row when the image is read in.
	 */
	array_width = MAX( out_hdr.xmax, in_hdr.xmax ) + 1;
	array_lines = MAX( out_hdr.ymax - out_hdr.ymin,
			   in_hdr.ymax - in_hdr.ymin ) + 1;
	outlinewidth = out_hdr.xmax - out_hdr.xmin + 1;

	/*
	 * Since the array begins at ymin, the four output corner points must be
	 * translated to this coordinate system.
	 */
	for (i = 1; i <= 4; i++)
	    p[i].y -= MIN( in_hdr.ymin, out_hdr.ymin );

	/* Oink. */
	for (i = 0; i < nchan; i++)
	{
	    in_rast[i] = (rle_pixel *)malloc( array_width * array_lines );
	    out_rast[i] = (rle_pixel *)malloc( array_width * array_lines );
	    if ((in_rast[i] == NULL) or (out_rast[i] == NULL))
	    {
		fprintf(stderr, "fant: Not enough memory for raster\n");
		exit(-1);
	    }
	}

	getraster(in_rast);

	/* Transform each channel */
	cur_chan = 0;
	for (cur_chan = 0; cur_chan < nchan; cur_chan++) 
	    xform_image(p);

	putraster(out_rast);

	rle_puteof( &out_hdr );
	for ( i =0; i < nchan; i++ )
	{
	    free( in_rast[i] );
	    free( out_rast[i] );
	}
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
xform_image(point *p)
{
    double real_outpos, sizefac, delta;
    rle_pixel * tmprast;
    int ystart, yend;
    int xinlen = in_hdr.xmax - in_hdr.xmin + 1;
    int yinlen = in_hdr.ymax - in_hdr.ymin + 1;

    /* Need to get around an HP C compiler bug. */
#ifdef hpux
    int tmp_int1, tmp_int2;
#endif /* hpux */

    if (verboseflag)
	fprintf(stderr, "transforming channel %d...\n",
		cur_chan - in_hdr.alpha);

    /* Vertical pass */
    pass = V_PASS;
    clear_raster( out_rast );

    real_outpos = p[1].y;

    sizefac = (p[4].y-p[1].y) / (yinlen-1);
    delta = (p[2].y - p[1].y) / xinlen;

    for ( const_ind = in_hdr.xmin;
          const_ind <= in_hdr.xmax; const_ind++ )
    {
	(*interp_row)( sizefac, 0, real_outpos,
	            in_hdr.ymax - in_hdr.ymin + 1,
		    array_lines - 1 );
	real_outpos += delta;
    }

    if (! vpassonlyflag )
    {
	/* Flip buffers */
	tmprast = in_rast[cur_chan];
	in_rast[cur_chan] = out_rast[cur_chan];
	out_rast[cur_chan] = tmprast;

	/* Horizontal pass */
	pass = H_PASS;
	clear_raster( out_rast );

	real_outpos = ( ((p[2].y - p[4].y) * (p[1].x - p[4].x))
		       / (p[1].y - p[4].y) ) + p[4].x;
	sizefac = (p[2].x - real_outpos) / (xinlen-1);
	delta = (p[4].x - real_outpos)
            / ((double) ((int) p[4].y) - ((int) p[2].y) + 1.0 );

	/* If we're moving backwards, start at p1 (needs more thought...) */
	if (delta < 0)
	    real_outpos = p[1].x;

#ifdef hpux
    	tmp_int1 = p[2].y;
    	tmp_int2 = p[1].y;
	ystart = MIN(tmp_int1, tmp_int2);
    	tmp_int1 = p[4].y;
    	tmp_int2 = p[3].y;
	yend = MAX(tmp_int1, tmp_int2);
#else
	ystart = MIN((int) p[2].y, (int) p[1].y);
	yend = MAX((int) p[4].y, (int) p[3].y);
#endif /* hpux */

	if (ystart < 0)		/* Ensure start isn't negative */
	{
	    real_outpos += delta * abs(ystart);
	    ystart = 0;
	}

	for ( const_ind = ystart; const_ind < yend; const_ind++ )
	{
	    (*interp_row)( sizefac, in_hdr.xmin, real_outpos,
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
xform_points(point *p, double xscale, double yscale, double angle)
{
    double s, c, xoff, yoff;		
    double tmp;
    int i;

    /* Sleazy - should build real matrix */

    c = cos(DEG(angle));
    s = sin(DEG(angle));
    if (!originflag)
    {
	xoff =  ((double) (in_hdr.xmax - in_hdr.xmin) / 2.0
		 + in_hdr.xmin);
	yoff =  ((double) (in_hdr.ymax - in_hdr.ymin) / 2.0
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

	p[i].x += ( xoff );	/* translate back from origin */
	p[i].y += ( yoff );

	if (verboseflag)
	    fprintf(stderr, "  %4.1f\t%4.1f\n", p[i].x, p[i].y);
    }
}




/*
 * Pixel indexing is determined by two the_hdr; cur_chan, which determines
 * which raster we read from, and pass, which tells if we're indexing rows
 * or columns.  The scan line or column is also fixed and is determined
 * by the const_ind global.
 */

#define getpxl(index)                                               \
   ( (pass == V_PASS) ?                                             \
      /*V_PASS*/   INDEX(in_rast[cur_chan], const_ind, index) :     \
      /*H_PASS*/   INDEX(in_rast[cur_chan], index, const_ind ) )

#define putpxl(index,pxlval)                                        \
{   unsigned int pxl;						    \
    pxl = pxlval;                                      		    \
    if (pxl > 255) pxl = 255;                                       \
    if (pass == V_PASS)                                             \
	INDEX(out_rast[cur_chan], const_ind, index) =  pxl;         \
    else                                                            \
	INDEX(out_rast[cur_chan], index, const_ind) =  pxl;         \
}


/* Multiply two fixed point numbers (16 bits above and below decimal point)
 * and return the integer part of the result (rounded).
 * No checks for overflow or underflow, sorry.
 *
 * This expression may be too complicated for some compilers.  If so, turn
 * it in to a full fledged function.  It slows things down a little since
 * it's called once per output pixel, but it's not too bad.
 *
 * (Warning: macro args are evaulated twice: beware of side effects in args.)
 */
static unsigned long  _a, _b, /* components of src1 */
                      _c, _d; /* components of src2 */
static unsigned long _result;
#define fixed_mult(src1,src2)           \
( _b = (src1) & 0xFFFF,                 \
  _a = (src1) >> 16,                    \
  _d = (src2) & 0xFFFF,                 \
  _c = (src2) >> 16,                    \
  _result = (_b*_d + 32767) >> 16,      \
  _result += (_a*_d) + (_b*_c) + 32767, \
  _result >>= 16,                       \
  _result += _a*_c                      \
)          


/*
 * Interpolate a row or column pixels.  
 * This is a FIXED POINT version of interp_row which interpolates
 * and box filters each input line (the "Painter" variant).
 *
 * Sizefac is the amount the row is scaled by, ras_strt is the position
 * to start in the input raster, and real_outpos is the place (in
 * floating point coordinates) to output pixels.  The algorithm loops
 * until it's read pixels up to inmax or output up to outmax.
 * 
 * There is one improvement over the algorithm in the paper: the original
 * algorithm holds the last value on input cycles and interpolates only
 * on output cycles.  This can cause a problem with when downsampling
 * images with narrow features (e.g. single pixel lines).  You can see
 * the transition between sample-and-hold and interpolation.  It makes
 * the lines appear alternately too dark and too light depending on their
 * phase with respect to the output grid.  The fix is to always
 * interpolate, integrating over trapezoids instead of rectangles.  It's
 * really no more expensive and the results are much better for
 * pathological examples such as narrow lines.  On the other hand, it
 * results in a slightly blurrier image when the scale factor is close
 * to 1.  The user gets to choose through the -b switch.
 *
 *
 *  It's pretty easy to turn this back into the floating point form
 *  should the need arise:  Replace SCALE by 1.0, eliminate >> 16 shifts,
 *  convert the >>17 shift into a divide by 2.0.
 *
 *   Jamie Painter  June 12, 1990.
 * 
 */
void
painter_interp_row (double sizefac, int ras_strt, double real_outpos, int inmax, int outmax)
{
    double inoff;
    /*
     **  The following are all stored in fixed point, scaled up by 2^16.
     */
    int pv, nv, inptr, outptr;
    register unsigned long inseg, outseg, accum, this1, last, avg, 
             insfac, isizefac;
    
    /*  SCALE is the fixed point scale factor */
#   define SCALE       0x10000

    /* Convert fixed point to integer (with rounding) */
#   define ROUND16(x)  (((x)+32767)>>16)

    isizefac = ((unsigned long) ROUND(sizefac * SCALE ));
    insfac   = (unsigned long) ROUND((double)SCALE / sizefac);
    
    real_outpos += 0.25;   /* Output occurs at end of pixel, not center.
                              Seems like this should be 0.5 but 0.25 is
                               a better empirical match.
                           */
    if (real_outpos > 0.0)
    {
	inseg = SCALE;
	outseg = ROUND( (double) SCALE * ((1.0 - FRAC(real_outpos))/sizefac));
	outptr = (int) real_outpos;
	inptr = ras_strt;
    }
    else				/* Must clip */
    {
	inoff = -real_outpos / sizefac;
	inseg = ROUND( (double) SCALE *(1.0 - FRAC( inoff )));
	outseg = insfac;
	inptr = ras_strt + ((int) inoff);
	outptr = 0;
    }
    accum = 0;

/*
**
**   What's going on here:
**
** Each iteration through the loop steps to the closer of:
**
**           1) the start of next input point (called an input cycle)
**        or
**           2) the end of the next output point (called an output cycle)
**
** The contribution in each cycle is the area of the trapezoid between last
** and this1 (i.e. a simple box filter).
**
**					   nv
**				     ------O
**			      -------
**			------   ^
**                 O----- ^       |
**                pv      |       this1
**		        last
**
**
** pv is the previous input value.
** nv is the next input value.
** this1 is the value at the current iteration (interpolated between pv and nv).
** last is the value at the last iteration.
**
** inseg is a fraction that tells how much of the current input pixel is left.
**      (0.0 <= inseg <= 1.0)
**
** outseg is a faction that tells how much of the current output pixel remains
**      (0.0 <= outseg <= insfac)
**
** The horizontal difference between last and this1 is the smaller of
** inseg and outseg.   
**
*/
    
     if ( (inptr < inmax) and (outptr < outmax) )
     {
	pv =  getpxl( inptr );
	inptr++;
        nv = (inptr < inmax) ?  getpxl( inptr) : 0;
        last = (pv*inseg + nv*(SCALE-inseg));
	for(;;)		
	{
	    if ( outseg > inseg ) /* Finish input pixel */
	    {
                /* new point is next input point */
		this1 = nv << 16;                    /* int->fixed */
		avg = (last + this1 + 65535) >> 17;  /* fixed->int and /2 */
		last = this1;
	        accum += inseg * avg; 
		outseg -= inseg;
		inseg = SCALE;
		inptr++;
		if (inptr > inmax) break;
		pv = nv;
		nv = (inptr < inmax) ?  getpxl( inptr) : 0;
	    }
	    else		/* output cycle */
	    {
		inseg -= outseg;
		/* new point is between input points -> interpolate */
		this1 = (pv*inseg + nv*(SCALE-inseg));
		avg = (last + this1 + 65535) >> 17;  /* fixed->int and /2 */
		last = this1;
	        accum += outseg * avg;  
		outseg = insfac;
		putpxl( outptr, fixed_mult(accum,isizefac));
		outptr++;
		accum = 0;
		if (outptr >= outmax) break;
	    }
	}
    }
    
    /* If we ran out of input, output partially completed pixel */
    if (outptr <= outmax)
      putpxl( outptr, fixed_mult(accum,isizefac));
}



/*
 * Interpolate a row or column pixels.  
 * This is a FIXED POINT version of interp_row.  This version is
 * faithful to Fant's original algorithm, integrating over rectangles
 * instead of trapezoids.  It results in slightly sharper images but
 * they can have nasty artifacts on certain pathological cases (e.g. single
 * pixel wide lines in the input image.
 */
void
fant_interp_row (double sizefac, int ras_strt, double real_outpos, int inmax, int outmax)
{
    double inoff;
    /*
     **  The following are all stored in fixed point, scaled up by 2^16.
     */
    register unsigned long inseg, outseg, pv, nv, accum, this1, insfac, 
             isizefac;
    register int inptr, outptr;

    isizefac = ((unsigned long) ROUND(sizefac * SCALE ));
    insfac   = (unsigned long) ROUND((double)SCALE / sizefac);
    
    if (real_outpos > 0.0)
    {
	inseg = SCALE;
	outseg = ROUND( (double) SCALE * ((1.0 - FRAC(real_outpos))/sizefac));
	outptr = (int) real_outpos;
	inptr = ras_strt;
    }
    else				/* Must clip */
    {
	inoff = -real_outpos / sizefac;
	inseg = (unsigned long) ROUND( (double) SCALE *(1.0 - FRAC( inoff )));
	outseg = insfac;
	inptr = ras_strt + ((int) inoff);
	outptr = 0;
    }
    accum = 0;
    
    if ( (inptr < inmax) and (outptr < outmax) )
    {
	pv =  getpxl( inptr );
	inptr++;
	nv = (inptr <= inmax) ?  getpxl( inptr) : 0;
	for(;;)		
	{
	    this1 = ROUND16(pv*inseg + nv*(SCALE-inseg));
	    if ( outseg > inseg ) /* Finish input pixel */
	    {
                /* new point is next input point */
	        accum += inseg * this1; 
		outseg -= inseg;
		inseg = SCALE;
		inptr++;
		if (inptr > inmax) break;
		pv = nv;
		nv = (inptr <= inmax) ?  getpxl( inptr) : 0;
	    }
	    else		/* output cycle */
	    {
	        accum += outseg * this1;  
		inseg -= outseg;
		outseg = insfac;
		putpxl( outptr, fixed_mult(accum,isizefac));
		outptr++;
		accum = 0;
		if (outptr >= outmax) break;
	    }
	}
    }
    
    /* If we ran out of input, output partially completed pixel */
    if (outptr <= outmax)
		putpxl( outptr, fixed_mult(accum,isizefac));
}


/*
 * Read all channels of the picture in, placing each channel directly
 * into a separate array.
 */
void
getraster(rle_pixel **ras_ptrs)
{
    int i, chan;
    rle_pixel *ptrs[MAXCHAN];

    for ( chan = 0; chan < nchan; chan++ )
	ptrs[chan] = ras_ptrs[chan];

    for (i = in_hdr.ymin; i <= in_hdr.ymax; i++)
    {
	for ( chan = 0; chan < nchan; chan++ )
	    rows[chan] = ptrs[chan];
	rle_getrow( &in_hdr, &(rows[in_hdr.alpha]) );
	

	/* Bump pointers */
	for ( chan = 0; chan < nchan; chan++ )
	    ptrs[chan] = &((ptrs[chan])[array_width]);	
    }
}

/* Write out the rasters */
void
putraster(rle_pixel **ras_ptrs)
{
    int i, chan;
    rle_pixel *ptrs[MAXCHAN];

    rle_put_setup( &out_hdr );

    /* 
     * If the output image is smaller than the input, we must offset
     * into the pixel array by the difference between the two.
     */
    if (in_hdr.ymin < out_hdr.ymin)
	for ( chan = 0; chan < nchan; chan++ )
	    ptrs[chan] = &((ras_ptrs[chan])[array_width
	                      * (out_hdr.ymin - in_hdr.ymin)]);
    else 
	for ( chan = 0; chan < nchan; chan++ )
	    ptrs[chan] = ras_ptrs[chan];

    for (i = out_hdr.ymin; i <= out_hdr.ymax; i++)
    {
	for ( chan = 0; chan < nchan; chan++ )
	    rows[chan] = &((ptrs[chan])[out_hdr.xmin]);
	rle_putrow( &(rows[out_hdr.alpha]), outlinewidth, &out_hdr );

	/* Bump pointers */
	for ( chan = 0; chan < nchan; chan++ )
	    ptrs[chan] = &((ptrs[chan])[array_width]);	
    }
}

/* Clear the raster's cur_chan channel */
void
clear_raster(rle_pixel **ras_ptr)
{
    bzero( (char *)ras_ptr[cur_chan], array_width * array_lines );
}

/*
 * Dump out a raster (used for debugging).
 */
void
dumpraster(rle_pixel **ras_ptrs)
{
    int j, i, chan;
    for (i = in_hdr.ymin; i <= in_hdr.ymax; i++)
    {
	for (j=in_hdr.xmin; j <= in_hdr.xmax; j++)
	{
	    for ( chan = 0; chan < nchan; chan++ )
		fprintf(stderr, "%2x", (ras_ptrs[chan])[i * array_width + j ]);
	    fprintf(stderr," ");
	}
	fprintf(stderr,"\n");
    }
}

