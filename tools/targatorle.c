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
 *
 * Copyright (c) 1986, John W. Peterson
 */

/* 
 * targatorle.c - Convert TIPS (Targa, AT&T format #2) images to RLE.
 * Modified from painttorle.c
 * Note that color map in the targa file is ignored in this routine.
 * 
 * Author:	Hann-Bin Chuang
 * 		Department of Chemistry
 *		Boston University
 * Date:	Tue. Aug 25 1987
 *
 * Usage is:
 *   targatorle  [-h header.lis] [-o outfile.rle] [infile.tga] 
 *
 * -h header.lis	write the targa header information to file "header.lis"
 * -o outfile.rle	instead of stdout, use outfile.rle as output.
 *
 * rleflip is not necessary because Targa images and RLE both use
 * the last line (in the file) as the upper left of the image.
 */

#include "conf.h"

#include <stdio.h>
#include <sys/types.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"			/* For malloc and free */
#include "rle.h"

/*
 * Description of header for files containing targa type 2 images
 */
struct targafile {
      unsigned char   num_char_id,    /* Number of Characters in ID Field */
                      cmap_type,      /* Color Map Type */
                      image_type;     /* Image Type Code */
      unsigned short  cmap_origin,    /* Color Map Origin */
                      cmap_length;    /* Color Map Length */
      unsigned char   cmap_size;      /* Color Map Entry Size */
      unsigned short  image_x_origin, /* X-Origin of Image */
                      image_y_origin, /* Y-Origin of Image */
                      image_width,    /* Width of Image */
                      image_height;   /* Height of Image */
      unsigned char   image_pix_size, /* Image Pixel Size */
                      image_descriptor;       /* Image Descriptor Byte */
      unsigned char   *image_id;      /* Image ID Field */
      unsigned char   *cmap_data;     /* color Map data */
      };

/* Default color values */
unsigned char	in_line[512];
unsigned char	*outrows[4];
unsigned char	redline[512], grnline[512], bluline[512], alfline[512];

short *svaxswap();
long *lvaxswap();
void fvaxget(), fsunput(), fsunget(), fvaxput();

int
main(argc,argv) 
int argc;
char *argv[];
{ 
    FILE       *infile,
    	       *outfile,
    	       *hdrfile=NULL;
    char       *infname=NULL, 
    	       *outfname=NULL, 
    	       *hdrfname=NULL;
    int		oflag=0,
    		hflag=0;
    int		i, j;
    struct targafile tga_head;

    if ( scanargs( argc,argv, "% h%-hdrfile!s o%-outfile!s infile%s",
		   &hflag, &hdrfname, &oflag, &outfname, &infname ) == 0 )
	exit(1);


    infile  = rle_open_f("targatorle", infname, "r");
    if ( hflag )
	hdrfile = rle_open_f("targatorle", hdrfname, "w");
    outfile = rle_open_f("targatorle", outfname, "w");
    /* Check that hdrfile and outfile aren't the same. */
    if ( hdrfile == outfile )
    {
	fprintf( stderr,
	       "targatorle: Can't write header and RLE data to same file.\n" );
	exit( 1 );
    }

    fread (&tga_head, 3, 1, infile);
    if (hflag) {
	fprintf (hdrfile, "num_char_id = %d \n", (int)tga_head.num_char_id);
	fprintf (hdrfile, "cmap_type   = %d \n", (int)tga_head.cmap_type);
	fprintf (hdrfile, "image_type  = %d \n", (int)tga_head.image_type);
    }

    if ((int)tga_head.cmap_type==0) { 
	if (hflag) {
	    fprintf (hdrfile, "Color Map Type = 0 \n");
	}
    }
    else if ((int)tga_head.cmap_type==1) {
	fread (&(tga_head.cmap_origin), 5, 1, infile);
	svaxswap(&(tga_head.cmap_origin), 2);
	if (hflag) {
	    fprintf (hdrfile, "cmap_origin = %d \n",
		     (int)tga_head.cmap_origin);
	    fprintf (hdrfile, "cmap_length = %d \n",
		     (int)tga_head.cmap_length);
	    fprintf (hdrfile, "cmap_size   = %d \n",
		     (int)tga_head.cmap_size);
	}
    }
    else {
	perror ("Invalid Color Map Type for RGB (UNMAPPED) IMAGES\n");
	exit (1);
    }

    fread (&(tga_head.image_x_origin), 10, 1, infile);
    svaxswap(&(tga_head.image_x_origin), 4);
    if (hflag) {
	fprintf (hdrfile, "image_x_origin = %d \n",
		 (int)tga_head.image_x_origin);
	fprintf (hdrfile, "image_y_origin = %d \n",
		 (int)tga_head.image_y_origin);
	fprintf (hdrfile, "image_width    = %d \n",
		 (int)tga_head.image_width);
	fprintf (hdrfile, "image_height   = %d \n",
		 (int)tga_head.image_height);
	fprintf (hdrfile, "image_pix_size = %d \n",
		 (int)tga_head.image_pix_size);
    }

    if ((i=(int)tga_head.num_char_id)>=1) {
	tga_head.image_id = (unsigned char *)malloc(i);
	fread (tga_head.image_id, i, 1, infile);
    }

    if ((int)tga_head.cmap_type==1) {
	if ((int)tga_head.cmap_size==24) {
	    i=24*256/8;
	    tga_head.cmap_data = (unsigned char *)malloc(i);
	    fread (tga_head.cmap_data, i, 1, infile);
	}
	else {
	    perror ("Invalid color map length \n");
	    exit(1);
	}
    }
          
    RLE_SET_BIT(rle_dflt_hdr, RLE_RED);
    RLE_SET_BIT(rle_dflt_hdr, RLE_GREEN);
    RLE_SET_BIT(rle_dflt_hdr, RLE_BLUE);
    RLE_SET_BIT(rle_dflt_hdr, RLE_ALPHA);
    rle_dflt_hdr.rle_file = outfile;
    rle_dflt_hdr.xmax = (int)tga_head.image_width-1;
    rle_dflt_hdr.ymax = (int)tga_head.image_height-1;
    rle_dflt_hdr.alpha = 1;

    outrows[0] = alfline;
    outrows[1] = redline;
    outrows[2] = grnline;
    outrows[3] = bluline;

    rle_addhist( argv, (rle_hdr *)NULL, &rle_dflt_hdr );
    rle_put_setup( &rle_dflt_hdr );

    /* read and process each of the tga_head.image_height targa scan lines */
    for (i=0;i<(int)tga_head.image_height;i++) {
	for (j=0;j<(int)tga_head.image_width;j++) {
	    bluline[j]=getc(infile);
	    grnline[j]=getc(infile);
	    redline[j]=getc(infile);
	    alfline[j]=getc(infile);
	}
	rle_putrow (&outrows[1], (int)tga_head.image_width, &rle_dflt_hdr);
    }
    rle_puteof( &rle_dflt_hdr );
    return 0;
}

/*
 *	long  *
 *	lvaxswap( lptr, nlong )
 *	long  *lptr;
 *	long   nlong;
 *
 *	lvaxswap converts an array of nlong long ints pointed to by lptr
 *	from VAX byte order to MC680n0 byte order or vice versa.
 *	It returns a pointer to the location immediately following
 *	the last position swapped.
 *
 *
 *	short  *
 *	svaxswap( sptr, nshort )
 *	short  *sptr;
 *	long   nshort;
 *
 *	svaxswap converts an array of nshort short ints pointed to by sptr
 *	from VAX byte order to MC680n0 byte order or vice versa.
 *	It returns a pointer to the location immediately following
 *	the last position swapped.
 *
 *
 *	float  *
 *	fvaxtosun( fptr, nfloat )
 *	c_addr  *fptr;
 *	long     nfloat;
 *
 *	fvaxtosun converts an array of nfloat VAX format F*4 data into
 *	their corresponding MC68020 equivalents in place.  fptr is declared
 *	as a caddr_t to remind the user that the VAX format floats may well
 *	not represent valid Sun floating point values before conversion.
 *	It returns a pointer to the location immediately following
 *	the last position swapped.
 *
 *	float *
 *	fsuntovax( fptr, nfloat )
 *	float  *fptr;
 *	long    nfloat;
 *
 *	fsuntovax converts an array of nfloat MC68020 format floats
 *	into their VAX format equivalents in place.  It returns a pointer
 *	to the location immediately following the last position swapped.
 */


/*
 *	Use a union to insure proper allignment
 */
union l_byt {
    unsigned char   lbs[4];
    long            llong;
};

long           *
lvaxswap(lptr, nlong)
long           *lptr;
long            nlong;
{
    long            i;
    unsigned char   tbyte;
    union l_byt     lb;
    unsigned char  *lbytes = lb.lbs;
    long           *Lbytes = &lb.llong;

    for (i = 0; i < nlong; i++) {
	*Lbytes = lptr[i];
	tbyte = lbytes[0];
	lbytes[0] = lbytes[3];
	lbytes[3] = tbyte;
	tbyte = lbytes[1];
	lbytes[1] = lbytes[2];
	lbytes[2] = tbyte;
	lptr[i] = *Lbytes;
    }
    return (&lptr[nlong]);
}


/*
 *	Use a union to insure proper allignment
 */
union s_byt {
    unsigned char   sbs[2];
    short           sshort;
};

short          *
svaxswap(sptr, nshort)
short          *sptr;
long            nshort;
{
    long            i;
    unsigned char   tbyte;
    union s_byt     sb;
    unsigned char  *sbytes = sb.sbs;
    short          *Sbytes = &sb.sshort;

    for (i = 0; i < nshort; i++) {
	*Sbytes = sptr[i];
	tbyte = sbytes[0];
	sbytes[0] = sbytes[1];
	sbytes[1] = tbyte;
	sptr[i] = *Sbytes;
    }
    return (&sptr[nshort]);
}


/*
 *	Floating point conversion:
 *
 *	1)  The general format is the same,
 *		1 bit sign,
 *		8 bit exponent, and
 *		23 bit normalized mantissa with a leading 1 understood.
 *	2)  The exponents have different biases:
 *		On VAX, exp = true exponent + 128;
 *		on Sun, exp = true exponent + 127.
 *	3)  VAX normalized mantissa represents binary fraction
 *		0.1{mant}
 *	    Sun normalized mantissa represents 1.{mant}
 *	4)  Sun format allows two representations for 0: 0 exp, 0 mant,
 *		and positive OR negative sign bit.  VAX representation
 *		allows only the former.
 *	5)  Sun byte order leads to the following representation
 *		(from most significant to least significant):
 *		sign bit -- 8 bit exponent -- 23 bit mantissa.
 *	6)  VAX byte order leads to the following representation
 *		of a VAX F*4 on the Sun (from most significant
 *		to least significant where exp = e0 - e7 and mant =
 *		m0 - m22 ):
 *		    e7m0-m6,se0-e6,m15-m22,m7-m14
 */

/*
 *	Nb. all quantities in the float structure are right justified.
 *	I.e., sign == 0 || sign == 1
 */

struct fstruct {
    unsigned short  sign;
    unsigned short  exp;
    unsigned long   mant;
};


float          *
fvaxtosun(fptr, nfloat)
caddr_t         fptr;
long            nfloat;
{
    struct fstruct  fs;
    long            i;
    caddr_t         tptr;

    tptr = fptr;
    for (i = 0; i < nfloat; i++) {
	fvaxget(&fs, tptr);
	if (fs.exp > 1)
	    fs.exp -= 2;
	else
	    fs.exp = 0;		/* subnormal */
	fsunput((float *) tptr, &fs);
	tptr += 4;
    }
    return ((float *) tptr);
}

float          *
fsuntovax(fptr, nfloat)
float          *fptr;
long            nfloat;
{
    struct fstruct  fs;
    long            i;

    for (i = 0; i < nfloat; i++) {
	fsunget(&fs, &fptr[i]);
	if (fs.exp == 0)
	    fs.sign = 0;	/* VAX 0 must be positive */
	else if (fs.exp > 253)
	    fs.exp = 255;
	else
	    fs.exp += 2;
	fvaxput((caddr_t) & fptr[i], &fs);
    }
    return (&fptr[nfloat]);
}

void
fvaxget(fsptr, fptr)
struct fstruct *fsptr;
caddr_t         fptr;
{
    register unsigned long ltmp;

    ltmp = *(unsigned long *) fptr;
    fsptr->mant = (ltmp << 8) & 0177400L;
    ltmp >>= 8;
    fsptr->mant |= ltmp & 037600377L;
    ltmp >>= 7;
    fsptr->exp = ltmp & 0376;
    ltmp >>= 8;
    fsptr->sign = ltmp & 01;
    fsptr->exp |= (ltmp >> 8) & 01;
}

void
fsunget(fsptr, fptr)
struct fstruct *fsptr;
float          *fptr;
{
    register unsigned long ltmp;

    ltmp = *(unsigned long *) fptr;
    fsptr->mant = ltmp & 037777777L;
    ltmp >>= 23;
    fsptr->exp = ltmp & 0377;
    fsptr->sign = (ltmp >> 8) & 01;
}

void
fvaxput(fptr, fsptr)
caddr_t         fptr;
struct fstruct *fsptr;
{
    register unsigned long ltmp;

    ltmp = fsptr->exp & 01;
    ltmp <<= 8;
    ltmp |= fsptr->sign;
    ltmp <<= 8;
    ltmp |= fsptr->exp & 0376;
    ltmp <<= 7;
    ltmp |= fsptr->mant & 037600377;
    ltmp <<= 8;
    ltmp |= (fsptr->mant >> 8) & 0377;
    *(unsigned long *) fptr = ltmp;
}

void
fsunput(fptr, fsptr)
float          *fptr;
struct fstruct *fsptr;
{
    register unsigned ltmp;

    ltmp = fsptr->sign & 01;
    ltmp <<= 8;
    ltmp |= fsptr->exp & 0377;
    ltmp <<= 23;
    ltmp |= fsptr->mant & 037777777L;
    *(unsigned long *) fptr = ltmp;
}
