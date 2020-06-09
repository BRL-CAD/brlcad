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
    s->quit = 0;

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
    s->norml_sig = NULL; /* active during interactive operation */
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


extern "C" int
_burst_cmd_critical_comp_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("critical-comp-file\n");
    return BRLCAD_OK;
}


extern "C" int
_burst_cmd_deflect_spall_cone (void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("deflect-spall-cone\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_dither_cells(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("dither-cells\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_enclose_target(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("enclose-target\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_enclose_portion(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("enclose-portion\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_error_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("error-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_execute(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("execute\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_grid_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("grid-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_ground_plane(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("ground-plane\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_help(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("help\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_air_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("burst-air-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_histogram_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("histogram-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_image_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("image-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_input_2d_shot(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("input-2d-shot\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_input_3d_shot(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("input-3d-shot\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_max_barriers(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("max-barriers\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_max_spall_rays(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("max-spall-rays\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_plot_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("plot-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_quit(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    s->quit = 1;

    bu_log("quit\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_2d_shot_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("read-2d-shot-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_3d_shot_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("read-3d-shot-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_armor_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("burst-armor-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_burst_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("read-burst-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_input_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("read-input-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_report_overlaps(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("report-overlaps\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_shotline_burst(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("shotline-burst\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_shotline_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("shotline-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_target_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("target-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_target_objects(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("target-objects\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_units(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("units\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_write_input_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("write-input-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_coordinates(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("burst-coordinates\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_distance(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("burst-distance\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("burst-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_cell_size(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("cell-size\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_color_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("color-file\n");
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_cone_half_angle(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    bu_log("cone-half-angle\n");
    return BRLCAD_OK;
}

const struct bu_cmdtab _burst_cmds[] = {
    { "attack-direction"  ,  _burst_cmd_attack_dir},
    { "critical-comp-file",  _burst_cmd_critical_comp_file },
    { "deflect-spall-cone",  _burst_cmd_deflect_spall_cone },
    { "dither-cells"      ,  _burst_cmd_dither_cells },
    { "enclose-target"    ,  _burst_cmd_enclose_target },
    { "enclose-portion"   ,  _burst_cmd_enclose_portion },
    { "error-file"        ,  _burst_cmd_error_file },
    { "execute"           ,  _burst_cmd_execute },
    { "grid-file"         ,  _burst_cmd_grid_file },
    { "ground-plane"      ,  _burst_cmd_ground_plane },
    { "help"              ,  _burst_cmd_help },
    { "burst-air-file"    ,  _burst_cmd_burst_air_file },
    { "histogram-file"    ,  _burst_cmd_histogram_file },
    { "image-file"        ,  _burst_cmd_image_file },
    { "input-2d-shot"     ,  _burst_cmd_input_2d_shot },
    { "input-3d-shot"     ,  _burst_cmd_input_3d_shot },
    { "max-barriers"      ,  _burst_cmd_max_barriers },
    { "max-spall-rays"    ,  _burst_cmd_max_spall_rays },
    { "plot-file"         ,  _burst_cmd_plot_file },
    { "quit"              ,  _burst_cmd_quit },
    { "read-2d-shot-file" ,  _burst_cmd_read_2d_shot_file },
    { "read-3d-shot-file" ,  _burst_cmd_read_3d_shot_file },
    { "burst-armor-file"  ,  _burst_cmd_burst_armor_file },
    { "read-burst-file"   ,  _burst_cmd_read_burst_file },
    { "read-input-file"   ,  _burst_cmd_read_input_file },
    { "report-overlaps"   ,  _burst_cmd_report_overlaps },
    { "shotline-burst"    ,  _burst_cmd_shotline_burst },
    { "shotline-file"     ,  _burst_cmd_shotline_file },
    { "target-file"       ,  _burst_cmd_target_file },
    { "target-objects"    ,  _burst_cmd_target_objects },
    { "units"             ,  _burst_cmd_units },
    { "write-input-file"  ,  _burst_cmd_write_input_file },
    { "burst-coordinates" ,  _burst_cmd_burst_coordinates },
    { "burst-distance"    ,  _burst_cmd_burst_distance },
    { "burst-file"        ,  _burst_cmd_burst_file },
    { "cell-size"         ,  _burst_cmd_cell_size },
    { "color-file"        ,  _burst_cmd_color_file },
    { "cone-half-angle"   ,  _burst_cmd_cone_half_angle },
    { (char *)NULL,      NULL}
};

int
burst_process_line(struct burst_state *s, const char *line)
{
    int ret = BRLCAD_OK;

    /* Skip a line if it is a comment */
    if (line[0] == '#') {
	return BRLCAD_OK;
    }

    /* Make an argv array from the input line */
    char *input = bu_strdup(line);
    char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    int ac = bu_argv_from_string(av, strlen(input), input);

    if (!ac || !bu_cmd_valid(_burst_cmds, av[0]) == BRLCAD_OK) {
	printf("unrecognzied command: %s\n", av[0]);
	ret = BRLCAD_ERROR;
	goto line_done;
    }

    if (bu_cmd(_burst_cmds, ac, (const char **)av, 0, (void *)s, &ret) != BRLCAD_OK) {
	printf("error running command: %s\n", av[0]);
	ret = BRLCAD_ERROR;
	goto line_done;
    }

line_done:
    bu_free(input, "input copy");
    bu_free(av, "input argv");
    return ret;
}


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
	linenoiseHistoryAdd(bu_vls_cstr(&iline));

	/* The "clear" command is only for the shell, not
	 * for burst evaluation */
	if (BU_STR_EQUAL(bu_vls_cstr(&iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	burst_process_line(&s, bu_vls_cstr(&iline));
	bu_vls_trunc(&iline, 0);

	if (s.quit) {
	    break;
	}
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
