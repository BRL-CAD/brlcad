#include <stdlib.h>
#include <stdio.h>
#include "../express/express.h"
#include "exppp.h"

static void
exppp_usage()
{
        fprintf(stderr,"usage: %s [-a|A] [-v] [-d #] [-p <object_type>] {-w|-i <warning>} express_file\n",EXPRESSprogram_name);
	fprintf(stderr,"where\t-a or -A causes output to be alphabetized\n");
	fprintf(stderr,"where\t-v produces a version description\n");
	fprintf(stderr,"\t-d turns on debugging (\"-d 0\" describes this further\n");
	fprintf(stderr,"\t-p turns on printing when processing certain objects (see below)\n");
	fprintf(stderr,"\t-w warning enable\n");
	fprintf(stderr,"\t-i warning ignore\n");
	fprintf(stderr,"and <warning> is one of:\n");
	fprintf(stderr,"\tnone\n\tall\n");
	LISTdo(ERRORwarnings, opt, Error_Warning)
		fprintf(stderr,"\t%s\n",opt->name);
	LISTod
	fprintf(stderr,"and <object_type> is one or more of:\n");
	fprintf(stderr,"	e	entity\n");
	fprintf(stderr,"	p	procedure\n");
	fprintf(stderr,"	r	rule\n");
	fprintf(stderr,"	f	function\n");
	fprintf(stderr,"	t	type\n");
	fprintf(stderr,"	s	schema or file\n");
	fprintf(stderr,"	#	pass #\n");
	fprintf(stderr,"	E	everything (all of the above)\n");
	exit(2);
}

int Handle_Exppp_Args(int i, char *arg)
{
    if( ((char)i == 'a') || ((char)i == 'A'))
      exppp_alphabetize = True; 
   else
      exppp_alphabetize = False;
    return 0;
}

void
EXPRESSinit_init()
{
	EXPRESSbackend = EXPRESSout;
	ERRORusage_function = exppp_usage;
	strcat(EXPRESSgetopt_options, "aA");
	EXPRESSgetopt = Handle_Exppp_Args;
}
