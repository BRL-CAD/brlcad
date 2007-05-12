/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file process.c
 *
 * process management routines
 *
 * Author -
 *   Christopher Sean Morrison
 *
 * Source -
 *   BRL-CAD Open Source
 */

#include "common.h"

/* system headers */
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/* common headers */
#include "bu.h"


/**
 * returns the process ID of the calling process
 */
int
bu_process_id()
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
