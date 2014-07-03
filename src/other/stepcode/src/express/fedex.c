

/************************************************************************
** Driver for Fed-X Express parser.
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: fedex.c,v $
 * Revision 1.20  1997/05/29 19:56:41  sauderd
 * Changed the prototype for extern getopt()
 *
 * Revision 1.19  1997/01/21 19:53:47  dar
 * made C++ compatible
 *
 * Revision 1.17  1995/06/08 22:59:59  clark
 * bug fixes
 *
 * Revision 1.16  1995/05/17  14:26:59  libes
 * Improved debugging hooks.
 *
 * Revision 1.15  1995/04/08  20:54:18  clark
 * WHERE rule resolution bug fixes
 *
 * Revision 1.15  1995/04/08  20:49:10  clark
 * WHERE
 *
 * Revision 1.14  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.13  1995/02/09  18:00:04  clark
 * changed version string
 *
 * Revision 1.12  1994/05/11  19:50:52  libes
 * numerous fixes
 *
 * Revision 1.11  1993/10/15  18:48:11  libes
 * CADDETC certified
 *
 * Revision 1.9  1993/02/22  21:47:03  libes
 * fixed main-hooks
 *
 * Revision 1.8  1993/02/16  03:22:56  libes
 * improved final success/error message handling
 *
 * Revision 1.7  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/27  23:40:21  libes
 * mucked around with version string
 *
 * Revision 1.4  1992/08/18  17:12:30  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:18  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:35:00  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:53:54  libes
 * Initial revision
 *
 * Revision 1.7  1992/05/14  10:12:23  libes
 * changed default to unsorted, added B for sort
 *
 * Revision 1.6  1992/05/10  01:41:01  libes
 * does enums and repeat properly
 *
 */

#include <sc_cf.h>
#include <sc_memmgr.h>
#include <sc_export.h>
#include "sc_version_string.h"
#include <stdlib.h>
#include <stdio.h>
#include "sc_getopt.h"
#include "express/error.h"
#include "express/express.h"
#include "express/resolve.h"

#ifdef YYDEBUG
extern int exp_yydebug;
#endif /*YYDEBUG*/

char EXPRESSgetopt_options[256] = "Bbd:e:i:w:p:rvz"; //larger than the string because exp2cxx, exppp, etc may append their own options
static int no_need_to_work = 0; /* TRUE if we can exit gracefully without doing any work */

void print_fedex_version( void ) {
    fprintf( stderr, "Build info for %s: %s\nhttp://github.com/stepcode/stepcode\n", EXPRESSprogram_name, sc_version() );
    no_need_to_work = 1;
}

int main( int argc, char ** argv ) {
    int c;
    int rc;
    char * cp;
    int no_warnings = 1;
    int resolve = 1;
    int result;

    bool buffer_messages = false;
    char * filename = 0;
    Express model;

    EXPRESSprogram_name = argv[0];
    ERRORusage_function = 0;

    EXPRESSinit_init();

    EXPRESSinitialize();

    if( EXPRESSinit_args ) {
        ( *EXPRESSinit_args )( argc, argv );
    }

    optind = 1;
    while( ( c = sc_getopt( argc, argv, EXPRESSgetopt_options ) ) != -1 )
        switch( c ) {
            case 'd':
                ERRORdebugging = 1;
                switch( atoi( optarg ) ) {
                    case 0:
                        fprintf( stderr, "\ndebug codes:\n" );
                        fprintf( stderr, "  0 - this help\n" );
                        fprintf( stderr, "  1 - basic debugging\n" );
#ifdef debugging
                        fprintf( stderr, "  4 - light malloc debugging\n" );
                        fprintf( stderr, "  5 - heavy malloc debugging\n" );
                        fprintf( stderr, "  6 - heavy malloc debugging while resolving\n" );
#endif /* debugging*/
#ifdef YYDEBUG
                        fprintf( stderr, "  8 - set YYDEBUG\n" );
#endif /*YYDEBUG*/
                        break;
                    case 1:
                        debug = 1;
                        break;
#ifdef debugging
                    case 4:
                        malloc_debug( 1 );
                        break;
                    case 5:
                        malloc_debug( 2 );
                        break;
                    case 6:
                        malloc_debug_resolve = 1;
                        break;
#endif /*debugging*/
#ifdef YYDEBUG
                    case 8:
                        exp_yydebug = 1;
                        break;
#endif /* YYDEBUG */
                }
                break;
            case 'B':
                buffer_messages = true;
                break;
            case 'b':
                buffer_messages = false;
                break;
            case 'e':
                filename = optarg;
                break;
            case 'r':
                resolve = 0;
                break;
            case 'i':
            case 'w':
                no_warnings = 0;
                ERRORset_warning( optarg, c == 'w' );
                break;
            case 'p':
                for( cp = optarg; *cp; cp++ ) {
                    if( *cp == '#' ) {
                        print_objects_while_running |= OBJ_PASS_BITS;
                    } else if( *cp == 'E' ) {
                        print_objects_while_running = OBJ_ANYTHING_BITS;
                    } else {
                        print_objects_while_running |= OBJget_bits( *cp );
                    }
                }
                break;
            case 'v':
                print_fedex_version();
                no_need_to_work = 1;
                break;
            default:
                rc = 1;
                if( EXPRESSgetopt ) {
                    rc = ( *EXPRESSgetopt )( c, optarg );
                }
                if( rc == 1 ) {
                    if( ERRORusage_function ) {
                        ( *ERRORusage_function )();
                    } else {
                        EXPRESSusage(1);
                    }
                }
                break;
        }

    if( !filename ) {
        filename = argv[optind];
        if( !filename ) {
            EXPRESScleanup();
            if( no_need_to_work ) {
                return( 0 );
            } else {
                ( *ERRORusage_function )();
            }
        }
    }

    if( no_warnings ) {
        ERRORset_all_warnings( 1 );
    }
    ERRORbuffer_messages( buffer_messages );

    if( EXPRESSinit_parse ) {
        ( *EXPRESSinit_parse )();
    }

    model = EXPRESScreate();
    EXPRESSparse( model, ( FILE * )0, filename );
    if( ERRORoccurred ) {
        result = EXPRESS_fail( model );
        EXPRESScleanup();
        EXPRESSdestroy( model );
        return result;
    }

#ifdef debugging
    if( malloc_debug_resolve ) {
        malloc_verify();
        malloc_debug( 2 );
    }
#endif /*debugging*/

    if( resolve ) {
        EXPRESSresolve( model );
        if( ERRORoccurred ) {
            result = EXPRESS_fail( model );
            EXPRESScleanup();
            EXPRESSdestroy( model );
            return result;
        }
    }

    if( EXPRESSbackend ) {
        ( *EXPRESSbackend )( model );
    }

    if( ERRORoccurred ) {
        result = EXPRESS_fail( model );
        EXPRESScleanup();
        EXPRESSdestroy( model );
        return result;
    }

    result = EXPRESS_succeed( model );
    EXPRESScleanup();
    EXPRESSdestroy( model );
    return result;
}
