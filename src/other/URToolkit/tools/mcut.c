/*
 * mcut.c - Generate a "median cut" color map for an image.
 *
 * Producer:	Robert Mecklenburg
 * Director:    John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 *
 *              Based on the novel by Paul Heckbert
 *		(See "Color Image Quantization for Frame Buffer Display",
 *		 Proc. SIGGRAPH '82)
 *
 * Date:	Sun Sep 27 1987
 * Copyright (c) 1987, University of Utah
 */

/*
 * Still left to do:
 *
 *   The five bits used for initial quantization should be generalized
 *   with a #define.  It would not be a good idea to make this a
 *   run-time option, since the arithmatic required to compute the bit
 *   shifting and masking on the fly would way slow down critical loops.
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

/*
 * Standard max min functions.
 */
#define MAX(i,j)   ( (i) > (j) ? (i) : (j) )
#define MIN(i,j)   ( (i) < (j) ? (i) : (j) )

/*
 * A TLISTLINKS at the front of an object struct definition provides for
 * linking together a list of that type of objects.
 */
#define	TLISTLINKS(type)    type * next, * prev

/*
 * Access for List pointer manipulations. (Previous and Next.)
 */
#define P(node) ((node)->prev)
#define N(node) ((node)->next)

/*
 * Trace a linear linked list.
 */
#define TRACE(t_var,ini)   for((t_var)=(ini);(t_var)!=NULL;(t_var)=N(t_var))

/*
 * ADD links new members into a list, given a pointer to the new member, and
 * a pointer to the first (left) element of the list.
 *
 * INSERT links members into the middle of a list, given a pointer to the new
 * member, and a pointer to the list element to insert after.
 *
 */
#define ADD(new,first) ( P(new) = NULL ,N(new) = (first),\
 			(  ((first)!=NULL) ? (P(first)=(new), 0) :0 ),\
			first = (new) )

#define INSERT(new,after) ( P(new) = (after),N(new) = N(after),\
	( (N(after)!=NULL) ? (P(N(after))=(new), 0) :0),\
	N(after) = (new) )


/*
 * Macros for dealing with the "hashed" colors.
 */
#define BLUEMASK(x) ((x) & 0x1f)
#define GREENMASK(x) (((x)>>5) & 0x1f)
#define REDMASK(x) (((x)>>10) & 0x1f)

#define TO_5_BITS(x) (((x)>>3) & 0x1f)
#define FROM_5_BITS(x) (((x)<<3) & 0xff)
#define PACK_15(r,g,b) (TO_5_BITS(r)<<10 | TO_5_BITS(g)<<5 | TO_5_BITS(b))
#define FAST_PACK(r,g,b) (((r)&0xf8)<<7 | ((g)&0xf8)<<2 | (TO_5_BITS(b)))

#define DISTANCE(x,y) (((x).r-(y).r) * ((x).r-(y).r) + \
		       ((x).g-(y).g) * ((x).g-(y).g) + \
		       ((x).b-(y).b) * ((x).b-(y).b))


/*****************************************************************
 * TAG( color_t )
 *
 * An rgb structure.
 */
typedef struct _color color_t;
struct _color
{
    int r, g, b;
};

/* Forward declaration */
typedef struct _color_box color_box_t;

/*****************************************************************
 * TAG( histogram_t )
 *
 * The histogram structure recording color frequency.
 */
typedef struct _histogram histogram_t;
struct _histogram
{
    int color, nsamples;
    color_box_t * color_ptr;
    int cmap_index;
};


/*****************************************************************
 * TAG( bounds_t )
 *
 * A bounding box structure.
 */
typedef struct _bounds bounds_t;
struct _bounds
{
    int max, min;
};


/*****************************************************************
 * TAG( color_box_t )
 *
 * A color bounding box structure which contains the size of the
 * largest dimension, a histogram of the points in the box, and the
 * number of samples in the histogram.
 */
struct _color_box
{
    TLISTLINKS( color_box_t );	/* Linked list header. */
    int		size;		/* Longest dimension for sorting. */
    int 	axis;		/* Which color axis is longest. */
    histogram_t **hist;		/* The histogram of enclosed points. */
    int 	hsize;		/* The size of the histogram. */
    int 	nsamples;	/* Number of values in histogram. */
    bounds_t 	r, g, b;	/* The bounds of the box. */
    color_t 	color;		/* New 24 bit RGB value */
    int		cindex;		/* RLE cmap index of this color value */
};

/*
 * Forward decls.
 */
void subdivide(), sum_hist(), average_colors(), calc_inverse_map();
void make_rle_map(), re_expand_map(), quantize_dither_rle();
void quantize_rle(), free_hist(), bound_rgb(), set_size_axis();
int break_box();
void radix_sort();

/*
 * Globals
 */

color_box_t 	  *cb_list,		/* List of subdivided colors. */
		  *cb;			/* Scratch ptr to hold first value. */
int 		  cb_list_size;		/* Number of el's in cb_list. */
static unsigned	  mask;
short 		  out_map[3][256];	/* Output color map constructed */
rle_hdr in_hdr,		/* For RLE routines. */
		  out_hdr;
int 		  max_colors = 200;	/* Number of colors allowed. */
histogram_t 	  *hist[32768];
int 		  hist_size;
char		  *cmd_nm;	/* Command name for error messages */


/*****************************************************************
 * TAG( mcut )
 *
 * Theory
 *
 * This program accepts a color image of 24 bit RGB and tries to find
 * N representative colors which best fit the image.  Its main purpose
 * is to display reasonable color pictures on shallow (8 bits or less)
 * frame buffers.
 *
 * The basic algorithm is to first bash a 24 bit color down to a 15
 * bit color and histogram the results.  Next we calculate a color
 * space bounding box for the image and begin subdividing.  A box is
 * subdivided at the median of its longest dimension.  These new boxes
 * are then candidates for subdividing.  We continue to subdivide
 * until we have N boxes.  We then walk the color box list calculating
 * an average color for each box.  Finally, we take the original 15
 * bit colors from the histogram and map them onto the averaged colors
 * from the bounding box list.  The new image is constructed by
 * rereading the old image, mapping 24 bit colors to 15 bits and
 * looking up the 15 bit color in the histogram.
 *
 * Implementation
 *
 * The key data structures for the program are histogram_t and
 * color_box_t.  A histogram contains a 15 bit color value, count of
 * the number of occurrences of that color, a pointer into an
 * associated color_box_t structure, and an 8 bit color map index.  An
 * array of 32768 histogram_t's is allocated to initially build the
 * color map.  The first part of the algorithm fills in the entry's
 * color value and counts the occurrences.  Then the histogram is
 * compacted forcing all non-empty histogram slots to the front.  This
 * constitutes the set of subdivideable colors.  A single color_box_t
 * structure (cb) is initialized to bound this color space and the
 * color_box_t is inserted into the cb_list.  The color_box_t
 * structure contains a bounding box for r, g, and b, an indicator of
 * which dimension is longest, the length of the longest dimension, a
 * pointer to a histogram of the elements within the bounding box, the
 * number of histogram entries, and the total number of pixels in the
 * histogram.  Two other structure members are the new 24 bit color,
 * and the color map index of this color.
 *
 * The subdivision process proceeds by removing the first element on
 * the cb_list and subdividing it along its longest dimension.
 * Subdivision occurs at the median pixel count, except a single
 * histogram entry is never subdivided.  The two new color_box_t
 * structures are added to the cb_list with an insertion sort so the
 * first element is always the next to be subdivided.
 *
 * Finally, the cb_list is traversed and an average color is
 * calculated for each color_box_t.  These are converted to 24 bit
 * colors with a simple left shift.  Next the histogram is scanned and
 * for each 15 bit color entry in the histogram the nearest new color
 * (from the new list of 24 bit colors) is calculated and a back
 * pointer from the histogram entry to the color_box_t structure for
 * the color is saved.  This is done because we need to be able to
 * translate quickly from a 15 bit color to its new color map index
 * which we don't yet know!  So the color map index is then calculated
 * and saved in the color_box_t structure and can be accessed quickly
 * through the back pointer.  In fact, the histogram_t structure also
 * has a slot for a color map index, so the back pointer it followed
 * only once.
 */
int
main ( argc, argv )
int argc;
char ** argv;
{
    register rle_pixel *rptr, *gptr, *bptr;
    int tmp, dither = 0, oflag = 0;
    register int i;
    rle_pixel **scan;
    char * infname = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    char cmap_comment[80];
    int rle_err, rle_cnt;
    long start;

    if ( ! scanargs( argc, argv, "% n%-colors!d d%- o%-outfile!s infile!s",
		    &tmp, &max_colors, &dither, &oflag, &out_fname, &infname ))
	exit( 1 );

    cmd_nm = cmd_name( argv );

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );
    in_hdr.rle_file = rle_open_f(cmd_nm, infname, "r");
    rle_names( &in_hdr, cmd_nm, infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	start = ftell( in_hdr.rle_file );
	if ( start < 0 )
	{
	    fprintf( stderr, "%s: Sorry, can't accept piped input.\n",
		     cmd_nm );
	    exit( -1 );
	}
	if ( (rle_err = rle_get_setup( &in_hdr )) != RLE_SUCCESS )
	    break;
	/*
	 * Setup rle buffers.
	 */

	if (in_hdr.ncolors != 3)
	{
	    fprintf(stderr,
		    "%s: input file must be three channel (RGB) image.\n",
		    cmd_nm);
	    exit(-5);
	}

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.ncolors = 1;	/* Just one chan, the cmap index */
	for (i = 1; i < in_hdr.ncolors; i++)
	    RLE_CLR_BIT( out_hdr, i );	/* Kill extra output channels */
	out_hdr.ncmap = 3;
	out_hdr.cmaplen = 8;	/* == 256 entries */
	out_hdr.cmap = (rle_map *) out_map;

	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	/* getrow and putrow have different origins, this makes them the same */
	in_hdr.xmax -= in_hdr.xmin;
	in_hdr.xmin = 0;

	if ( rle_row_alloc( &in_hdr, &scan ) < 0 )
	    RLE_CHECK_ALLOC( cmd_nm, 0, "input image data" );

	/*
	 * Build histogram.
	 */
	while ( rle_getrow(&in_hdr,scan) <= in_hdr.ymax )
	{
	    rptr = scan[RLE_RED];
	    gptr = scan[RLE_GREEN];
	    bptr = scan[RLE_BLUE];

	    for ( i=0; i <= in_hdr.xmax; i++ )
	    {
		register int tmp = FAST_PACK( *rptr++, *gptr++, *bptr++ );
		register histogram_t *hp = hist[tmp];

		if ( hp == NULL )
		{
		    hist[tmp] = hp = (histogram_t *) malloc( sizeof(histogram_t) );
		    RLE_CHECK_ALLOC( cmd_nm, hp, "histogram" );
		}
		hp->color = tmp;
		hp->nsamples++;
	    }
	}

	sum_hist();
	subdivide();
	average_colors();
	calc_inverse_map();

	make_rle_map();
	sprintf( cmap_comment, "color_map_length=%d", cb_list_size );
	rle_putcom( cmap_comment, &out_hdr );
	rle_addhist( argv, &in_hdr, &out_hdr );
	rle_put_setup( &out_hdr );

	re_expand_map();

	/*
	 * Now rewind the input and convert the pixels from RGB to the index.
	 */
	fseek( in_hdr.rle_file, start, 0 );
	rle_get_setup_ok( &in_hdr, cmd_nm, infname );
	in_hdr.xmax -= in_hdr.xmin;	/* Spencer trick */
	in_hdr.xmin = 0;

	if (dither)
	    quantize_dither_rle(scan);
	else
	    quantize_rle(scan);

	rle_row_free( &in_hdr, scan );
	free_hist();
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_nm, infname );


    exit( 0 );
}


/*****************************************************************
 * TAG( sum_hist )
 *
 * Build the first bounding box from the histogram.  This also
 * compacts the histogram.
 */
void
sum_hist ()
{
    register int oldh, newh, i, total;

    /* Get a color box. */
    cb = (color_box_t *) malloc( sizeof( color_box_t ) );
    RLE_CHECK_ALLOC( cmd_nm, cb, 0 );
    bzero( cb, sizeof( color_box_t ) );

    /* Compact histogram. */
    for ( oldh = newh = 0; oldh < 32768; oldh++ )
	if ( hist[oldh] )
	{
	    hist[newh] = hist[oldh];
	    if ( oldh != newh ) hist[oldh] = NULL;
	    newh++;
	}
    hist_size = cb->hsize = newh;

    /* Calculate total pixels. */
    for ( total = i = 0; i<cb->hsize; i++ )
	if ( hist[i] )
	    total += hist[i]->nsamples;

    cb->hist = hist;
    cb->nsamples = total;
    bound_rgb( cb );
    set_size_axis( cb );

    cb_list = cb;
    cb_list_size = 1;
}


/*****************************************************************
 * TAG( subdivide )
 *
 * Break up bounding boxes until the desired number of colors is reached.
 */
void
subdivide ()
{
    while ( cb_list_size < max_colors )
	if ( break_box() )
	    return;
}


/*****************************************************************
 * TAG( break_box )
 *
 * Break a bounding box along its largest axis.  Grab the first box
 * off the color box list (this will be the largest box) and sort it
 * according to the largest dimension color.  This is so we can split
 * the box into two boxes with "contiguous" colors.
 */
int
break_box ()
{
    color_box_t *newb, *box = cb_list, *split_box(), *insert_elt();

    cb_list = N(cb_list);
    if ( cb_list ) P(cb_list) = NULL;
    N(box) = P(box) = NULL;

    switch ( box->axis )
    {
      case RLE_RED:
	radix_sort( box, 0, 5 );
	break;

      case RLE_GREEN:
	radix_sort( box, 5, 5 );
	break;

      case RLE_BLUE:
	radix_sort( box, 10, 5 );
    }

    newb = split_box( box );
    cb_list = insert_elt( cb_list, box );

    if ( newb == NULL )
	return 1;

    cb_list = insert_elt( cb_list, newb );
    cb_list_size++;
    return 0;
}


/*****************************************************************
 * TAG( split_box )
 *
 * Given a box which needs to be split, split it.  Split a color box
 * at it median.  The granularity of the split is a historgram entry,
 * so we never have the same color in two boxes.  Recalculate the
 * bounding boxes for each new color_box_t.  We play some nifty games
 * here with pointers.  There is only one histogram, but each
 * color_box_t structure has a pointer into this histogram along with
 * the number of entries it "owns".  Those entries are re-arranged
 * (sorted) whenever the color_box_t is split.
 */
color_box_t *
split_box ( box )
color_box_t *box;
{
    int t, i;
    color_box_t *newbox;

    if ( box->size <= 0 )
	return NULL;

    /* Calculate median of the box. */
    for ( t = 0, i = 0; i < box->hsize; i++ )
	if ( box->hist[i] )
	{
	    if ( t+box->hist[i]->nsamples > box->nsamples/2 && t > 0 )
		break;
	    t += box->hist[i]->nsamples;
	}

    /*
     * Allocate a new box and set the bounding box and histogram
     * stuff.  Note the new box gets the second half of the old box.
     */
    newbox = (color_box_t *) malloc( sizeof(color_box_t) );
    RLE_CHECK_ALLOC( cmd_nm, newbox, 0 );
    bzero( newbox, sizeof( color_box_t ) );

    newbox->hist = &box->hist[i];
    newbox->hsize = box->hsize - i;
    newbox->nsamples = box->nsamples - t;
    bound_rgb( newbox );
    set_size_axis( newbox );

    box->hsize = box->hsize - newbox->hsize;
    box->nsamples = box->nsamples - newbox->nsamples;
    bound_rgb( box );
    set_size_axis( box );

    return newbox;
}


/*****************************************************************
 * TAG( average_colors )
 *
 * Walk the list of bounding boxes calculating the average colors.
 */
void
average_colors ()
{
    color_box_t *cp;
    int i;
    float red, grn, blu, nsamp;

    TRACE( cp, cb_list )
    {
	red = grn = blu = nsamp = 0;
	for ( i=0; i<cp->hsize; i++ )
	    if ( cp->hist[i] )
	    {
		float samp = cp->hist[i]->nsamples * cp->hist[i]->nsamples;
		red += REDMASK(cp->hist[i]->color) * samp;
		grn += GREENMASK(cp->hist[i]->color) * samp;
		blu += BLUEMASK(cp->hist[i]->color) * samp;
		nsamp += samp;
	    }
	cp->color.r = (int)(red / nsamp);
	cp->color.g = (int)(grn / nsamp);
	cp->color.b = (int)(blu / nsamp);
    }
}


/*****************************************************************
 * TAG( calc_inverse_map )
 *
 * Calculate the inverse mapping from original colors to new (averaged
 * colors).  This is a simple linear exhaustive search.  For each
 * color box in cb_list, walk the color box's histogram.  For each
 * histogram entry calculate the distance from the histogram color to
 * the color box's color, then check that against all other colors in
 * the cb_list.  Choose the color_box_t color which is nearest the
 * histogram_t's color.  When you find the nearest color_box_t set a
 * back pointer from the histogram entry to the color_box_t.
 */
void
calc_inverse_map ()
{
    register color_box_t *cp, *tmp;
    int i, dist;
    color_t ref_col;

    TRACE( cp, cb_list )
	for ( i=0; i<cp->hsize; i++ )
	    if ( cp->hist[i] )
	    {
		ref_col.r = REDMASK( cp->hist[i]->color );
		ref_col.g = GREENMASK( cp->hist[i]->color );
		ref_col.b = BLUEMASK( cp->hist[i]->color );
		dist = DISTANCE( ref_col, cp->color );

		TRACE( tmp, cb_list )
		{
		    register color_t *newcol = &tmp->color;
		    register int newdist = DISTANCE( ref_col, *newcol );

		    if ( newdist < dist )
		    {
			dist = newdist;
		    }
		}
		/* Back pointer to 24 bit value */
		cp->hist[i]->color_ptr = cp;
	    }
}


/*****************************************************************
 * TAG( make_rle_map )
 *
 * Generate the RLE color map entries from the color box list.
 */
void
make_rle_map()
{
    register int index;
    color_box_t * cp;

    index = 0;
    TRACE( cp, cb_list )
    {
	out_map[RLE_RED][index]   = FROM_5_BITS(cp->color.r) << 8;
	out_map[RLE_GREEN][index] = FROM_5_BITS(cp->color.g) << 8;
	out_map[RLE_BLUE][index]  = FROM_5_BITS(cp->color.b) << 8;
	cp->cindex = index++;
    }
}


/*****************************************************************
 * TAG( re_expand_map )
 *
 * In order to convert RGB pixel values into final color map indices,
 * we need to "re-hash" the values into their original slots.  The table
 * is first sorted, so we can walk backwards without trashing anything.
 */
void
re_expand_map()
{
    register int i;
    int resort_compare();
    histogram_t *tmp, **hist_ptr;

    qsort( hist, hist_size, sizeof( histogram_t * ), resort_compare );
    hist_ptr = &(hist[hist_size-1]);
    for ( i = hist_size-1; i >= 0; i-- )
    {
	(*hist_ptr)->cmap_index = (*hist_ptr)->color_ptr->cindex;
	tmp = *hist_ptr;
	if (tmp->color != i)	/* Don't stomp existing slot. */
	{
	    hist[tmp->color] = tmp;
	    *hist_ptr = NULL;
	}
	hist_ptr--;
    }
}

/*****************************************************************
 * TAG( quantize_rle )
 *
 * Read the RLE file and quantize to the reduced color set.
 */
void
quantize_rle(scan)
rle_pixel **scan;
{
    register rle_pixel *rptr, *gptr, *bptr, *optr;
    register int i;
    int tmp;

    while ( rle_getrow(&in_hdr,scan) <= in_hdr.ymax )
    {
	optr = rptr = scan[RLE_RED];
	gptr = scan[RLE_GREEN];
	bptr = scan[RLE_BLUE];

	for ( i=0; i<=in_hdr.xmax; i++ )
	{
	    tmp = FAST_PACK( *rptr++, *gptr++, *bptr++ );
	    if (hist[tmp] == NULL)
		fprintf(stderr, "bogus hash: %d\n", tmp);
	    *optr++ = hist[tmp]->cmap_index;
	}

	rle_putrow( scan, in_hdr.xmax + 1, &out_hdr );
    }
    rle_puteof( &out_hdr );
}

/*****************************************************************
 * TAG( quantize_dither_rle )
 *
 * Read the RLE file and quantize to the reduced color set.
 * Use Floyd/Steinberg dither to hide contouring.
 */
void
quantize_dither_rle(scan)
rle_pixel **scan;
{
    register rle_pixel *rptr, *gptr, *bptr, *optr;
    register int i, j;
    short *thisline, *nextline, *tmpptr ;
    register short *thisptr, *nextptr ;
    int tmp;
    int	lastline, lastpixel ;

    thisline = (short *) malloc( (in_hdr.xmax + 1) * 3 * sizeof(short));
    RLE_CHECK_ALLOC( cmd_nm, thisline, 0 );
    nextline = (short *) malloc( (in_hdr.xmax + 1) * 3 * sizeof(short));
    RLE_CHECK_ALLOC( cmd_nm, nextline, 0 );

    /* Store the first scanline into nextline */

    rle_getrow(&in_hdr,scan);
    rptr = scan[RLE_RED];
    gptr = scan[RLE_GREEN];
    bptr = scan[RLE_BLUE];
    nextptr = nextline;
    for (j=0; j <= in_hdr.xmax; j++)
    {
	*nextptr++ = *bptr++ ;
	*nextptr++ = *gptr++ ;
	*nextptr++ = *rptr++ ;
    }

    for (i=in_hdr.ymin; i <= in_hdr.ymax; i++)
    {
	/* swap nextline into thisline and read new nextline */
	tmpptr = thisline ;
	thisline = nextline ;
	nextline = tmpptr ;
	lastline = (i == in_hdr.ymax) ;
	if (!lastline)
	{
	    rle_getrow(&in_hdr,scan);
	    rptr = scan[RLE_RED];
	    gptr = scan[RLE_GREEN];
	    bptr = scan[RLE_BLUE];
	    nextptr = nextline;
	    for (j=0; j <= in_hdr.xmax; j++)
	    {
		*nextptr++ = *bptr++ ;
		*nextptr++ = *gptr++ ;
		*nextptr++ = *rptr++ ;
	    }
	}

	optr = scan[RLE_RED];
	thisptr = thisline ;
	nextptr = nextline ;

	for(j=0; j <= in_hdr.xmax ; j++)
	{
	    int	red,green,blue ;
	    rle_pixel r2,g2,b2 ;

	    lastpixel = (j == in_hdr.xmax) ;

	    blue = *thisptr++ ;
	    green = *thisptr++ ;
	    red = *thisptr++ ;

	    /* Current pixel has been accumulating error, it could be
	     * out of range.
	     */
	    if( red < 0 ) red = 0 ;
	    else if( red > 255 ) red = 255 ;
	    if( green < 0 ) green = 0 ;
	    else if( green > 255 ) green = 255 ;
	    if( blue < 0 ) blue = 0 ;
	    else if( blue > 255 ) blue = 255 ;

	    r2 = red;
	    g2 = green;
	    b2 = blue;

	    tmp = FAST_PACK( r2, g2, b2 );
	    if (hist[tmp] == NULL)
	    {
		register color_box_t *tmp_cb;
		color_t ref_col;
		int dist;

		hist[tmp] = (histogram_t *) malloc( sizeof(histogram_t) );
		RLE_CHECK_ALLOC( cmd_nm, hist[tmp], 0 );
		hist[tmp]->color = tmp;

		/* The error propagation has produced a color that was
		 * not in the original image (its not in the histogram!).
		 * So, we need to find the closest color.
		 */
		ref_col.r = TO_5_BITS( r2 );
		ref_col.g = TO_5_BITS( g2 );
		ref_col.b = TO_5_BITS( b2 );

		if ( cb_list )
		{
		    dist = DISTANCE( ref_col, cb_list->color );
		    hist[tmp]->color_ptr = cb_list;
		    hist[tmp]->cmap_index = cb_list->cindex;

		    TRACE( tmp_cb, cb_list )
		    {
			register color_t *newcol = &tmp_cb->color;
			register int newdist = DISTANCE( ref_col, *newcol );

			if ( newdist < dist )
			{
			    dist = newdist;
			    hist[tmp]->color_ptr = tmp_cb;
			    hist[tmp]->cmap_index = tmp_cb->cindex;
			}
		    }
		}
		else
		    fprintf( stderr, "Color box list is empty.\n" );
	    }

	    *optr++ = hist[tmp]->cmap_index;

	    red -= FROM_5_BITS(hist[tmp]->color_ptr->color.r);
	    green -= FROM_5_BITS(hist[tmp]->color_ptr->color.g);
	    blue -= FROM_5_BITS(hist[tmp]->color_ptr->color.b);

	    if( !lastpixel )
	    {
		thisptr[0] += blue * 7 / 16 ;
		thisptr[1] += green * 7 / 16 ;
		thisptr[2] += red * 7 / 16 ;
	    }
	    if( !lastline )
	    {
		if( j != 0 )
		{
		    nextptr[-3] += blue * 3 / 16 ;
		    nextptr[-2] += green * 3 / 16 ;
		    nextptr[-1] += red * 3 / 16 ;
		}
		nextptr[0] += blue * 5 / 16 ;
		nextptr[1] += green * 5 / 16 ;
		nextptr[2] += red * 5 / 16 ;
		if( !lastpixel )
		{
		    nextptr[3] += blue / 16 ;
		    nextptr[4] += green / 16 ;
		    nextptr[5] += red / 16 ;
		}
		nextptr += 3 ;
	    }
	}

	rle_putrow( scan, in_hdr.xmax + 1, &out_hdr );
    }
    rle_puteof( &out_hdr );

    free( thisline );
    free( nextline );
}

/*****************************************************************
 * TAG( resort_compare )
 *
 * Extract the color for resorting the structure of the histogram.
 */
int
resort_compare( c1, c2 )
histogram_t **c1, **c2;
{
    return ( (*c1)->color > (*c2)->color );
}


/*****************************************************************
 * TAG( set_size_axis )
 *
 * Determine the largest axis and its dimension.
 */
void
set_size_axis ( cb )
color_box_t *cb;
{
    int rsize = cb->r.max - cb->r.min,
	gsize = cb->g.max - cb->g.min,
	bsize = cb->b.max - cb->b.min;

    if ( rsize > gsize )
    {
	if ( rsize > bsize )
	{
	    cb->axis = RLE_RED;
	    cb->size = rsize;
	}
	else
	{
	    cb->axis = RLE_BLUE;
	    cb->size = bsize;
	}
    }
    else
    {
	if ( gsize > bsize )
	{
	    cb->axis = RLE_GREEN;
	    cb->size = gsize;
	}
	else
	{
	    cb->axis = RLE_BLUE;
	    cb->size = bsize;
	}
    }
}


/*****************************************************************
 * TAG( bound_rgb )
 *
 * Calcluate the rgb bounding box for the color_box_t.
 */
void
bound_rgb ( box )
register color_box_t *box;
{
    register int i;
    register histogram_t **hp = box->hist;
    int first_time = 1;

    for ( i=0; i<box->hsize; i++, hp++ )
	if ( *hp )
	{
	    if ( first_time )
	    {
		box->r.min = box->r.max = REDMASK( (*hp)->color );
		box->g.min = box->g.max = GREENMASK( (*hp)->color );
		box->b.min = box->b.max = BLUEMASK( (*hp)->color );
		first_time = 0;
	    }

	    box->r.min = MIN( box->r.min, REDMASK((*hp)->color) );
	    box->r.max = MAX( box->r.max, REDMASK((*hp)->color) );
	    box->g.min = MIN( box->g.min, GREENMASK((*hp)->color) );
	    box->g.max = MAX( box->g.max, GREENMASK((*hp)->color) );
	    box->b.min = MIN( box->b.min, BLUEMASK((*hp)->color) );
	    box->b.max = MAX( box->b.max, BLUEMASK((*hp)->color) );
	}
}


/*****************************************************************
 * TAG( radix_sort )
 *
 * Simulate a radix sort with qsort.
 */
void
radix_sort ( bbox, start_bit, num_bits )
color_box_t *bbox;
int start_bit, num_bits;
{
    int cmp_radices();

    mask = ~(~0 << num_bits) << start_bit;

    qsort( (char *)bbox->hist, bbox->hsize,
	   sizeof(histogram_t *), cmp_radices );
}


/*****************************************************************
 * TAG( cmp_radices )
 *
 * The comparison function for qsort.
 */
int
cmp_radices ( h1, h2 )
histogram_t **h1, **h2;
{
    register int c1 = -1, c2 = -1;

    if ( *h1 )
	c1 = (*h1)->color & mask;
    if ( *h2 )
	c2 = (*h2)->color & mask;

    return c1>c2 ? 1 : ( c2>c1 ? -1 : 0 );
}


/*****************************************************************
 * TAG( insert_elt )
 *
 * Insert an element into a linked list decreasing order.
 */
color_box_t *
insert_elt ( list, elt )
color_box_t *list, *elt;
{
    color_box_t *lp, *ll;

    /* Special cases: NULL list and one element list. */
    if ( list == NULL )
	list = elt;
    else if ( P(list) == NULL && N(list) == NULL )
	if ( list->size <= elt->size )
	    ADD( elt, list );
	else
	{
	    ADD( list, elt );
	    list = elt;
	}
    else
    {
	/* Search a multi-element list for proper spot to insert. */
	for ( lp = list; N(lp); lp = N(lp) )
	    if ( lp->size <= elt->size )
		break;

	/*
	 * Special cases: insert before beginning, insert after end,
	 * insert in middle.
	 */
	if ( P(lp) == NULL )
	    ADD( elt, list );
	else
        if ( N(lp) == NULL && elt->size < lp->size )
	{
	    /* Append elt to lp */
	    for (ll = lp; N(ll) != NULL; ll = N(ll)) ;
	    N(ll) = elt;
	    P(elt) = ll;
	}
	else
	{
	    lp = P(lp);
	    INSERT( elt, lp );
	}
    }

    return list;
}


/*****************************************************************
 * free_hist -- free all histogram and color box storage.
 */
void
free_hist()
{
    register color_box_t *tmp_cb, *next_cb;
    register int i;

    for( tmp_cb = cb_list; tmp_cb; tmp_cb = next_cb )
    {
	next_cb = N(tmp_cb);
	free( tmp_cb );
    }
    cb_list = NULL;
    cb_list_size = 0;

    for ( i = 0; i < 32768; i++ )
    {
	if ( hist[i] )
	    free( hist[i] );
	hist[i] = NULL;
    }
}
