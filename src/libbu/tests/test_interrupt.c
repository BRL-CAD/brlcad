/*                T E S T _ I N T E R R U P T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <signal.h>

#include "bu/app.h"
#include "bu/interrupt.h"
#include "bu/log.h"

static volatile sig_atomic_t interrupt_delivered;

static void
test_interrupt_handler(int UNUSED(signum))
{
    interrupt_delivered = 1;
}

static int
check_interrupt_restored(int defer_interrupt)
{
    interrupt_delivered = 0;
    if (signal(SIGINT, test_interrupt_handler) == SIG_ERR ||
	    bu_interrupt_suspend())
	return 1;
    if (defer_interrupt && (raise(SIGINT) || interrupt_delivered))
	return 1;
    if (bu_interrupt_restore())
	return 1;
    if (!defer_interrupt && raise(SIGINT))
	return 1;
    return !interrupt_delivered;
}

int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    if (check_interrupt_restored(0)) {
	bu_log("SIGINT handler was not restored after a quiet critical section\n");
	return 1;
    }
    if (check_interrupt_restored(1)) {
	bu_log("SIGINT was not deferred until the original handler was restored\n");
	return 1;
    }
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=n-s
 */
