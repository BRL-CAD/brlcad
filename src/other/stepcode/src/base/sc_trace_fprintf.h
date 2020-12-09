#ifndef SC_TRACE_FPRINTF_H
#define SC_TRACE_FPRINTF_H

/** \file sc_trace_fprintf.h
 * Used to track the source file and line where generated code is printed from
 * When enabled, comments are printed into the generated files for every 'fprintf':
 * / * source: scl/src/exp2cxx/selects.c:1375 * /
 * To enable, configure with 'cmake .. -DSC_TRACE_FPRINTF=ON'
 *
 * This header must be included *after* all other headers, otherwise the compiler will
 * report errors in system headers.
 * \sa trace_fprintf()
**/

#include "sc_export.h"

#ifdef __cplusplus
extern "C" {
#endif
    /** Used to find where generated c++ originates from in exp2cxx.
     * To enable, configure with 'cmake .. -DSC_TRACE_FPRINTF=ON'
     */
    SC_BASE_EXPORT void trace_fprintf( char const * sourcefile, int line, FILE * file, const char * format, ... );
#ifdef __cplusplus
}
#endif

#ifdef SC_TRACE_FPRINTF
#   define fprintf(...) trace_fprintf(__FILE__, __LINE__, __VA_ARGS__)
#endif /* SC_TRACE_FPRINTF */

#endif /* SC_TRACE_FPRINTF_H */
