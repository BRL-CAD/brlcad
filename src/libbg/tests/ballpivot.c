/*                     B A L L P I V O T . C
 * BRL-CAD
 *
 * Published in 2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file ballpivot.c
 *
 * Exercising code for the BallPivot routine in libbg
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vmath.h"
#include "bu.h"
#include "bn/rand.h"
#include "bn/sobol.h"
#include "bg.h"

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
