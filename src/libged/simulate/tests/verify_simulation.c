/*             V E R I F Y _ S I M U L A T I O N . C
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file verify_simulation.c
 *
 */

#include "common.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "ged.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * Parse bounding box from ged bb command output
 * Format: "xmin ymin zmin xmax ymax zmax"
 */
int parse_bbox(const char *output, point_t min, point_t max) {
    double xmin, ymin, zmin, xmax, ymax, zmax;

    if (sscanf(output, "%lf %lf %lf %lf %lf %lf",
		&xmin, &ymin, &zmin, &xmax, &ymax, &zmax) == 6) {
	VSET(min, xmin, ymin, zmin);
	VSET(max, xmax, ymax, zmax);
	return 1;
    }
    return 0;
}

/**
 * Get bounding box of an object using ged
 */
int get_bbox(struct ged *gedp, const char *obj_name, point_t min, point_t max) {
    const char *argv[] = {"bb", "-q", obj_name};

    if (ged_exec(gedp, 3, argv) != BRLCAD_OK) {
	return 0;
    }

    const char *output = bu_vls_addr(gedp->ged_result_str);
    return parse_bbox(output, min, max);
}

/**
 * Check pre-simulation setup
 */
int check_presim(const char *db_path, const char *truck_name, const char *terrain_name) {
    struct ged *gedp;
    point_t truck_min, truck_max, terrain_min, terrain_max;
    double separation;
    int result = 1;

    printf("\n");
    printf("================================================================\n");
    printf("PRE-SIMULATION VERIFICATION\n");
    printf("================================================================\n\n");

    printf("Opening database: %s\n", db_path);
    gedp = ged_open("db", db_path, 0);
    if (!gedp) {
	printf("ERROR: Failed to open database\n");
	return 0;
    }

    /* Get truck bounding box */
    printf("Getting truck bounding box: %s\n", truck_name);
    if (!get_bbox(gedp, truck_name, truck_min, truck_max)) {
	printf("ERROR: Failed to get truck bounding box\n");
	ged_close(gedp);
	return 0;
    }

    /* Get terrain bounding box */
    printf("Getting terrain bounding box: %s\n", terrain_name);
    if (!get_bbox(gedp, terrain_name, terrain_min, terrain_max)) {
	printf("ERROR: Failed to get terrain bounding box\n");
	ged_close(gedp);
	return 0;
    }

    /* Print dimensions */
    printf("\n");
    printf("Truck '%s' bounding box (mm):\n", truck_name);
    printf("  Min: (%.2f, %.2f, %.2f)\n", truck_min[X], truck_min[Y], truck_min[Z]);
    printf("  Max: (%.2f, %.2f, %.2f)\n", truck_max[X], truck_max[Y], truck_max[Z]);
    printf("  Dimensions: %.2f x %.2f x %.2f mm\n",
	    truck_max[X] - truck_min[X],
	    truck_max[Y] - truck_min[Y],
	    truck_max[Z] - truck_min[Z]);

    printf("\n");
    printf("Terrain '%s' bounding box (mm):\n", terrain_name);
    printf("  Min: (%.2f, %.2f, %.2f)\n", terrain_min[X], terrain_min[Y], terrain_min[Z]);
    printf("  Max: (%.2f, %.2f, %.2f)\n", terrain_max[X], terrain_max[Y], terrain_max[Z]);
    printf("  Dimensions: %.2f x %.2f x %.2f mm\n",
	    terrain_max[X] - terrain_min[X],
	    terrain_max[Y] - terrain_min[Y],
	    terrain_max[Z] - terrain_min[Z]);

    /* Check vertical separation */
    printf("\n");
    printf("Vertical Position Analysis:\n");
    printf("  Terrain top Z: %.2f mm\n", terrain_max[Z]);
    printf("  Truck bottom Z: %.2f mm\n", truck_min[Z]);

    separation = truck_min[Z] - terrain_max[Z];
    printf("  Separation (truck bottom to terrain top): %.2f mm\n", separation);

    /* Verify truck is above terrain */
    printf("\n");
    if (separation < -100) {
	printf("FAIL: Truck is significantly BELOW terrain!\n");
	printf("         Separation = %.2f mm (should be positive)\n", separation);
	result = 0;
    } else if (separation < 0) {
	printf("WARNING: Truck and terrain may be intersecting\n");
	printf("         Separation = %.2f mm (should be positive)\n", separation);
	/* This might be ok if wheels are just touching */
    } else if (separation < 10) {
	printf("✓ PASS: Truck is just above terrain (will land quickly)\n");
	printf("        Separation = %.2f mm\n", separation);
    } else {
	printf("✓ PASS: Truck is above terrain and will fall\n");
	printf("        Separation = %.2f mm\n", separation);
    }

    ged_close(gedp);

    printf("\n");
    printf("================================================================\n");
    printf("Pre-simulation check: %s\n", result ? "PASSED" : "FAILED");
    printf("================================================================\n");

    return result;
}

/**
 * Check post-simulation state
 */
int check_postsim(const char *db_path, const char *truck_name, const char *terrain_name) {
    struct ged *gedp;
    point_t truck_min, truck_max, terrain_min, terrain_max;
    double separation, truck_height;
    int result = 1;

    printf("\n");
    printf("================================================================\n");
    printf("POST-SIMULATION VERIFICATION\n");
    printf("================================================================\n\n");

    printf("Opening database: %s\n", db_path);
    gedp = ged_open("db", db_path, 0);
    if (!gedp) {
	printf("ERROR: Failed to open database\n");
	return 0;
    }

    /* Get truck bounding box after simulation */
    printf("Getting truck final position: %s\n", truck_name);
    if (!get_bbox(gedp, truck_name, truck_min, truck_max)) {
	printf("ERROR: Failed to get truck bounding box\n");
	ged_close(gedp);
	return 0;
    }

    /* Get terrain bounding box */
    printf("Getting terrain position: %s\n", terrain_name);
    if (!get_bbox(gedp, terrain_name, terrain_min, terrain_max)) {
	printf("ERROR: Failed to get terrain bounding box\n");
	ged_close(gedp);
	return 0;
    }

    /* Calculate metrics */
    truck_height = truck_max[Z] - truck_min[Z];
    separation = truck_min[Z] - terrain_max[Z];

    /* Estimate wheel clearance (wheels are typically at bottom of truck) */
    /* For a proper landed truck, separation should be close to 0 or slightly negative */
    /* (wheels compress into terrain surface) */

    printf("\n");
    printf("Final Position Analysis:\n");
    printf("  Terrain top Z: %.2f mm\n", terrain_max[Z]);
    printf("  Truck bottom Z: %.2f mm\n", truck_min[Z]);
    printf("  Truck top Z: %.2f mm\n", truck_max[Z]);
    printf("  Truck height: %.2f mm\n", truck_height);
    printf("  Truck-terrain gap: %.2f mm\n", separation);

    printf("\n");
    printf("Landing Assessment:\n");

    /* Check if truck landed properly */
    if (separation > 100) {
	printf("FAIL: Truck did NOT land on terrain!\n");
	printf("         Still %.2f mm above terrain surface\n", separation);
	result = 0;
    } else if (separation > 10) {
	printf("WARNING: Truck may not have fully landed\n");
	printf("         Gap of %.2f mm from terrain\n", separation);
    } else if (separation >= -10) {
	printf("PASS: Truck appears to have landed on terrain\n");
	printf("        Gap = %.2f mm (wheels on/near surface)\n", separation);
    } else if (separation >= -truck_height * 0.2) {
	printf("PASS: Truck landed with wheels on terrain\n");
	printf("        Penetration = %.2f mm (expected for wheel contact)\n", -separation);
    } else {
	printf("FAIL: Truck appears to have excessive penetration\n");
	printf("         Penetration = %.2f mm (%.1f%% of truck height)\n",
		-separation, (-separation / truck_height) * 100);
	result = 0;
    }

    /* Check for catastrophic failures */
    if (truck_max[Z] < terrain_max[Z]) {
	printf("FAIL: Truck is completely BELOW terrain top!\n");
	result = 0;
    }

    ged_close(gedp);

    printf("\n");
    printf("================================================================\n");
    printf("Post-simulation check: %s\n", result ? "PASSED" : "FAILED");
    printf("================================================================\n");

    return result;
}

int main(int argc, char **argv) {
    const char *db_path;
    const char *truck_name;
    const char *terrain_name;
    const char *mode;
    int result;

    bu_setprogname(argv[0]);

    if (argc < 5) {
	printf("Usage: %s <mode> <database.g> <truck_object> <terrain_object>\n", argv[0]);
	printf("Modes:\n");
	printf("  presim  - Check setup before simulation\n");
	printf("  postsim - Check results after simulation\n");
	printf("  both    - Check both (run simulation between checks)\n");
	printf("\nExample:\n");
	printf("  %s presim test_truck_terrain.g componentcomponent terra.sterra.s\n", argv[0]);
	return 1;
    }

    mode = argv[1];
    db_path = argv[2];
    truck_name = argv[3];
    terrain_name = argv[4];

    printf("\n");
    printf("================================================================\n");
    printf("M35 TRUCK SIMULATION VERIFICATION TOOL\n");
    printf("================================================================\n");
    printf("Mode: %s\n", mode);
    printf("Database: %s\n", db_path);
    printf("Truck: %s\n", truck_name);
    printf("Terrain: %s\n", terrain_name);

    if (BU_STR_EQUAL(mode, "presim")) {
	result = check_presim(db_path, truck_name, terrain_name);
    } else if (BU_STR_EQUAL(mode, "postsim")) {
	result = check_postsim(db_path, truck_name, terrain_name);
    } else if (BU_STR_EQUAL(mode, "both")) {
	result = check_presim(db_path, truck_name, terrain_name);
	if (result) {
	    printf("\nNote: 'both' mode requires running simulation manually between checks\n");
	    printf("    Run simulate command, then use 'postsim' mode\n");
	}
    } else {
	printf("\nERROR: Unknown mode '%s'\n", mode);
	printf("Valid modes: presim, postsim, both\n");
	return 1;
    }

    printf("\n");
    printf("================================================================\n");
    printf("FINAL RESULT: %s\n", result ? "PASSED" : "FAILED");
    printf("================================================================\n\n");

    return result ? 0 : 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
