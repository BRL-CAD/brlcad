/*                    I N T E R R U P T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/interrupt.c
 *
 * Pure decision helpers for MGED's cooperative interrupt handling.  See
 * mged_interrupt.h for the contract.  Kept dependency-free so the
 * interrupt policy is unit-testable without an MGED/Tcl/GED context.
 */

#include "common.h"

#include "./mged_interrupt.h"


int
mged_interrupt_service(int prev, int pending, int cmd_running, int subp_len, int *should_log)
{
    int rising = (pending && !prev);
    if (should_log)
	*should_log = (rising && (cmd_running || subp_len > 0));
    return rising;
}


int
mged_interrupt_should_act(int cmd_running, int subp_len)
{
    return (cmd_running || subp_len > 0);
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
