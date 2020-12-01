
#include "path2str.h"
#include "sc_memmgr.h"
#include <string.h>

/* for windows, rewrite backslashes in paths
 * that will be written to generated code
 */
const char * path2str_fn( const char * fileMacro ) {
    static char * result = 0;
    static size_t rlen = 0;
    char * p;
    if( rlen < strlen( fileMacro ) ) {
        if( result ) {
            sc_free( result );
        }
        rlen = strlen( fileMacro );
        result = ( char * )sc_malloc( rlen * sizeof( char ) + 1 );
    }
    strcpy( result, fileMacro );
    p = result;
    while( *p ) {
        if( *p == '\\' ) {
            *p = '/';
        }
        p++;
    }
    return result;
}
