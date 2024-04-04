
/* Driver for exp2python (generation of python from EXPRESS) */

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 */

#include <stdlib.h>
#include <stdio.h>
#include "../express/express.h"
#include "../express/resolve.h"

extern void print_fedex_version( void );

static void exp2python_usage( void ) {
    char *warnings_help_msg = ERRORget_warnings_help("\t", "\n");
    fprintf( stderr, "usage: %s [-v] [-d # | -d 9 -l nnn -u nnn] [-n] [-p <object_type>] {-w|-i <warning>} express_file\n", EXPRESSprogram_name );
    fprintf( stderr, "\t-v produces the version description below\n" );
    fprintf( stderr, "\t-d turns on debugging (\"-d 0\" describes this further\n" );
    fprintf( stderr, "\t-w warning enable\n" );
    fprintf( stderr, "\t-i warning ignore\n" );
    fprintf( stderr, "and <warning> is one of:\n" );
    fprintf( stderr, "\tnone\n\tall\n" );
    fprintf( stderr, "%s", warnings_help_msg);
    fprintf( stderr, "and <object_type> is one or more of:\n" );
    fprintf( stderr, "	e	entity\n" );
    fprintf( stderr, "	p	procedure\n" );
    fprintf( stderr, "	r	rule\n" );
    fprintf( stderr, "	f	function\n" );
    fprintf( stderr, "	t	type\n" );
    fprintf( stderr, "	s	schema or file\n" );
    fprintf( stderr, "	#	pass #\n" );
    fprintf( stderr, "	E	everything (all of the above)\n" );
    print_fedex_version();
    exit( 2 );
}

int Handle_FedPlus_Args( int, char * );
void print_file( Express );

void resolution_success( void ) {
    printf( "Resolution successful.\nWriting python module..." );
}

int success( Express model ) {
    (void) model; /* unused */
    printf( "Done.\n" );
    return( 0 );
}

/* This function is called from main() which is part of the NIST Express
   Toolkit.  It assigns 2 pointers to functions which are called in main() */
void EXPRESSinit_init( void ) {
    EXPRESSbackend = print_file;
    EXPRESSsucceed = success;
    EXPRESSgetopt = Handle_FedPlus_Args;
    /* so the function getopt (see man 3 getopt) will not report an error */
    strcat( EXPRESSgetopt_options, "sSLcCaA" );
    ERRORusage_function = exp2python_usage;
}

