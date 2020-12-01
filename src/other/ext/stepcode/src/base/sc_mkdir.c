
#include "sc_mkdir.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifdef _WIN32
#  include <direct.h>
#endif /* _WIN32 */

/* cross-platform mkdir */
int sc_mkdir( const char * path ) {
    #ifdef _WIN32
    return mkdir( path );
    #else
    return mkdir( path, 0777 );
    #endif /* _WIN32 */
}

/* return -1 if error, 0 if created, 1 if dir existed already */
int mkDirIfNone( const char * path ) {
    struct stat s;
    if( stat( path, &s ) != 0 ) {
        if( errno == ENOENT ) {
            return sc_mkdir( path );
        }
    } else if( s.st_mode & S_IFDIR ) {
        return 1;
    }
    /* either stat returned an error other than ENOENT, or 'path' exists but isn't a dir */
    return -1;
}
