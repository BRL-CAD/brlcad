/* derived from symlink.c */
/* prints names of schemas used in an EXPRESS file */
/* symlink.c author: Don Libes, NIST, 20-Mar-1993 */

#include <stdlib.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "express/express.h"

void
print_schemas( Express model ) {
    DictionaryEntry de;
    Schema s;

    printf( "File: %s\n  ", model->u.express->filename );

    DICTdo_init( model->symbol_table, &de );
    while( 0 != ( s = DICTdo( &de ) ) ) {
        printf( "%s", s->symbol.name );
    }
    printf( "\n" );
    exit( 0 );
}

void EXPRESSinit_init() {
    EXPRESSbackend = print_schemas;
}

