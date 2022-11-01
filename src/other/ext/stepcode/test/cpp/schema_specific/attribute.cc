/** \file attribute.cc
 * 1-Jul-2012
 * Test attribute access; uses a tiny schema similar to a subset of IFC2x3
 */
#include "config.h"
#include <STEPfile.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>
#include <algorithm>
#include <string>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "schema.h"

char  * sc_optarg;        // global argument pointer
int sc_optind = 0;     // global argv index

int sc_getopt( int argc, char * argv[], char * optstring ) {
    static char * next = NULL;
    if( sc_optind == 0 ) {
        next = NULL;
    }

    sc_optarg = NULL;

    if( next == NULL || *next == '\0' ) {
        if( sc_optind == 0 ) {
            sc_optind++;
        }

        if( sc_optind >= argc || argv[sc_optind][0] != '-' || argv[sc_optind][1] == '\0' ) {
            sc_optarg = NULL;
            if( sc_optind < argc ) {
                sc_optarg = argv[sc_optind];
            }
            return EOF;
        }

        if( strcmp( argv[sc_optind], "--" ) == 0 ) {
            sc_optind++;
            sc_optarg = NULL;
            if( sc_optind < argc ) {
                sc_optarg = argv[sc_optind];
            }
            return EOF;
        }

        next = argv[sc_optind];
        next++;     // skip past -
        sc_optind++;
    }

    char c = *next++;
    char * cp = strchr( optstring, c );

    if( cp == NULL || c == ':' ) {
        return '?';
    }

    cp++;
    if( *cp == ':' ) {
        if( *next != '\0' ) {
            sc_optarg = next;
            next = NULL;
        } else if( sc_optind < argc ) {
            sc_optarg = argv[sc_optind];
            sc_optind++;
        } else {
            return '?';
        }
    }

    return c;
}
int main( int argc, char * argv[] ) {
    Registry  registry( SchemaInit );
    InstMgr   instance_list;
    STEPfile  sfile( registry, instance_list, "", false );
    bool foundMatch = false;
    const char * attrname = "description";
    if( argc != 2 ) {
        exit( EXIT_FAILURE );
    }
    sfile.ReadExchangeFile( argv[1] );

    if( sfile.Error().severity() <= SEVERITY_INCOMPLETE ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }
    const SdaiWindow * wind = dynamic_cast< SdaiWindow * >( instance_list.GetApplication_instance( "window" ) );
    int i = 0;
    if( wind ) {
        STEPattributeList attrlist = wind->attributes;
        for( ; i < attrlist.list_length(); i++ ) {
            cout << "attr " << i << ": " << attrlist[i].Name() << endl;
            if( 0 == strcmp( attrname, attrlist[i].Name() ) ) {
                foundMatch = true;
                cout << "attribute " << '"' << attrname << '"' << " found at " << i << endl;
            }
        }
    }
    if( !i ) {
        cout << "no attrs found" << endl;
        exit( EXIT_FAILURE );
    }
    if( !foundMatch ) {
        cout << "attribute " << '"' << attrname << '"' << " not found" << endl;
        exit( EXIT_FAILURE );
    } else {
        exit( EXIT_SUCCESS );
    }
}

