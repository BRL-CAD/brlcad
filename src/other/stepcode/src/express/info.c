#include <stdio.h>
#include <stdlib.h>

#include "express/info.h"
#include "express/express.h"

#ifndef SCHEMA_SCANNER
#  include "sc_version_string.h"
#else
  /* dummy string to make the compiler happy when building the schema scanner,
   * should never be seen by users */
  const char * sc_version = "ERROR: version unknown / SCHEMA_SCANNER defined in libexpress!";
#endif

char * EXPRESSversion( void ) {
    return( "Express Language, IS (N65), October 24, 1994" );
}

void EXPRESSusage( int _exit ) {
    fprintf( stderr, "usage: %s [-v] [-d #] [-p <object_type>] {-w|-i <warning>} express_file\n", EXPRESSprogram_name );
    fprintf( stderr, "where\t-v produces the following version description:\n" );
    fprintf( stderr, "Build info for %s: %s\nhttp://github.com/stepcode/stepcode\n", EXPRESSprogram_name, sc_version );
    fprintf( stderr, "\t-d turns on debugging (\"-d 0\" describes this further\n" );
    fprintf( stderr, "\t-p turns on printing when processing certain objects (see below)\n" );
    fprintf( stderr, "\t-w warning enable\n" );
    fprintf( stderr, "\t-i warning ignore\n" );
    fprintf( stderr, "and <warning> is one of:\n" );
    fprintf( stderr, "\tnone\n\tall\n" );
    fprintf( stderr, ERRORget_warnings_help("\t", "\n") );
    fprintf( stderr, "and <object_type> is one or more of:\n" );
    fprintf( stderr, "  e   entity\n" );
    fprintf( stderr, "  p   procedure\n" );
    fprintf( stderr, "  r   rule\n" );
    fprintf( stderr, "  f   function\n" );
    fprintf( stderr, "  t   type\n" );
    fprintf( stderr, "  s   schema or file\n" );
    fprintf( stderr, "  #   pass #\n" );
    fprintf( stderr, "  E   everything (all of the above)\n" );
    if( _exit ) {
        exit( 2 );
    }
}

