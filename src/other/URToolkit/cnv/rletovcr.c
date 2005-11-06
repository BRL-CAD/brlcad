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
 * rletovcr.c - Convert RLE to VICAR format.
 * 
 * Author:	Spencer W. Thomas
 * 		Information Technology and Networking
 * 		University of Michigan Medical Center
 * Date:	Tue Feb 18 1992
 * Copyright (c) 1992, University of Michigan
 */

/*
Based on.
  File: wff2vcr.c
  Author: K.R. Sloan
  Last Modified: 17 February 1992
  Purpose: convert a .wff file to VICAR

 NOTE: this program is based on bare minimum specifications for VICAR
       images.  It works on the images that I have seen, and on the images
       produced by wff2vcr (of course?).  It is not to be taken as a 
       specification of VICAR images.  If you spot an obvious error, or
       know enough to provide guidance on further extensions, please
       contact <sloan@cis.uab.edu>

  NOTE: the VICAR files have 8 bits per sample

        the .wff files must be I.  If the BitsPerSample is less than
        8, then samples are extended to 8 bits (correctly!).  If the 
        BitsPerSample is greater than 8, the samples are truncated.   
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

int VERBOSE = 0;


/* VICAR stuff */
 /* tags, and guesses as to meaning... */
static int  LBLSIZE;       /* size of header, must be int mult of NS */
#if 0
/* These aren't used by the program, but are kept for documentation. */
static char FORMAT[80];    /* 'BYTE' is OK */
static char TYPE[80];      /* 'IMAGE' is OK */
static int  BUFSIZe;       /* integer multiple of NS ? */
static int  DIM;           /* == 3? */
static int  EOL;           /* == 0? */
static int  RECSIZE;       /* == LBLSIZE? */
static char ORG[80];       /* `BSQ` is OK */
static int  NL;            /* height */
static int  NS;            /* width */ 
static int  NB;            /* samples per pixel? */
static int  N1;            /* == NL? */ 
static int  N2;            /* == NS? */
static int  N3;            /* == NB? */
static int  N4;            /* 0 is OK */
static int  NBB;           /* 0 is OK */ 
static int  NLB;           /* 0 is OK */
static char HOST[80];      /* machine type? */
static char INTFMT[80];    /* integer format? */
static char REALFMT[80];   /* read format? */
static char TASK[80];      /* processing applied? */
static char USER[80];      /* who was responsible? */
static char DAT_TIM[80];   /* when? */
static char COMMENT[80];   /* comment! */
#endif
    
void WriteVICARHeader(fd, width, height, BandsPerPixel)
FILE *fd;
int width, height, BandsPerPixel;
{
    char *buffer, *bp;

    /*
      LBLSIZE must be an integer multiple of width.
      It also needs to be large enough for everything below to fit.
      We use 1024 as a reasonable minimum size, and pick the first integer
      multiple of 'width' to be the LBLSIZE.
      
      Look - I don't really understand VICAR format.  We're just hacking...
      
      */
    LBLSIZE = width;  while(LBLSIZE < 1024) LBLSIZE += width;
    /* Allocate a buffer. */
    buffer = (char *)malloc( LBLSIZE );
    bp = buffer;

#define incr(bp,fudge) \
    bp += strlen( bp ); \
    if ( bp - buffer + fudge > LBLSIZE ) \
    { \
	bp = buffer = realloc( buffer, LBLSIZE += width ); \
	bp += strlen( bp ); \
    }

    sprintf(bp,"LBLSIZE=%-d ",LBLSIZE);	/* see above */
    incr( bp, 20 );
    sprintf(bp," FORMAT='BYTE'");        
    incr( bp, 20 );
    sprintf(bp," TYPE='IMAGE'");
    incr( bp, 20 );
    sprintf(bp," BUFSIZ=%-d",20*LBLSIZE);
    incr( bp, 20 );
    sprintf(bp," DIM=3");  
    incr( bp, 20 );
    sprintf(bp," EOL=0");             
    incr( bp, 20 );
    sprintf(bp," RECSIZE=%-d",LBLSIZE); 
    incr( bp, 20 );
    sprintf(bp," ORG='BSQ'");         
    incr( bp, 20 );
    sprintf(bp," NL=%-d",height);     
    incr( bp, 20 );
    sprintf(bp," NS=%-d",width);      
    incr( bp, 20 );
    sprintf(bp," NB=1");              
    incr( bp, 20 );
    sprintf(bp," N1=%-d",height);     
    incr( bp, 20 );
    sprintf(bp," N2=%-d",width);      
    incr( bp, 20 );
    sprintf(bp," N3=1");              
    incr( bp, 20 );
    sprintf(bp," N4=0");              
    incr( bp, 20 );
    sprintf(bp," NBB=0");             
    incr( bp, 20 );
    sprintf(bp," NLB=0");             
    incr( bp, 100 );
    sprintf(bp," COMMENT='created by rletovcr'");
    bp += strlen( bp );

#undef incr

    /* Rewrite LBLSIZE in case it changed (unlikely). */
    sprintf( buffer, "LBLSIZE=%-d", LBLSIZE );

    while ( bp < buffer + LBLSIZE )
	*bp++ = 0;

    fwrite( buffer, 1, LBLSIZE, fd );
    free( buffer );
}

static void WriteVICARScanLine(fd, VICARScanLine, VICARScanLineLength)
FILE *fd;
unsigned char *VICARScanLine;
int VICARScanLineLength;
{
    (void)fwrite(VICARScanLine, 1, VICARScanLineLength, fd);
}

static unsigned char *
read_image( the_hdr )
rle_hdr *the_hdr;
{
    int y,width,height;
    unsigned char *VICARImage;
    rle_pixel **rows;

    /* Read the RLE file header. */
    rle_get_setup_ok( the_hdr, NULL, NULL );
    /* Don't read alpha channel. */
    RLE_CLR_BIT( *the_hdr, RLE_ALPHA );
    
    /* Sanity check. */
    if ( the_hdr->ncolors > 1 || the_hdr->cmap )
    {
	fprintf( stderr,
		 "%s: Only black & white images can be converted to VICAR.\n",
		 the_hdr->cmd );
	if ( the_hdr->ncolors > 1 )
	    fprintf( stderr, "\t%s has %d colors.\n",
		     the_hdr->file_name, the_hdr->ncolors );
	else
	    fprintf( stderr, "\t%s has a color map.\n",
		     the_hdr->file_name );
	exit( 1 );
    }

    /* Shift image over to save space. */
    the_hdr->xmax -= the_hdr->xmin;
    the_hdr->xmin = 0;
    width = the_hdr->xmax + 1;
    height = the_hdr->ymax - the_hdr->ymin + 1;

    VICARImage = (unsigned char *) malloc(width * height);
    RLE_CHECK_ALLOC( the_hdr->cmd, VICARImage, "image" );
    rows = (rle_pixel **)malloc( the_hdr->ncolors * sizeof(rle_pixel *));

    for ( y = the_hdr->ymin; y <= the_hdr->ymax; y++ )
    {
	rows[0] = VICARImage + width * (the_hdr->ymax - y);
	rle_getrow( the_hdr, rows );
    }
    free( rows );

    return VICARImage;
}

static void
write_image( the_hdr, outFD, VICARImage )
rle_hdr *the_hdr;
FILE *outFD;
unsigned char *VICARImage;
{
    int width, height, y;

    if (VERBOSE) fprintf(stderr,"%s: Writing VICARHeader\n", the_hdr->cmd);

    width = the_hdr->xmax - the_hdr->xmin + 1;
    height = the_hdr->ymax - the_hdr->ymin + 1;
    WriteVICARHeader(outFD, width, height, the_hdr->ncolors); 

    if (VERBOSE) fprintf(stderr,"%s: Writing VICAR image", the_hdr->cmd);

    for ( y = 0; y < height; y++ )
    {
	WriteVICARScanLine(outFD, VICARImage + y * width, width);
	if (VERBOSE) fprintf(stderr,".");
    }
  
    if (VERBOSE) fprintf(stderr,"\n");

    if (ferror(outFD))
	fprintf(stderr,"%s: Error writing image\n", the_hdr->cmd);

    if (VERBOSE)
	fprintf(stderr,"%s: finished writing the image\n", the_hdr->cmd);

    fflush(outFD); 
}

int main(argc,argv)
int argc;
char *argv[];
{
    char *infname = NULL, *outfname = NULL;
    int oflag = 0;
    unsigned char *VICARImage;
    rle_hdr the_hdr;
    FILE *outFD;

    if ( scanargs( argc, argv, "% v%- o%-outfile!s infile%s\n(\
\tConvert URT image to VICAR format (as currently understood).)",
		   &VERBOSE, &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
 
    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &the_hdr, cmd_name( argv ), infname, 0 );

    the_hdr.rle_file = rle_open_f( the_hdr.cmd, infname, "r" );

    VICARImage = read_image( &the_hdr );

    outFD = rle_open_f( the_hdr.cmd, outfname, "w" );

    write_image( &the_hdr, outFD, VICARImage );

    exit(0); 
}
