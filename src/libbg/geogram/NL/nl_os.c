/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#include "common.h"
#include "bu/exit.h"
#include "nl_private.h"

#if (defined (WIN32) || defined(_WIN64))
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/times.h>
#endif

/******************************************************************************/
/* Assertions */


void nl_assertion_failed(const char* cond, const char* file, int line) {
    nl_fprintf(
        stderr,
        "OpenNL assertion failed: %s, file:%s, line:%d\n",
        cond,file,line
    ) ;
    bu_bomb("OpenNL assertion failed") ;
}

void nl_range_assertion_failed(
    double x, double min_val, double max_val, const char* file, int line
) {
    nl_fprintf(
        stderr,
        "OpenNL range assertion failed: "
        "%f in [ %f ... %f ], file:%s, line:%d\n",
        x, min_val, max_val, file,line
    ) ;
    bu_bomb("OpenNL range assertion failed") ;
}

void nl_should_not_have_reached(const char* file, int line) {
    nl_fprintf(
        stderr,
        "OpenNL should not have reached this point: file:%s, line:%d\n",
        file,line
    ) ;
    bu_bomb("OpenNL should not have reached this point") ;
}


/******************************************************************************/
/* Timing and number of cores */

#ifdef WIN32
NLdouble nlCurrentTime(void) {
    return (NLdouble)GetTickCount() / 1000.0 ;
}
#else
double nlCurrentTime(void) {
    clock_t user_clock ;
    struct tms user_tms ;
    user_clock = times(&user_tms) ;
    return (NLdouble)user_clock / 100.0 ;
}
#endif


NLuint nlGetNumCores(void) {
    return 1;
}

NLuint nlGetNumThreads(void) {
    return 1;
}

void nlSetNumThreads(NLuint n) {
    nl_arg_used(n);
}


/******************************************************************************/
/* Error-reporting functions */

NLprintfFunc nl_printf = printf;
NLfprintfFunc nl_fprintf = fprintf;

void nlError(const char* function, const char* message) {
    nl_fprintf(stderr, "OpenNL error in %s(): %s\n", function, message) ;
}

void nlWarning(const char* function, const char* message) {
    nl_fprintf(stderr, "OpenNL warning in %s(): %s\n", function, message) ;
}

void nlPrintfFuncs(NLprintfFunc f1, NLfprintfFunc f2) {
    nl_printf = f1;
    nl_fprintf = f2;
}

/******************************************************************************/
