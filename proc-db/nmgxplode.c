/*	
 *
 *	Options
 *	h	help
 */
#include <stdio.h>

/* declarations to support use of getopt() system call */
char *options = "h";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ -%s ] file [ file ... ]\n",
			progname, options);
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
	struct db_i	*dbip;
	

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) >= ac) usage("database/solid specified");

	db_filename = argv[arg_index++];
	if ((rtip = rt_dirbuild(db_name, idbuf, sizeof(idbuf))) == RTI_NULL) {
		fprintf(stderr,"rt:  rt_dirbuild(%s) failure\n", db_filename);
		exit(2);
	}

	/* process each remaining argument */
	while (arg_index < ac )
		printf("%s", av[arg_index++]);
}
