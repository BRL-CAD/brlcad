/*	
 *
 *	Options
 *	h	help
 */
#include <stdio.h>
#include <math.h>
/* declarations to support use of getopt() system call */
char *options = "w:h:d:W:H:sn:t:Du:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
double brick_width=9.0;
double brick_height=3.0;
double brick_depth=4.0;
double wall_width=0.0;
double wall_height=0.0;
char *brick_name="brick";
int standalone=0;
double tol=0.125;	/* minimum mortar thickness in mm */
double tol2;		/* minimum brick dimension allowed */
int debug=0;
double units_conv=25.4;	/* default to inches */


/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, 
"Usage: %s -w brick_width -h brick_height -d brick_depth -n brick_name\n%s\n",
			progname, 
"  -W wall_width -H wall_height\n  [-u units] [ -s(tandalone) ] [-t tolerance] > mged_commands \n");
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(ac, av)
int ac;
char *av[];
{
	int  c;
	char *strrchr();
	double d;

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'u'	: units_conv = rt_units_conversion(optarg); break;
		case 'D'	: debug = !debug; break;
		case 't'	: if ((d=atof(optarg)) != 0.0) tol = d;
		case 'w'	: if ((d=atof(optarg)) != 0.0) brick_width = d;
				break;
		case 'h'	: if ((d=atof(optarg)) != 0.0) brick_height = d;
				break;
		case 'd'	: if ((d=atof(optarg)) != 0.0) brick_depth = d;
				break;
		case 'W'	: if ((d=atof(optarg)) != 0.0) wall_width = d;
				break;
		case 'H'	: if ((d=atof(optarg)) != 0.0) wall_height = d;
				break;
		case 'n'	: brick_name = optarg; break;
		case 's'	: standalone = !standalone; break;
		case '?'	:
		default		: usage("bad command line option"); break;
		}

	brick_width *= units_conv;
	brick_height *= units_conv;
	brick_depth *= units_conv;

	wall_width *= units_conv;
	wall_height *= units_conv;

	tol *= units_conv;
	tol2 = tol * tol;

	if (brick_width <= tol2)
		usage("brick width too small\n");

	if (brick_height <= tol2)
		usage("brick height too small\n");
	
	if (brick_depth <= tol2)
		usage("brick depth too small\n");
		
	if (wall_width < brick_width)
		usage("wall width < brick width\n");
	
	if (wall_height < brick_height)
		usage("wall height < brick height\n");
	
	if (brick_name == (char *)NULL || *brick_name == '\0')
	    	usage("bad or no brick name\n");

	return(optind);
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	int arg_index;
	int horiz_bricks;
	int vert_bricks;
	int row;
	int brick;
	int offset;
	double xstart;
	double zstart;
	double horiz_mortar;
	double vert_mortar;


	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) < ac) usage((char *)NULL);


	/* build the wall */

	if (debug)
	    (void)fprintf(stderr,
		"bw %g  bh %g  bd %g  ww %g  wh %g  bn\"%s\"\n",
		brick_width, brick_height, brick_depth,
		wall_width, wall_height, brick_name);


	horiz_bricks = (int)(wall_width / brick_width);

	/* leave room for sideways brick at one end */
	while (horiz_bricks * brick_width + brick_depth > wall_width &&
	    horiz_bricks > 0)
		--horiz_bricks;

	if (horiz_bricks <= 0) {
		(void)fprintf(stderr, "wall not wide enough for brick\n");
		return(-1);
	}

	if (standalone) {
		horiz_mortar =
		    (wall_width - horiz_bricks * brick_width) /
					(horiz_bricks - 1);
	} else {
		horiz_mortar =
		    (wall_width - (horiz_bricks * brick_width + brick_depth))/
					horiz_bricks;
	}

	vert_bricks = (int)(wall_height / brick_height);

	vert_mortar = (wall_height - vert_bricks * brick_height) /
				(vert_bricks - 1);


	(void)fprintf(stderr, "wall %d bricks wide,  %d bricks high\n",
		horiz_bricks, vert_bricks);
	(void)fprintf(stderr, "distance between adjacent bricks %g\n",
		horiz_mortar);
	(void)fprintf(stderr, "distance between layers of bricks %g\n",
		vert_mortar);


	/* print the commands to make the wall */

	(void)fprintf(stdout, "\n\n");

	for (row=0 ; row < vert_bricks ; ++row) {

		if (row % 2) offset = brick_depth + horiz_mortar;
		else offset = 0;


		for (brick=0 ; brick < horiz_bricks ; ++ brick) {
			xstart = brick * brick_width +
				 brick * horiz_mortar + offset;
			zstart = row * brick_height + row * vert_mortar;

			(void)fprintf(stdout, 
				"in s.%s.%d.%d rpp %g %g  %g %g  %g %g\n",
					brick_name, row, brick,
					xstart, xstart + brick_width,
					0, brick_depth,
					zstart, zstart + brick_height);
			
			(void)fprintf(stdout, 
				"r r.%s.%d.%d u s.%s.%d.%d\n",
					brick_name, row, brick,
					brick_name, row, brick);

			(void)fprintf(stdout, "g g.row.%d r.%s.%d.%d\n",
				row, brick_name, row, brick);
		}

		(void)fprintf(stdout, "g g.wall g.row.%d\n", row);
		
	}

	return(0);
}
