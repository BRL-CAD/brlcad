#include "common.h"

#include <math.h>

#include "bu.h"
#include "bg.h"


static int
test_inverse_direction(void)
{
    vect_t dir = VINIT_ZERO;
    vect_t original_dir = VINIT_ZERO;
    vect_t invdir = VINIT_ZERO;

    VSET(dir, 0.5 * SQRT_SMALL_FASTF, 1.0, -2.0);
    VMOVE(original_dir, dir);
    bg_ray_invdir(&invdir, dir);

    return (dir[X] != original_dir[X]
	|| dir[Y] != original_dir[Y]
	|| dir[Z] != original_dir[Z]
	|| !isinf(invdir[X])
	|| !NEAR_EQUAL(invdir[Y], 1.0, SMALL_FASTF)
	|| !NEAR_EQUAL(invdir[Z], -0.5, SMALL_FASTF));
}


static int
test_line_intersections(void)
{
    point_t origin = VINIT_ZERO;
    vect_t dir = VINIT_ZERO;
    vect_t invdir = VINIT_ZERO;
    point_t aabb_min = VINIT_ZERO;
    point_t aabb_max = VINIT_ZERO;
    fastf_t rmin = 0.0;
    fastf_t rmax = 0.0;

    VSET(dir, 1.0, 0.0, 0.0);
    bg_ray_invdir(&invdir, dir);

    VSET(aabb_min, 5.0, -1.0, -1.0);
    VSET(aabb_max, 7.0, 1.0, 1.0);
    if (!bg_isect_aabb_ray(&rmin, &rmax, origin, invdir, aabb_min, aabb_max)
	|| !NEAR_EQUAL(rmin, 5.0, SMALL_FASTF)
	|| !NEAR_EQUAL(rmax, 7.0, SMALL_FASTF)) {
	return 1;
    }

    VSET(aabb_min, -7.0, -1.0, -1.0);
    VSET(aabb_max, -5.0, 1.0, 1.0);
    if (!bg_isect_aabb_ray(&rmin, &rmax, origin, invdir, aabb_min, aabb_max)
	|| !NEAR_EQUAL(rmin, -7.0, SMALL_FASTF)
	|| !NEAR_EQUAL(rmax, -5.0, SMALL_FASTF)) {
	return 1;
    }

    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    return test_inverse_direction() || test_line_intersections();
}
