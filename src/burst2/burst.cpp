/*                        B U R S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file burst/burst.cpp
 *
 */

#include "common.h"

#include "bio.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "linenoise.h"
}

#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/vls.h"

#include "fb.h"
#include "raytrace.h"

#include "./burst.h"

#define DEFAULT_BURST_PROMPT "burst> "

void
burst_state_init(struct burst_state *s)
{
    //Colors colorids;
    s->fbiop = NULL;
    s->burstfp = NULL;
    s->gridfp = NULL;
    s->histfp = NULL;
    s->outfp = NULL;
    s->plotfp = NULL;
    s->shotfp = NULL;
    s->shotlnfp = NULL;
    s->tmpfp = NULL;
    //Ids airids;
    //Ids armorids;
    //Ids critids;
    s->pixgrid = NULL;
    VSET(s->pixaxis, 255,   0,   0);
    VSET(s->pixbhit, 200, 255, 200);
    VSET(s->pixbkgr, 150, 100, 255);
    VSET(s->pixblack,  0,   0,   0);
    VSET(s->pixcrit, 255, 200, 200);
    VSET(s->pixghit, 255,   0, 255);
    VSET(s->pixmiss, 200, 200, 200);
    VSET(s->pixtarg, 255, 255, 255);
    s->plotline = 0;
    s->batchmode = 0;
    s->cantwarhead = 0;
    s->deflectcone = DFL_DEFLECT;
    s->dithercells = DFL_DITHER;
    s->fatalerror = 0;
    s->groundburst = 0;
    s->reportoverlaps = DFL_OVERLAPS;
    s->reqburstair = 1;
    s->shotburst = 0;
    s->tty = 1;
    s->userinterrupt = 0;
    memset(s->airfile, 0, LNBUFSZ);
    memset(s->armorfile, 0, LNBUFSZ);
    memset(s->burstfile, 0, LNBUFSZ);
    memset(s->cmdbuf, 0, LNBUFSZ);
    memset(s->cmdname, 0, LNBUFSZ);
    memset(s->colorfile, 0, LNBUFSZ);
    memset(s->critfile, 0, LNBUFSZ);
    memset(s->errfile, 0, LNBUFSZ);
    memset(s->fbfile, 0, LNBUFSZ);
    memset(s->gedfile, 0, LNBUFSZ);
    memset(s->gridfile, 0, LNBUFSZ);
    memset(s->histfile, 0, LNBUFSZ);
    memset(s->objects, 0, LNBUFSZ);
    memset(s->outfile, 0, LNBUFSZ);
    memset(s->plotfile, 0, LNBUFSZ);
    memset(s->scrbuf, 0, LNBUFSZ);
    memset(s->scriptfile, 0, LNBUFSZ);
    memset(s->shotfile, 0, LNBUFSZ);
    memset(s->shotlnfile, 0, LNBUFSZ);
    memset(s->title, 0, TITLE_LEN);
    memset(s->timer, 0, TIMER_LEN);
    memset(s->tmpfname, 0, TIMER_LEN);
    s->cmdptr = NULL;
    s->bdist = DFL_BDIST;
    VSET(s->burstpoint, 0.0, 0.0, 0.0);
    s->cellsz = DFL_CELLSIZE;
    s->conehfangle = DFL_CONEANGLE;
    VSET(s->fire, 0.0, 0.0, 0.0);
    s->griddn = 0.0;
    s->gridlf = 0.0;
    s->gridrt = 0.0;
    s->gridup = 0.0;
    VSET(s->gridhor, 0.0, 0.0, 0.0);
    VSET(s->gridsoff, 0.0, 0.0, 0.0);
    VSET(s->gridver, 0.0, 0.0, 0.0);
    s->grndbk = 0.0;
    s->grndht = 0.0;
    s->grndfr = 0.0;
    s->grndlf = 0.0;
    s->grndrt = 0.0;
    VSET(s->modlcntr, 0.0, 0.0, 0.0);
    s->modldn = 0.0;
    s->modllf = 0.0;
    s->modlrt = 0.0;
    s->modlup = 0.0;
    s->raysolidangle = 0.0;
    s->standoff = 0.0;
    s->unitconv = 1.0;
    s->viewazim = DFL_AZIMUTH;
    s->viewelev = DFL_ELEVATION;
    s->pitch = 0.0;
    s->yaw = 0.0;
    VSET(s->xaxis, 1.0, 0.0, 0.0);
    VSET(s->zaxis, 0.0, 0.0, 1.0);
    VSET(s->negzaxis, 0.0, 0.0, -1.0);
    s->co = 0;
    s->devwid = 0;
    s->devhgt = 0;
    s->firemode = FM_DFLT;
    s->gridsz = 512;
    s->gridxfin = 0;
    s->gridyfin = 0;
    s->gridxorg = 0;
    s->gridyorg = 0;
    s->gridwidth = 0;
    s->gridheight = 0;
    s->li = 0;
    s->nbarriers = DFL_BARRIERS;
    s->noverlaps = 0;
    s->nprocessors = 0;
    s->nriplevels = DFL_RIPLEVELS;
    s->nspallrays = DFL_NRAYS;
    s->units = 1;
    s->zoom = 1;
    s->rtip = RTI_NULL;
    s->norml_sig = NULL;	/* active during interactive operation */
    s->abort_sig = NULL; /* active during ray tracing only */
}


extern "C" int
_burst_cmd_attack_dir(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("attack-direction\n");
    return BRLCAD_OK;
}



const struct bu_cmdtab _burst_cmds[] = {
    { "attack-direction",     _burst_cmd_attack_dir},
    { (char *)NULL,      NULL}
};

int
main(int argc, const char **argv)
{
    int ret = EXIT_SUCCESS;
    char *line = NULL;
    int print_help = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_vls iline= BU_VLS_INIT_ZERO;
    const char *gpmpt = DEFAULT_BURST_PROMPT;
    struct burst_state s;
    struct bu_opt_desc d[2];
    BU_OPT(d[0],  "h", "help", "",       NULL,              &print_help,     "print help and exit");
    BU_OPT_NULL(d[1]);

    if (argc == 0 || !argv) return -1;

    /* Let bu_brlcad_root and friends know where we are */
    bu_setprogname(argv[0]);

    /* Done with program name */
    argv++; argc--;

    /* Parse options, fail if anything goes wrong */
    if ((argc = bu_opt_parse(&msg, argc, argv, d)) == -1) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, NULL);
    }
    bu_vls_trunc(&msg, 0);

    if (print_help) {
	const char *help = bu_opt_describe(d, NULL);
	bu_vls_sprintf(&msg, "Usage: 'burst [options] cmd_file'\n\nOptions:\n%s\n", help);
	bu_free((char *)help, "done with help string");
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* We're in business - initalize the burst state */
    burst_state_init(&s);

    /* If we have a file, batch mode it. */
    if (argc > 1) {
	argv++; argc--;
	bu_exit(EXIT_SUCCESS, "Batch mode: [%s]\n", argv[0]);
    }

    /* Start the interactive loop */
    while ((line = linenoise(gpmpt)) != NULL) {
	bu_vls_sprintf(&iline, "%s", line);
	free(line);
	bu_vls_trimspace(&iline);
	if (!bu_vls_strlen(&iline)) continue;
	linenoiseHistoryAdd(bu_vls_addr(&iline));

	/* The "clear" command is only for the shell, not
	 * for burst evaluation */
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	/* Make an argv array from the input line */
	char *input = bu_strdup(bu_vls_cstr(&iline));
	char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(input), input);

	if (!ac || !bu_cmd_valid(_burst_cmds, av[0]) == BRLCAD_OK) {
	    printf("unrecognzied command: %s\n", av[0]);
	    goto cmd_cont;
	}

	if (bu_cmd(_burst_cmds, ac, (const char **)av, 0, (void *)&s, &ret) != BRLCAD_OK) {
	    printf("error running command: %s\n", av[0]);
	    goto cmd_cont;
	}

cmd_cont:
	bu_free(input, "input copy");
	bu_free(av, "input argv");
	bu_vls_trunc(&iline, 0);
    }

    linenoiseHistoryFree();
    bu_vls_free(&msg);
    bu_vls_free(&iline);
    return ret;
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
