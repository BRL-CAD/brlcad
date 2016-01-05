/*                             N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
/** @file libged/nmg.c
 *
 * The nmg command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_private.h"

extern int ged_nmg_mm(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_cmface(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kill_v(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kill_f(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_move_v(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_make_v(struct ged *gedp, int argc, const char *argv[]);

int
ged_nmg(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "nmg object subcommand [V|F|R|S] [suffix]";
    const char *subcmd = argv[2];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* must be wanting help */
    if (argc < 3) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
    bu_vls_printf(gedp->ged_result_str, "commands:\n");
    bu_vls_printf(gedp->ged_result_str, "\tmm             -  creates a new "
            "NMG model structure and fills the appropriate fields. The result "
            "is an empty model.\n");
    bu_vls_printf(gedp->ged_result_str, "\tcmface         -  creates a "
                "manifold face in the first encountered shell of the NMG "
                "object. Vertices are listed as the suffix and define the "
                "winding-order of the face.\n");
    bu_vls_printf(gedp->ged_result_str, "\tkill V         -  removes the "
            "vertexuse and vertex geometry of the selected vertex (via its "
            "coordinates) and higher-order topology containing the vertex. "
            "When specifying vertex to be removed, user generally will display "
            "vertex coordinates in object via the MGED command labelvert.\n");
    bu_vls_printf(gedp->ged_result_str, "\tkill F         -  removes the "
                "faceuse and face geometry of the selected face (via its "
                "index). When specifying the face to be removed, user generally "
                "will display face indices in object via the MGED command "
                "labelface.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmove V         -  moves an existing "
                "vertex specified by the coordinates x_initial y_initial "
                "z_initial to the position with coordinates x_final y_final "
                "z_final.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmake V         -  creates a new "
                "vertex in the nmg object.\n");
    return GED_HELP;
    }

    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_ERROR;
    }

    /* advance CLI arguments for subcommands */
    --argc;
    ++argv;

    if( BU_STR_EQUAL( "mm", subcmd ) ) {
        ged_nmg_mm(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "cmface", subcmd ) ) {
        ged_nmg_cmface(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kill", subcmd ) ) {
        const char* opt = argv[2];
        if ( BU_STR_EQUAL( "V", opt ) ) {
            ged_nmg_kill_v(gedp, argc, argv);
        } else if ( BU_STR_EQUAL( "F", opt ) ) {
            ged_nmg_kill_f(gedp, argc, argv);
        }
    }
    else if( BU_STR_EQUAL( "move", subcmd ) ) {
        const char* opt = argv[2];
        if ( BU_STR_EQUAL( "V", opt ) ) {
            ged_nmg_move_v(gedp, argc, argv);
        }
    }
    else if( BU_STR_EQUAL( "make", subcmd ) ) {
        const char* opt = argv[2];
        if ( BU_STR_EQUAL( "V", opt ) ) {
            ged_nmg_make_v(gedp, argc, argv);
        }
    }
    else {
        bu_vls_printf(gedp->ged_result_str, "%s is not a subcommand.", subcmd );
        return GED_ERROR;
    }

    return GED_OK;
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
