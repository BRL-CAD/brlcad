/*                    G A N A L Y Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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
/** @file ganalyze.cpp
 *
 * Command line interface to libanalyze capabilities
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "analyze.h"

extern "C" {
#include "linenoise.h"
}

#define ANALYZE_DO_QUIT -999
#define ANALYZE_FATAL_ERROR -1

struct analyze_command_table {
    const char *cmd;
    const char *parms;
    const char *comment;
    int (*func)(const int, const char **, void *);
    int argc_min;         /**< @brief  min number of words in cmd */
    int argc_max;         /**< @brief  max number of words in cmd */
};

struct analyze_command_state {
    struct bv_vlblock *vbp;
    int draw_pipe;
};

int
analyze_cmd_draw(const int argc, const char **argv, void *UNUSED(data))
{
    fprintf(stdout, "draw(%d): %s\n", argc, argv[0]);
    return 0;
}

int
analyze_cmd_quit(const int UNUSED(argc), const char **argv, void *UNUSED(data))
{
    bu_log("%s\n", argv[0]);
    return ANALYZE_DO_QUIT;
}

int
analyze_do_cmd(const char *cmdline, const struct analyze_command_table *tp, void *data)
{
    int cmd_found = 0;
    int retval = 0;
    int nwords = 0;
    char **cmd_args = NULL;
    char *lp = NULL;
    const struct analyze_command_table *tcmd = tp;

    if (!cmdline || !tp || !data) {
	bu_log("fatal command error\n");
	return ANALYZE_FATAL_ERROR;
    }

    lp = bu_strdup(cmdline);
    cmd_args = (char **)bu_calloc(strlen(lp) + 1, sizeof(char *), "args");
    nwords = bu_argv_from_string(cmd_args, strlen(lp), lp);
    if (nwords <= 0) {
	bu_free(cmd_args, "args array");
	bu_free(lp, "buffer copy");
	bu_log("fatal command error\n");
	return ANALYZE_FATAL_ERROR;
    }

    for (; tcmd->cmd != NULL; tcmd++) {
	if (!BU_STR_EQUAL(cmd_args[0], tcmd->cmd)) {
	    continue;
	}

	cmd_found = 1;

	if ((nwords >= tcmd->argc_min) && ((tcmd->argc_max < 0) || (nwords <= tcmd->argc_max))) {
	    retval = tcmd->func(nwords, (const char **)cmd_args, data);
	    break;
	}

	/* Let the user know what got us here */
	if (nwords < tcmd->argc_min) {
	    bu_log("Error: a %s command needs at least %d arguments, found %d\n", tcmd->cmd, tcmd->argc_min, nwords);
	    retval = 0;
	} else {
	    bu_log("Error: a %s command supports at most %d arguments, found %d\n", tcmd->cmd, tcmd->argc_max, nwords);
	    retval = 0;
	}
	break;
    }

    if (!cmd_found) {
	bu_log("Error: unknown command %s\n", cmd_args[0]);
	retval = 0;
    }

    bu_free(cmd_args, "args array");
    bu_free(lp, "buffer copy");

    return retval;
}

struct analyze_command_table analyze_commands[] = {
    {"draw", "obj", "add an object to the active list", analyze_cmd_draw, 2, -1},
    {"q", "", "quit", analyze_cmd_quit, 1, 1},
    {"quit", "", "quit", analyze_cmd_quit, 1, 1},
    {NULL, NULL, NULL, 0, 0, 0 /* END */}
};

int main(int UNUSED(argc), const char **argv)
{
    int ret = 0;
    int nret = 0;
    struct analyze_command_state *acs = NULL;
    struct bu_vls iline = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    /* For Windows */
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    /* Provide data in per-line chunks (TODO - why exactly does rt do this?  performance?) */
    bu_setlinebuf(stdout);
    bu_setlinebuf(stderr);

    BU_GET(acs, struct analyze_command_state);

    /* If we're on a tty we're interactive using linenoise, else read commands
     * from the stdin pipe.  (Eventually we will short circuit this rtwizard
     * style if the supplied options give us all we need to run a job or
     * interactive mode is disabled by explicit option, but at this stage we
     * just expose the two command driven modes.) */
    if (isatty(fileno(stdin))) {
	char *line = NULL;

	while ((line = linenoise("ganalyze: ")) != NULL) {
	    bu_vls_sprintf(&iline, "%s", line);
	    free(line);
	    bu_vls_trimspace(&iline);
	    if (!bu_vls_strlen(&iline)) continue;
	    linenoiseHistoryAdd(bu_vls_addr(&iline));

	    /* The "clear screen" subcommand only makes sense in interactive mode */
	    if (BU_STR_EQUAL(bu_vls_addr(&iline), "clear screen")) {
		linenoiseClearScreen();
		bu_vls_trunc(&iline, 0);
		continue;
	    }

	    nret = analyze_do_cmd(bu_vls_cstr(&iline), analyze_commands, (void *)acs);

	    bu_vls_trunc(&iline, 0);

	    if (nret == ANALYZE_DO_QUIT) {
		goto analyze_cleanup;
	    }

	    if (nret < -1) {
		ret = ANALYZE_FATAL_ERROR;
		goto analyze_cleanup;
	    }

	}
    } else {
	char *buf;
	while ((buf = rt_read_cmd( stdin )) != (char *)0) {
	    fprintf(stderr, "cmd: %s\n", buf);

	    nret = analyze_do_cmd(buf, analyze_commands, (void *)acs);

	    bu_free(buf, "analyze command buffer");

	    if (nret == ANALYZE_DO_QUIT) {
		break;
	    }

	    if (nret < -1) {
		ret = ANALYZE_FATAL_ERROR;
		break;
	    }
	}
    }

analyze_cleanup:
    BU_PUT(acs, struct analyze_command_state);
    bu_vls_free(&iline);

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

