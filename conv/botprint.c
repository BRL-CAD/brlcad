/*
 *	Options
 *	h	help
 */
#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

/* declarations to support use of getopt() system call */
char *options = "h";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ -%s ] [<] infile [> outfile]\n",
			progname, options);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char *av[])
{
	int  c;
	char *strrchr();

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(optind);
}

int 
process(FILE *inp, char *name)
{

	return 0;
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{
	int arg_count;
	FILE *inp;
	int status;
	char idbuf[132];		/* First ID record info */
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	struct rt_i *rtip;
	int i;

	arg_count = parse_args(ac, av);
	
	if ((ac - arg_count) < 2 ) {
	    fprintf(stderr, "usage: %s geom.g obj...\n", progname);
	    exit(-1);
	}

	RT_INIT_DB_INTERNAL(&intern);

	if ((rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf)))==RTI_NULL){
		fprintf(stderr,"rtexample: rt_dirbuild failure\n");
		exit(2);
	}

	arg_count++;

	/* process command line objects */
	for ( ; arg_count < ac ; arg_count++ ) {
	    struct directory *dirp;

	    if (!rt_db_lookup_internal(rtip->rti_dbip, av[arg_count], 
				       &dirp,
				       &intern, 
				       LOOKUP_QUIET,
				       &rt_uniresource)) {
		fprintf(stderr, "db_lookup failed on %s\n", av[arg_count]);
		continue;
	    }

	    if (intern.idb_minor_type != ID_BOT) {
		fprintf(stderr, "not a BOT\n");
		continue;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;

	    bu_log("%s:\n%d verticies\n", av[arg_count], bot->num_vertices);
	    for (i=0 ; i < bot->num_vertices ; i++) {
		fastf_t *v = &bot->vertices[i*3];
		bu_log("\t%g %g %g\n", V3ARGS(v));
	    }

	    bu_log("%d faces\n", bot->num_faces);
	    for (i=0 ; i < bot->num_faces ; i++) {
		int *f = &bot->faces[i*3];
		bu_log("\t%d %d %d\n", V3ARGS(f));
	    }

	}
}
