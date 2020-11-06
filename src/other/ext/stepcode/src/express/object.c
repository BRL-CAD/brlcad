

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log$
 * Revision 1.7  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.6  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/22  21:48:18  libes
 * added arg to ERRORabort
 *
 * Revision 1.4  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */


#include <stdlib.h>
#include "express/object.h"

struct Object * OBJ;

Symbol * UNK_get_symbol( Generic x ) {
    fprintf( stderr, "OBJget_symbol called on object of unknown type\n" );
    ERRORabort( 0 );
    return 0;
}

/** Initialize the Object module */
void OBJinitialize() {
    int i;

    OBJ = ( struct Object * )malloc( MAX_OBJECT_TYPES * sizeof( struct Object ) );
    for( i = 0; i < MAX_OBJECT_TYPES; i++ ) {
        OBJ[i].get_symbol = UNK_get_symbol;
        OBJ[i].type = "of unknown_type";
        OBJ[i].bits = 0;
    }
}

/** Clean up the Object module */
void OBJcleanup() {
    free( OBJ );
}

void OBJcreate( char type, struct Symbol_ * ( *get_symbol )( Generic ), char * printable_type, int bits ) {
    int index = ( int )type;
    OBJ[type].get_symbol = get_symbol;
    OBJ[type].type = printable_type;
    OBJ[type].bits = bits;
}
