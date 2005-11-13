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
 * vcrtorle.c - Convert from VICAR to RLE.
 * 
 * Author:	Spencer W. Thomas
 * 		Information Technology and Networking
 * 		University of Michigan Medical Center
 * Date:	Mon Feb 17 1992
 * Copyright (c) 1992, University of Michigan
 * 
 * From: vcr2wff.c
 * Author: K.R. Sloan
 */

/*
 * The VICAR file starts with a bunch of labels in ASCII of the form
 * "keyword=value", separated by spaces.  The entire label string is
 * terminated by a 0 byte.  The very first label is always LBLSIZE,
 * which tells you the size in bytes of the space that has been set
 * aside for the label.  The actual size of the label (to the 0
 * terminator byte) may be shorter than the space allocated for it.
 * So, the first characters of the file are always "LBLSIZE=" followed
 * by an integer in ASCII format.  The other items in the label you
 * need to look for are NL (number of lines) and NS (number of
 * samples).  The keywords are always upper case.
 * 
 *  The image data itself starts immediately after the space allocated
 * for the label.  So, skip LBLSIZE bytes at the beginning of the file
 * to find the start of the image.  The image is stored in binary (not
 * ASCII), one byte per pixel (gray scale), with 0 being black and 255
 * being white.  It is stored in a simple matrix of NS samples (X
 * dimension) by NL lines (Y dimension), with X varying the fastest.
 * So, the first NS bytes contain the entire first line of the image,
 * the second NS bytes contain the entire second line, etc.  The order
 * goes left-to-right, top-to-bottom.
 * 
 *  The full VICAR format can be more complex than this, but this
 * description will hold for most files.  If you want to do a sanity
 * check, then the following have to be true for this description to
 * work: FORMAT='BYTE' ORG='BSQ' NB=1 NBB=0 NLB=0.
 * 
 * For example, here's the beginning of the file from an actual image,
 * dumped in ASCII format (the newlines are artificial for this
 * message, and are not in the file):
 * 
 * LBLSIZE=1024        FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=20480  DIM=3  EOL=0
 * RECSIZE=1024  ORG='BSQ'  NL=1024  NS=1024  NB=1  N1=1024  N2=1024  N3=1
 * N4=0 NBB=0  NLB=0  HOST='VAX-VMS'  INTFMT='LOW'  REALFMT='VAX'  TASK= ...
 * 
 * This says the image is 1024 samples (NS, X dimension) by 1024 lines
 * (NL, Y dimension).  The space allocated for the label is 1024
 * bytes.  So, the image data starts at byte number 1024 (0-based) in
 * the file.  That first byte is the upper left pixel of the image.
 * The first 1024 bytes in the image are the first horizontal line of
 * the image.  The next 1024 bytes are the second line, and so on.
 * There is no data compression.
 * 
 *  Note that items in the label do not have to be in the same order
 * (except LBLSIZE is always first), so don't count on that.  Also,
 * LBLSIZE does not have to equal NS, although it does have to be an
 * integer multiple of NS.  NL and NS do not have to be equal, i.e.
 * the image does not have to be square.
 * 
 *  Ron Baalke                           baalke@mars.jpl.nasa.gov 
 *  Jet Propulsion Lab  
 *  
 */
#ifndef lint
static char rcs_id[] = "$Header$";
#endif
#if 0
vcrtorle()			/* Tag. */
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "rle.h"


static int VERBOSE = 0;


static void FatalError(s)
 char *s;
 {
  fprintf(stderr,"%s\n",s); exit(1);
 }

/* VICAR stuff */
 /* tags, and guesses as to meaning... */
static int  LBLSIZE;       /* size of header, must be int mult of NS */
static char FORMAT[80];    /* 'BYTE' is OK */
static char TYPE[80];      /* 'IMAGE' is OK */
static char ORG[80];       /* `BSQ` is OK */
static int  NL;            /* height */
static int  NS;            /* width */ 
static int  NB;            /* samples per pixel? */
static char HOST[80];      /* machine type? */
static char INTFMT[80];    /* integer format? */
static char REALFMT[80];   /* real format? */
static char TASK[80];      /* processing applied? */
static char USER[80];      /* who was responsible? */
static char DAT_TIM[80];   /* when? */
static char COMMENT[80];   /* comment! */

static void ParseVICARHeader(fd)
FILE *fd;
{
    char Name[81],Value[1024];
    int n;

    for ( n = 0; ; n++ )
    {
	Value[n] = fgetc( fd );
	if ( Value[n] == ' ' )
	    break;
    }
    Value[n++] = 0;

    sscanf(Value,"%80[^=]=%d",Name,&LBLSIZE);
    if (VERBOSE) fprintf(stderr,"[%s = %d]\n",Name,LBLSIZE);
    if (0 != strcmp("LBLSIZE",Name)) FatalError("This is not a VICAR file");

    /* There must be a better way to read the header.  This is gross. */
    while(n < LBLSIZE)
    {
	register char *cp;
	register char c = ' ';
	int done;

	/* Skip spaces. */
	while ( c == ' ' && n < LBLSIZE )
	{
	    c = fgetc( fd );
	    n++;
	}

	/* Get Name. */
	cp = &Name[0];
	while ( c != '=' && c != '\0' && n < LBLSIZE )
	{
	    *cp++ = c;
	    c = fgetc( fd );
	    n++;
	}
	*cp = 0;

	if ( n >= LBLSIZE )
	    break;

	if ('\0' == Name[0] || c == '\0')
	    break;

	/* At this point, c is '='.  Get the next character. */
	Value[0] = fgetc( fd );
	n++;

	cp = &Value[1];
	if ('\'' == Value[0]) 
	    for(;n < LBLSIZE;cp++)
	    {
		*cp = fgetc( fd );
		n++;
		if ('\'' == *cp)
		{
		    *++cp = '\0';
		    break;
		}
	    }
	else 
	    for(;n < LBLSIZE;cp++)
	    {
		*cp = fgetc(fd);
		n++;
		if (' ' == *cp)
		{
		    *cp = '\0';
		    done =  0;
		    break;
		}
		if ('\0' == *cp)
		{
		    done = -1;
		    break;
		}
	    }

	if (VERBOSE) fprintf(stderr,"[%s = %s]\n",Name,Value);
	Value[80] = '\0';	/* for our own protection... */

	if (0 == strcmp("FORMAT" ,Name)) {strcpy(FORMAT ,Value); continue;} 
	if (0 == strcmp("TYPE"   ,Name)) {strcpy(TYPE   ,Value); continue;} 
	if (0 == strcmp("BUFSIZ" ,Name)) {   (void) atoi(Value); continue;}
	if (0 == strcmp("DIM"    ,Name)) {   (void) atoi(Value); continue;}
	if (0 == strcmp("EOL"    ,Name)) {   (void) atoi(Value); continue;}
	if (0 == strcmp("RECSIZE",Name)) {   (void) atoi(Value); continue;}
	if (0 == strcmp("ORG"    ,Name)) {strcpy(ORG    ,Value); continue;} 
	if (0 == strcmp("NL"     ,Name)) {NL      = atoi(Value); continue;}
	if (0 == strcmp("NS"     ,Name)) {NS      = atoi(Value); continue;}
	if (0 == strcmp("NB"     ,Name)) {NB      = atoi(Value); continue;}
	if (0 == strcmp("N1"     ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("N2"     ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("N3"     ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("N4"     ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("NBB"    ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("NLB"    ,Name)) {     (void) atoi(Value); continue;}
	if (0 == strcmp("HOST"   ,Name)) {strcpy(HOST   ,Value); continue;} 
	if (0 == strcmp("INTFMT" ,Name)) {strcpy(INTFMT ,Value); continue;} 
	if (0 == strcmp("REALFMT",Name)) {strcpy(REALFMT,Value); continue;} 
	if (0 == strcmp("TASK"   ,Name)) {strcpy(TASK   ,Value); continue;} 
	if (0 == strcmp("USER"   ,Name)) {strcpy(USER   ,Value); continue;} 
	if (0 == strcmp("DAT_TIM",Name)) {strcpy(DAT_TIM,Value); continue;} 
	if (0 == strcmp("COMMENT",Name)) {strcpy(COMMENT,Value); continue;} 
	if (done) break;
    }  

    /* Skip the rest of the label. */
    while ( n < LBLSIZE )
    {
	(void)fgetc( fd );
	n++;
    }
	
}

/* RLE stuff */
void
addVICARcomment( the_hdr, label, value )
rle_hdr *the_hdr;
char *label, *value;
{
    char *comment;

    if ( *value == 0 )
	return;
    /* 8 = length of "VICAR" + "=" + nul byte. */
    comment = (char *)malloc( 8 + strlen( label ) + strlen( value ) );
    sprintf( comment, "VICAR-%s=%s", label, value );
    rle_putcom( comment, the_hdr );
}

void SetUpRLEFile(the_hdr, xsize, ysize, zsize)
rle_hdr *the_hdr;
int xsize, ysize, zsize;
{

    the_hdr->xmin = 0;
    the_hdr->xmax = xsize - 1;
    the_hdr->ymin = 0;
    the_hdr->ymax = ysize - 1;

    /* 1=B&W, 3=RGB? */
    the_hdr->ncolors = zsize;

    /* Carry over info from VICAR file as comments. */
    addVICARcomment( the_hdr, "USER", USER );
    addVICARcomment( the_hdr, "DAT_TIM", DAT_TIM );
    addVICARcomment( the_hdr, "TASK", TASK );
    addVICARcomment( the_hdr, "COMMENT", COMMENT );
}  

int main(argc,argv)
int argc;
char *argv[];
{
    char *VICARFileName = NULL;
    FILE *fd;
    unsigned char *VICARImage;
    char *outfname = NULL;
    int oflag = 0;
    int y;
    long int nread;
    rle_hdr the_hdr;
    rle_pixel **rows;

    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    if ( scanargs( argc, argv, "% v%- o%-outfile!s infile%s\n(\
\tConvert VICAR format image to URT.\n\
\t-v\tVerbose -- print out VICAR header information.)",
		   &VERBOSE, &oflag, &outfname, &VICARFileName ) == 0 )
	exit( 1 );
    rle_names( &the_hdr, cmd_name( argv ), outfname, 0 );
    rle_addhist( argv, NULL, &the_hdr );
  
    fd = rle_open_f(the_hdr.cmd,VICARFileName,"r");

    ParseVICARHeader(fd);

    /* Check for what we know how to handle. */
    if ( NB != 1 )
    {
	fprintf( stderr, "%s: Can't handle NB (%d) != 1 in %s.\n",
		 the_hdr.cmd, NB, the_hdr.file_name );
	exit( 1 );
    }
    if ( strcmp( FORMAT, "'BYTE'" ) != 0 )
    {
	fprintf( stderr, "%s: Can't handle FORMAT (%s) != 'BYTE' in %s.\n",
		 the_hdr.cmd, FORMAT, the_hdr.file_name );
	exit( 1 );
    }

    /* !!! Will have to be modified when we handle NB>1. !!! */
    VICARImage = (unsigned char *)malloc( NS * NL );
    RLE_CHECK_ALLOC(the_hdr.cmd, VICARImage, "image");

    the_hdr.rle_file = rle_open_f( the_hdr.cmd, outfname, "w" );

    SetUpRLEFile(&the_hdr, NS, NL, NB);
    rle_put_setup( &the_hdr );

    rows = (rle_pixel **)malloc( NB * sizeof(rle_pixel *) );

    /* !!! Will have to be modified when we handle NB>1. !!! */
    nread = fread( VICARImage, 1, NS*NL, fd );
    if ( nread < NS*NL )
	fprintf( stderr, "%s: Short read from %s (%d bytes missing)\n",
		 the_hdr.cmd, the_hdr.file_name, NS*NL - nread );
    for(y=(NL-1);y>=0;y--)	/* flip in y */
    {
	/* !!! Loop somehow if NB > 1. !!! */
	rows[0] = VICARImage + NS * y;

	rle_putrow( rows, NS, &the_hdr );
    }
    rle_puteof( &the_hdr );
    exit(0);
}
