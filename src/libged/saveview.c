/*                         S A V E V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file saveview.c
 *
 * The saveview command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bio.h"
#include "solid.h"
#include "ged_private.h"

static char *ged_basename_without_suffix(register const char *p1, register const char *suff);


int
ged_saveview(struct ged *gedp, int argc, const char *argv[])
{
    register struct solid *sp;
    register int i;
    register FILE *fp;
    char *base;
    int c;
    char rtcmd[255] = {'r', 't', 0};
    char outlog[255] = {0};
    char outpix[255] = {0};
    char inputg[255] = {0};
    static const char *usage = "[-e] [-i] [-l] [-o] filename [args]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* invalid command name */
    if (argc < 1) {
	bu_vls_printf(&gedp->ged_result_str, "Error: command name not provided");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "e:i:l:o:")) != EOF) {
	switch (c) {
	    case 'e':
		snprintf(rtcmd, 255, "%s", bu_optarg);
		break;
	    case 'l':
		snprintf(outlog, 255, "%s", bu_optarg);
		break;
	    case 'o':
		snprintf(outpix, 255, "%s", bu_optarg);
		break;
	    case 'i':
		snprintf(inputg, 255, "%s", bu_optarg);
		break;
	    default: {
		bu_vls_printf(&gedp->ged_result_str, "Option '%c' unknown\n", c);
		bu_vls_printf(&gedp->ged_result_str, "help saveview");
		return BRLCAD_ERROR;
	    }
	}
    }
    argc -= bu_optind-1;
    argv += bu_optind-1;

    if (argc < 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if ( (fp = fopen( argv[1], "a")) == NULL )  {
	perror(argv[1]);
	return BRLCAD_ERROR;
    }
    (void)bu_fchmod(fp, 0755);	/* executable */

    if (!gedp->ged_wdbp->dbip->dbi_filename) {
	bu_log("Error: geometry file is not specified\n");
	return BRLCAD_ERROR;
    }

    if (!bu_file_exists(gedp->ged_wdbp->dbip->dbi_filename)) {
	bu_log("Error: %s does not exist\n", gedp->ged_wdbp->dbip->dbi_filename);
	return BRLCAD_ERROR;
    }

    base = ged_basename_without_suffix( argv[1], ".sh" );
    if (outpix[0] == '\0') {
	snprintf(outpix, 255, "%s.pix", base);
    }
    if (outlog[0] == '\0') {
	snprintf(outlog, 255, "%s.log", base);
    }

    /* Do not specify -v option to rt; batch jobs must print everything. -Mike */
    (void)fprintf(fp, "#!/bin/sh\n%s -M ", rtcmd);
    if ( gedp->ged_gvp->gv_perspective > 0 )
	(void)fprintf(fp, "-p%g ", gedp->ged_gvp->gv_perspective);
    for ( i=2; i < argc; i++ )
	(void)fprintf(fp, "%s ", argv[i]);

    if (strncmp(rtcmd,"nirt",4) != 0)
	(void)fprintf(fp, "\\\n -o %s\\\n $*\\\n", outpix);

    if (inputg[0] == '\0') {
	snprintf(inputg, 255, "%s", gedp->ged_wdbp->dbip->dbi_filename);
    }
    (void)fprintf(fp, " %s\\\n ", inputg);

    /* Find all unique top-level entries.
     *  Mark ones already done with s_wflag == UP
     */
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid)
	sp->s_wflag = DOWN;
    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid)  {
	register struct solid *forw;	/* XXX */
	struct directory *dp = FIRST_SOLID(sp);

	if ( sp->s_wflag == UP )
	    continue;
	if (dp->d_addr == RT_DIR_PHONY_ADDR) continue;
	(void)fprintf(fp, "'%s' ", dp->d_namep);
	sp->s_wflag = UP;
	for (BU_LIST_PFOR(forw, sp, solid, &gedp->ged_gdp->gd_headSolid)) {
	    if ( FIRST_SOLID(forw) == dp )
		forw->s_wflag = UP;
	}
    }
    (void)fprintf(fp, "\\\n 2>> %s\\\n", outlog);
    (void)fprintf(fp, " <<EOF\n");

    {
	vect_t eye_model;

	ged_rt_set_eye_model(gedp, eye_model);
	ged_rt_write(gedp, fp, eye_model);
    }

    (void)fprintf(fp, "\nEOF\n");
    (void)fclose( fp );

    FOR_ALL_SOLIDS(sp, &gedp->ged_gdp->gd_headSolid)
	sp->s_wflag = DOWN;

    return BRLCAD_OK;
}

/**
 *  Return basename of path, removing leading slashes and trailing suffix.
 */
static char *
ged_basename_without_suffix(register const char *p1, register const char *suff)
{
    register char *p2, *p3;
    static char buf[128];

    /* find the basename */
    p2 = (char *)p1;
    while (*p1) {
	if (*p1++ == '/')
	    p2 = (char *)p1;
    }

    /* find the end of suffix */
    for (p3=(char *)suff; *p3; p3++)
	;

    /* early out */
    while (p1>p2 && p3>suff) {
	if (*--p3 != *--p1)
	    return(p2);
    }

    /* stash and return filename, sans suffix */
    bu_strlcpy( buf, p2, p1-p2+1 );
    return(buf);
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
