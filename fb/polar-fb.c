/*
 *			P O L A R - F B . C
 *
 *	This routine plots normalized polar functions on a frame buffer.
 *
 *  Author -
 *	Paul J. Tanenbaum <pjt@brl.mil>
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

void PrintUsage();
void ArgCompat();

/* Program constants */
#define		High_Size	1024
#define		GRID_RHO_EPS	0.005
#define		GRID_THETA_EPS	0.2
#ifndef M_PI
#define		M_PI		3.14159265358979323846
#define		M_PI_2		1.57079632679489661923
#endif

/* Color[] indices */
#define		C_BKGRND	   0
#define		C_PERIM		   1
#define		C_RAMP		   2
#define		C_INTERIOR	   2
#define		C_BLACK		   3
#define		C_WHITE		   4
#define		C_RED		   5
#define		C_ORANGE	   6
#define		C_YELLOW	   7
#define		C_GREEN		   8
#define		C_BLUE		   9

/* Interior filling codes */
#define		BI_EMPTY	   0
#define		BI_CONSTANT	   1
#define		BI_RINGS	   2
#define		BI_WEDGES	   3
#define		BI_RAMP		   4

/* Half codes */
#define		H_LEFT		   1
#define		H_RIGHT		  -1

/* Macros */
#define		npf_index(a)	((int) ((a) / Quantum + 0.5))

#ifndef	bool
#define		bool		int
#endif
#ifndef TRUE
#define		TRUE		(1)
#endif
#ifndef FALSE
#define		FALSE		(0)
#endif

/* Global variables */
char		*ProgName;	/* To save argv[0] */
char		*ExplainOpts[] = {
    " -F fbfile specifies frame-buffer file\n", 
    " -N h      specifies FB height (pixels)\n",
    " -S s      specifies FB height and width (pixels)\n",
    " -W w      specifies FB width (pixels)\n",
    " -a u v    plots only the part of function between u and v (degrees)\n", 
    " -b r g b  sets background color (0 .. 255)\n", 
    " -c        clears FB to background color\n", 
    " -e        plots no function interior (useful with -p)\n", 
    " -g        plots no polar grid\n", 
    " -h        specifies high-resolution mode (same as -S 1024)\n", 
    " -i r g b  plots function with constant interior color (0 .. 255)\n", 
    " -l r g b  plots function with a linear ramp (0 .. 255)\n", 
    " -m        merges plot with current contents of FB\n", 
    " -n        no-warning mode.  Aborts if any irregular input\n", 
    " -o x y    translates plot (pixels)\n", 
    " -p r g b  plots function perimeter (0 .. 255)\n", 
    " -q q      sets angular quantum (degrees)\n", 
    " -r        specifies radian input (default is degrees)\n", 
    " -s s      scales plot (pixels)\n", 
    " -t t      rotates function (degrees)\n", 
    " -w        plots function with angular wedges\n", 
    " -z side   plots only one half of function ('l' or 'r')\n",
    ""
};
double		Deg2Rad;	/* Factor to convert degrees to radians */
RGBpixel	Color[] = {
    { 255, 255, 255 },		/* Background */
    {   0,   0,   0 },		/* Perimeter */
    {   0,   0,   0 },		/* Interior or ramp top */
    {   0,   0,   0 },		/* Black */
    { 255, 255, 255 },		/* White */
    { 255,   0,   0 },		/* Red */
    { 255, 128,   0 },		/* Orange */
    { 255, 255,   0 },		/* Yellow */
    { 100, 255,  75 },		/* Green */
    {   0,   0, 255 }		/* Blue */
};

main (argc, argv)

int	argc;
char	*argv[];

{
    bool	clr_fb = FALSE;	/* Clear the frame buffer first? */
    bool	draw_grid = TRUE;	/* Plot the plolar axes? */
    bool	merge = FALSE;	/* Overlay data on current contents FB? */
    bool	NoWarnings = FALSE;	/* Abort if any irregular input? */
    bool	perimeter = FALSE;	/* Plot perimeter of function? */
    char	*FB_Name;	/* Name of frame-buffer file */
    char	*FileName;	/* Name of input file */
    char	*Opt;		/* Used in parsing command-line options */
    double	angle_cvt;	/* Factor to convert input units to radians */
    double	arc_max = 360.0;/* Greatest value of theta to plot */
    double	arc_min = 0.0;	/* Least      "    "   "    "   "  */
    double	npf_rho;	/* Current entry in npf_tbl */
    double	npf_tbl[360];	/* The function (in (theta, rho) pairs) */
    double	rho, theta;	/* Polar coordinates of current pixel */
    double	twist = 0.0;	/* Clockwise rotation of image (in degrees) */
    int		ctr_x = -1;	/* X-coord of plot center (in pixels) */
    int		ctr_y = -1;	/* Y-  "    "         "     "   "     */
    int		fb_x_loc;	/* X-coord of lower-left of plot (in pixels) */
    int		fb_y_loc;	/* Y-  "    "   "     "   "   "    "   "     */
    int		Half = 0;	/* Plot only half of the function (L or R) */
    int		intensity;	/* RGBpixel component read from command line */
    int		Interior;	/* Manner of filling graph */
    int		LineLength;	/* Width of the plot (in pixels) */
    int		Quantum = 1;	/* Angular resolution of npf_tbl */
    int		fb_width = 0;	/* Width (in pixels) */
    int		fb_height = 0;	/* Height (in pixels) */
    int		theta_index;	/* Theta's index into npf_tbl */
    int		unit_r = -1;	/* Radius of unit circle (in pixels) */
    int		x, y;		/* Cartesian coordinates of current pixel */
    int		X, Y;		/* Translated pixel */
    FBIO	*fbPtr;		/* Pointer to the frame-buffer file */
    unsigned char *fbb;		/* Buffer for current line of frame buffer */
    unsigned char *fbbPtr;	/* Pointer to within fbb */

    void		(*Fill_Func)();
    void		Fill_Empty();
    void		Fill_Constant();
    void		Fill_Ramp();
    void		Fill_Wedges();
    void		Fill_Rings();

/* Initialize things */
    ProgName = *argv;
    angle_cvt = Deg2Rad = M_PI / 180.0;
    FB_Name = "";
    Interior = BI_RINGS;
    Color[C_BKGRND][RED] = Color[C_BKGRND][GRN] = Color[C_BKGRND][BLU] = 255;

/* Handle command-line options */
    while ((--argc > 0) && ((*++argv)[0] == '-'))
        for (Opt = argv[0] + 1; *Opt != '\0'; Opt++)
            switch (*Opt)
	    {
		case 'o':		/* Translate the plot */
		    if (argc < 3)
		    {
			(void) fputs("Illegal -o option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &ctr_x) != 1) || (ctr_x < 0))
		    {
			(void) fprintf(stderr, "Illegal -o value: %s\n",
			    argv[1]);
			goto error;
		    }
		    if ((sscanf(argv[2], "%d", &ctr_y) != 1) || (ctr_y < 0))
		    {
			(void) fprintf(stderr, "Illegal -o value: %s\n",
			    argv[2]);
			goto error;
		    }
		    argv += 2;
		    argc -= 2;
		    break;
		case 'W':		/* Scale the plot */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -W option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &fb_width) != 1)
			|| (fb_width < 0))
		    {
			(void) fprintf(stderr, "Illegal -W value: %s\n",
			    argv[1]);
			goto error;
		    }
		    argv++;
		    argc--;
		    break;
		case 'N':		/* Scale the plot */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -N option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &fb_height) != 1)
			|| (fb_height < 0))
		    {
			(void) fprintf(stderr, "Illegal -N value: %s\n",
			    argv[1]);
			goto error;
		    }
		    argv++;
		    argc--;
		    break;
		case 'S':		/* Scale the plot */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -S option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &fb_height) != 1)
			|| (fb_height < 0))
		    {
			(void) fprintf(stderr, "Illegal -S value: %s\n",
			    argv[1]);
			goto error;
		    }
		    fb_width = fb_height;
		    argv++;
		    argc--;
		    break;
		case 's':		/* Scale the plot */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -s option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &unit_r) != 1) || (unit_r <= 0))
		    {
			(void) fprintf(stderr, "Illegal -s value: %s\n",
			    argv[1]);
			goto error;
		    }
		    argv++;
		    argc--;
		    break;
		case 'q':		/* Angular resolution of npf_tbl */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -q option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &Quantum) != 1) ||
			(Quantum <= 0))
		    {
			(void) fprintf(stderr, "Illegal -q value: %s\n",
			    argv[1]);
			goto error;
		    }
		    argv++;
		    argc--;
		    break;
		case 'a':		/* Display an arc of the function */
		    if (argc < 3)
		    {
			(void) fputs("Illegal -a option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%F", &arc_min) != 1) || (arc_min < 0))
		    {
			(void) fprintf(stderr, "Illegal -a value: %s\n",
			    argv[1]);
			goto error;
		    }
		    if ((sscanf(argv[2], "%F", &arc_max) != 1) ||
			(arc_max > 360))
		    {
			(void) fprintf(stderr, "Illegal -a value: %s\n",
			    argv[2]);
			goto error;
		    }
		    if (arc_max < arc_min)
		    {
			(void) fprintf(stderr, "Illegal -a values: %g > %g\n",
			    arc_min, arc_max);
			goto error;
		    }
		    argv += 2;
		    argc -= 2;
		    break;
		case 't':		/* Rotate the function */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -t option\n", stderr);
			goto error;
		    }
		    if (sscanf(argv[1], "%F", &twist) != 1)
		    {
			(void) fprintf(stderr, "Illegal -t value: %s\n",
			    argv[1]);
			goto error;
		    }
		    twist *= Deg2Rad;
		    argv++;
		    argc--;
		    break;
		case 'z':		/* Plot only half of the function */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -z option\n", stderr);
			goto error;
		    }
		    if ((argv[1][0] != 'l') && (argv[1][0] != 'r') ||
			(argv[1][1] != '\0'))
		    {
			(void) fprintf(stderr, "Illegal -z value: %s\n",
			    argv[1]);
			goto error;
		    }
		    Half = (argv[1][0] == 'l') ? H_LEFT : H_RIGHT;
		    argv++;
		    argc--;
		    break;
		case 'b':		/* Specify the background color */
		    if (argc < 4)
		    {
			(void) fputs("Illegal -b option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -b value: %s\n",
			    argv[1]);
			goto error;
		    }
		    Color[C_BKGRND][RED] = intensity;
		    if ((sscanf(argv[2], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -b value: %s\n",
			    argv[2]);
			goto error;
		    }
		    Color[C_BKGRND][GRN] = intensity;
		    if ((sscanf(argv[3], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -b value: %s\n",
			    argv[3]);
			goto error;
		    }
		    Color[C_BKGRND][BLU] = intensity;
		    argv += 3;
		    argc -= 3;
		    break;
		case 'i':		/* Plot with one interior color */
		    ArgCompat(Interior);
		    Interior = BI_CONSTANT;
		    if (argc < 4)
		    {
			(void) fputs("Illegal -i option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -i value: %s\n",
			    *Opt, argv[1]);
			goto error;
		    }
		    Color[C_INTERIOR][RED] = intensity;
		    if ((sscanf(argv[2], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -i value: %s\n",
			    *Opt, argv[2]);
			goto error;
		    }
		    Color[C_INTERIOR][GRN] = intensity;
		    if ((sscanf(argv[3], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -i value: %s\n",
			    *Opt, argv[3]);
			goto error;
		    }
		    Color[C_INTERIOR][BLU] = intensity;
		    argv += 3;
		    argc -= 3;
		    break;
		case 'l':		/* Plot using a linear ramp */
		    ArgCompat(Interior);
		    Interior = BI_RAMP;
		    if (argc < 4)
		    {
			(void) fputs("Illegal -l option\n", stderr);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -l value: %s\n",
			    *Opt, argv[1]);
			goto error;
		    }
		    Color[C_RAMP][RED] = intensity;
		    if ((sscanf(argv[2], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -l value: %s\n",
			    *Opt, argv[2]);
			goto error;
		    }
		    Color[C_RAMP][GRN] = intensity;
		    if ((sscanf(argv[3], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -l value: %s\n",
			    *Opt, argv[3]);
			goto error;
		    }
		    Color[C_RAMP][BLU] = intensity;
		    argv += 3;
		    argc -= 3;
		    break;
		case 'm':		/* Merge data with contents of FB */
		    merge = TRUE;
		    break;
		case 'p':		/* Plot the perimeter */
		    perimeter = TRUE;
		    if (argc < 4)
		    {
			(void) fprintf(stderr, "Illegal -%c option\n", *Opt);
			goto error;
		    }
		    if ((sscanf(argv[1], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -%c value: %s\n",
			    *Opt, argv[1]);
			goto error;
		    }
		    Color[C_PERIM][RED] = intensity;
		    if ((sscanf(argv[2], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -%c value: %s\n",
			    *Opt, argv[2]);
			goto error;
		    }
		    Color[C_PERIM][GRN] = intensity;
		    if ((sscanf(argv[3], "%d", &intensity) != 1) ||
			(intensity < 0) || (intensity > 255))
		    {
			(void) fprintf(stderr, "Illegal -%c value: %s\n",
			    *Opt, argv[3]);
			goto error;
		    }
		    Color[C_PERIM][BLU] = intensity;
		    argv += 3;
		    argc -= 3;
		    break;
		case 'F':		/* Name of frame-buffer file */
		    if (argc < 2)
		    {
			(void) fputs("Illegal -F option\n", stderr);
			goto error;
		    }
		    FB_Name = argv[1];
		    argv++;
		    argc--;
		    break;
		case 'c':		/* Clear the FB initially */
		    clr_fb = TRUE;
		    break;
		case 'e':		/* Plot empty interior */
		    ArgCompat(Interior);
		    Interior = BI_EMPTY;
		    break;
		case 'g':		/* Do not plot axes */
		    draw_grid = FALSE;
		    break;
		case 'h':		/* High-res mode */
		    fb_width = fb_height = High_Size;
		    break;
		case 'r':		/* Input in radians, not degrees */
		    angle_cvt = 1.0;
		    break;
		case 'n':		/* Abort if any irregular input */
		    NoWarnings = TRUE;
		    break;
		case 'w':		/* Plot radius-colored wedges */
		    ArgCompat(Interior);
		    Interior = BI_WEDGES;
		    break;
		case '?':
		error:
		default:
		    PrintUsage(1);
		    (void) exit (*Opt != '?');
	    }

    /* Determine source of input */
    switch (argc)
    {
	case 0:			/* Read stdin */
	    FileName="";
	    break;
	case 1:			/* File name was given */
	    FileName=argv[0];
	    break;
	default:
	    PrintUsage(1);
	    (void) exit(1);
    }

    /* Fill npf_tbl from the input stream */
    if (LoadNPF (FileName, npf_tbl, Quantum, angle_cvt, arc_min, arc_max)
	&& NoWarnings)
	(void) exit(1);
    arc_min *= Deg2Rad;
    arc_max *= Deg2Rad;

    /* Prep the frame buffer */
    if ((fbPtr = fb_open(FB_Name, fb_width, fb_height)) == FBIO_NULL)
	(void) exit (1);
    fb_width = fb_getwidth(fbPtr);
    fb_height = fb_getheight(fbPtr);

    /* Determine both size of FB, and, where necessary, position and
     * scale of plot.
     */
    if (ctr_x == -1)
	ctr_x = fb_width / 2;
    if (ctr_y == -1)
	ctr_y = fb_height / 2;
    if (unit_r == -1)
	unit_r = (ctr_x < ctr_y) ? ctr_x : ctr_y;

    /* Ensure that location and size of the plot and size of
     * the FB are mutually compatible
     */
    if ((ctr_x + unit_r > fb_width) || (ctr_x < unit_r) ||
	(ctr_y + unit_r > fb_height) || (ctr_y < unit_r))
    {
	(void) fputs("Plot not entirely within frame buffer\n", stderr);
	(void) exit (1);
    }

    if (clr_fb)
	fb_clear(fbPtr, Color[C_BKGRND]);

    /* Decide how much of each FB line to process...
     * Half a plot means only half of each scan line,
     * UNLESS the function undergoes non-zero rotation
     */
    if (Half)
	LineLength = unit_r;
    else
	LineLength = 2 * unit_r;
    
    /* Decide where in the FB the lower-left corner of the output goes */
    if (Half == H_RIGHT)
	fb_x_loc = ctr_x;
    else
	fb_x_loc = ctr_x - unit_r;
    fb_y_loc = ctr_y - unit_r;

    /* Decide which Interior-filling routine to use */
    switch (Interior)
    {
	case BI_EMPTY:
	    Fill_Func = Fill_Empty;
	    break;
	case BI_CONSTANT:
	    Fill_Func = Fill_Constant;
	    break;
	case BI_RAMP:
	    Fill_Func = Fill_Ramp;
	    break;
	case BI_WEDGES:
	    Fill_Func = Fill_Wedges;
	    break;
	case BI_RINGS:
	    Fill_Func = Fill_Rings;
	    break;
	default:
	    (void) fputs("Bad interior.  Shouldn't happen\n",
			    stderr);
	    (void) exit (1);
	    break;
    }

    if ((fbb = (unsigned char *) malloc(fb_width * sizeof(RGBpixel))) == NULL)
    {
	(void) fputs("Ran out of memory\n", stderr);
	(void) exit (1);
    }

    /* Fill fbb */
    for (y = 0; y < 2 * unit_r; y++)
    {
	if (merge)
	    fb_read(fbPtr, fb_x_loc, fb_y_loc + y, fbb, LineLength);

	Y = y - unit_r;
	for (x = 0, fbbPtr = fbb; x <  LineLength; x++, fbbPtr+=sizeof(RGBpixel))
	{
	    X = x + fb_x_loc - ctr_x;

	    /* If this point is beyond the unit circle, then skip it */
	    if ((rho = sqrt((double) (X * X) + (double) (Y * Y))) >  unit_r)
	    {
		if (! merge)
		    COPYRGB(fbbPtr, Color[C_BKGRND])
		continue;
	    }

	    /* Determine this point's "azimuth" (i.e., theta) */
	    if (X == 0)
		theta = (Y > 0) ? M_PI : 0.0;
	    else
		theta = atan2((double) Y, (double) X) + M_PI_2;

	    /* If this point is in the wrong half of the plot, skip it */
 	    if (Half && (Half * X > 0))
 		continue;

	    /* Rotate the point for display */
	    theta += twist;
	    while (theta < 0)
		theta += M_PI * 2;
	    while (theta > M_PI * 2)
		theta -= M_PI * 2;

	    /* If this point is outside the arc of interest, skip it */
	    if ((theta < arc_min) || (theta > arc_max))
	    {
		if (! merge)
		    COPYRGB(fbbPtr, Color[C_BKGRND])
		continue;
	    }

	    /* Look up the value of the function for this value of theta */
	    theta_index = npf_index(theta / Deg2Rad);
	    npf_rho = npf_tbl[theta_index];

	    /*
	     * Do the actual filling
	     */

	    /* If this point is outside the function, color it BKGRND */
	    if (rho > npf_rho * unit_r)
	    {
		if (! merge)
		    COPYRGB(fbbPtr, Color[C_BKGRND])
	    }
	    else
	    {
		(*Fill_Func)(fbbPtr, rho, npf_rho, unit_r, merge);

		if (perimeter && (npf_rho - rho / unit_r < .02))
		    COPYRGB(fbbPtr, Color[C_PERIM])
	    }

	    if (draw_grid && OnGrid(theta, rho/unit_r))
		COPYRGB(fbbPtr, Color[C_BLACK])
	}
	fb_write(fbPtr, fb_x_loc, fb_y_loc + y, fbb, LineLength);
    }

    /* Wrap up */
    fb_close(fbPtr);
}

void
PrintUsage (ShoOpts)

bool	ShoOpts;

{
    char	**oPtr;		/* Pointer to option string */

    (void) fprintf(stderr, "Usage: '%s [options] [file]'\n", ProgName);
    if (ShoOpts)
    {
	(void) fputs("Options:\n", stderr);
	for (oPtr = ExplainOpts; **oPtr != '\0'; oPtr++)
	    (void) fputs(*oPtr, stderr);
    }
    else
	(void) fputs(" -? option for help\n", stderr);
}

LoadNPF (FileName, Table, Quantum, convert, arc_min, arc_max)

char	*FileName;	/* Name of input file */
double	*Table;		/* Location for storing function */
int	Quantum;	/* Angular resolution of Table (in degrees) */
double	convert;	/* Factor to convert input units to radians */
double	arc_min;	/* First angle of interest */
double	arc_max;	/* Last    "    "     "    */

{
    bool	Warnings = 0;	/* Have any warning messages been printed? */
    int		angle;
    int		gap_min;	/* Used in composing warnings */
    int		gap_max;	/*   "   "     "         "    */
    double	theta, rho;
    FILE	*fPtr;

    /* N.B. -	The possible values of Warnings and their meanings are:
     *
     *		Value		Meaning
     *		------------------------------------------------------
     *		  0     All is OK
     *		  1	Let the calling routine worry about it.
     *		  2	Fatal... bomb as soon as all input is checked.
     */

    /* Open the file, if necessary */
    if (*FileName == '\0')
	fPtr = stdin;
    else if ((fPtr = fopen(FileName, "r")) == NULL)
    {
	(void) fprintf(stderr, "%s:  Cannot open input file '%s'\n",
	    ProgName, FileName);
	(void) exit (1);
    }

    /* Initialize the table */
    for (angle = 0; angle < 360; angle += Quantum)
	*(Table + npf_index(angle)) = -1.0;
    
    /* Fill the table */
    while ((fscanf(fPtr, "%F", &theta) == 1) &&
	(fscanf(fPtr, "%F", &rho) == 1))
    {
	theta *= convert;
	if ((theta < 0.0) || (npf_index(theta / Deg2Rad) * Quantum > 360))
	{
	    (void) fprintf(stderr,
		"Fatal:  Theta out of range: %g %s\n",
		theta / convert, (convert == 1.0) ? "radians" : "degrees");
	    Warnings = 2;
	}
	if ((rho < 0.0) || (rho > 1.0))
	{
	    (void) fprintf(stderr, "Fatal:  Rho out of range: %g\n", rho);
	    Warnings = 2;
	}
	if (Warnings != 2)
	    *(Table + npf_index(theta / Deg2Rad)) = rho;
    }

    if (Warnings == 2)
	(void) exit (1);

    /* Check the table for completeness */
    gap_min = gap_max = -1;
    for (angle = floor(arc_min); angle < ceil(arc_max); angle+=Quantum)
    {
	if (*(Table + npf_index(angle)) == -1.0)
	{
	    *(Table + npf_index(angle)) = 0.0;
	    if (gap_min == -1)
		gap_min = angle;
	    gap_max = angle;
	    Warnings = 1;
	}
	else
	{
	    if (gap_min > -1)
	    {
		(void) fprintf(stderr, "Warning:  No entry in input for ");
		if (gap_max > gap_min)
		    (void) fprintf(stderr, "%d <= theta <= %d degrees\n",
			    gap_min, gap_max);
		else
		    (void) fprintf(stderr, "theta == %d degrees\n", gap_min);
	    }
	    gap_min = gap_max = -1;
	}
    }

    if (gap_min > -1)
    {
	(void) fprintf(stderr, "Warning:  No entry in input for ");
	if (gap_max > gap_min)
	    (void) fprintf(stderr, "%d <= theta <= %d degrees\n",
		    gap_min, gap_max);
	else
	    (void) fprintf(stderr, "theta == %d degrees\n", gap_min);
    }
    
     if (fPtr != stdin)
 	(void) fclose(fPtr);
     return(Warnings);
}

OnGrid (theta, rho)

double	theta;
double	rho;

{
    int		t;
    double	squeeze;	/* factor to squeeze the radii */

    theta /= Deg2Rad;
    squeeze = 1.0 / rho * GRID_THETA_EPS;

    /* Determine whether the point is on a radius */
    for (t = 0; t < 360; t += 30)
    {
	if (fabs(theta - t) < squeeze)
	    return(1);
    }

    /* Determine whether the point is on a circle */
    if ((((1.0 - rho) > 0) && ((1.0 - rho) < GRID_RHO_EPS)) ||
	(((0.8 - rho) > 0) && ((0.8 - rho) < GRID_RHO_EPS)) ||
	(((0.6 - rho) > 0) && ((0.6 - rho) < GRID_RHO_EPS)) ||
	(((0.4 - rho) > 0) && ((0.4 - rho) < GRID_RHO_EPS)) ||
	(((0.2 - rho) > 0) && ((0.2 - rho) < GRID_RHO_EPS)))
	return(1);

    return (0);
}

void
ArgCompat (Interior)

{
    if (Interior != BI_RINGS)
    {
        (void) fputs("Only one of -e, -i, -l, and -w may be specified\n",
		     stderr);
	(void) exit (1);
    }
}

void
Fill_Empty (fbbPtr, rho, npf_rho, unit_r, merge)

unsigned char	*fbbPtr;	/* Pointer to within fbb */
double		rho;		/* Radius of current pixel */
double		npf_rho;	/* Value of function at this theta */
int		unit_r;		/* Unit radius (in pixels) */
bool		merge;		/* Overlay onto current FB contents? */

{
    if (! merge)
	COPYRGB(fbbPtr, Color[C_BKGRND])
}

void
Fill_Constant (fbbPtr, rho, npf_rho, unit_r, merge)

unsigned char	*fbbPtr;	/* Pointer to within fbb */
double		rho;		/* Radius of current pixel */
double		npf_rho;	/* Value of function at this theta */
int		unit_r;		/* Unit radius (in pixels) */
bool		merge;		/* Overlay onto current FB contents? */

{
    COPYRGB(fbbPtr, Color[C_INTERIOR])
}

void
Fill_Ramp (fbbPtr, rho, npf_rho, unit_r, merge)

unsigned char	*fbbPtr;	/* Pointer to within fbb */
double		rho;		/* Radius of current pixel */
double		npf_rho;	/* Value of function at this theta */
int		unit_r;		/* Unit radius (in pixels) */
bool		merge;		/* Overlay onto current FB contents? */

{
    RGBpixel	ThisPix;	/* Ramped color for current pixel */

    ThisPix[RED] = ((double)Color[C_RAMP][RED]) * rho / unit_r +
	255 * (1 - rho / unit_r);
    ThisPix[GRN] = ((double)Color[C_RAMP][GRN]) * rho / unit_r +
	255 * (1 - rho / unit_r);
    ThisPix[BLU] = ((double)Color[C_RAMP][BLU]) * rho / unit_r +
	255 * (1 - rho / unit_r);
    COPYRGB(fbbPtr, ThisPix)
}

void
Fill_Wedges (fbbPtr, rho, npf_rho, unit_r, merge)

unsigned char	*fbbPtr;	/* Pointer to within fbb */
double		rho;		/* Radius of current pixel */
double		npf_rho;	/* Value of function at this theta */
int		unit_r;		/* Unit radius (in pixels) */
bool		merge;		/* Overlay onto current FB contents? */

{
    if (npf_rho > .8)
	COPYRGB(fbbPtr, Color[C_RED])
    else if (npf_rho > .6)
	COPYRGB(fbbPtr, Color[C_ORANGE])
    else if (npf_rho > .4)
	COPYRGB(fbbPtr, Color[C_YELLOW])
    else if (npf_rho > .2)
	COPYRGB(fbbPtr, Color[C_GREEN])
    else
	COPYRGB(fbbPtr, Color[C_BLUE])
}

void
Fill_Rings (fbbPtr, rho, npf_rho, unit_r, merge)

unsigned char	*fbbPtr;	/* Pointer to within fbb */
double		rho;		/* Radius of current pixel */
double		npf_rho;	/* Value of function at this theta */
int		unit_r;		/* Unit radius (in pixels) */
bool		merge;		/* Overlay onto current FB contents? */

{
    if (rho / unit_r > .8)
	COPYRGB(fbbPtr, Color[C_RED])
    else if (rho / unit_r > .6)
	COPYRGB(fbbPtr, Color[C_ORANGE])
    else if (rho / unit_r > .4)
	COPYRGB(fbbPtr, Color[C_YELLOW])
    else if (rho / unit_r > .2)
	COPYRGB(fbbPtr, Color[C_GREEN])
    else
	COPYRGB(fbbPtr, Color[C_BLUE])
}
