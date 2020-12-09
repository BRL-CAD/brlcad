/** \file sc_getopt.h
 * this was xgetopt.h
 * XGetopt.h  Version 1.2
 *
 * Author:  Hans Dietrich
 *          hdietrich2@hotmail.com
 *
 * This software is released into the public domain.
 * You are free to use it in any way you like.
 *
 * This software is provided "as is" with no expressed
 * or implied warranty.  I accept no liability for any
 * damage or loss of business that this software may cause.
 */
#ifndef XGETOPT_H
#define XGETOPT_H
#include "sc_export.h"

#ifdef __cplusplus
extern "C" {
#endif

    extern SC_BASE_EXPORT int sc_optind, sc_opterr;
    extern SC_BASE_EXPORT char * sc_optarg;

    int    SC_BASE_EXPORT sc_getopt( int argc, char * argv[], char * optstring );

#ifdef __cplusplus
}
#endif

#endif /* XGETOPT_H */
