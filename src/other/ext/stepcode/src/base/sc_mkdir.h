#ifndef SC_MKDIR
#define SC_MKDIR

#include <sc_export.h>

/** cross-platform mkdir() */
SC_BASE_EXPORT int sc_mkdir( const char * path );

/** create a dir 'path' if 'path' doesn't exist
 * \return -1 if error, 0 if created, 1 if dir existed already
 * if it returns -1, check errno
 */
SC_BASE_EXPORT int mkDirIfNone( const char * path );

#endif /* SC_MKDIR */
