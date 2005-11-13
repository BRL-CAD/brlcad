/******************************************************
 *
 * rleintrp.c -
 *
 *   Interpolation between UTAH_RLE images making nbimage files
 *   by linear interpolation between the values of the pixels in
 *   two input files or between 0 and the value of a file
 *   (or the standard input if filename = '-' ).
 *
 *   We  check the mapping of the characteristics of each
 *   image in input : size, number of channels, background color
 *   color map, alpha channel ...
 *
 *   Input :    - prefix of output files
 *              - name of files between which interpolation occurs.
 *                If no mame : error.
 *                If name = '-' : standard input. Only one file can be
 *		  specified like this.
 *              - Number of images created (default 1).
 *
 *   Usage : rleintrp [-n nbimage] [-o outfile] [infile1 [infile2]]
 *
 *
 * Author: Michel GAUDET
 *   SLX Onera
 *  29 Avenue Division Leclerc
 *  92322 CHATILLON CEDEX
 *  France
 * Date: Mon Jul  1 09:18:26 1991
 */

#include <stdlib.h>
#include <stdio.h>
#include <rle.h>
#include <fcntl.h>

int comp_hdr ();

extern char *optarg;
extern int optind;

main( argc, argv )
int argc;
char **argv;
{
    char       *infname1 = NULL,
    	       *infname2 = NULL,
	       *outfname = NULL,
		nom_sortie[BUFSIZ],
    		extension[80] ,
    	       *basenom, *rep ;
    int  i, n, nbimage=1;
    int  width, y, x ;
    int  iflag1 = 0 , iflag2 = 0, oflag = 0, fd ;
    double alpha, beta ;
    FILE        * outfile;
    rle_hdr in1_hdr, in2_hdr, *out_hdr; /* Headers for input and output. */
    rle_pixel **rows1, ** rows2, **rowsout, *point1, *point2, *pointout, *tab0;
    /* Will be used for scanline storage. */

    in1_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    in2_hdr = *rle_hdr_init( (rle_hdr *)NULL );

#define ARG_FORMAT   "% n%-nimage!d o%-outfile!s infile1%s infile2%s\n(\
\tInterpolate between two images or between one image and black.\n\
\t-n\tSpecify number of images to produce as output.\n\
\tIf neither input is specified, infile1 defaults to standard input.\n\
\tIf infile2 is not specified, fade from infile1 to black.\n\
\tTo fade from black to an image, specify infile1 as \"\" (empty string).\n\
\tBoth inputs cannot be standard input (-).)"

    if ( scanargs( argc, argv, ARG_FORMAT,
		   &nbimage, &nbimage, &oflag, &outfname,
		   &infname1, &infname2 ) == 0 )
	exit( 1 );

    iflag1 = (infname1 && *infname1);
    iflag2 = (infname2 && *infname2);

    /*   Check if we have two arguments
     *   and if we have not two  '-'
     *
     */
    if ( ( infname1 && ! strcmp(infname1,"-") ) &&
	 ( infname2 && ! strcmp(infname2,"-") ) )
    {
	fprintf(stderr,"%s : At most one input can be standard input\n",
		cmd_name( argv ) ) ;
	scan_usage( argv, ARG_FORMAT ) ;
	exit (1) ;
    }

    if ( infname1 && *infname1 && ! outfname ) outfname = infname1 ;
    else if ( infname2 && *infname2 && ! outfname )  outfname = infname2 ;

    /*  If only one file and name is '-' and no output file name prefix
     *  arbitrary output file name : process Number and extension 'rle'.
     *  Memory allocation to store outfname (no area because no argument
     *  in command line)
     */
    if ( !outfname || ! strcmp(outfname,"-") )
    {
	outfname = ( char *) malloc(80*sizeof(char)) ;
	sprintf(outfname,"i%-d.rle",getpid()) ;
    }

    rle_names( &in1_hdr, cmd_name( argv ), infname1, 0 );
    rle_names( &in2_hdr, cmd_name( argv ), infname2, 0 );

    /* limit number of images at 99 */
    if ( nbimage > 99 )
    {
	fprintf(stderr,"%s : Number of images too big\n",argv[0] ) ;
	exit (1) ;
    }
    out_hdr = (rle_hdr *) malloc ( nbimage * sizeof(rle_hdr) );
    RLE_CHECK_ALLOC( in1_hdr.cmd, out_hdr, "output header array" );

    if ( iflag1 )
	in1_hdr.rle_file = rle_open_f( cmd_name( argv ), infname1, "r" );
    if ( iflag2 )
	in2_hdr.rle_file = rle_open_f( cmd_name( argv ), infname2, "r" );

    /* Read images from the input file until the end of file is
     * encountered or an error occurs.
     */
    if ( iflag1 )
	rle_get_setup_ok( &in1_hdr, NULL, NULL );

    if ( iflag2 )
	rle_get_setup_ok( &in2_hdr, NULL, NULL );


    /*   Comparison of header or creation if needed
     */
    if ( iflag1 && iflag2 )
    {
        if ( comp_hdr(&in1_hdr,&in2_hdr ) )
        {
	    fprintf(stderr,"%s : Input files not compatibles\n",argv[0]) ;
	    exit (1) ;
        }
    }
    else if ( iflag1 ) rle_hdr_cp( &in1_hdr, &in2_hdr );
    else if ( iflag2 ) rle_hdr_cp( &in2_hdr, &in1_hdr );

    /*   Creation of array of 0s */
    tab0 = (rle_pixel *)
	calloc ( in1_hdr.xmax - in1_hdr.xmin + 1, sizeof(rle_pixel) );

    /* Since rle_getrow and rle_putrow use different array origins,
     * we will compensate by adjusting the xmin and xmax values in
     * the input header.  [rle_getrow assumes that the scanline
     * array starts at pixel 0, while rle_putrow assumes that the
     * scanline array starts at pixel xmin.  This is a botch, but
     * it's too late to change it now.]
     */
    in1_hdr.xmax -= in1_hdr.xmin;
    in1_hdr.xmin = 0;
    width = in1_hdr.xmax + 1;	/* Width of a scanline. */

    /*  If only one of input file is defined we interpolate between
     *  the values of this one and 0
     *  It is then necessary to have a pointer for an array of 0
     *
     *  If the alpha channel exists it is numbered -1
     */
    if ( iflag1 && rle_row_alloc( &in1_hdr, &rows1 ) < 0)
	RLE_CHECK_ALLOC( in1_hdr.cmd, 0, "image 1 scanline" );
    else if ( ! iflag1 )
    {
	rows1 = (rle_pixel **)
	    malloc((in1_hdr.ncolors+in1_hdr.alpha)*sizeof(rle_pixel *));
	RLE_CHECK_ALLOC( in1_hdr.cmd, rows1, "black scanline" );
	if ( in1_hdr.alpha )
	    rows1++ ;
    }
    if ( iflag2 && rle_row_alloc( &in2_hdr, &rows2 ) < 0)
    {
	fprintf( stderr, "%s : Memory allocation error.\n",argv[0] );
	exit( RLE_NO_SPACE );
    }
    else if ( ! iflag2 )
    {
	rows2 = (rle_pixel **)
	    malloc((in1_hdr.ncolors+in1_hdr.alpha)*sizeof(rle_pixel *));
	RLE_CHECK_ALLOC( in1_hdr.cmd, rows2, "black scanline" );
	if ( in2_hdr.alpha )
	    rows2++ ;
    }

    /*  File base-name reduction to 7 characters because
     *  may be limitations to 14 characters of final file name
     */
    basenom = (rep=strrchr(outfname,'/'))==NULL?outfname:rep+1 ;

    /*  Save extension for making final name
     */
    if ( (rep=strrchr(basenom,'.')) == NULL )
	extension[0]='\0' ;
    else
    {
#ifdef FILE_NAME_14_CHARS
	strncpy(extension,rep,4);
	extension[4] ='\0';
#else
	strcpy(extension, rep);
#endif
	*rep = '\0' ;
    }

#ifdef FILE_NAME_14_CHARS
    if ( ( strlen(basenom) - (i=strlen(extension)) ) > 10 )
	*(basenom+10-i) = '\0' ;
#endif

    for ( y = in1_hdr.ymin; y <= in1_hdr.ymax; y++ )
    {
	/* Read a scanline. */
	if ( iflag1 ) rle_getrow( &in1_hdr, rows1 );
	if ( iflag2 ) rle_getrow( &in2_hdr, rows2 );

	/*   I create all the files in one pass to avoid to read many time
	 *   the input files
	 */

	for ( i = 0 ; i < nbimage ; i++ )
	{
	    if ( y == in1_hdr.ymin )
	    {
		sprintf(nom_sortie , "%s_%03d%s",outfname,i,extension) ;

		if ( ( fd = open(nom_sortie, O_RDONLY ) ) != -1 )
		{
		    close(fd) ;
		    fprintf(stderr,"%s : File %s exists\n",
			    in1_hdr.cmd, nom_sortie ) ;
		    exit (1) ;
		}
		else
		{
		    fprintf(stderr,"Create image %s\n",nom_sortie) ;
		    outfile = rle_open_f(cmd_name(argv), nom_sortie, "w" ) ;
		}
		/* The output header is a copy of the input header.  The only
		 * difference is the FILE pointer.
		 */
		rle_hdr_cp( &in2_hdr, &out_hdr[i] );
		rle_names( &out_hdr[i], in2_hdr.cmd, nom_sortie, 0 );
		out_hdr[i].rle_file = outfile;

		/* Add to the history comment. */
		rle_addhist( argv, (rle_hdr *) 0, &out_hdr[i] );

		/* Write the output image header. */
		rle_put_setup( &out_hdr[i] );

		if ( rle_row_alloc(&out_hdr[i],&rowsout) < 0 )
		    RLE_CHECK_ALLOC( in1_hdr.cmd, 0, "output scanline" );
	    }
	    /*  Only one input file : the other is '0' image
	     */
	    if ( ! iflag1 )
		for (n = -in1_hdr.alpha; n<in1_hdr.ncolors; n++)
		    rows1[n] = tab0 ;
	    if ( ! iflag2 )
		for (n = -in2_hdr.alpha; n<in2_hdr.ncolors; n++)
		    rows2[n] = tab0 ;
	    beta = (i+1) * 1.0/(nbimage + 1) ;
	    alpha = 1.0 - beta ;

	    for ( (n = in1_hdr.alpha?-1:0) ; n < in1_hdr.ncolors ; n++ )
	    {
		point1 = rows1[n] ;
		point2 = rows2[n] ;
		pointout = rowsout[n] ;

		for ( x = in1_hdr.xmin ; x <= in1_hdr.xmax ; x++ )
		{
		    *pointout++ = (int) ( (*point1++) * alpha + beta * (*point2++) + 0.5 ) ;
		}
	    }
	    /* Write the processed scanline. */
	    rle_putrow( rowsout, width, &out_hdr[i] );

	    if ( y == in1_hdr.ymax )
	    {
		/* Write an end-of-image code. */
		rle_puteof( &out_hdr[i] );
		fclose(out_hdr[i].rle_file) ;
	    }
	}

    }
    /* Free memory. */
    if ( iflag1 ) rle_row_free( &in1_hdr, rows1 );
    if ( iflag2 ) rle_row_free( &in2_hdr, rows2 );
    rle_row_free( &out_hdr[0], rowsout ) ;

    exit( 0 );
}

int comp_hdr ( hdr1, hdr2 )
rle_hdr * hdr1 , * hdr2  ;

{
    int comp , i, j;
    rle_map *point1, *point2 ;
    int cmapsize, cmaplen1, cmaplen2;
    char *v;

    comp =   hdr1->ncolors != hdr2->ncolors  ;
    comp +=  hdr1->alpha != hdr2->alpha ;
    comp +=  hdr1->background != hdr2->background ;
    comp +=  hdr1->xmin != hdr2->xmin ;
    comp +=  hdr1->xmax != hdr2->xmax ;
    comp +=  hdr1->ymin != hdr2->ymin ;
    comp +=  hdr1->ymax != hdr2->ymax ;
    comp +=  hdr1->ncmap != hdr2->ncmap ;
    comp +=  hdr1->cmaplen != hdr2->cmaplen ;
    /* Color map length is complex. */
    if ( !comp && hdr1->ncmap )
    {
	cmapsize = 1 << hdr1->cmaplen;
	if ( (v = rle_getcom( "color_map_length", hdr1 )) != NULL )
	    cmaplen1 = atoi( v );
	else
	    cmaplen1 = 1 << hdr1->cmaplen;
	if ( (v = rle_getcom( "color_map_length", hdr2 )) != NULL )
	    cmaplen2 = atoi( v );
	else
	    cmaplen2 = 1 << hdr2->cmaplen;
	comp += cmaplen1 != cmaplen2;
	if ( !comp )
	    for ( j=0; j<hdr1->ncmap; j++)
		for ( i=0, point1 = hdr1->cmap+j*cmapsize,
		      point2 = hdr2->cmap+j*cmapsize;
		      i < cmaplen1 ; i++ )
		    comp += *point1++ != *point2++ ;
    }
    return comp ;
}


