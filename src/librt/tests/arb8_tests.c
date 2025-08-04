/*                    A R B 8 _ T E S T S . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file arb8_tests.c
 *
 * Test different configurations of valid and invalid ARB8
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/log.h"
#include "vmath.h"
#include "wdb.h"
#include "raytrace.h"


// ARB8 configurations
typedef struct {
    const char *label;
    point_t vertices[8];
} arb8_config_t;

arb8_config_t arb8_configs[] = {
    // Spanning cube (all 8 quadrants)
    {"SpanningUnitCube", {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}},

    // Unit cubes in each quadrant
    {"UnitCube_Q1", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
    {"UnitCube_Q2", {{-1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {-1, 1, 0}, {-1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {-1, 1, 1}}},
    {"UnitCube_Q3", {{-1, -1, 0}, {0, -1, 0}, {0, 0, 0}, {-1, 0, 0}, {-1, -1, 1}, {0, -1, 1}, {0, 0, 1}, {-1, 0, 1}}},
    {"UnitCube_Q4", {{0, -1, 0}, {1, -1, 0}, {1, 0, 0}, {0, 0, 0}, {0, -1, 1}, {1, -1, 1}, {1, 0, 1}, {0, 0, 1}}},
    {"UnitCube_Q5", {{0, 0, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}},
    {"UnitCube_Q6", {{-1, 0, -1}, {0, 0, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {-1, 1, 0}}},
    {"UnitCube_Q7", {{-1, -1, -1}, {0, -1, -1}, {0, 0, -1}, {-1, 0, -1}, {-1, -1, 0}, {0, -1, 0}, {0, 0, 0}, {-1, 0, 0}}},
    {"UnitCube_Q8", {{0, -1, -1}, {1, -1, -1}, {1, 0, -1}, {0, 0, -1}, {0, -1, 0}, {1, -1, 0}, {1, 0, 0}, {0, 0, 0}}},

    // Elongated cubes
    {"Elongated_X1",
     {{-1, 0, 0}, {1, 0, 0}, {1, 1, 0}, {-1, 1, 0}, {-1, 0, 1}, {1, 0, 1}, {1, 1, 1}, {-1, 1, 1}}}, /* +-X in +Y,+Z*/
    {"Elongated_X2",
     {{-1, -1, 0}, {1, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {-1, -1, 1}, {1, -1, 1}, {1, 0, 1}, {-1, 0, 1}}}, /* +-X in -Y,+Z */
    {"Elongated_Y1",
     {{0, -1, 0}, {1, -1, 0}, {1, 1, 0}, {0, 1, 0}, {0, -1, 1}, {1, -1, 1}, {1, 1, 1}, {0, 1, 1}}}, /* +-Y in +X,+Z */
    {"Elongated_Y2",
     {{-1, -1, 0}, {0, -1, 0}, {0, 1, 0}, {-1, 1, 0}, {-1, -1, 1}, {0, -1, 1}, {0, 1, 1}, {-1, 1, 1}}}, /* +-Y in -X,+Z */
    {"Elongated_Z1",
     {{0, 0, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}}, /* +-Z in +X,+Y */
    {"Elongated_Z2",
     {{-1, 0, -1}, {0, 0, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {-1, 1, 1}}}, /* +-Z in -X,+Y */

    // Adjusted configurations (all near 1st quadrant, some extending outside)
    {"ReverseUnitCube", {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}},
    {"TopSlanted", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0.2, 0.2, 1}, {1.2, 0.2, 1}, {1.2, 1.2, 1}, {0.2, 1.2, 1}}},
    {"UnevenSlanted", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0.1, 0.3, 1}, {1.1, 0.3, 1}, {1.3, 1.3, 1}, {0.1, 1.3, 1}}},
    {"OppositeSlanted", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-0.2, 0.2, 1}, {0.8, 0.2, 1}, {0.8, 1.2, 1}, {-0.2, 1.2, 1}}},
    {"PlanarARB6",
     {{0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {1.0, 1.0, 0.0},
      {0.5, 0.5, 0.0},
      {0.0, 0.0, 1.0},
      {1.0, 0.0, 1.0},
      {1.0, 1.0, 1.0},
      {0.5, 0.5, 1.0}}},

    // Planar concave configurations
    {"ConcavePlanar",
     {{0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {1.0, 1.0, 0.0},
      {0.6, 0.4, 0.0},
      {0.0, 0.0, 1.0},
      {1.0, 0.0, 1.0},
      {1.0, 1.0, 1.0},
      {0.6, 0.4, 1.0}}},
    {"MoreConcavePlanar",
     {{0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.3, 0.3, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 1.0},
      {1.0, 0.0, 1.0},
      {0.3, 0.3, 1.0},
      {0.0, 1.0, 1.0}}},

    // Additional planar configurations (not at origin)
    {"LongConvexPlanar",
     {{4281, -119, 1531},
      {4281, 78, 1531},
      {3322, 78, 1475},
      {3322, -119, 1475},
      {4275, -119, 1629},
      {4275, 78, 1649},
      {3315, 78, 1593},
      {3315, -119, 1573}}},
    {"TaperedConvexPlanar",
     {{122.749, 2.4, 40.725},
      {122.749, -7.4, 40.725},
      {122.611, -5, 43.244},
      {122.611, 0, 43.244},
      {105.434, 2.4, 39.708},
      {105.434, -7.4, 39.708},
      {105.295, -5, 42.228},
      {105.295, 0, 42.228}}},

    // Complex Non-planar configurations
    {"WarpedNonPlanar", {{0, 0, 0}, {1, 0, 0.1}, {1.1, 1, 0}, {0, 1.1, -0.1}, {0.1, 0, 1}, {1, 0.1, 1.1}, {1.1, 1.1, 0.9}, {0, 1, 1}}},
    {"TwistedTopNonPlanar", {{0, 0, 0}, {1, 0, -0.1}, {1.1, 1.1, 0}, {0, 1.2, 0.1}, {0.2, 0, 0.9}, {1, 0.1, 1.2}, {1.2, 1.2, 0.8}, {0, 1, 1}}},
    {"ComplexNonPlanar", {{0, 0, 0}, {0.8, 0, 0.1}, {1.1, 0.9, 0.2}, {0.1, 1.1, -0.2}, {0.2, -0.2, 0.8}, {1, 0, 1.1}, {1.1, 0.9, 0.9}, {0, 1, 0.9}}},
    {"ConcaveNonPlanar",
     {{0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {1.0, 1.0, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 1.0},
      {0.4, 0.4, 1.0},
      {1.0, 1.0, 1.0},
      {0.0, 1.0, 1.0}}},
    {"MoreConcaveNonPlanar",
     {{0.0, 0.0, 0.00},
      {1.0, 0.0,-0.05},
      {1.0, 1.0, 0.03},
      {0.7, 0.3, 0.32},
      {0.0, 0.0, 1.00},
      {1.0, 0.0, 1.10},
      {1.0, 1.0, 0.92},
      {0.7, 0.3, 0.70}}},
    {"MostConcaveNonPlanar",
     {{0.2, 0.0, 0.0},
      {1.0, 0.3, 0.10},
      {1.1, 1.0, -0.25},
      {0.4, 0.6, 0.00},
      {0.0, 0.2, 1.0},
      {0.9, 0.0, 1.25},
      {1.0, 0.8, 0.60},
      {0.4, 0.3, 0.80}}}

};


// Ray inputs
typedef struct {
    const char *label;
    point_t origin;
    vect_t direction;
} ray_input_t;


// Expected results (to be filled manually or programmatically based on geometry)
typedef struct {
    const char *arb8_label;
    const char *ray_label;
    int expected_hits; // typically 2 in/out hits per segment
} expected_result_t;

expected_result_t expected_results[] = {
    {"SpanningUnitCube", "SpanningUnitCube", 2},
    {"UnitCube_Q1", "UnitCube_Q1", 2},
    {"UnitCube_Q2", "UnitCube_Q2", 2},
    {"UnitCube_Q3", "UnitCube_Q3", 0},
    {"UnitCube_Q4", "UnitCube_Q4", 0},
    {"UnitCube_Q5", "UnitCube_Q5", 0},
    {"UnitCube_Q6", "UnitCube_Q6", 0},
    {"UnitCube_Q7", "UnitCube_Q7", 0},
    {"UnitCube_Q8", "UnitCube_Q8", 0},
    {"Elongated_X1", "Elongated_X1", 2},
    {"Elongated_X2", "Elongated_X2", 0},
    {"Elongated_Y1", "Elongated_Y1", 2},
    {"Elongated_Y2", "Elongated_Y2", 2},
    {"Elongated_Z1", "Elongated_Z1", 2},
    {"Elongated_Z2", "Elongated_Z2", 2},

    {"ReverseUnitCube", "ReverseUnitCube", 2},
    {"TopSlanted", "TopSlanted", 2},
    {"UnevenSlanted", "UnevenSlanted", 2},
    {"OppositeSlanted", "OppositeSlanted", 2},
    {"PlanarARB6", "PlanarARB6", 2},
    {"ConcavePlanar", "ConcavePlanar", 2},
    {"MoreConcavePlanar", "MoreConcavePlanar", 2},

    {"LongConvexPlanar", "LongConvexPlanar", 0}, // not near origin
    {"TaperedConvexPlanar", "TaperedConvexPlanar", 0}, // not near origin

    {"WarpedNonPlanar", "WarpedNonPlanar", -2},
    {"TwistedTopNonPlanar", "TwistedTopNonPlanar", -2},
    {"ComplexNonPlanar", "ComplexNonPlanar", -2},
    {"ConcaveNonPlanar", "ConcaveNonPlanar", -2},
    {"MoreConcaveNonPlanar", "MoreConcaveNonPlanar", -2},
    {"MostConcaveNonPlanar", "MostConcaveNonPlanar", -2}
};


static ray_input_t *
setupRays(const arb8_config_t *arbp, size_t *count)
{
    // TODO: allocate grids and vertex-specific rays
    ray_input_t *rays = (ray_input_t *)bu_calloc(1, sizeof(ray_input_t), "alloc rays");

    rays[0].label = arbp->label;
    VSET(rays[0].origin, 2, 0.5, 0.5);
    VSET(rays[0].direction, -1, 0, 0);

    *count = 1;

    return rays;
}


static void
releaseRays(ray_input_t *rays)
{
    bu_free(rays, "free rays");
}


// Main execution and validation
int
main(int ac, char *av[])
{
    if (ac != 1 && ac != 3) {
	bu_log("Usage: %s [-g file.g]\n", av[0]);
	return 1;
    }

    /* write out arb8's to a .g file */
    if (ac == 3) {
	if (bu_file_exists(av[2], NULL)) {
	    bu_log("ERROR: Output file [%s] already exists.\n", av[2]);
	    bu_exit(1, "\tRemove file and retry.\n");
	}

	struct rt_wdb *wdbp = wdb_fopen(av[2]);
	if (!wdbp) {
	    bu_log("ERROR: Cannot open [%s] as writable database.\n", av[2]);
	    bu_exit(2, "\tUnable to proceed.\n");
	}

	for (size_t i = 0; i < sizeof(arb8_configs) / sizeof(arb8_config_t); i++) {
	    fastf_t pts[24];

	    for (size_t j = 0; j < 8; j++) {
		VMOVE(&pts[j*3], arb8_configs[i].vertices[j]);
	    }
	    mk_arb8(wdbp, arb8_configs[i].label, pts);
	}

	wdb_close(wdbp);
    }

    /* test them by shooting rays */
    for (size_t i = 0; i < sizeof(arb8_configs) / sizeof(arb8_config_t); i++) {
        const arb8_config_t *arb = &arb8_configs[i];
        printf("Testing ARB8: %s\n", arb->label);

	size_t nrays = 0;
	ray_input_t *rays = setupRays(arb, &nrays);

        for (size_t j = 0; j < nrays; j++) {
            const ray_input_t *ray = &rays[j];
            printf("  Testing Ray: %s\n", ray->label);

            struct application ap;
	    RT_APPLICATION_INIT(&ap);
            VSET(ap.a_ray.r_pt, ray->origin[X], ray->origin[Y], ray->origin[Z]);
            VSET(ap.a_ray.r_dir, ray->direction[X], ray->direction[Y], ray->direction[Z]);
	    ap.a_ray.magic = RT_RAY_MAGIC;
	    ap.a_resource = &rt_uniresource;

            // TODO - call rt_arb_shot() here, set hit/miss result
	    struct seg seghead;
	    BU_LIST_INIT(&(seghead.l));
	    struct xray *rayp = &ap.a_ray;

	    struct soltab st = {0};
	    st.l.magic = RT_SOLTAB_MAGIC;
	    st.l2.magic = RT_SOLTAB2_MAGIC;
	    st.st_uses = 1;
	    st.st_id = ID_ARB8;

	    struct rt_db_internal db;
	    db.idb_magic = RT_DB_INTERNAL_MAGIC;
	    db.idb_major_type = ID_ARB8;

	    struct rt_arb_internal arbint;
	    memcpy(arbint.pt, arb->vertices, sizeof(point_t)*8);
	    arbint.magic = RT_ARB_INTERNAL_MAGIC;
	    db.idb_ptr = &arbint;

	    int prep = rt_obj_prep(&st, &db, NULL);
	    int hits = rt_obj_shot(&st, rayp, &ap, &seghead);

	    if (prep < 0) {
		bu_log("prep failed\n");
		hits = -hits; /* flip negative */
	    }
#if 0
	    if (hits <= 0)
		bu_log("shot missed\n");
#endif

            // Validate against expected result
            int expected_hits = -999;
            for (size_t k = 0; k < sizeof(expected_results) / sizeof(expected_result_t); k++) {
                if (bu_strcmp(expected_results[k].arb8_label, arb->label) == 0 &&
                    bu_strcmp(expected_results[k].ray_label, ray->label) == 0) {
                    expected_hits = expected_results[k].expected_hits;
                    break;
                }
            }

            if (expected_hits != -999) {
                if (hits == expected_hits || -hits == expected_hits) {
		    if (expected_hits < 0) {
			printf("    PASS (unsupported, right)\n");
		    } else {
			printf("    PASS\n");
		    }
                } else {
		    if (expected_hits < 0) {
			printf("    PASS (unsupported, wrong)\n");
		    } else {
			printf("    FAIL (expected %d, got %d)\n", expected_hits, hits);
		    }
                }
            } else {
                printf("    SKIPPED (no expectation defined)\n");
            }
        }

	releaseRays(rays);
    }

    return 0;
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
