/*
 * random_csg.c - Procedural random CSG boolean geometry generator
 * Generates a .g database with randomly combined BRL-CAD primitives
 * using union, subtract, and intersect boolean operations.
 */

#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "vmath.h"
#include "wdb.h"
#include "bu/app.h"

#define NUM_PRIMITIVES 5

 /* Random float between min and max */
static double rand_range(double min, double max) {
    return min + ((double)rand() / RAND_MAX) * (max - min);
}

int main(int argc, char* argv[]) {
    struct rt_wdb* wdbp;
    char prim_name[32];
    char* members[NUM_PRIMITIVES];
    int op[NUM_PRIMITIVES];
    int i;
    point_t center;
    char* ops_str[] = { "union", "subtract", "intersect" };
    int bool_ops[] = { WMOP_UNION, WMOP_SUBTRACT, WMOP_INTERSECT };

    bu_setprogname(argv[0]);
    srand((unsigned int)time(NULL));

    /* Open/create the output .g database */
    wdbp = wdb_fopen("random_csg.g");
    if (wdbp == NULL) {
        fprintf(stderr, "ERROR: cannot create random_csg.g\n");
        return 1;
    }

    printf("Creating %d random primitives...\n", NUM_PRIMITIVES);

    for (i = 0; i < NUM_PRIMITIVES; i++) {
        sprintf(prim_name, "prim_%d.s", i);
        members[i] = bu_strdup(prim_name);

        /* Random center position */
        VSET(center,
            rand_range(-50.0, 50.0),
            rand_range(-50.0, 50.0),
            rand_range(-50.0, 50.0));

        /* Alternate between sphere and rpp (box) */
        if (i % 2 == 0) {
            double radius = rand_range(5.0, 20.0);
            mk_sph(wdbp, prim_name, center, radius);
            printf("  [%d] sphere '%s' at (%.1f, %.1f, %.1f) r=%.1f\n",
                i, prim_name, V3ARGS(center), radius);
        }
        else {
            point_t min_pt, max_pt;
            double half = rand_range(5.0, 15.0);
            point_t half_pt;
            VSETALL(half_pt, half);
            VSUB2(min_pt, center, half_pt);
            VADD2(max_pt, center, half_pt);
            mk_rpp(wdbp, prim_name, min_pt, max_pt);
            printf("  [%d] box    '%s' at (%.1f, %.1f, %.1f) half=%.1f\n",
                i, prim_name, V3ARGS(center), half);
        }

        /* First primitive is always union; rest are random */
        op[i] = (i == 0) ? WMOP_UNION : bool_ops[rand() % 3];
        printf("         op: %s\n", ops_str[op[i] == WMOP_UNION ? 0 :
            op[i] == WMOP_SUBTRACT ? 1 : 2]);
    }

    /* Build the boolean combination */
    struct wmember wm;
    BU_LIST_INIT(&wm.l);
    for (i = 0; i < NUM_PRIMITIVES; i++) {
        mk_addmember(members[i], &wm.l, NULL, op[i]);
    }

    mk_lcomb(wdbp, "random_csg.c", &wm, 0, NULL, NULL, NULL, 0);
    printf("\nCombination 'random_csg.c' written to random_csg.g\n");
    printf("Open with: mged random_csg.g\n");

    wdb_close(wdbp);

    for (i = 0; i < NUM_PRIMITIVES; i++)
        bu_free(members[i], "member name");

    return 0;
}
