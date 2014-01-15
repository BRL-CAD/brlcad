/*              T E S T _ B U _ A R G _ P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/test_bu_arg_parse.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

#include "bu_arg_parse.h"

static char usage[] = "Example: test_bu_arg_parse f1 f2";
int
main(int ac, char *av[])
{
    struct stat sb;

    /* vars expected from cmd line parsing */
    const char* dsp1_fname = NULL;
    const char* dsp2_fname = NULL;
    const char* dsp3_fname = NULL;
    int arg_err = 0;
    long has_force = 0;
    long has_help  = 0;

    /* note the arg structs have to be static to compile */
    /* FIXME: this '-?' arg doesn't wok correctly due to some TCLAPisms */
    static bu_arg_vars h_arg = {
      BU_ARG_SwitchArg,
      "?",
      "short-help",
      "Same as '-h' or '--help'",
      BU_ARG_NOT_REQUIRED,
      BU_ARG_NOT_REQUIRED,
      {0},        /* value in first field of union */
      BU_ARG_BOOL /* type in union */
    };

    /* define a force option to allow user to shoot himself in the foot */
    static bu_arg_vars f_arg = {
      BU_ARG_SwitchArg,
      "f",
      "force",
      "Allow overwriting existing files.",
      BU_ARG_NOT_REQUIRED,
      BU_ARG_NOT_REQUIRED,
      {0},        /* value in first field of union */
      BU_ARG_BOOL /* type in union */
    };

    /* need two file names */
    static bu_arg_vars dsp1_arg = {
      BU_ARG_UnlabeledValueArg,
      "",
      "dsp_infile1",
      "first dsp input file name",
      BU_ARG_REQUIRED,
      BU_ARG_REQUIRED,
      {0},        /* value in first field of union */
      BU_ARG_BOOL /* type in union */
    };

    /* need two file names */
    static bu_arg_vars dsp2_arg = {
      BU_ARG_UnlabeledValueArg,
      "",
      "dsp_infile2",
      "second dsp input file name",
      BU_ARG_REQUIRED,
      BU_ARG_REQUIRED,
      {0},        /* value in first field of union */
      BU_ARG_BOOL /* type in union */
    };

    /* the output file name */
    static bu_arg_vars dsp3_arg = {
      BU_ARG_UnlabeledValueArg,
      "",
      "dsp_outfile",
      "dsp output file name",
      BU_ARG_REQUIRED,
      BU_ARG_REQUIRED,
      {0},        /* value in first field of union */
      BU_ARG_BOOL /* type in union */
    };

    /* place the arg pointers in an array */
    static bu_arg_vars *args[]
      = {&h_arg, &f_arg,
         &dsp1_arg, &dsp2_arg, &dsp3_arg,
         NULL};

    /* for C90 we have to initialize a struct's union
     * separately for other than its first field
     */
    dsp1_arg.val.s    = 0;
    dsp1_arg.val_type = BU_ARG_STRING;

    dsp2_arg.val.s    = 0;
    dsp2_arg.val_type = BU_ARG_STRING;

    dsp3_arg.val.s    = 0;
    dsp3_arg.val_type = BU_ARG_STRING;

    /* parse the args */
    arg_err = bu_arg_parse(args, ac, av);

    if (arg_err == BU_ARG_PARSE_ERR) {
        /* the TCLAP exception handler has fired with its own message
         * so need no words here */
        bu_exit(EXIT_SUCCESS, NULL);
    }

    /* Get the value parsed by each arg. */
    has_force  = f_arg.val.l;
    has_help   = h_arg.val.l;
    dsp1_fname = dsp1_arg.val.s;
    dsp2_fname = dsp2_arg.val.s;
    dsp3_fname = dsp3_arg.val.s;

    /* take appropriate action... */

    /* note this exit is success because it is expected
     * behavior--important for good auto-man-page handling */
    if (has_help) {
      bu_arg_free(args);
      bu_exit(EXIT_SUCCESS, usage);
    }

    /* TCLAP doesn't check for confusion in file names */
    if (BU_STR_EQUAL(dsp3_fname, dsp1_fname)
        || BU_STR_EQUAL(dsp3_fname, dsp2_fname)) {
      bu_arg_free(args);
      bu_exit(EXIT_FAILURE, "overwriting an input file (use the '-f' option to continue)\n");
    }

    /* nor does it check for existing files (FIXME: add to TCLAP) */
    if (!stat(dsp3_fname, &sb)) {
      if (has_force) {
        printf("WARNING: overwriting an existing file...\n");
        bu_file_delete(dsp3_fname);
      }
      else {
        bu_arg_free(args);
        bu_exit(EXIT_FAILURE, "overwriting an existing file (use the '-f' option to continue)\n");
      }
    }

    /* show results */

    bu_arg_free(args);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
