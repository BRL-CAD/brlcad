#ifndef PATH2STR_H
#define PATH2STR_H

#include <sc_export.h>

/** windows only: rewrite backslashes in paths as forward slashes
 * call as path2str(__FILE__) to take advantage of macro
 *
 * silence "unknown escape sequence" warning when contents of __FILE__
 * are fprintf'd into string in generated code
 */
SC_BASE_EXPORT const char * path2str_fn( const char * fileMacro );

#ifdef _WIN32
#  define path2str(path) path2str_fn(path)
#else
#  define path2str(path) path
#endif

#endif /* PATH2STR_H */
