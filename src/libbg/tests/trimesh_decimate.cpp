/*              T R I M E S H _ D E C I M A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "bg/trimesh.h"


int
main(int argc, char **argv)
{
    int faces[] = {
	0, 1, 2,
	1, 3, 4,
	1, 4, 2
    };
    point_t points[] = {
	{0.0, 0.0, 0.0},
	{0.001, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}
    };
    struct bg_trimesh_decimation_settings settings =
	BG_TRIMESH_DECIMATION_SETTINGS_INIT;
    int *output_faces = NULL;
    int *face_sources = NULL;
    int output_face_count = 0;
    int failed = 0;
    int invalid_faces[] = {-1, 1, 2};
    int *invalid_output = (int *)1;
    int *invalid_sources = (int *)1;
    int invalid_output_count = 99;
    bu_setprogname(argv[0]);
    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    /* The first two vertices collapse into one bin, removing only face 0. */
    settings.feature_size = 0.1;
    if (bg_trimesh_run_decimater(&output_faces, &face_sources,
	&output_face_count, faces, 3, points, 5, &settings) != BRLCAD_OK) {
	bu_log("FAIL: decimation returned an error: %s\n",
	    bu_vls_addr(&settings.msgs));
	failed = 1;
	goto cleanup;
    }

    if (output_face_count != 2 || !output_faces || !face_sources
	|| face_sources[0] != 1 || face_sources[1] != 2) {
	bu_log("FAIL: decimation did not preserve face provenance\n");
	failed = 1;
    }

    if (bg_trimesh_run_decimater(&invalid_output, &invalid_sources,
	&invalid_output_count, invalid_faces, 1, points, 5, &settings) !=
	BRLCAD_ERROR || invalid_output != NULL || invalid_sources != NULL ||
	invalid_output_count != 0) {
	bu_log("FAIL: decimation accepted an invalid vertex reference\n");
	failed = 1;
    }

cleanup:
    if (output_faces)
	bu_free(output_faces, "decimated test faces");
    if (face_sources)
	bu_free(face_sources, "decimated test face sources");
    bu_vls_free(&settings.msgs);
    return failed;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
