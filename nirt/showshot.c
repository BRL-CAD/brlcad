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
 */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <strings.h>
#include <machine.h>
#include <vmath.h>

#define		BUF_LEN		128
#define		OPT_STRING	"dr:?"
#define		RAY_COLOR	"255 255 0"
#define		print_usage()	fputs("Usage: 'show-shot [-d] [-r rad]'\n",\
					stderr)

main (argc, argv)

int	argc;
char	**argv;

{
    char	buf[BUF_LEN];	/* Contents of current input line */
    char	*bp;		/* Pointer into buf */
    char	gname[BUF_LEN];	/* Name of the group */
    char	rname[BUF_LEN];	/* Name of current region */
    char	rayname[BUF_LEN];/* Name of ray */
    fastf_t	ray_radius=1.0;	/* Thickness of the RCC */
    point_t	entryp;		/* Ray's entry into current region */
    point_t	exitp;		/* Ray's exit from current region */
    point_t	first_entryp;	/* Ray's entry into the entire geometry */
    int		debug = 0;	/* Show actions instead of performing them? */
    int		i;		/* Index into rname */
    int		line_nm = 0;	/* Number of current line of input */
    int		opt;		/* Command-line option returned by getopt() */
    int		pid = getpid();	/* Process ID for unique group name */

    extern char *optarg;
    extern int  optind, opterr;

    int         getopt();

    /* Handle command-line options */
    while ((opt = getopt(argc, argv, OPT_STRING)) != -1)
	switch (opt)
	{
	    case 'd':
		debug = 1;
		break;
	    case 'r':
		if (sscanf(optarg, "%F", &ray_radius) != 1)
		{
		    fprintf(stderr, "Illegal radius: '%s'\n", optarg);
		    exit (1);
		}
		break;
	    case '?':
		print_usage();
		exit (1);
	}
    
    /* Ensure proper command-line syntax */
    if (! debug && (argc != optind))
    {
	print_usage();
	exit (1);
    }

    /* Construct the name of the group in which the results will be put */
    sprintf(rayname, "nirt.ray.%d", pid);
    printf("in %s.s sph 0 0 0 1\n", rayname);
    printf("r %s.r u %s.s\n", rayname, rayname);
    printf("mater %s.r\n\n\n%s\n\n", rayname, RAY_COLOR);
    sprintf(gname, "nirt.%d", pid);
    printf("g %s", gname);

    /* Read the input */
    while (fgets(buf, BUF_LEN, stdin) != NULL)
    {
	char	*nlp;	/* Location of newline in buf */

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
	    fprintf(stderr, "Illegal data on line %d: '%s'\n", line_nm, bp);
	    exit (1);
	}

	printf(" %s", rname);
#if 0
	printf("entered %s at (%f, %f, %f) and exited at (%f, %f, %f)\n",
	    rname, entryp[X], entryp[Y], entryp[Z], exitp[X], exitp[Y], exitp[Z]);
#endif
	
	if (line_nm == 1)
	{
	    VMOVE(first_entryp, entryp);
	}
    }
    if (! feof(stdin))
    {
	fputs("Error from fgets().  This shouldn't happen", stderr);
	exit (1);
    }

    printf("\nkill %s.s\nin %s.s rcc\n\t%f %f %f\n\t%f %f %f\n\t%f\n",
	rayname, rayname,
	first_entryp[X], first_entryp[Y], first_entryp[Z],
	exitp[X] - first_entryp[X],
	exitp[Y] - first_entryp[Y],
	exitp[Z] - first_entryp[Z],
	ray_radius);
    printf("g %s %s.r\n", gname, rayname);
    fprintf(stderr, "Group is '%s'\n", gname);
}
