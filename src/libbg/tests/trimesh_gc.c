#include "common.h"

#include "bu.h"
#include "bg.h"


int
main(int argc, char **argv)
{
    int ifaces[] = {2, 0, 3};
    point2d_t ipnts[4] = {{0.0}};
    int *ofaces = NULL;
    point2d_t *opnts = NULL;
    int n_opnts = 0;
    int ret;
    int failed = 0;

    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    V2SET(ipnts[0], 1.0, 1.0);
    V2SET(ipnts[1], 99.0, 99.0);
    V2SET(ipnts[2], 2.0, 3.0);
    V2SET(ipnts[3], -1.0, 4.0);

    ret = bg_trimesh_2d_gc(&ofaces, &opnts, &n_opnts, ifaces, 1, ipnts);
    if (ret != 1 || n_opnts != 3 || !ofaces || !opnts) {
	failed = 1;
	goto cleanup;
    }

    if (ofaces[0] != 1 || ofaces[1] != 0 || ofaces[2] != 2
	|| !NEAR_EQUAL(opnts[0][X], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[0][Y], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[1][X], 2.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[1][Y], 3.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[2][X], -1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(opnts[2][Y], 4.0, SMALL_FASTF)) {
	failed = 1;
    }

cleanup:
    if (ofaces)
	bu_free(ofaces, "2D mesh faces");
    if (opnts)
	bu_free(opnts, "2D mesh points");

    return failed;
}
