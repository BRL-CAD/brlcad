/*			S H O W - S H O T . C
 *
 *	Filter output from NIRT(1) to generate an MGED(1) object
 *	representing a shotline through some geometry.
 *
 *		Written by:	Paul Tanenbaum
 *		Date:		4 December 1990
 *
 *		To compile:	cc -I/usr/include/brlcad source.c
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	    15 Jan 91	Eliminated the mandatory argument model.g
 *			from the command line.  The database name
 *			wasn't used anyway!
 *	    14 Feb 91	Added check to ensure that the names of
 *			the objects created won't exceed NAMESIZE,
 *			and added the '-n name' option.  Eliminated
 *			the '-d' debug option.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <machine.h>
#include <vmath.h>
#include <db.h>

#define		BUF_LEN		128
#define		OPT_STRING	"n:r:?"
#define		RAY_COLOR	"255 255 0"
#define		print_usage()	(void) fputs("Usage: 'show-shot [-n name] [-r radius]'\n",\
					stderr)

main (argc, argv)

int	argc;
char	**argv;

{
    char	buf[BUF_LEN];	/* Contents of current input line */
    char	*bp;		/* Pointer into buf */
    char	*nlp;		/* Location of newline in buf */
    char	rname[BUF_LEN];	/* Name of current region */
    char	rayname[BUF_LEN];	/* Name of ray */
    fastf_t	ray_radius = 1.0;	/* Thickness of the RCC */
    point_t	entryp;		/* Ray's entry into current region */
    point_t	exitp;		/* Ray's exit from current region */
    point_t	first_entryp;	/* Ray's entry into the entire geometry */
    int		i;		/* Index into rname */
    int		line_nm = 0;	/* Number of current line of input */
    int		opt;		/* Command-line option returned by getopt() */
    int		pid = getpid();	/* Process ID for unique group name */

    extern char *optarg;
    extern int  optind, opterr;

    int         getopt();

    *rayname = '\0';
    /* Handle command-line options */
    while ((opt = getopt(argc, argv, OPT_STRING)) != -1)
	switch (opt)
	{
	    case 'n':
		(void) strcpy(rayname, optarg);
		break;
	    case 'r':
		if (sscanf(optarg, "%F", &ray_radius) != 1)
		{
		    (void) fprintf(stderr, "Illegal radius: '%s'\n", optarg);
		    exit (1);
		}
		break;
	    case '?':
		print_usage();
		exit (1);
	}
    
    /* Ensure proper command-line syntax */
    if (optind != argc)
    {
	print_usage();
	exit (1);
    }

    /* Construct the names of the objects to add to the database */
    if (*rayname == '\0')	/* Was one given on command line? */
	(void) sprintf(rayname, "ray.%d", pid);
    if (strlen(rayname) > NAMESIZE - 3)
    {
	(void) fprintf(stderr,
	    "Name '%s.s' for ray solid may not exceed %d characters\n",
	    rayname, NAMESIZE - 1);
	(void) fputs("Use the '-n name' option to specify a different name\n",
	    stderr);
	exit (1);
    }
    (void) printf("in %s.s sph 0 0 0 1\n", rayname);
    (void) printf("r %s.r u %s.s\n", rayname, rayname);
    (void) printf("mater %s.r\n\n\n%s\n\n", rayname, RAY_COLOR);
    (void) printf("g %s", rayname);

    /* Read the input */
    while (fgets(buf, BUF_LEN, stdin) != NULL)
    {
	++line_nm;
	bp = buf;
	if ((nlp = index(bp, '\n')) != 0)
	    *nlp = '\0';

	/* Skip initial white space */
	while (isspace(*bp))
	    ++bp;
	
	/* Get region name */
	for (i = 0; ! isspace(*bp) && (*bp != '\0'); ++i, ++bp)
	    rname[i] = *bp;
	rname[i] = '\0';
	
	/* Read entry and exit coordinates for this partition */
	if (sscanf(bp, "%F%F%F%F%F%F",
		&entryp[X], &entryp[Y], &entryp[Z],
		&exitp[X], &exitp[Y], &exitp[Z])
	    != 6)
	{
	    (void) fprintf(stderr,
		"Illegal data on line %d: '%s'\n", line_nm, bp);
	    exit (1);
	}

	(void) printf(" %s", rname);

	if (line_nm == 1)
	{
	    VMOVE(first_entryp, entryp);
	}
    }
    if (! feof(stdin))
    {
	(void) fputs("Error from fgets().  This shouldn't happen", stderr);
	exit (1);
    }

    (void) printf("\nkill %s.s\nin %s.s rcc\n\t%f %f %f\n\t%f %f %f\n\t%f\n",
	rayname, rayname,
	first_entryp[X], first_entryp[Y], first_entryp[Z],
	exitp[X] - first_entryp[X],
	exitp[Y] - first_entryp[Y],
	exitp[Z] - first_entryp[Z],
	ray_radius);
    (void) printf("g %s %s.r\n", rayname, rayname);
    (void) fprintf(stderr, "Group is '%s'\n", rayname);
}
