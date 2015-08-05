/*                             N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
extern int ged_nmg_mmr(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_msv(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mrsv(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mvvu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mvu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_me(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_meonvu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_eusplit(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_esplit(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_eins(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_ml(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mlv(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mf(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_cface(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_cmface(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_keu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kfu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_klu(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_km(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kr(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_ks(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kvu(struct ged *gedp, int argc, const char *argv[]);

int
ged_nmg(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "nmg [command|suffix] ";
    const char *subcmd = argv[1];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* must be wanting help */
    if (argc < 2) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
    bu_vls_printf(gedp->ged_result_str, "commands:\n");
    bu_vls_printf(gedp->ged_result_str, "\tmm             -  creates a new "
            "NMG model structure and fills the appropriate fields. The result "
            "is an empty model.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmmr            -  creates a new "
            "model and creates a region within the model.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmsv            -  creates a new "
            "shell consisting of a single vertex in the parameter region. A "
            "new vertex is created for the shell.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmrsv           -  creates a new "
            "region within the existing model, and creates a shell of a single "
            "vertex within the region.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmvvu           -  exists so that "
            "shells, loops and edges can be created on a new vertex.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmvu            -  allocates a new "
            "vertexuse for an existing vertex.\n");
    bu_vls_printf(gedp->ged_result_str, "\tme             -  creates a new "
            "wire edge in the shell specified.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmeonvu         -  is used to create "
            "an edge on a vertex in a loop or shell. The resultant edge has "
            "the same vertex at each endpoint.\n");
    bu_vls_printf(gedp->ged_result_str, "\teusplit        -  splits an "
            "existing edgeuse pair of a wire or dangling face-edge by "
            "inserting a new vertex.\n");
    bu_vls_printf(gedp->ged_result_str, "\tesplit         -  causes a new "
            "vertex to be inserted along an existing edge.\n");
    bu_vls_printf(gedp->ged_result_str, "\teins           -  inserts a new, "
            "zero length edge between the edge associated with the parameter "
            "edgeuse and the edge associated with the edgeuse previous to the "
            "parameter edgeuse.\n");
    bu_vls_printf(gedp->ged_result_str, "\tml             -  takes the largest "
            "possible number of contiguous wire edges which form a circuit "
            "from the parameter shell and uses them to create a wire loop in "
            "the shell.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmlv            -  creates a new "
            "vertex-loop. The loop will be a child of the structure indicated "
            "by the magic number pointer parameter, and will have the "
            "specified orientation. If the vertex is NULL, a new vertex is "
            "created for the loop.\n");
    bu_vls_printf(gedp->ged_result_str, "\tmlf            -  generates a new "
            "face from the parameter wire loop and its mate.\n");
    return GED_HELP;
    }

    if (argc < 2) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
    }

    /* advance CLI arguments for subcommands */
    --argc;
    ++argv;

    /* NMG CONSTRUCTION ROUTINES */
    if( BU_STR_EQUAL( "mm", subcmd ) ) {
        ged_nmg_mm(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mmr", subcmd ) ) {
        ged_nmg_mmr(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "msv", subcmd ) ) {
        ged_nmg_msv(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mrsv", subcmd ) ) {
        ged_nmg_mrsv(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mvvu", subcmd ) ) {
        ged_nmg_mvvu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mvu", subcmd ) ) {
        ged_nmg_mvu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "me", subcmd ) ) {
        ged_nmg_me(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "meonvu", subcmd ) ) {
        ged_nmg_meonvu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "eusplit", subcmd ) ) {
        ged_nmg_eusplit(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "esplit", subcmd ) ) {
        ged_nmg_esplit(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "eins", subcmd ) ) {
        ged_nmg_eins(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "ml", subcmd ) ) {
        ged_nmg_ml(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mlv", subcmd ) ) {
        ged_nmg_mlv(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "mf", subcmd ) ) {
        ged_nmg_mf(gedp, argc, argv);
    }

    /* NMG CONSTRUCTION CONVENIENCE ROUTINES */
    else if( BU_STR_EQUAL( "cface", subcmd ) ) {
        ged_nmg_cface(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "cmface", subcmd ) ) {
        ged_nmg_cmface(gedp, argc, argv);
    }

    /* NMG DESTRUCTION ROUTINES */
    else if( BU_STR_EQUAL( "keu", subcmd ) ) {
        ged_nmg_keu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kfu", subcmd ) ) {
        ged_nmg_kfu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "klu", subcmd ) ) {
        ged_nmg_klu(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "km", subcmd ) ) {
        ged_nmg_km(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kr", subcmd ) ) {
        ged_nmg_kr(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "ks", subcmd ) ) {
        ged_nmg_ks(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kvu", subcmd ) ) {
        ged_nmg_kvu(gedp, argc, argv);
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
