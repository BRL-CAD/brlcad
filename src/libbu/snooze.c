/*                        S N O O Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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
/** @file snooze.c
 *
 * Routines for suspending the current thread of execution.
 *
 */

#include "common.h"
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include "bsocket.h" /* for timeval */

#include "bu/snooze.h"


int
bu_snooze(int64_t useconds)
{
    int ret;
    struct timeval wait;

    wait.tv_sec = useconds / 1000000LL;
    wait.tv_usec = useconds % 1000000LL;

    ret = select(0, 0, 0, 0, &wait);

    return ret;
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
