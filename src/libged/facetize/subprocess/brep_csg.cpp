/*                    B R E P _ C S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/tessellate/brep_csg.cpp
 *
 * Try to break a brep solid down into a CSG implicit tree and/or
 * simpler B-Rep solids to see if we can achieve a more successful
 * facetization result.
 */

#include "common.h"

#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "rt/primitives/bot.h"
#include "ged.h"
#include "./tessellate.h"

int
_brep_csg_tessellate(struct ged *gedp, struct directory *dp, tess_opts *s)
{
    if (!gedp || !dp || !s)
	return BRLCAD_ERROR;

    char tmpfil[MAXPATHLEN];
    bu_dir(tmpfil, MAXPATHLEN, BU_DIR_TEMP, bu_temp_file_name(NULL, 0), NULL);

    const char *av[MAXPATHLEN];
    av[0] = "keep";
    av[1] = tmpfil;
    av[2] = dp->d_namep;
    av[3] = NULL;
    ged_exec_keep(gedp, 3, av);

    // Work on the brep
    struct ged *wgedp = ged_open("db", tmpfil, 1);
    if (!wgedp) {
	bu_file_delete(tmpfil);
	return BRLCAD_ERROR;
    }

    av[0] = "brep";
    av[1] = dp->d_namep;
    av[2] = "csg";
    av[3] = NULL;
    if (ged_exec_brep(wgedp, 3, av) != BRLCAD_OK) {
	bu_file_delete(tmpfil);
	return BRLCAD_ERROR;
    }

    av[0] = "kill";
    av[2] = dp->d_namep;
    av[3] = NULL;
    if (ged_exec_kill(wgedp, 2, av) != BRLCAD_OK) {
	bu_file_delete(tmpfil);
	return BRLCAD_ERROR;
    }

    struct bu_vls core_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_path_component(&core_name, dp->d_namep, BU_PATH_BASENAME_EXTLESS);
    bu_vls_sprintf(&comb_name, "csg_%s.c", bu_vls_cstr(&core_name));
    bu_vls_free(&core_name);


    av[0] = "facetize";
    av[1] = "-q";
    av[2] = "--methods";
    av[3] = "NMG";
    av[4] = bu_vls_cstr(&comb_name);
    av[5] = dp->d_namep;
    av[6] = NULL;
    if (ged_exec_facetize(wgedp, 6, av) != BRLCAD_OK) {
	bu_vls_free(&comb_name);
	bu_file_delete(tmpfil);
	return BRLCAD_ERROR;
    }

    av[0] = "killtree";
    av[1] = "-f";
    av[2] = bu_vls_cstr(&comb_name);
    av[3] = NULL;
    if (ged_exec_killtree(wgedp, 3, av) != BRLCAD_OK) {
	bu_vls_free(&comb_name);
	bu_file_delete(tmpfil);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&comb_name);
    ged_close(wgedp);

    std::string oname(dp->d_namep);

    av[0] = "dbconcat";
    av[1] = "-O";
    av[2] = tmpfil;
    av[3] = NULL;
    if (ged_exec_dbconcat(gedp, 3, av) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    bu_file_delete(tmpfil);

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

