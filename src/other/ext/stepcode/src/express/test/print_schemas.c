/* derived from symlink.c */
/* prints names of schemas used in an EXPRESS file */
/* symlink.c author: Don Libes, NIST, 20-Mar-1993 */

#include "sc_cf.h"
#include <stdlib.h>
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include "express/express.h"

void
print_schemas( Express model ) {
    DictionaryEntry de;
    Schema s;

    printf( "File: %s\n  ", model->u.express->filename );

    DICTdo_init( model->symbol_table, &de );
    while( 0 != ( s = (Schema) DICTdo( &de ) ) ) {
        printf( "%s", s->symbol.name );
    }
    printf( "\n" );
    exit( 0 );
}

void EXPRESSinit_init() {
    EXPRESSbackend = print_schemas;
}

