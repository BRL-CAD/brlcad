/*			P I X - S U N
 *
 *	Program to take a BRLCAD PIX format image file and convert the
 *	image to a Sun Microsystems 8-bit deep color "rasterfile" format
 *	image.  No color dithering is performed currently.  That will come
 *	later.
 *
 *   Author(s)
 *   Lee A. Butler
 *
 *  Source -
 *      SECAD/VLD Computing Consortium, Bldg 394
 *      The U. S. Army Ballistic Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *      This software is Copyright (C) 1986 by the United States Army.
 *      All rights reserved.
 *
 */
#include <stdio.h>

/* declarations to support use of getopt() system call */
char *options = "hs:w:n:d";
char optflags[sizeof(options)];
extern char *optarg;
extern int optind, opterr, getopt();
char *progname = "(noname)";

#define MAPSIZE 256   /* Number of unique color values in Sun Colormap */
/* Description of header for files containing raster images */
struct rasterfile {
    int	 ras_magic;	/* magic number */
    int	 ras_width;	/* width (pixels) of image */
    int	 ras_height;	/* height (pixels) of image */
    int	 ras_depth;	/* depth (1, 8, or 24 bits) of pixel */
    int	 ras_length;	/* length (bytes) of image */
    int	 ras_type;	/* type of file; see RT_* below */
    int	 ras_maptype;	/* type of colormap; see RMT_* below */
    int	 ras_maplength;	/* length (bytes) of following map */
    /* color map follows for ras_maplength bytes, followed by image */
} ras = {
    0x59a66a95,	/* Magic Number */
    512,	/* default width */
    512,	/* default height */
    8,		/* bits per pixel */
    0,		/* length of image */
    1,		/* standard rasterfile format */
    1,		/* equal RGB color map */
    MAPSIZE*3	/* length (bytes) of RGB colormap */
};

/* The Sun Rasterfile Colormap
 * This colormap has a 6x6x6 color cube, plus 10 extra values for each of
 * the primary colors and the grey levels
 */
unsigned char redmap[MAPSIZE] = 
{ 0,051,102,153,204,255,000,051,102,153,204,255,000,051,102,153,
204,255,000,051,102,153,204,255,000,051,102,153,204,255,000,051,
102,153,204,255,000,051,102,153,204,255,000,051,102,153,204,255,
  0,051,102,153,204,255,000,051,102,153,204,255,000,051,102,153,
204,255,000,051,102,153,204,255,000,051,102,153,204,255,000,051,
102,153,204,255,000,051,102,153,204,255,000,051,102,153,204,255,
  0,051,102,153,204,255,000,051,102,153,204,255,000,051,102,153,
204,255,000,051,102,153,204,255,000,051,102,153,204,255,000,051,
102,153,204,255,000,051,102,153,204,255,000,051,102,153,204,255,
  0,051,102,153,204,255,000,051,102,153,204,255,000,051,102,153,
204,255,000,051,102,153,204,255,000,051,102,153,204,255,000,051,
102,153,204,255,000,051,102,153,204,255,000,051,102,153,204,255,
  0,051,102,153,204,255,000,051,102,153,204,255,000,051,102,153,
204,255,000,051,102,153,204,255,017,034,068,085,119,136,170,187,
221,238,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
  0,000,000,000,000,000,017,034,068,085,119,136,170,187,221,238 };

unsigned char grnmap[MAPSIZE] = 
{ 0,000,000,000,000,000,051,051,051,051,051,051,102,102,102,102,
102,102,153,153,153,153,153,153,204,204,204,204,204,204,255,255,
255,255,255,255,000,000,000,000,000,000,051,051,051,051,051,051,
102,102,102,102,102,102,153,153,153,153,153,153,204,204,204,204,
204,204,255,255,255,255,255,255,000,000,000,000,000,000,051,051,
 51,051,051,051,102,102,102,102,102,102,153,153,153,153,153,153,
204,204,204,204,204,204,255,255,255,255,255,255,000,000,000,000,
  0,000,051,051,051,051,051,051,102,102,102,102,102,102,153,153,
153,153,153,153,204,204,204,204,204,204,255,255,255,255,255,255,
  0,000,000,000,000,000,051,051,051,051,051,051,102,102,102,102,
102,102,153,153,153,153,153,153,204,204,204,204,204,204,255,255,
255,255,255,255,000,000,000,000,000,000,051,051,051,051,051,051,
102,102,102,102,102,102,153,153,153,153,153,153,204,204,204,204,
204,204,255,255,255,255,255,255,000,000,000,000,000,000,000,000,
  0,000,017,034,068,085,119,136,170,187,221,238,000,000,000,000,
  0,000,000,000,000,000,017,034,068,085,119,136,170,187,221,238 };

unsigned char blumap[MAPSIZE] =
{ 0,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
  0,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
  0,000,000,000,051,051,051,051,051,051,051,051,051,051,051,051,
 51,051,051,051,051,051,051,051,051,051,051,051,051,051,051,051,
 51,051,051,051,051,051,051,051,102,102,102,102,102,102,102,102,
102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,
102,102,102,102,102,102,102,102,102,102,102,102,153,153,153,153,
153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,
153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,
204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,204,
204,204,204,204,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,000,000,000,000,000,000,000,000,
  0,000,000,000,000,000,000,000,000,000,000,000,017,034,068,085,
119,136,170,187,221,238,017,034,068,085,119,136,170,187,221,238 };


/* indicies of the primary colors and grey values in the color map */
static unsigned char rvec[16] = { 0, 216, 217, 1, 218, 219, 2, 220, 221,
				3, 222, 223, 4, 224, 225, 5};
static unsigned char gvec[16] = {  0, 226, 227,  6, 228, 229, 12, 230,
				231,  18, 232, 233, 24, 234, 235, 30};
static unsigned char bvec[16] = {  0, 236, 237,  36, 238, 239,  72, 240,
				241, 108, 242, 243, 144, 244, 245, 180 };
static unsigned char nvec[16] = {  0, 246, 247,  43, 248, 249,  86, 250, 
				251, 129, 252, 253, 172, 254, 255, 215};

/* map an 8 bit value into a "bin" on the color cube */
#define MAP(x, c) { \
	if (c < 26 ) x=0; \
	else if (c < 77 ) x=1; \
	else if (c < 128 ) x=2; \
	else if (c < 179 ) x=3; \
	else if (c < 230 ) x=4; \
	else x=5; }

/* convert 24 bit pixel into appropriate 8 bit pixel */
#define REMAPIXEL(red, green, blue, i) { \
	MAP(r, red); MAP(g, green); MAP(b, blue); \
	if (r == g) { \
		if (r == b) i = nvec[((red+green+blue)/3)/16]; /* grey */ \
		else if (r == 0)  i = bvec[blue/16];	/* all blue */ \
		else i = (unsigned char)(r + g * 6 + b * 36); /* color cube # */ \
	} \
	else if (g == b && g == 0) i = rvec[red/16];	/* all red */ \
	else if (r == b && r == 0) i = gvec[green/16]; 	/* all green */ \
	else i = (unsigned char)(r + g * 6 + b * 36);	/* color cube # */ }
	

/*
 *   D O I T --- convert stdin pix file to stdout rasterfile
 */
void doit()
{
    int i, cx, cy;
    unsigned char *pix, *rast, *malloc();
    register unsigned char r, g, b, red, green, blue;

    if ( ((ras.ras_width/2)*2) != ras.ras_width ) {
	(void)fprintf(stderr, "%s: Cannot handle odd x dimension\n",progname);
	exit(1);
    }

    i = ras.ras_width * ras.ras_height;
    /* allocate buffer for the pix file */
    if ((pix=(unsigned char *)malloc(i*3)) == (unsigned char *)NULL) {
	(void)fprintf(stderr, "%s: cannot get memory for a %d x %d pix file\n",
		progname, ras.ras_width, ras.ras_height );
	exit(1);
    }

    if ((rast=(unsigned char *)malloc(i)) == (unsigned char *)NULL) {
	(void)fprintf(stderr, "%s: cannot get memory for a %d x %d pixrect\n",
		progname, ras.ras_width, ras.ras_height );
	exit(1);
    }

    /* load the pix file into memory (What's Virtual Memory for anyway?)
     * we reverse the order of the scan lines to compensate
     * for differences of origin location in rasterfiles vs. PIX files
     */
    for (i=ras.ras_height-1 ; i >= 0 ; i--)
	if (fread(&pix[i*ras.ras_width*3], ras.ras_width*3, 1, stdin) != 1) {
	 (void)fprintf(stderr, "%s: error reading %d x %d pix file scanline %d\n",
		progname, ras.ras_width, ras.ras_height, i);
	 exit(1);
	}

    /* convert 24 bit pixels to 8 bits, 
     * switching top to bottom to compensate for the different origin 
     * representations of PIX files and Sun pixrects
     */
    for(cy=0 ; cy < ras.ras_height ; cy++)
	for(cx=0 ; cx < ras.ras_width ; cx++) {
	    red = pix[(cx + cy * ras.ras_width)*3];
	    green = pix[1 + (cx + cy * ras.ras_width)*3];
	    blue = pix[2 + (cx + cy * ras.ras_width)*3];
	    REMAPIXEL(red, green, blue, rast[cx + cy * ras.ras_width]);
	}

    /* now that we have the 8 bit pixels,
     *  we don't need the 24 bit pixels
     */
    free(pix);

    /* fill in miscelaneous rasterfile header fields */
    ras.ras_length = ras.ras_width * ras.ras_height;

    /* write the rasterfile header */
    if (fwrite(&ras, sizeof(ras), 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing rasterfile header to stdout\n", progname);
	exit(1);
    }

    /* write the colormap */
    if (fwrite(redmap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    if (fwrite(grnmap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    if (fwrite(blumap, MAPSIZE, 1, stdout) != 1) {
	(void)fprintf(stderr, "%s: error writing colormap\n", progname);
	exit(1);
    }

    /* write out the actual pixels */
    if (fwrite(rast, ras.ras_width, ras.ras_height, stdout) != ras.ras_height){
	(void)fprintf(stderr, "%s: error writing image\n", progname);
	exit(1);
    }
    free(rast);
}

/*   O F F S E T
 *
 *   return offset of character c in string s, or strlen(s) if c not in s
 */
int offset(s, c)
char s[], c;
{
    register unsigned int i=0;
    while (s[i] != '\0' && s[i] != c) i++;
    return(i);
}

void usage()
{
    (void)fprintf(stderr, "Usage: %s [-s squaresize] [-w width] [-n height] < pixfile > rasterfile\n", progname, options);
    exit(1);
}

/*
 *   M A I N
 *
 *   Perform miscelaneous tasks such as argument parsing and
 *   I/O setup and then call "doit" to perform the task at hand
 */
main(ac,av)
int ac;
char *av[];
{
    int	c, optlen;

    progname = *av;
    if (isatty(fileno(stdin))) usage();
    
    /* Get # of options & turn all the option flags off
    */
    optlen = strlen(options);

    for (c=0 ; c < optlen ; optflags[c++] = '\0');
    
    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line
    */
    while ((c=getopt(ac,av,options)) != EOF)
	switch (c) {
	case '?'    :
	case 'h'    : usage(); break;
	case 'w'    : ras.ras_width = atoi(optarg); break;
	case 'n'    : ras.ras_height = atoi(optarg); break;
	case 's'    : ras.ras_width = ras.ras_height = 
				atoi(optarg); break;
	default     : if (offset(options, c) != strlen(options))
			    optflags[offset(options, c)]++;
			else
			    usage();
			break;
	}


    if (optind < ac) usage();

    doit();
}
