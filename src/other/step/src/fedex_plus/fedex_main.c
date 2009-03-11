static char rcsid[] = "$Id: fedex_main.c,v 3.0.1.3 1997/11/05 23:12:18 sauderd DP3.1 $";

/************************************************************************
** Driver for Fed-X Express parser.
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: fedex_main.c,v $
 * Revision 3.0.1.3  1997/11/05 23:12:18  sauderd
 * Adding a new state DP3.1 and associated revision
 *
 * Revision 3.0.1.2  1997/09/26 15:59:10  sauderd
 * Finished implementing the -a option (changed from -e) to generate the early
 * bound access functions the old way. Implemented the change to generate them
 * the new correct way by default.
 *
 * Revision 3.0.1.1  1997/09/18 21:18:41  sauderd
 * Added a -e or -E option to generate attribute get and put functions the old
 * way (without an underscore). It sets the variable old_accessors. This doesn't
 * currently do anything. It needs to be implemented to generate attr funcs
 * correctly.
 *
 * Revision 3.0.1.0  1997/04/16 19:29:03  dar
 * Setting the first branch
 *
 * Revision 3.0  1997/04/16  19:29:02  dar
 * STEP Class Library Pre Release 3.0
 *
 * Revision 2.1.0.5  1997/03/11  15:33:59  sauderd
 * Changed code so that if fedex_plus is passed the -c or -C option it would
 * generate implementation objects for Orbix (CORBA). Look for code that is
 * inside stmts such as if(corba_binding)
 *
 * Revision 2.1.0.4  1996/09/25 22:56:55  sauderd
 * Added a command line argument for logging SCL use. The option added is -l or
 * -L. It also says the option in the usage stmt when you run fedex_plus without
 * an argument. Added the options to the EXPRESSgetopt_options string.
 *
 * Revision 2.1.0.3  1996/06/18 18:14:17  sauderd
 * Changed the line that gets printed when you run fedex_plus with no
 * arguments to include the option for single inheritance.
 *
 * Revision 2.1.0.2  1995/05/19 22:40:03  sauderd
 * Added a command line argument -s or -S for generating code based on the old
 * method as opposed to the new method of multiple inheritance.
 *
 * Revision 2.1.0.1  1995/05/16  19:52:18  lubell
 * setting state to dp21
 *
 * Revision 2.1.0.0  1995/05/12  18:53:48  lubell
 * setting branch
 *
 * Revision 2.1  1995/05/12  18:53:47  lubell
 * changing version to 2.1
 *
 * Revision 1.7  1995/03/16  20:58:50  sauderd
 * ? changes.
 *
 * Revision 1.6  1992/09/29  15:46:55  libes
 * added messages for KC
 *
 * Revision 1.5  1992/08/27  23:28:52  libes
 * moved Descriptor "new"s to precede assignments
 * punted SELECT type
 *
 * Revision 1.4  1992/08/19  18:49:59  libes
 * registry support
 *
 * Revision 1.3  1992/06/05  19:55:28  libes
 * Added * to typedefs.  Replaced warning kludges with ERRORoption.
 */

#include <stdlib.h>
#include <stdio.h>
#include "../express/express.h"
#include "../express/resolve.h"

static void
fedex_plus_usage()
{
        fprintf(stderr,"usage: %s [-s|-S] [-a|-A] [-c|-C] [-l|-L] [-v] [-d #] [-p <object_type>] {-w|-i <warning>} express_file\n",EXPRESSprogram_name);
	fprintf(stderr,"where\t-s or -S uses only single inheritance in the generated C++ classes\n");
	fprintf(stderr,"\t-a or -A generates the early bound access functions for entity classes the old way (without an underscore)\n");
	fprintf(stderr,"\t-c or -C generates C++ classes for use with CORBA (Orbix)\n");
	fprintf(stderr,"\t-l or -L prints logging code in the generated C++ classes\n");
	fprintf(stderr,"\t-v produces a version description\n");
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

int Handle_FedPlus_Args(int, char *);
void print_file(Express);

void resolution_success(void)
{
	printf("Resolution successful.  Writing C++ output...\n");
}

int success(Express model)
{
	printf("Finished writing files.\n");
	return(0);
}

/* This function is called from main() which is part of the NIST Express 
   Toolkit.  It assigns 2 pointers to functions which are called in main() */
void
EXPRESSinit_init() {
	EXPRESSbackend = print_file;
	EXPRESSsucceed = success;
	EXPRESSgetopt = Handle_FedPlus_Args;
  /* so the function getopt (see man 3 getopt) will not report an error */
	strcat(EXPRESSgetopt_options, "sSlLcCaA");
	ERRORusage_function = fedex_plus_usage;
}

