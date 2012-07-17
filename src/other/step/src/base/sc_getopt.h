/****** RENAMED from xgetopt.h to sc_getopt.h ***********/
// XGetopt.h  Version 1.2
//
// Author:  Hans Dietrich
//          hdietrich2@hotmail.com
//
// This software is released into the public domain.
// You are free to use it in any way you like.
//
// This software is provided "as is" with no expressed
// or implied warranty.  I accept no liability for any
// damage or loss of business that this software may cause.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef XGETOPT_H
#define XGETOPT_H
#include "scl_export.h"

#ifdef __cplusplus
extern "C" {
#endif

    extern SCL_BASE_EXPORT int optind, opterr;
    extern SCL_BASE_EXPORT char * optarg;

    int    SCL_BASE_EXPORT sc_getopt( int argc, char * argv[], char * optstring );

#ifdef __cplusplus
}
#endif

#endif //XGETOPT_H
