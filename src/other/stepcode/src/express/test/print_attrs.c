/*
 * derived from symlink.c, written by Don Libes, NIST, 20-Mar-1993
 *
 * print_attrs: late-bound implementation of an attribute printer
 * attrs are ordered as they would be in a p21 file
 * print_attrs -a <entity> <schema>
 */

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
#include <express/info.h>
#include <express/express.h>
#include <express/scope.h>
#include <express/variable.h>
#include "ordered_attrs.h"
#include <assert.h>

char * entityName, _buf[512] = { 0 };

/** prints usage info specific to print_attrs */
void my_usage(void) {
    EXPRESSusage( 0 );
    printf( "   ----\n\t-a <entity>: print attrs for <entity>\n" );
    exit( 2 );
}

/** prints info about one attr */
void describeAttr( const orderedAttr * oa ) {
    const char * visible_p21 = "    Y    ", * hidden_p21 = "    N    ", * explicit_derived = "    *    ";
    const char * visibility, * descrip1="", * descrip2="", * descrip3=0;
    if( oa->deriver ) {
        assert( 0 == oa->attr->inverse_attribute && "Can't be derived *and* an inverse attribute" );
        descrip1 = "derived in ";
        descrip2 = oa->deriver->symbol.name;
        if( oa->deriver == oa->creator ) {
            visibility = hidden_p21;
        } else {
            visibility = explicit_derived;
        }
    } else if( oa->attr->inverse_attribute ) {
        visibility = hidden_p21;
        descrip1 = "inverse of ";
        descrip2 = oa->attr->inverse_attribute->name->symbol.name;
        descrip3 = oa->attr->inverse_attribute->type->superscope->symbol.name;
    } else {
        visibility = visible_p21;
    }
    printf("%s|%22s |%22s | %s%s%s%s\n", visibility, oa->attr->name->symbol.name,
           oa->creator->symbol.name, descrip1, descrip2, ( ( descrip3 ) ? " in " : "" ), ( ( descrip3 ) ? descrip3 : "" ) );
}

void print_attrs( Entity ent ) {
    const orderedAttr * oa;
    const char * dashes="--------------------------------------------------------------------------";
    printf( "Entity %s\n%s\n%s\n%s\n", ent->symbol.name, dashes,
            " In P21? |       attr name       |        creator        |     detail", dashes );
    orderedAttrsInit( ent );
    while( 0 != ( oa = nextAttr() ) ) {
        describeAttr( oa );
    }
    orderedAttrsCleanup();
}

void find_and_print( Express model ) {
    DictionaryEntry de;
    Schema s;
    Entity e;
    DICTdo_init( model->symbol_table, &de );
    while( 0 != ( s = (Schema) DICTdo( &de ) ) ) {
        printf( "Schema %s\n", s->symbol.name );
        e = (Entity) DICTlookup( s->symbol_table, entityName );
        if( e ) {
            print_attrs( e );
        }
    }
}

/** reads arg setting entity name */
int attr_arg( int i, char * arg ) {
    const char * src = arg;
    int count = 0;
    if( ( char )i == 'a' ) {
        entityName = _buf;
        while( *src ) {
            _buf[count] = tolower( *src );
            src++;
            count++;
            if( count == 511 ) {
                break;
            }
        }
        if( count == 0 ) {
            entityName = 0;
        }
    } else if( !entityName ) {
        /* if libexpress comes across an unrecognized arg that isn't '-a',
         * and if the entityName isn't set, print usage and exit
         */
        return 1;
    }
    return 0;
}

/** set the functions to be called by main() in libexpress */
void EXPRESSinit_init() {
    entityName = 0;
    EXPRESSbackend = find_and_print;
    ERRORusage_function = my_usage;
    strcat( EXPRESSgetopt_options, "a:" );
    EXPRESSgetopt = attr_arg;
}
