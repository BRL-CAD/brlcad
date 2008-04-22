/*
 * Copyright (C) 1988 Research Institute for Advanced Computer Science.
 * All rights reserved.  The RIACS Software Policy contains specific
 * terms and conditions on the use of this software, and must be
 * distributed with any copies.  This file may be redistributed.  This
 * copyright and notice must be preserved in all copies made of this file.
 */

/*
 * rletoabA62
 * ----------
 *
 * This program converts Utah Raster Toolkit files into the dump format of the
 * Abekas A62 video storage disk.
 *
 * Options:
 *    -N ... do no digital filtering.
 *    -n N ... N specifies the number for frames to write
 *    -f N ... N specifies the first frame number (1-4) to write.
 *    -b R G B ... clear background to this color
 *
 * Author/Status:
 *	Bob Brown	    rlb@riacs.edu
 *	First write: 29 Jan 1988
 *
 * The interface to the particular raster format used is mostly separated from
 * the logic of the conversion code.  Two routines define the interface:
 *
 *	rasterInit(fd, width, height)
 *
 *	    This specifies the integer file descriptor for the input file,
 *	    the width and the height of the image.  It is called exactly once.
 *
 *	rasterGetRow(red, green, blue)
 *
 *	    This is called to return the next row of the raster. The parameters
 *	    point to arrays of unsigned char to hold the RGB values.
 */
static char rcsid[] = "$Header$";
/*
rletoabA62()				Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
typedef char bool;

/*
 * Dimensions of the raster image within the A62
 */

#define WIDTH 768
#define HEIGHT 512

/*
 * Global variables are always capitalized.
 *
 * These are globals set by program arguments.  The static initializations are
 * the default values.
 */

int		Width = WIDTH;	    /* Width of the output image */
int		Height = HEIGHT;    /* Height of the output image */
int		Debug = 0;
bool		NoFilter = FALSE;
int		Frame = 1;
int		NumFrames = 2;
int		BkgRed = 0;
int		BkgBlue = 0;
int		BkgGreen= 0;

extern char    *optarg;		/* interface to getopt() */
extern int	optind;		/* interface to getopt() */

typedef struct {
    char            y, i, q;
} yiq_t;

#ifdef USE_PROTOTYPES
extern void rasterInit(int fd, int width, int height);
extern void rasterRowGet(unsigned char *red, unsigned char *green, unsigned char *blue);
extern void rasterDone(void);
extern void filterY(float *yVal, float c0, float c1, float c2);
extern void filterIQ(float *iqVal, float c0, float c1, float c2, int start, int stride);
extern void dump1(register yiq_t **raster, int start);
extern void dump2(register yiq_t **raster, int start);
#else
extern void rasterInit();
extern void rasterRowGet(), rasterDone();
extern void filterY();
extern void filterIQ();
extern void dump1();
extern void dump2();
#endif

/*
 * Main entry...
 */

int
main(int argc,char *argv[])
{
    register int    i, j;
    int             errors, c, file;
    float           rTOy[256], gTOy[256], bTOy[256];
    float           rTOi[256], gTOi[256], bTOi[256];
    float           rTOq[256], gTOq[256], bTOq[256];
    float          *iVal, *yVal, *qVal, *tmpVal;
    unsigned char  *red, *green, *blue;
    int             r, g, b;
    yiq_t         **raster;

    /*
     * Parse program arguments...
     *
     * The -w and -h args should actually not be used.
     */

    errors = 0;
    while ((c = getopt(argc, argv, "Nn:f:w:h:d:D:b")) != EOF) {
	switch (c) {
	case 'b':
	    BkgRed = atoi(argv[optind++]);
	    BkgGreen = atoi(argv[optind++]);
	    BkgBlue = atoi(argv[optind++]);
	    break;
	case 'N':
	    NoFilter = TRUE;
	    break;
	case 'w':
	    Width = atoi(optarg);
	    break;
	case 'f':
	    Frame = atoi(optarg);
	    break;
	case 'n':
	    NumFrames = atoi(optarg);
	    break;
	case 'h':
	    Height = atoi(optarg);
	    break;
	case 'D':
	    Debug = atoi(optarg);
	    break;
	case '?':
	    errors++;
	}
    }
    if (errors > 0) {
	fprintf(stderr, "Usage: %s [-n] [-D debug-level] [file]\n", argv[0]);
	exit(1);
    }
    if (optind < argc) {
	if ((file = open(argv[optind], 0)) < 0) {
	    perror(argv[optind]);
	    exit(1);
	}
    } else {
	file = 0;
    }

    /*
     * Initialize the type manager for Utah RLE files
     */

    rasterInit(file, Width, Height, BkgRed, BkgGreen, BkgBlue);

    /*
     * Allocate storage for the RGB inputs, the computed YIQ, and the
     * results. This is all dynamically allocated because the dimensions of
     * the framestore are only decided at run time.
     *
     * The YIQ arrays are extended two entries at either end to simplify the
     * filtering code. The effect is that the iVal and qVal arrays can be
     * indexed [-2 .. Width+2] and yVal [-4 .. Width+4] (the stuff at the
     * end of the yVal array isn't used).
     */

    red = (unsigned char *) calloc(Width, sizeof *red);
    green = (unsigned char *) calloc(Width, sizeof *green);
    blue = (unsigned char *) calloc(Width, sizeof *blue);

    tmpVal = (float *) calloc(Width + 8, sizeof *yVal);
    yVal = tmpVal + 4;
    tmpVal = (float *) calloc(Width + 4, sizeof *iVal);
    iVal = tmpVal + 2;
    tmpVal = (float *) calloc(Width + 4, sizeof *qVal);
    qVal = tmpVal + 2;

    /*
     * Allocate storage to hold the 8-bit YIQ values for the entire
     * picture.
     */

    raster = (yiq_t **) calloc(Height, sizeof *raster);
    for (i = 0; i < Height; i++) {
	raster[i] = (yiq_t *) calloc(Width, sizeof **raster);
    }

    /*
     * Build the mappings from R,G,B to Y,I,Q. The pedastal is factored
     * into this mapping.
     */

    for (i = 0; i < 256; i++) {
	rTOy[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.299;
	gTOy[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.587;
	bTOy[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.114;

	rTOi[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.596;
	gTOi[i] = ((float) i / 255.0 * 0.925 + 0.075) * -0.274;
	bTOi[i] = ((float) i / 255.0 * 0.925 + 0.075) * -0.322;

	rTOq[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.211;
	gTOq[i] = ((float) i / 255.0 * 0.925 + 0.075) * -0.523;
	bTOq[i] = ((float) i / 255.0 * 0.925 + 0.075) * 0.312;
    }

    /*
     * process the input raster line by raster line
     */

    for (i = 0; i < Height; i++) {
	rasterRowGet(red, green, blue);
	yVal[-4] = yVal[-3] = yVal[-2] = yVal[-1] = 0;
	iVal[-2] = iVal[-1] = 0;
	qVal[-2] = qVal[-1] = 0;
	yVal[Width + 3] = yVal[Width + 2] = yVal[Width + 1] = yVal[Width + 0] = 0;
	iVal[Width + 0] = iVal[Width + 1] = 0;
	qVal[Width + 0] = qVal[Width + 1] = 0;

	/*
	 * Convert RGB to YIQ.  The multiplication is done by table lookup
	 * into the [rgb]TO[yiq] arrays.
	 */

	for (j = 0; j < Width; j++) {
	    r = red[j];
	    g = green[j];
	    b = blue[j];
	    yVal[j] = rTOy[r] + gTOy[g] + bTOy[b];
	    iVal[j] = rTOi[r] + gTOi[g] + bTOi[b];
	    qVal[j] = rTOq[r] + gTOq[g] + bTOq[b];
	}
	if (!NoFilter) {
	    filterY(yVal, 0.62232, 0.21732, -0.02848);
	    filterIQ(iVal, 0.42354, 0.25308, 0.03515, 0, 2);
	    filterIQ(qVal, 0.34594, 0.25122, 0.07581, 1, 2);
	}
	/*
	 * Build the YIQ raster
	 */

	for (j = 0; j < Width; j++) {
	    raster[Height - i - 1][j].y = yVal[j] * 140.0;
	    raster[Height - i - 1][j].i = iVal[j] * 140.0;
	    raster[Height - i - 1][j].q = qVal[j] * 140.0;
	}
    }

    /*
     * Dump the raster as four color fields.
     *
     * Assert that Width%4 ==0 and Height%4 == 0
     *
     * Despite what the A62 SCSI manual says, for frames I and IV, the even
     * numbered lines (starting with line 0) begin Y-I and the others begin
     * Y+I.
     */

    for (i = 0; i < NumFrames; i++) {
	switch ((i + Frame - 1) % 4 + 1) {
	case 1:
	    dump2(raster, 0);	/* even lines, starts Y-I */
	    break;
	case 2:
	    dump1(raster, 1);	/* odd lines, starts Y+I */
	    break;
	case 3:
	    dump1(raster, 0);	/* even lines, starts Y+I */
	    break;
	case 4:
	    dump2(raster, 1);	/* odd lines, starts Y-I */
	    break;
	}
    }

    return 0;
}

/*
 * filterY
 * -------
 *
 * Apply and RIF filter to the luminance data. The signal is shifted two pixels
 * to the right in the process.
 *
 * The multiplier arrays m0, m1, and m2 exist to reduce the number of floating
 * point multiplications and to allow the filtering to occur in place.
 */
void
filterY(yVal, c0, c1, c2)
float *yVal, c0, c1, c2;
{
    static float   *m0 = NULL;
    static float   *m1 = NULL;
    static float   *m2 = NULL;
    register int    i;

    if (m1 == NULL) {
	m0 = (float *) calloc(Width + 8, sizeof(float));
	m1 = (float *) calloc(Width + 8, sizeof(float));
	m2 = (float *) calloc(Width + 8, sizeof(float));
    }
    for (i = -4; i < Width + 4; i++) {
	m0[i] = c0 * yVal[i];
	m1[i] = c1 * yVal[i];
	m2[i] = c2 * yVal[i];
    }
    for (i = 0; i < Width; i++) {
	yVal[i] = m2[i - 4] + m1[i - 3] + m0[i - 2] + m1[i - 1] + m2[i];
    }
}
/*
 * filterIQ
 * --------
 *
 * Apply and RIF filter to the color difference data.
 *
 * The multiplier arrays m0, m1, and m2 exist to reduce the number of floating
 * point multiplications and to allow the filtering to occur in place.
 *
 * The "start" and "stride" parameters are used to reduce the number of
 * computations because I and Q are only used every other pixel.
 *
 * This is different from the manual in than the filtering is done on adjacent
 * pixels, rather than every other pixel.  This may be a problem...
 */
void
filterIQ(iqVal, c0, c1, c2, start, stride)
float *iqVal, c0, c1, c2;
int start, stride;
{
    static float   *m0 = NULL;
    static float   *m1 = NULL;
    static float   *m2 = NULL;
    register int    i;

    if (m1 == NULL) {
	m0 = (float *) calloc(Width + 4, sizeof(float));
	m1 = (float *) calloc(Width + 4, sizeof(float));
	m2 = (float *) calloc(Width + 4, sizeof(float));
    }
    for (i = -2; i < Width + 2; i++) {
	m0[i] = c0 * iqVal[i];
	m1[i] = c1 * iqVal[i];
	m2[i] = c2 * iqVal[i];
    }
    for (i = start; i < Width; i += stride) {
	iqVal[i] = m2[i - 2] + m1[i - 1] + m0[i] + m1[i + 1] + m2[i + 2];
    }
}

/*
 * dump1
 * -----
 *
 * Dumps the raster starting with the sequence Y+I Y+Q Y-I Y-Q
 *
 * This routine also adds 60 to put the data in the 60 .. 200 range.
 */

#define OUT(y, OP, iq) putc((char) (raster[i][j + 0].y OP raster[i][j + 0].iq + 60), stdout);
void
dump1(raster, start)
register yiq_t **raster;
int start;
{
    register int    i, j;

    for (i = start; i < Height; i += 4) {	/* field I */
	for (j = 0; j < Width; j += 4) {
	    putc((char) (raster[i][j + 0].y + raster[i][j + 0].i + 60), stdout);
	    putc((char) (raster[i][j + 1].y + raster[i][j + 1].q + 60), stdout);
	    putc((char) (raster[i][j + 2].y - raster[i][j + 2].i + 60), stdout);
	    putc((char) (raster[i][j + 3].y - raster[i][j + 3].q + 60), stdout);
	}
	for (j = 0; j < Width; j += 4) {
	    putc((char) (raster[i + 2][j + 0].y - raster[i + 2][j + 0].i + 60), stdout);
	    putc((char) (raster[i + 2][j + 1].y - raster[i + 2][j + 1].q + 60), stdout);
	    putc((char) (raster[i + 2][j + 2].y + raster[i + 2][j + 2].i + 60), stdout);
	    putc((char) (raster[i + 2][j + 3].y + raster[i + 2][j + 3].q + 60), stdout);
	}
    }
}

/*
 * dump2
 * -----
 *
 * Dumps the raster starting with the sequence Y-I Y-Q Y+I Y+Q
 *
 * This routine also adds 60 to put the data in the 60 .. 200 range.
 */

void
dump2(raster, start)
register yiq_t **raster;
int start;
{
    register int    i, j;

    for (i = start; i < Height; i += 4) {	/* field I */
	for (j = 0; j < Width; j += 4) {
	    putc((char) (raster[i][j + 0].y - raster[i][j + 0].i + 60), stdout);
	    putc((char) (raster[i][j + 1].y - raster[i][j + 1].q + 60), stdout);
	    putc((char) (raster[i][j + 2].y + raster[i][j + 2].i + 60), stdout);
	    putc((char) (raster[i][j + 3].y + raster[i][j + 3].q + 60), stdout);
	}
	for (j = 0; j < Width; j += 4) {
	    putc((char) (raster[i + 2][j + 0].y + raster[i + 2][j + 0].i + 60), stdout);
	    putc((char) (raster[i + 2][j + 1].y + raster[i + 2][j + 1].q + 60), stdout);
	    putc((char) (raster[i + 2][j + 2].y - raster[i + 2][j + 2].i + 60), stdout);
	    putc((char) (raster[i + 2][j + 3].y - raster[i + 2][j + 3].q + 60), stdout);
	}
    }
}
