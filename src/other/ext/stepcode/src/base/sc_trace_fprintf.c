
#include <stdarg.h>
#include <stdio.h>

#include "sc_trace_fprintf.h"

void trace_fprintf( char const * sourcefile, int line, FILE * file, const char * format, ... ) {
    va_list args;
    if( ( file != stdout ) && ( file != stderr ) ) {
        fprintf( file, "/* source: %s:%d */", sourcefile, line );
    }
    va_start( args, format );
    vfprintf( file, format, args );
    va_end( args );
}

