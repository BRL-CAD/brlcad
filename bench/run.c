/*                           R U N . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file run.c
 *
 * run functions
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu.h"


/* TODO: !!! obviously */
struct bu_vls bu_argv_to_string(int ac, const char *av[]) {struct bu_vls v = BU_VLS_INIT_ZERO; if (ac) {bu_vls_printf(&v, "%s", av[0]);} return v;}
int (*echo)(const char *, ...) = &bu_log;
extern int bu_exec(const char *program, void *data, struct bu_vls *out, struct bu_vls *err);


/**
 * run file_prefix geometry hypersample [..rt args..]
 * runs a single benchmark test assuming the following are preset:
 *
 *  RT := path/name of the raytracer to use
 *  DB := path to the geometry file
 *
 * it is assumed that stdin will be the view/frame input
 */
int
run(int ac, char *av[])
{
    int retval;
    const char *run_geomname = av[0];
    const char *run_geometry = av[1];
    long int hypersample = strtod(av[2], NULL);
    av += 3;
    ac -= 3;

    {
	struct bu_vls args = bu_argv_to_string(ac, (const char **)av);
	echo("DEBUG: Running %s -B -M -s512 -H%ld -J0 %s -o %s.pix %s/%s.g %s\n", av[0], hypersample, args, run_geomname, run_geomname, run_geometry);
	bu_vls_free(&args);
    }

    /* pipe(stdin_for_bu_exec); */
    retval = bu_exec("program", NULL, NULL, NULL);
    echo("DEBUG: Running %s returned %d\n", av[0], retval);

    return retval;
}


/**
 * runs a series of benchmark tests assuming the following are preset:
 *
 *   TIMEFRAME := maximum amount of wallclock time to spend per test
 *
 * is is assumed that stdin will be the view/frame input
 */
int
bench(const char *test_name)
{
    if (!test_name)
	return -1;

    /* FIXME: !!! lots missing here */

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
