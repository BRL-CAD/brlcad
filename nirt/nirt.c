/*                        NIRT
 *
 *       This program is an Interactive Ray-Tracer
 *       
 *
 *       Written by:  Natalie L. Barker <barker@brl>
 *                    U.S. Army Ballistic Research Laboratory
 *
 *       Date:  Jan 90 -
 * 
 *       To compile:  /bin/cc -I/usr/include/brlcad nirt.c 
 *                    /usr/brlcad/lib/librt.a -lm -o nirt
 *
 *       To run:  nirt [-options] file.g object[s] 
 *
 *       Help menu:  nirt -?
 *
 */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./nirt.h"
#include "./usrfmt.h"

extern char	version[];		/* from vers.c */
extern void	cm_libdebug();

char		*db_name;	/* the name of the MGED file      */
com_table	ComTab[] =
		{
		    { "ae", az_el, "set/query azimuth and elevation", 
			"azimuth elevation" },
		    { "dir", dir_vect, "set/query direction vector", 
			"x-component y-component z-component" },
		    { "hv", grid_coor, "set/query gridplane coordinates",
			"horz vert [dist]" },
		    { "xyz", target_coor, "set/query target coordinates", 
			"X Y Z" },
		    { "s", shoot, "shoot a ray at the target" },
		    { "backout", backout, "back out of model" },
		    { "useair", use_air, "set/query use of air",
			"<0|1|2|...>" },
		    { "units", nirt_units, "set/query local units",
			"<mm|cm|m|in|ft>" },
		    { "fmt", format_output, "set/query output formats",
			"{rhpfmo} format item item ..." },
		    { "dest", direct_output, "set/query output destination",
			"file/pipe" },
		    { "statefile", state_file,
			"set/query name of state file", "file" },
		    { "dump", dump_state,
			"write current state of NIRT to the state file" },
		    { "load", load_state,
			"read new state for NIRT from the state file" },
		    { "print", print_item, "query an output item",
			"item" },
		    { "libdebug", cm_libdebug,
			"set/query librt debug flags", "hex_flag_value" },
		    { "!", sh_esc, "escape to the shell" },
		    { "q", quit, "quit" },
		    { "?", show_menu, "display this help menu" },
		    { 0 }
		};
int		do_backout = 0;			/* Backout before shooting? */
int		silent_flag = SILENT_UNSET;	/* Refrain from babbling? */
int		nirt_debug = 0;			/* Control of diagnostics */

/* Parallel structures needed for operation w/ and w/o air */
struct rt_i		*rti_tab[2];
struct rt_i		*rtip;
struct resource		res_tab[2];

struct application	ap;
struct nirt_obj		object_list = {"", 0};

struct script_rec
{
    struct bu_list	l;
    int			sr_type;	/* Direct or indirect */
    struct bu_vls	sr_script;	/* Literal or file name */
};
#define	SCRIPT_REC_NULL	((struct script_rec *) 0)
#define SCRIPT_REC_MAGIC	0x73637270
#define	sr_magic		l.magic

static void enqueue_script (qp, type, string)

struct bu_list	*qp;
int		type;
char		*string;	/* Literal or file name */

{
    struct script_rec	*srp;

    BU_CK_LIST_HEAD(qp);

    srp = (struct script_rec *)
	    bu_malloc(sizeof(struct script_rec), "script record");
    srp -> sr_magic = SCRIPT_REC_MAGIC;
    srp -> sr_type = type;
    bu_vls_init(&(srp -> sr_script));
    bu_vls_strcat(&(srp -> sr_script), string);

    BU_LIST_INSERT(qp, &(srp -> l));
}

static void show_scripts (sl, text)

struct bu_list	*sl;
char		*text;		/* for title line */

{
    int			i;
    struct script_rec	*srp;

    BU_CK_LIST_HEAD(sl);

    i = 0;
    bu_log("- - - - - - - The command-line scripts %s\n");
    for (BU_LIST_FOR(srp, script_rec, sl))
    {
	BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");

	bu_log("%d. script %s '%s'\n",
	    ++i,
	    (srp -> sr_type == READING_STRING) ? "string" :
	    (srp -> sr_type == READING_FILE) ? "file" : "???",
	    bu_vls_addr(&(srp -> sr_script)));
    }
    bu_log("- - - - - - - - - - - - - - - - - - - - - - - - - -\n");
}

static void free_script (srp)

struct script_rec	*srp;

{
    BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");

    bu_vls_free(&(srp -> sr_script));
    bu_free((genptr_t) srp, "script record");
}

static void run_scripts (sl)

struct bu_list	*sl;

{
    struct script_rec	*srp;
    char		*cp;
    FILE		*fPtr;

    if (nirt_debug & DEBUG_SCRIPTS)
	show_scripts(sl, "before running them");
    while (BU_LIST_WHILE(srp, script_rec, sl))
    {
	BU_LIST_DEQUEUE(&(srp -> l));
	BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");
	cp = bu_vls_addr(&(srp -> sr_script));
	if (nirt_debug & DEBUG_SCRIPTS)
	    bu_log("  Attempting to run %s '%s'\n",
		(srp -> sr_type == READING_STRING) ? "literal" :
		(srp -> sr_type == READING_FILE) ? "file" : "???",
		cp);
	switch (srp -> sr_type)
	{
	    case READING_STRING:
		interact(READING_STRING, cp);
		break;
	    case READING_FILE:
		if ((fPtr = fopen(cp, "r")) == NULL)
		    bu_log("Cannot open script file '%s'\n", cp);
		else
		{
		    interact(READING_FILE, fPtr);
		    fclose(fPtr);
		}
		break;
	    default:
		bu_log("%s:%d: script of type %d.  This shouldn't happen\n",
		    __FILE__, __LINE__, srp -> sr_type);
		exit (1);
	}
	free_script(srp);
    }
    if (nirt_debug & DEBUG_SCRIPTS)
	show_scripts(sl, "after running them");
}

main (argc, argv)
int argc;
char **argv;
{
    char                db_title[TITLE_LEN+1];/* title from MGED file      */
    char		*sp;
    extern char		*local_unit[];
    extern char		local_u_name[];
    extern double	base2local;
    extern double	local2base;
    FILE		*fPtr;
    int                 i;               /* counter                       */
    int			Ch;		/* Option name */
    int			mat_flag = 0;	/* Read matrix from stdin? */
    int			use_of_air = 0;
    outval		*vtp;
    struct bu_list	script_list;	/* For -e and -f options */
    struct script_rec	*srp;
    extern outval	ValTab[];
    extern int 		optind;		/* index from getopt(3C) */
    extern char		*optarg;	/* argument from getopt(3C) */

    /* FUNCTIONS */
    int                    if_overlap();    /* routine if you overlap         */
    int             	   if_hit();        /* routine if you hit target      */
    int             	   if_miss();       /* routine if you miss target     */
    void                   do_rt_gettree();
    void                   printusage();
    void		   grid2targ();
    void		   targ2grid();
    void		   ae2dir();
    void		   dir2ae();
    void	           set_diameter();
    int	           	   str_dbl();	
    void		   az_el();
    void		   sh_esc();
    void		   grid_coor();
    void		   target_coor();
    void		   dir_vect();
    void		   backout();
    void		   quit();
    void		   show_menu();
    void		   print_item();
    void		   shoot();

    BU_LIST_INIT(&script_list);

    /* Handle command-line options */
    while ((Ch = getopt(argc, argv, OPT_STRING)) != EOF)
        switch (Ch)
        {
	    case 'b':
		do_backout = 1;
		break;
	    case 'E':
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "before erasure");
		while (BU_LIST_WHILE(srp, script_rec, &script_list))
		{
		    BU_LIST_DEQUEUE(&(srp -> l));
		    free_script(srp);
		}
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after erasure");
		break;
	    case 'e':
		enqueue_script(&script_list, READING_STRING, optarg);
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after enqueueing a literal");
		break;
	    case 'f':
		enqueue_script(&script_list, READING_FILE, optarg);
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after enqueueing a file name");
		break;
	    case 'M':
		mat_flag = 1;
		break;
	    case 's':
		silent_flag = SILENT_YES;	/* Positively yes */
		break;
	    case 'v':
		silent_flag = SILENT_NO;	/* Positively no */
		break;
            case 'x':
		sscanf( optarg, "%x", &rt_g.debug );
		break;
            case 'X':
		sscanf( optarg, "%x", &nirt_debug );
		break;
            case 'u':
                if (sscanf(optarg, "%d", &use_of_air) != 1)
                {
                    (void) fprintf(stderr,
                        "Illegal use-air specification: '%s'\n", optarg);
                    exit (1);
                }
                break;
            case '?':
	    default:
                printusage();
                exit (Ch != '?');
        }
    if (argc - optind < 2)
    {
	printusage();
	exit (1);
    }
    if (isatty(0))
    {
	if (silent_flag != SILENT_YES)
	    silent_flag = SILENT_NO;
    }
    else	/* stdin is not a TTY */
    {
	if (silent_flag != SILENT_NO)
	    silent_flag = SILENT_YES;
    }
    if (silent_flag != SILENT_YES)
	(void) fputs(version + 5, stdout);	/* skip @(#) */

    if (use_of_air && (use_of_air != 1))
    {
	fprintf(stderr,
	    "Warning: useair=%d specified, will set to 1\n", use_of_air);
	use_of_air = 1;
    }
    db_name = argv[optind];

    /* build directory for target object */
    if (silent_flag != SILENT_YES)
    {
	printf("Database file:  '%s'\n", db_name);
	printf("Building the directory...");
    }
    if ((rtip = rt_dirbuild( db_name , db_title, TITLE_LEN )) == RTI_NULL)
    {
	fflush(stdout);
	fprintf(stderr, "Could not load file %s\n", db_name);
	exit(1);
    }
    rti_tab[use_of_air] = rtip;
    rti_tab[1 - use_of_air] = RTI_NULL;
    rtip -> useair = use_of_air;

    if (silent_flag != SILENT_YES)
	printf("\nPrepping the geometry...");
    while (++optind < argc)    /* prepare the objects that are to be included */
	do_rt_gettree( rtip, argv[optind], 1 );
 
    /* Initialize the table of resource structures */
    res_tab[use_of_air].re_magic =
	(res_tab[1 - use_of_air].re_magic = RESOURCE_MAGIC);

    /* initialization of the application structure */
    ap.a_hit = if_hit;        /* branch to if_hit routine            */
    ap.a_miss = if_miss;      /* branch to if_miss routine           */
    ap.a_overlap = if_overlap;/* branch to if_overlap routine        */
    ap.a_onehit = 0;          /* continue through shotline after hit */
    ap.a_resource = &res_tab[use_of_air];
    ap.a_purpose = "NIRT ray";
    ap.a_rt_i = rtip;         /* rt_i pointer                        */
    ap.a_zero1 = 0;           /* sanity check, sayth raytrace.h      */
    ap.a_zero2 = 0;           /* sanity check, sayth raytrace.h      */

    rt_prep( rtip );

    /* initialize variables */
    azimuth() = 0.0;
    elevation() = 0.0;
    direct(X) = -1.0; 
    direct(Y) = 0.0;
    direct(Z) = 0.0;
    grid(HORZ) = 0.0;
    grid(VERT) = 0.0;
    grid(DIST) = 0.0;
    grid2targ();
    set_diameter(rtip);

    /* initialize the output specification */
    default_ospec();

    /* initialize NIRT's local units */
    strncpy(local_u_name, local_unit[rtip -> rti_dbip -> dbi_localunit], 64);
    base2local = rtip -> rti_dbip -> dbi_base2local;
    local2base = rtip -> rti_dbip -> dbi_local2base;

    if (silent_flag != SILENT_YES)
    {
	printf("Database title: '%s'\n", db_title);
	printf("Database units: '%s'\n", local_u_name);
	printf("model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
	    rtip -> mdl_min[X] * base2local,
	    rtip -> mdl_min[Y] * base2local,
	    rtip -> mdl_min[Z] * base2local,
	    rtip -> mdl_max[X] * base2local,
	    rtip -> mdl_max[Y] * base2local,
	    rtip -> mdl_max[Z] * base2local);
    }

    /* Run the run-time configuration file, if it exists */
    if ((fPtr = fopenrc()) != NULL)
    {
	interact(READING_FILE, fPtr);
	fclose(fPtr);
    }

    /*	Run all scripts specified on the command line */
    run_scripts(&script_list);

    /* Perform the user interface */
    if (mat_flag)
    {
	read_mat();
	exit (0);
    }
    else
	interact(READING_FILE, stdin);
}
 
char	usage[] = "\
Usage: 'nirt [options] model.g objects...'\n\
Options:\n\
 -b        back out of geometry before first shot\n\
 -e script run script before interacting\n\
 -f sfile  run script sfile before interacting\n\
 -M        read matrix, cmds on stdin\n\
 -s        run in short (non-verbose) mode\n\
 -u n      set use_air=n (default 0)\n\
 -v        run in verbose mode\n\
 -x v      set librt(3) diagnostic flag=v\n\
 -X v      set nirt diagnostic flag=v\n\
";

void printusage() 
{
    (void) fputs(usage, stderr);
}

void do_rt_gettree(rip, object_name, save)
struct rt_i	*rip;
char 		*object_name;
int		save;		/* Add object_name to object_list? */
{
    static struct nirt_obj	*op = &object_list;

    if (rt_gettree( rip, object_name ) == -1)
    {
	fflush(stdout);
	fprintf (stderr, "rt_gettree() could not preprocess object '%s'\n", 
	    object_name);
	exit(1);
    }
    if (save)
    {
	if (op == NULL)
	{
	    fputs("Ran out of memory\n", stderr);
	    exit (1);
	}
	op -> obj_name = object_name;
	op -> obj_next = (struct nirt_obj *)
				rt_malloc(sizeof(struct nirt_obj), "obj_next");
	op = op -> obj_next;
	if (op != NULL)
	    op -> obj_next = NULL;
    }
    if (silent_flag != SILENT_YES)
	printf("\nObject '%s' processed\n", object_name);
}
