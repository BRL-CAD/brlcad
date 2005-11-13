/*
 * rleClock
 * --------
 *
 * Generates a clock face with digital output above it, in Utah Raster toolkit
 * format.
 *
 * SYNOPSIS
 *	rleClock [options]
 *
 *	Writes an RLE file to stdout containins a clock face image according to
 *	the options.
 *
 * OPTIONS
 *
 *	Too many to describe.  Type "rleClock -help" for a listing.
 * 
 * AUTHOR
 *	Bob Brown	    rlb@riacs.edu
 *	RIACS		    415 694 5407
 *	Mail Stop 230-5
 *	NASA Ames Research Center
 *	Moffett Field
 *	CA  94035
 *
 *	First draft, December 3, 1987
 *	lineDots() written by Nancy Blachman Jan, 1987
 *	font.src derived from NBS fonts.
 */
static char rcsid[] = "$Header$";
/*
rleClock()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "rle.h"
#include <sys/types.h>
#include <time.h>
#include "font.h"

/*
 * Program parameters defaults
 *
 * Note: within the program, radius is on a scale of 0..1 and then converted in
 * the polar coordinate conversion routines.
 */

#define XSIZE 128
#define YTEXTSIZE 0
#define YCLOCKSIZE XSIZE
#define TICKS 12
#define DOTS 1
#define FORMATSTRING "%02l:%02b"
#define FACE_EDGE_COLOR { 255, 255, 255 }
#define HAND_COLOR { 255, 255, 255 }
#define TEXT_COLOR { 255, 255, 255 }
#define LITTLEHANDSCALE 12
#define BIGHANDSCALE 60

#define HANDWIDTH 0.075

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
typedef char bool;

/*
 * These bits define what is in the elements of the "Raster" array
 */

#define RAST_FACE_EDGE 1
#define RAST_FACE_MASK 2
#define RAST_LHAND_EDGE 4
#define RAST_LHAND_MASK 8
#define RAST_BHAND_EDGE 16
#define RAST_BHAND_MASK 32
#define RAST_TEXT 64
#define RAST_TEXT_BACK 128

/*
 * Type definitions
 */

typedef struct{
    unsigned char red, green, blue;
} color_t;

/*
 * Global variables
 */

color_t FaceEdgeColor = FACE_EDGE_COLOR;
color_t FaceColor;
color_t HandEdgeColor;
color_t HandColor = HAND_COLOR;
color_t TextColor = TEXT_COLOR;
color_t TextBackColor;

rle_pixel **RedLine;
rle_pixel **GreenLine;
rle_pixel **BlueLine;
rle_pixel **Raster;
rle_pixel **AlphaLine;

int XSize = XSIZE;
int YSize;
int YClockSize = YCLOCKSIZE;
int YTextSize = YTEXTSIZE;
int XRadius;
int YRadius;
int Ticks = TICKS;
bool DebugAlpha = FALSE;
bool Debug = FALSE;
int Dots = DOTS;
float BigHandValue;
float LittleHandValue;
float LittleHandScale = LITTLEHANDSCALE;
float BigHandScale = BIGHANDSCALE;
CONST_DECL char *FormatString = FORMATSTRING;

/*
 * External globals.
 */

extern move_t Moves[];
extern int Base[];

/*
 * Forward declarations
 */

#ifdef USE_PROTOTYPES
void main(int argc, char *argv[]);
void ifImageSet(int i, int j, int value, color_t *color);
void drawHand(double place, double scale, double radius, int mask, int edge);
void rasterAddBits(int mask, int match, int value);
void polarLine(double r0, double a0, double r1, double a1, int arg1, int arg2);
void setDot(int x, int y, int arg1, int arg2);
int polarToX(double fRadius, double angle);
int polarToY(double fRadius, double angle);
double radians(double degrees);
rle_pixel **rasterAllocate(int height, int width);
void rasterWrite(FILE *fd);
void lineDots(int x0, int y0, int x1, int y1, void (*func)(), int arg1, int arg2);
void procargs(int argc, char *argv[]);
bool argGiven(char *argVar);
void usageExit(char *pgm);
void charMinMaxWidth(int ch, int *min, int *max);
void charMinMaxHeight(int ch, int *min, int *max);
char *formatInterp(CONST_DECL char *str);
void drawText(void);
void areaFlood(int firstX, int firstY, int mask, int match, int value);
void stackPush(int x, int y, int dir);
int stackPop(void);
#else
void main();
void ifImageSet();
void drawHand();
void rasterAddBits();
void polarLine();
void setDot();
int polarToX();
int polarToY();
double radians();
rle_pixel **rasterAllocate();
void rasterWrite();
void lineDots();
void procargs();
bool argGiven();
void usageExit();
void charMinMaxWidth();
void charMinMaxHeight();
char *formatInterp();
void drawText();
void areaFlood();
void stackPush();
int stackPop();
#endif
char **gargv;

void
main (argc, argv)
int	argc;
char	*argv[];
{
    int             i, j;
    float           theta;
    time_t	    now;
    struct tm      *tm, *localtime();
    bool haveFaceColor;
    bool haveHandEdgeColor;
    bool haveTextBackColor;

    gargv = argv;

    procargs(argc, argv);
    YRadius = (YClockSize - Dots) / 2;
    XRadius = (XSize - Dots) / 2;
    YSize = YClockSize + YTextSize + Dots + Dots;
    if (!argGiven((char *) &LittleHandValue) || !argGiven((char *) &BigHandValue)) {
	(void)time(&now);
	tm = localtime(&now);

	if (!argGiven((char *) &BigHandValue))
	    BigHandValue = (float) tm->tm_min;
	if (!argGiven((char *) &LittleHandValue))
	    LittleHandValue = (float) ((tm->tm_hour % 12)) + BigHandValue / 60.0;
    }
    /*
     * Allocate the storage for the raster 
     */

    RedLine = (rle_pixel **) rasterAllocate(YSize, XSize);
    GreenLine = (rle_pixel **) rasterAllocate(YSize, XSize);
    BlueLine = (rle_pixel **) rasterAllocate(YSize, XSize);
    AlphaLine = (rle_pixel **) rasterAllocate(YSize, XSize);
    Raster = (rle_pixel **) rasterAllocate(YSize, XSize);

    /*
     * Initialize the raster to the background color 
     */

    for (i = 0; i < YSize; i++) {
	for (j = 0; j < XSize; j++) {
	    Raster[i][j] = 0;
	}
    }

    /*
     * Draw the clock face as a circle with tick marks 
     */

    for (i = 0; i < 360; i++) {
	polarLine(1.0, (float) i, 1.0, (float) (i + 1), RAST_FACE_EDGE, Dots);
    }
    for (i = 0; i < Ticks; i++) {
	theta = (float) i *360.0 / (float) Ticks;

	polarLine(1.0, theta, 0.85, theta, RAST_FACE_EDGE, Dots);
    }

    /*
     * Compute the RAST_FACE_MASK portion - includes what is inside the
     * dial face plus the dial face itself.  So first flood the inside, and
     * then OR in the stuff under the face lines 
     */

    areaFlood(polarToX(0.0, 0.0), polarToY(0.0, 0.0), RAST_FACE_EDGE, 0, RAST_FACE_MASK);
    rasterAddBits(RAST_FACE_EDGE, RAST_FACE_EDGE, RAST_FACE_MASK);

    /*
     * Draw the hands and the text... 
     */

    drawHand(BigHandValue, BigHandScale, 0.85, RAST_BHAND_MASK, RAST_BHAND_EDGE);
    drawHand(LittleHandValue, LittleHandScale, 0.60, RAST_LHAND_MASK, RAST_LHAND_EDGE);
    if (YTextSize > 0) {
	drawText();
    }
    /*
     * Compose the clock image from the generated raster and program
     * arguments 
     */

    haveFaceColor = argGiven((char *)&FaceColor);
    haveHandEdgeColor = argGiven((char *)&HandEdgeColor);
    haveTextBackColor = argGiven((char *)&TextBackColor);
    for (i = 0; i < YSize; i++) {
	for (j = 0; j < XSize; j++) {
	    if (haveFaceColor) {
		ifImageSet(i, j, RAST_FACE_MASK, &FaceColor);
	    }
	    ifImageSet(i, j, RAST_FACE_EDGE, &FaceEdgeColor);
	    ifImageSet(i, j, RAST_LHAND_MASK|RAST_BHAND_MASK, &HandColor);
	    if (haveHandEdgeColor) {
		ifImageSet(i, j, RAST_LHAND_EDGE|RAST_BHAND_EDGE, &HandEdgeColor);
	    }
	    if (haveTextBackColor) {
		ifImageSet(i, j, RAST_TEXT_BACK, &TextBackColor);
	    }
	    ifImageSet(i, j, RAST_TEXT, &TextColor);

	    /*
	     * Now compute the Alpha channel
	     */

    	    if ( (haveFaceColor && (Raster[i][j]&RAST_FACE_MASK)!=0)
	     || ((Raster[i][j]&(RAST_FACE_EDGE|RAST_LHAND_MASK|RAST_BHAND_MASK))!=0)
	     || (haveTextBackColor && (Raster[i][j] & RAST_TEXT_BACK)!=0)
	     || ((Raster[i][j] & RAST_TEXT)!=0)) {
		AlphaLine[i][j] = 255;
	    } else {
		AlphaLine[i][j] = 0;
	    }
	}
    }

    /*
     * Dump the raster file to stdout... 
     */

    rasterWrite(stdout);

    exit(0);
}

void
ifImageSet(i, j, value, color)
int i, j, value;
color_t *color;
{
    if (Raster[i][j] & value) {
	RedLine[i][j] = color->red;
	GreenLine[i][j] = color->green;
	BlueLine[i][j] = color->blue;
    }
}

void
drawHand(place, scale, radius, mask, edge)
double place;
double scale;
double radius;
int mask, edge;
{
    float angle;
    angle = place / scale * 360;
    polarLine(HANDWIDTH,    angle+180.0,  HANDWIDTH,    angle-90.0,  edge, 1);
    polarLine(HANDWIDTH,    angle-90.0,   radius,       angle,       edge, 1);
    polarLine(radius,       angle,        HANDWIDTH,    angle+90.0,  edge, 1);
    polarLine(HANDWIDTH,    angle+90.0,   HANDWIDTH,    angle+180.0, edge, 1);
    areaFlood(polarToX(0.0, 0.0), polarToY(0.0, 0.0), edge, 0,
	mask);
    polarLine(HANDWIDTH,    angle+180.0,  HANDWIDTH,    angle-90.0,  edge, Dots);
    polarLine(HANDWIDTH,    angle-90.0,   radius,       angle,       edge, Dots);
    polarLine(radius,       angle,        HANDWIDTH,    angle+90.0,  edge, Dots);
    polarLine(HANDWIDTH,    angle+90.0,   HANDWIDTH,    angle+180.0, edge, Dots);
    rasterAddBits(edge, edge, mask);
}

void
rasterAddBits(mask, match, value)
int mask, match, value;
{
    int i, j;

    for (i = 0 ; i < YSize; i++) {
	for (j = 0; j < XSize; j++) {
	    if ( (Raster[i][j]&mask) == match )
		Raster[i][j] |= value;
	}
    }
}

void
polarLine(r0, a0, r1, a1, arg1, arg2)
double r0, a0, r1, a1;
int arg1, arg2;
{
	lineDots(polarToX(r0, a0), 
		 polarToY(r0, a0), 
		 polarToX(r1, a1), 
		 polarToY(r1, a1), 
		 setDot, arg1, arg2);
}

/*
 * setDot
 * ------
 *
 * Draw a dot (actually a square) or a certain size.  This is called from
 * lineDots to draw the dots that comprise a line.
 */

void
setDot(x, y, arg1, arg2)
int x, y, arg1, arg2;
{
    int i, j;

if(Debug)fprintf(stderr, "Setting %d, %d\n", x, y);
    for (i = 0; i < arg2; i++) {
	for (j = 0; j < arg2; j++) {
	    Raster[y+i][x+j] |= arg1;
	}
    }
}


/*
 * polar conversion
 * ----------------
 *
 * These routines convert from polar coordinates to cartesian coordinates in
 * the clock part of the pixel rectangle.
 */

int
polarToX(fRadius, angle)
double fRadius;
double angle;
{
    return (int)(fRadius * sin(radians(angle)) * XRadius) + XSize/2;
}

int
polarToY(fRadius, angle)
double fRadius;
double angle;
{
    return (int)(fRadius * cos(radians(angle)) * YRadius) + YRadius;
}

double
radians(degrees)
double degrees;
{
    return degrees/180.0 * 3.1415926;
}

/*
 * rasterAllocate
 * --------------
 *
 * Allocate a raster for a single color.
 */

rle_pixel **
rasterAllocate(height, width)
int height, width;
{
    rle_pixel **new, *row;
    int i;

    new = (rle_pixel **)calloc(height, sizeof(rle_pixel *));
    row = (rle_pixel *)calloc(height*width, sizeof(rle_pixel));
    for ( i=0 ; i<height ; i++) {
	new[i] = row;
	row += width;
    }
    return new;
}

/*
 * rasterWrite
 * -----------
 *
 * Dump the entire raster to a Utah RLE format file.
 */

void
rasterWrite(fd)
FILE *fd;
{
    rle_hdr 	    the_hdr;
    rle_pixel      *rows[4];
    int             i;

    the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &the_hdr, cmd_name( gargv ), NULL, 0 );

    RLE_SET_BIT(the_hdr, RLE_ALPHA);
    the_hdr.rle_file = fd;
    the_hdr.xmax = XSize;
    the_hdr.ymax = YSize;
    the_hdr.alpha = 1;
    rle_addhist( gargv, NULL, &the_hdr );

    rle_put_setup(&the_hdr);
    for (i = 0; i < YSize; i++) {
	rows[0] = AlphaLine[i];
	if (DebugAlpha) {
	    rows[1] = AlphaLine[i];
	    rows[2] = AlphaLine[i];
	    rows[3] = AlphaLine[i];
	} else {
	    rows[1] = RedLine[i];
	    rows[2] = GreenLine[i];
	    rows[3] = BlueLine[i];
	}
	rle_putrow(rows + 1, XSize, &the_hdr);
    }
    rle_close_f( the_hdr.rle_file );
}

/*
 * Bresenham's line drawing algorithm based on the general Bresenham
 * line drawing algorithm described in Rogers "Procedural Elements for
 * Computer Graphics" on page 40. 
 *
 * Written by	Nancy Blachman 
 *		CS 248A, Winter 
 *		Prof. Leo Guibas 
 *		Stanford University 
 *		January 13, 1987 
 *
 * This is why RIACS sent Nancy to grad school!
 */

void
lineDots(x0, y0, x1, y1, func, arg1, arg2)
int x0, y0, x1, y1;
void (*func)();
int arg1, arg2;
{
    int             e, x, y, delta_x, delta_y, tmp;
    bool            interchg = FALSE;
    int		    dir_x, dir_y;
    int             two_dy, two_dx;	/* calculated outside of loop */
    register int    i;


    if (x0 == x1 && y0 == y1) {	/* is starting point = end point ? */
	(*func)(x0, y0, arg1, arg2);
	return;
    }
    x = x0;
    y = y0;

    delta_x = x1 - x0;
    delta_y = y1 - y0;

    delta_x = delta_x > 0 ? delta_x : -delta_x;	/* absolute value,
						 * abs(x1-x0) */
    delta_y = delta_y > 0 ? delta_y : -delta_y;	/* absolute value,
						 * abs(y1-x0) */

    dir_x = (x1 - x0) > 0 ? 1 : -1;	/* sign (x1 - x0) */
    dir_y = (y1 - y0) > 0 ? 1 : -1;	/* sign (y1 - y0) */

    if (delta_y > delta_x) {
	tmp = delta_x;
	delta_x = delta_y;
	delta_y = tmp;
	interchg = TRUE;
    }
    two_dx = 2 * delta_x;
    two_dy = 2 * delta_y;

    e = two_dy - delta_x;
    for (i = 1; i <= delta_x; ++i) {
	(*func) (x, y, arg1, arg2);
	while (e >= 0) {
	    if (interchg)
		x += dir_x;
	    else
		y += dir_y;
	    e -= two_dx;
	}
	if (interchg)
	    y += dir_y;
	else
	    x += dir_x;
	e += two_dy;
    }
}

/*
 * procargs
 * --------
 *
 * Argument line parser.  This version is table driven and requires exact match
 * on the argument switches.
 */

struct {
    bool show;
    CONST_DECL char *arg;
    enum { INT, FLOAT, STRING, BOOL, COLOR, HELP, TEXT } type;
    CONST_DECL char *description;
    CONST_DECL char *value;
    bool given;
} Args[] ={
    { TRUE,  "-x",  INT,    "Image width in pixels",              (char *)&XSize }, 
    { TRUE,  "-cy", INT,    "Clock image height in pixels",       (char *)&YClockSize },
    { TRUE,  "-ty", INT,    "Text image height in pixels",        (char *)&YTextSize },
    { TRUE,  "-help", HELP, "Prints this help message",           NULL },

    { TRUE,  "-bv", FLOAT,  "Big hand value",                     (char *)&BigHandValue }, 
    { TRUE,  "-bs", FLOAT,  "Big hand full scale value",          (char *)&BigHandScale }, 
    { TRUE,  "-lv", FLOAT,  "Little hand value",                  (char *)&LittleHandValue },
    { TRUE,  "-ls", FLOAT,  "Little hand full scale value",       (char *)&LittleHandScale },
    { TRUE,  "-t",  INT,    "Number of ticks around the face",    (char *)&Ticks },
    { TRUE,  "-lw", INT,    "Line width in pixels",               (char *)&Dots },

    { TRUE,  "-fc", COLOR,  "Clock face edges color",             (char *)&FaceEdgeColor }, 
    { TRUE,  "-Fc", COLOR,  "Clock face background color",        (char *)&FaceColor }, 
    { TRUE,  "",    TEXT,   " - if omitted, then the clock is transparent" },  
    { TRUE,  "-hc", COLOR,  "Clock hands edges color",            (char *)&HandEdgeColor },
    { TRUE,  "",    TEXT,   " - if omitted,  no hand edges shown" }, 
    { TRUE,  "-Hc", COLOR,  "Clock hands fill color",             (char *)&HandColor }, 
    { TRUE,  "-tc", COLOR,  "Text color",			  (char *)&TextColor}, 
    { TRUE,  "-Tc", COLOR,  "Text background color",	          (char *)&TextBackColor}, 
    { TRUE,  "",    TEXT,   " - if omitted, then the text is transparent" },  
    { TRUE,  "-tf", STRING, "Text area format string", 	          (char *)&FormatString }, 
    { FALSE, "-Xm", BOOL,   "Output the alpha channel on RGB",    (char *)&DebugAlpha },
    { FALSE, "-D",  BOOL,   "Turn on debugging",	          (char *)&Debug },
    NULL
};

void
procargs(argc, argv)
int argc;
char *argv[];
{
    int arg, i;
    color_t *color;

    for ( arg = 1 ; arg<argc ; arg++) {
	for ( i=0 ; Args[i].arg != NULL ; i++) {
	    if (Args[i].type != TEXT && strcmp(argv[arg], Args[i].arg) == 0) {
		break;
	    }
	}
	if (Args[i].arg==NULL) {
	    fprintf(stderr, "Unknown argument: \"%s\"\n", argv[arg]);
	    usageExit(argv[0]);
	}
	Args[i].given = TRUE;
	switch (Args[i].type) {
	case HELP:
	    usageExit(argv[0]);
	    break;
	case INT:
	    arg += 1;
	    *(int *)Args[i].value = atoi(argv[arg]);
	    break;
	case FLOAT:
	    arg += 1;
	    *(float *)Args[i].value = atof(argv[arg]);
	    break;
	case BOOL:
	    *(bool *)Args[i].value = TRUE;
	    break;
	case STRING:
	    arg += 1;
	    *(char **)Args[i].value = (char *)malloc(strlen(argv[arg])+1);
	    strcpy(*(char **)Args[i].value, argv[arg]);
	    break;
	case COLOR:
	    color = (color_t *)Args[i].value;
	    if ( arg+3 >= argc || !isdigit(argv[arg+1][0]) 
	                       || !isdigit(argv[arg+2][0]) 
			       || !isdigit(argv[arg+3][0])) {
		fprintf(stderr, "%s: %s takes three numeric arguments\n", argv[0],
		    Args[i].arg);
		usageExit(argv[0]);
	    }
	    color->red = atoi(argv[arg+1]);
	    color->green = atoi(argv[arg+2]);
	    color->blue = atoi(argv[arg+3]);
	    arg += 3;
	    break;
	default:
	    break;
	}
    }
}
bool argGiven(argVar)
char *argVar;
{
    int i;

    for ( i=0 ; Args[i].arg != NULL ; i++) {
	if (Args[i].value == argVar) {
	    return Args[i].given;
	}
    }
    return FALSE;
}

void
usageExit(pgm)
char *pgm;
{
    int             i;

    fprintf(stderr, "Usage: %s [args]\n", pgm);
    for (i = 0; Args[i].arg != NULL; i++) {
	if (Args[i].show) {
	    fprintf(stderr, "\t%s", Args[i].arg);
	    switch (Args[i].type) {
	    case INT:
		fprintf(stderr, " INT");
		break;
	    case FLOAT:
		fprintf(stderr, " FLOAT");
		break;
	    case BOOL:
		break;
	    case STRING:
		fprintf(stderr, " STR");
		break;
	    case COLOR:
		fprintf(stderr, " RED GREEN BLUE");
		break;
	    default:
		break;
	    }
	    fprintf(stderr, " ... %s\n", Args[i].description);
	}
    }
    exit(1);

}

/*
 * charMinMax{Width,Height}
 * ------------------------
 *
 * Compute the minimum and maximum width and height of a character as defined
 * in the font.
 */

void
charMinMaxWidth(ch, min, max)
int ch;
int *min, *max;
{
    int epos, pos;

    if (!isprint(ch)) {
	*min = *max = 0;
	return;
    }
    if (ch == ' ') {
	ch = 'n';
    }
    *min = 999;
    *max = -999;
    epos = Base[(int)ch - 33 + 1];
    pos = Base[(int)ch - 33];
    for (; pos < epos; pos++) {
	if (Moves[pos].x > *max) {
	    *max = Moves[pos].x;
	}
	if (Moves[pos].x < *min) {
	    *min = Moves[pos].x;
	}
    }
}

void
charMinMaxHeight(ch, min, max)
int ch;
int *min, *max;
{
    int epos, pos;

    if (!isprint(ch)) {
	*min = *max = 0;
	return;
    }
    if (ch == ' ') {
	ch = 'n';
    }
    *min = 999;
    *max = -999;
    epos = Base[(int)ch - 33 + 1];
    pos = Base[(int)ch - 33];
    for (; pos < epos; pos++) {
	if (Moves[pos].y > *max) {
	    *max = Moves[pos].y;
	}
	if (Moves[pos].y < *min) {
	    *min = Moves[pos].y;
	}
    }
}

/*
 * formatInterp
 * ------------
 *
 * Interpret the format string - returns the value string as specified.
 */

char *
formatInterp(str)
CONST_DECL char *str;
{
    char *buf, *bufp;
    int state;
    static char outBuf[1024];
    char tmpBuf[1024];

    buf = (char *)malloc(strlen(str)*2+1);
    bufp = buf;
    state = 0;
    strcpy(outBuf, "");
    while ( *str != 0) {
	switch (state) {
	case 0:
	    *bufp++ = *str;
	    if (*str == '%') {
		state = 1;
	    }
	    break;
	case 1:
	    if (isdigit(*str) || *str == '.') {
		*bufp++ = *str;
	    } else {
		if ( *str=='B' || *str=='L') {
		    *bufp++ = 'f';
		    *bufp = 0;
		    sprintf(tmpBuf, buf, *str == 'B' ? (float)BigHandValue
						     : (float)LittleHandValue);
		} else if ( *str=='b' || *str=='l') {
		    *bufp++ = 'd';
		    *bufp = 0;
		    sprintf(tmpBuf, buf, *str == 'b' ? (int)BigHandValue
						     : (int)LittleHandValue);
		} else {
		    *bufp++ = *str;
		    *bufp = 0;
		    strcpy(tmpBuf, buf);
		}
		strcat(outBuf, tmpBuf);
		bufp = buf;
		state = 0;
	    }
	    break;
	}
	str++;
    }
    *bufp = 0;
    strcat(outBuf, buf);
    free(buf);
    return outBuf;
}

/*
 * drawText
 * --------
 *
 * Draw vectors for the given tect string, in the TEXT area of the raster.
 */

#define CHARPAD 25

void
drawText()
{
    char           *string;
    int             i, j, xsize, ysize, min, max, basex;
    int             curx, cury, x, y, pos, epos, charBasex;
    float           scale, scalex;

    string = formatInterp(FormatString);
    xsize = CHARPAD+Dots;
    ysize = 0;
    for (i = 0; string[i] != 0; i++) {
	charMinMaxWidth(string[i], &min, &max);
	xsize += (max - min) + CHARPAD+Dots;
	charMinMaxHeight(string[i], &min, &max);
	if (ysize < (max - min)+Dots) {
	    ysize = max - min+Dots;
	}
    }
    scale = (float) YTextSize / (float) ysize;
    scalex = (float) XSize / (float) xsize;
    if (scale > scalex) {
	scale = scalex;
    }
    basex = (XSize - (int) ((float)xsize * scale)) / 2;
    curx = cury = 0;
    charBasex = CHARPAD;
    for (i = 0; string[i] != 0; i++) {
	if (isprint(string[i]) && string[i] != ' ') {
	    charMinMaxWidth(string[i], &min, &max);
	    epos = Base[((int) (string[i])) - 33 + 1];
	    for (pos = Base[(int) string[i] - 33]; pos < epos; pos++) {
		x = basex + (int) (scale * (charBasex + Moves[pos].x + (-min)));
		y = (int) (scale * Moves[pos].y);
		if (Moves[pos].type == 'n') {
		    lineDots(curx, YClockSize + Dots + 1 + cury, x, YClockSize + Dots + 1 + y, setDot, RAST_TEXT, Dots);
		}
		curx = x;
		cury = y;
	    }
	}
	charBasex += (max-min) + CHARPAD + Dots;
    }
    x = basex + (int)(scale * charBasex);
    if (x > XSize) {
	x = XSize;
    }
    y = scale * ysize + YClockSize+Dots+1;
    if (y+Dots > YSize) {
	y = YSize-Dots;
    }
    for (i = YClockSize+Dots; i < y+Dots ; i++) {
	for (j = basex; j < x; j++) {
	    Raster[i][j] |= RAST_TEXT_BACK;
	}
    }
}

/*
 * areaFlood
 * ---------
 *
 * A flooding algorithm for painting regions of the clock
 */

typedef struct {
    short x, y;
    int dir;
} clock_stack_t;

#define NORTH 0
#define WEST 1
#define SOUTH 2
#define EAST 3
struct {
	clock_stack_t *s;
	int	top;
	int allocked;
} Stack;

int XMove[4] = {0, 1, 0, -1};
int YMove[4] = {1, 0, -1, 0};

void
areaFlood(firstX, firstY, mask, match, value)
int firstX, firstY;
int mask, match, value;
{
    register clock_stack_t *sp;

    Stack.s = (clock_stack_t *) calloc(256, sizeof(clock_stack_t));
    Stack.allocked = 256;
    Stack.top = -1;
    stackPush(firstX, firstY, NORTH);

    while (Stack.top >= 0) {
	sp = &Stack.s[Stack.top];
	if ((Raster[sp->y][sp->x]&mask)==match && (Raster[sp->y][sp->x]&value)!=value) {
	    Raster[sp->y][sp->x] |= value;
	    if (Debug)
		fprintf(stderr, "Marking %d, %d at stack %d\n", sp->x, sp->y, Stack.top);
	    stackPush(sp->x + XMove[sp->dir], sp->y + YMove[sp->dir],
		      NORTH);
	} else {
	    do {
		if (stackPop())
		    break;
		sp = &Stack.s[Stack.top];
		sp->dir++;
	    } while (sp->dir >= 4);
	    if (Stack.top >= 0)
		stackPush(sp->x + XMove[sp->dir], sp->y + YMove[sp->dir],
			  NORTH);
	}
    }
}

void
stackPush(x, y, dir)
int x, y, dir;
{
    if (++Stack.top >= Stack.allocked) {
	    Stack.allocked += 256;
	    Stack.s = (clock_stack_t *) realloc(Stack.s, Stack.allocked * sizeof(clock_stack_t));
if(Debug)fprintf(stderr, "Stack growing to %d\n", Stack.allocked);
    }
	Stack.s[Stack.top].x = x;
	Stack.s[Stack.top].y = y;
	Stack.s[Stack.top].dir = dir;
}

int
stackPop()
{
	Stack.top -= 1;
	return Stack.top < 0;
}
