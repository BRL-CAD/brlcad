/** \file pretty_express.c
 * split out of exppp.c 9/21/13
 */

#include <express/express.h>
#include <exppp/exppp.h>

#include "pp.h"
#include "pretty_schema.h"
#include "pretty_express.h"

void EXPRESSout( Express e ) {
    Schema s;
    DictionaryEntry de;

    exppp_init();

    DICTdo_init( e->symbol_table, &de );
    while( 0 != ( s = ( Schema )DICTdo( &de ) ) ) {
        ( void ) SCHEMAout( s );
    }
}
