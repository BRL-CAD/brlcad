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

/*	INCLUDES	*/
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <machine.h>
#include <vmath.h>
#include <raytrace.h>
#include "nirt.h"
#include "usrfmt.h"

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
		    { "!", sh_esc, "escape to the shell" },
		    { "q", quit, "quit" },
		    { "?", show_menu, "display this help menu" },
		    { 0 }
		};

char                     *progname;
struct rt_i              *rtip;           /* pointer to an rt_i structure   */
struct application       ap;

main (argc, argv)
int argc;
char **argv;
{
    char                *db_name;        /* the name of the MGED file      */
    char                db_title[TITLE_LEN+1];/* title from MGED file      */
    extern char		*local_unit[];
    extern char		local_u_name[];
    extern double	base2local;
    extern double	local2base;
    FILE		*fPtr;
    int                 i;               /* counter                       */
    outval		*vtp;
    extern outval	ValTab[];

    /* FUNCTIONS */
    int                    if_overlap();    /* routine if you overlap         */
    int             	   if_hit();        /* routine if you hit target      */
    int             	   if_miss();       /* routine if you miss target     */
    void                   interact();      /* handle user interaction        */
    void                   do_rt_gettree();
    void                   printusage();
    void		   grid2targ();
    void		   targ2grid();
    void		   ae2dir();
    void		   dir2ae();
    double	           dist_default();  /* computes grid[DIST] default val*/
    int	           	   str_dbl();	
    void		   az_el();
    void		   sh_esc();
    void		   grid_coor();
    void		   target_coor();
    void		   dir_vect();
    void		   quit();
    void		   show_menu();
    void		   print_item();
    void		   shoot();

    progname = *argv++;
    if ((--argc < 2) || (strcmp(*argv, "-?") == 0))
    {
	printusage();
	exit(1);
    }
    db_name = *argv;

    /* build directory for target object */
    printf("Database file:  '%s'\n", db_name);
    printf("Building the directory...");
    if ((rtip = rt_dirbuild( db_name , db_title, TITLE_LEN )) == RTI_NULL)
    {
	fflush(stdout);
	fprintf(stderr, "Could not load file %s\n", db_name);
	printusage();
	exit(1);
    }
    putchar('\n');
    /*
     *	Here useair is hardwired to 0.
     *	A command-line option should be added to allow the user to
     *	specify that he wants to use air.  Eventualy it would be
     *	nice to allow interactive toggling of useair, but that
     *	would require calling rt_gettree() and rt_prep() again!
     */
    rtip -> useair = 1;

    printf("Prepping the geometry...");
    while (--argc > 0)    /* prepare the objects that are to be included */
	do_rt_gettree( rtip, *(++argv) );
 
    /* initialization of the application structure */
    ap.a_hit = if_hit;        /* branch to if_hit routine            */
    ap.a_miss = if_miss;      /* branch to if_miss routine           */
    ap.a_overlap = if_overlap;/* branch to if_overlap routine        */
    ap.a_onehit = 0;          /* continue through shotline after hit */
    ap.a_resource = 0;
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
    grid(DIST) = dist_default();     /* extreme of the target */
    grid(VERT) = 0.0;
    grid(DIST) = 0.0;
    grid2targ();

    /* initialize the output specification */
    default_ospec();

    /* initialize NIRT's local units */
    strncpy(local_u_name, local_unit[rtip -> rti_dbip -> dbi_localunit], 64);
    base2local = rtip -> rti_dbip -> dbi_base2local;
    local2base = rtip -> rti_dbip -> dbi_local2base;

    /* Run the run-time configuration file, if it exists */
    if ((fPtr = fopenrc()) != NULL)
    {
	interact(fPtr);
	fclose(fPtr);
    }

    printf("Database title: '%s'\n", db_title);
    printf("Database units: '%s'\n", local_u_name);
    printf("model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
	rtip -> mdl_min[X] * base2local,
	rtip -> mdl_min[Y] * base2local,
	rtip -> mdl_min[Z] * base2local,
	rtip -> mdl_max[X] * base2local,
	rtip -> mdl_max[Y] * base2local,
	rtip -> mdl_max[Z] * base2local);

    /* Perform the user interface */
    interact(stdin);
}
 
void printusage() 
{
    fprintf (stderr, "Usage:   %s file.g object[s]\n", progname); 
}

void do_rt_gettree(rip, object_name)
struct rt_i	*rip;
char 		*object_name;
{
    /* All objects (groups and regions) which are to be included in the */
    /* description to be raytraced must be preprocessed with rt_gettree */
    if (rt_gettree( rip, object_name ) == -1)
    {
	fflush(stdout);
	fprintf (stderr, "rt_gettree() could not preprocess object '%s'\n", 
	    object_name);
	printusage();
	exit(1);
    }
    putchar('\n');
    printf("Object '%s' processed\n", object_name);
}
