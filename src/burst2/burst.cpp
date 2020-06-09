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
#include "bu/units.h"
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
    s->errfile = NULL;
    bu_vls_init(&s->fbfile);
    bu_vls_init(&s->gedfile);
    memset(s->gridfile, 0, LNBUFSZ);
    memset(s->histfile, 0, LNBUFSZ);
    bu_vls_init(&s->objects);
    memset(s->outfile, 0, LNBUFSZ);
    bu_vls_init(&s->plotfile);
    memset(s->scrbuf, 0, LNBUFSZ);
    memset(s->scriptfile, 0, LNBUFSZ);
    memset(s->shotfile, 0, LNBUFSZ);
    memset(s->shotlnfile, 0, LNBUFSZ);
    memset(s->title, 0, TITLE_LEN);
    memset(s->timer, 0, TIMER_LEN);
    bu_vls_init(&s->cmdhist);
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
    s->cmds = NULL;
}


extern "C" int
_burst_cmd_attack_dir(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 3) {
	printf("Usage: attack-direction az(deg) el(deg)\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->viewazim) < 0) {
	printf("problem reading azimuth: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->viewelev) < 0) {
	printf("problem reading elevation: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }

    // TODO logCMd

    bu_vls_free(&msg);
    return ret;
}


extern "C" int
_burst_cmd_critical_comp_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: critical-comp-file file\n");
	return BRLCAD_ERROR;
    }

    FILE *critfp = fopen(argv[1], "rb");
    if (!critfp) {
    	printf("failed to open critical component file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    // TODO logCMd

    printf("Reading critical component idents...\n");

    // TODO readIdents

    printf("Reading critical component idents... done.\n");
    fclose(critfp);

    return ret;
}


extern "C" int
_burst_cmd_deflect_spall_cone (void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: deflect-spall-cone yes|no\n");
	return BRLCAD_ERROR;
    }

    int tval = bu_str_true(argv[1]);
    int fval = bu_str_false(argv[1]);

    if (!tval && !fval) {
    	printf("Invalid boolean string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->deflectcone = (fval) ? 0 : tval;

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_dither_cells(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: dither-cells yes|no\n");
	return BRLCAD_ERROR;
    }

    int tval = bu_str_true(argv[1]);
    int fval = bu_str_false(argv[1]);

    if (!tval && !fval) {
    	printf("Invalid boolean string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->dithercells = (fval) ? 0 : tval;

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_enclose_target(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct burst_state *s = (struct burst_state *)bs;
    s->firemode = FM_GRID;
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_enclose_portion(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 5) {
	printf("Usage: enclose-portion left right bottom top\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->gridlf) < 0) {
	printf("problem reading left border of grid: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->gridlf = s->gridlf * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->gridrt) < 0) {
	printf("problem reading right border of grid: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->gridrt = s->gridrt * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[3], (void *)&s->griddn) < 0) {
	printf("problem reading bottom border of grid: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->griddn = s->griddn * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[4], (void *)&s->gridup) < 0) {
	printf("problem reading top border of grid: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->gridup = s->gridup * s->unitconv;

    bu_vls_free(&msg);
    return ret;
}

extern "C" int
_burst_cmd_error_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: error-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->errfile) {
	fclose(s->errfile);
	s->errfile = NULL;
    }

    /* If we're given a NULL argument, disable the error file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	return ret;
    }

    /* Try to open the file - we want to write messages to the file
     * as they are generated. */
    s->errfile = fopen(argv[1], "wb");
    if (!s->errfile) {
    	printf("failed to open error file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    return ret;
}

extern "C" int
_burst_cmd_execute(void *bs, int UNUSED(argc), const char **UNUSED(argv))
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s) return BRLCAD_ERROR;

    if (!bu_vls_strlen(&s->gedfile)) {
    	printf("Execute failed: no target file has been specified\n");
	return BRLCAD_ERROR;
    }

    // TODO
    //ret = execute_run(s);

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_grid_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: grid-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->gridfp) {
	fclose(s->gridfp);
	s->gridfp = NULL;
    }

    /* If we're given a NULL argument, disable the grid file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	return ret;
    }

    /* Try to open the file */
    s->gridfp = fopen(argv[1], "wb");
    if (!s->gridfp) {
    	printf("failed to open grid file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    return ret;
}

extern "C" int
_burst_cmd_ground_plane(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2 && argc != 7) {
	printf("Usage: ground-plane no|yes height xpos xneg ypos yneg\n");
	return BRLCAD_ERROR;
    }

    int tval = bu_str_true(argv[1]);
    int fval = bu_str_false(argv[1]);

    if (!tval && !fval) {
    	printf("Invalid boolean string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->groundburst = (fval) ? 0 : tval;

    if (!s->groundburst) {
	return ret;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->grndht) < 0) {
	printf("problem reading distance of target origin above ground plane: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->grndht = s->grndht * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->grndfr) < 0) {
	printf("problem reading distance out positive X-axis of target to edge: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->grndfr = s->grndfr * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->grndbk) < 0) {
	printf("problem reading distance out negative X-axis of target to edge: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->grndbk = s->grndbk * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->grndlf) < 0) {
	printf("problem reading distance out positive Y-axis of target to edge: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->grndlf = s->grndlf * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->grndrt) < 0) {
	printf("problem reading distance out negative Y-axis of target to edge: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->grndrt = s->grndrt * s->unitconv;

    bu_vls_free(&msg);
    return ret;
}

extern "C" int
_burst_cmd_help(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    const struct bu_cmdtab *ctp = NULL;
    for (ctp = s->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	printf("%s\n", ctp->ct_name);
    }
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_air_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: burst-air-file file\n");
	return BRLCAD_ERROR;
    }

    FILE *airfp = fopen(argv[1], "rb");
    if (!airfp) {
    	printf("failed to open burst air file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    // TODO logCMd

    printf("Reading burst air idents...\n");

    // TODO readIdents

    printf("Reading burst air idents... done.\n");
    fclose(airfp);

    return ret;
}

extern "C" int
_burst_cmd_histogram_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;


    if (argc != 2) {
	printf("Usage: histogram-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->histfp) {
	fclose(s->histfp);
	s->histfp = NULL;
    }

    /* If we're given a NULL argument, disable the grid file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	return ret;
    }

    /* Try to open the file */
    s->histfp = fopen(argv[1], "wb");
    if (!s->histfp) {
    	printf("failed to open histogram file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_image_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: image-file file\n");
	return BRLCAD_ERROR;
    }

    bu_vls_sprintf(&s->fbfile, "%s", argv[1]);
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_input_2d_shot(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 3) {
	printf("Usage: input-2d-shot Y Z\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->fire[X]) < 0) {
	printf("problem reading coordinate: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->fire[X] = s->fire[X]* s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->fire[Y]) < 0) {
	printf("problem reading coordinate: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->fire[Y] = s->fire[Y]* s->unitconv;

    s->firemode = FM_SHOT;

    return ret;
}

extern "C" int
_burst_cmd_input_3d_shot(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 4) {
	printf("Usage: input-3d-shot X Y Z\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->fire[X]) < 0) {
	printf("problem reading coordinate: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->fire[X] = s->fire[X]* s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[2], (void *)&s->fire[Y]) < 0) {
	printf("problem reading coordinate: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->fire[Y] = s->fire[Y]* s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[3], (void *)&s->fire[Z]) < 0) {
	printf("problem reading coordinate: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->fire[Z] = s->fire[Z]* s->unitconv;

    s->firemode = FM_SHOT | FM_3DIM;

    return ret;
}

extern "C" int
_burst_cmd_max_barriers(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: max-barriers #\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_int(&msg, 1, &argv[1], (void *)&s->nbarriers) < 0) {
	printf("problem reading spall barriers per ray count: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }

    return ret;
}

extern "C" int
_burst_cmd_max_spall_rays(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: max-spall-rays #\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_int(&msg, 1, &argv[1], (void *)&s->nspallrays) < 0) {
	printf("problem reading maximum rays per burst: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }

    return ret;
}

extern "C" int
_burst_cmd_plot_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: plot-file file\n");
	return BRLCAD_ERROR;
    }

    bu_vls_sprintf(&s->plotfile, "%s", argv[1]);
    
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_quit(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    s->quit = 1;

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_2d_shot_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: read-2d-shot-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->shotfp) {
	fclose(s->shotfp);
	s->shotfp = NULL;
    }

    /* If we're given a NULL argument, disable the error file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	s->firemode = 0;
	return ret;
    }

    s->shotfp = fopen(argv[1], "rb");
    if (!s->shotfp) {
    	printf("failed to open critical component file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    s->firemode = FM_SHOT | FM_FILE;

    return ret;
}

extern "C" int
_burst_cmd_read_3d_shot_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: read-3d-shot-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->shotfp) {
	fclose(s->shotfp);
	s->shotfp = NULL;
    }

    /* If we're given a NULL argument, disable the error file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	s->firemode = 0;
	return ret;
    }

    s->shotfp = fopen(argv[1], "rb");
    if (!s->shotfp) {
    	printf("failed to open critical component file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    s->firemode = FM_SHOT | FM_FILE | FM_3DIM;
    
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_burst_armor_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: burst-armor-file file\n");
	return BRLCAD_ERROR;
    }

    FILE *armorfp = fopen(argv[1], "rb");
    if (!armorfp) {
    	printf("failed to open burst armor file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    // TODO logCMd

    printf("Reading burst armor idents...\n");

    // TODO readIdents

    printf("Reading burst armor idents... done.\n");
    fclose(armorfp);

    return ret;
}

extern "C" int
_burst_cmd_read_burst_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: read-burst-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->burstfp) {
	fclose(s->burstfp);
	s->burstfp= NULL;
    }

    /* If we're given a NULL argument, disable the error file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	s->firemode = 0;
	return ret;
    }

    s->burstfp = fopen(argv[1], "rb");
    if (!s->burstfp) {
    	printf("failed to open 3-D burst input file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    s->firemode = FM_BURST | FM_3DIM | FM_FILE;

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_read_input_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    FILE *cmdfp = fopen(argv[1], "rb");
    if (!cmdfp) {
    	printf("failed to open command file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    // TODO - use bu_cmd to process the lines in the file

    fclose(cmdfp);
    return ret;
}

extern "C" int
_burst_cmd_report_overlaps(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: report-overlaps yes|no\n");
	return BRLCAD_ERROR;
    }

    int tval = bu_str_true(argv[1]);
    int fval = bu_str_false(argv[1]);

    if (!tval && !fval) {
    	printf("Invalid boolean string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->reportoverlaps = (fval) ? 0 : tval;

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_shotline_burst(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc < 2 || argc > 3) {
	printf("Usage: shotline-burst no|yes no|yes\n");
	return BRLCAD_ERROR;
    }

    int tval = bu_str_true(argv[1]);
    int fval = bu_str_false(argv[1]);

    if (!tval && !fval) {
    	printf("Invalid boolean string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    s->shotburst = (fval) ? 0 : tval;

    if (s->shotburst) {

	if (argc != 3) {
	    printf("Usage: shotline-burst no|yes no|yes\n");
	    return BRLCAD_ERROR;
	}

	tval = bu_str_true(argv[2]);
	fval = bu_str_false(argv[2]);

	if (!tval && !fval) {
	    printf("Invalid boolean string: %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	s->reqburstair = (fval) ? 0 : tval;

	s->firemode &= ~FM_BURST; /* disable discrete burst points */
    }

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_shotline_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: shotline-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->shotlnfp) {
	fclose(s->shotlnfp);
	s->shotlnfp = NULL;
    }

    /* If we're given a NULL argument, disable the shotline file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	return ret;
    }

    /* Try to open the file - we want to write output to the file
     * as they are generated. */
    s->shotlnfp = fopen(argv[1], "wb");
    if (!s->shotlnfp) {
    	printf("failed to open error file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    bu_log("shotline-file\n");
    return ret;
}

extern "C" int
_burst_cmd_target_file(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: target-file file\n");
	return BRLCAD_ERROR;
    }

    bu_vls_sprintf(&s->gedfile, "%s", argv[1]);
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_target_objects(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: target-objects objects\n");
	return BRLCAD_ERROR;
    }

    bu_vls_sprintf(&s->objects, "%s", argv[1]);
    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_units(void *bs, int argc, const char **argv)
{
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: units unit\n");
	return BRLCAD_ERROR;
    }

    s->unitconv =  bu_units_conversion(argv[1]);
    if (NEAR_ZERO(s->unitconv, SMALL_FASTF)) {
	printf("Invalid unit: %s\n", argv[1]);
	s->unitconv = 1.0;
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_burst_cmd_write_input_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    /* Try to open the file - we want to write messages to the file
     * as they are generated. */
    FILE *cmdfp = fopen(argv[1], "wb");
    if (!cmdfp) {
    	printf("failed to open cmd file for writing: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    fprintf(cmdfp, "%s", bu_vls_cstr(&s->cmdhist));

    fclose(cmdfp);

    return ret;
}

extern "C" int
_burst_cmd_burst_coordinates(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 4) {
	printf("Usage: burst-coordinates X Y Z\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->burstpoint[X]) < 0) {
	printf("problem reading coordinate X value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->burstpoint[X] = s->burstpoint[X] * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->burstpoint[Y]) < 0) {
	printf("problem reading coordinate Y value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->burstpoint[Y] = s->burstpoint[Y] * s->unitconv;

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->burstpoint[Z]) < 0) {
	printf("problem reading coordinate Z value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->burstpoint[Z] = s->burstpoint[Z] * s->unitconv;

    bu_vls_free(&msg);
    return ret;
}

extern "C" int
_burst_cmd_burst_distance(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: burst-distance #\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->bdist) < 0) {
	printf("problem reading distance value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->bdist = s->bdist * s->unitconv;

    return ret;
}

extern "C" int
_burst_cmd_burst_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: burst-file file\n");
	return BRLCAD_ERROR;
    }

    /* If we had a previous file open, close it */
    if (s->outfp) {
	fclose(s->outfp);
	s->outfp = NULL;
    }

    /* If we're given a NULL argument, disable the burst file */
    if (BU_STR_EQUAL(argv[1], "NULL") || BU_STR_EQUAL(argv[1], "/dev/NULL")) {
	return ret;
    }

    /* Try to open the file - we want to write output to the file
     * as it is generated. */
    s->outfp = fopen(argv[1], "wb");
    if (!s->outfp) {
    	printf("failed to open burst file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    return ret;
}

extern "C" int
_burst_cmd_cell_size(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: cell-size #\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->cellsz) < 0) {
	printf("problem reading cell size value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }
    /* convert to mm */
    s->cellsz = s->cellsz * s->unitconv;

    return ret;
}

extern "C" int
_burst_cmd_color_file(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;


    FILE *colorfp = fopen(argv[1], "rb");
    if (!colorfp) {
    	printf("failed to open ident-to-color mapping file: %s\n", argv[1]);
	ret = BRLCAD_ERROR;
    }

    // TODO logCMd

    printf("Reading ident-to-color mappings...\n");

    // TODO readColors

    printf("Reading ident-to-color mappings... done.\n");
    
    fclose(colorfp);

    return ret;
}

extern "C" int
_burst_cmd_cone_half_angle(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct burst_state *s = (struct burst_state *)bs;

    if (!s || !argc || !argv) return BRLCAD_ERROR;

    if (argc != 2) {
	printf("Usage: cos-half-angle angle(deg)\n");
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(&msg, 1, &argv[1], (void *)&s->conehfangle) < 0) {
	printf("problem reading cone half angle value: %s\n", bu_vls_cstr(&msg));
	ret = BRLCAD_ERROR;
    }

    return ret;
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
    s.cmds = _burst_cmds;

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

	/* execute the command */
	burst_process_line(&s, bu_vls_cstr(&iline));

	/* build up a command history buffer for write-input-file */
	bu_vls_printf(&s.cmdhist, "%s\n", bu_vls_cstr(&iline));

	/* reset the line */
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
