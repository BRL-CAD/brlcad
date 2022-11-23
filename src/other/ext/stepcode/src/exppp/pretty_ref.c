/** \file pretty_ref.c
 * split out of exppp.c 9/21/13
 */

#include <express/schema.h>
#include <exppp/exppp.h>

#include "pp.h"
#include "pretty_ref.h"

void REFout( Dictionary refdict, Linked_List reflist, char * type, int level ) {
    Dictionary dict;
    DictionaryEntry de;
    struct Rename * ren;
    Linked_List list;

    LISTdo( reflist, s, Schema )
    raw( "%s FROM %s;\n", type, s->symbol.name );
    LISTod

    if( !refdict ) {
        return;
    }
    dict = DICTcreate( 10 );

    /* sort each list by schema */

    /* step 1: for each entry, store it in a schema-specific list */
    DICTdo_init( refdict, &de );
    while( 0 != ( ren = ( struct Rename * )DICTdo( &de ) ) ) {
        Linked_List nameList;

        nameList = ( Linked_List )DICTlookup( dict, ren->schema->symbol.name );
        if( !nameList ) {
            nameList = LISTcreate();
            DICTdefine( dict, ren->schema->symbol.name, nameList, NULL, OBJ_UNKNOWN );
        }
        LISTadd_last( nameList, ren );
    }

    /* step 2: for each list, print out the renames */
    level = 6;  /* no special reason, feels good */
    indent2 = level + exppp_continuation_indent;
    DICTdo_init( dict, &de );
    while( 0 != ( list = ( Linked_List )DICTdo( &de ) ) ) {
        bool first_time = true;
        LISTdo( list, r, struct Rename * ) {
            if( first_time ) {
                raw( "%s FROM %s\n", type, r->schema->symbol.name );
            } else {
                /* finish previous line */
                raw( ",\n" );
            }

            if( first_time ) {
                raw( "%*s( ", level, "" );
                first_time = false;
            } else {
                raw( "%*s ", level, "" );
            }
            raw( r->old->name );
            if( r->old != r->nnew ) {
                wrap( " AS %s", r->nnew->name );
            }
        } LISTod
        raw( " );\n" );
    }
    HASHdestroy( dict );
}
