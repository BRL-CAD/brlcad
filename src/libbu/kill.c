/*                          K I L L . C
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
/** @file kill.c
 *
 * terminate a given process.
 *
 * Author -
 *   Christopher Sean Morrison
 *
 * Source -
 *   BRL-CAD Open Source
 */
#include "common.h"

#include "bu.h"

/**
 * b u _ t e r m i n a t e
 *
 * terminate a given process.
 *
 * returns truthfully whether the process could be killed.
 */
int
bu_terminate(int process)
{
    int successful = 0;

#ifdef HAVE_KILL
    /* kill() returns 0 for success */
    successful = kill(process, SIGKILL);
    successful = !successful; 
#else
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, process);
    if(hProcess != NULL) {
	successful = TerminateProcess(hProcess, 0);
    }
#endif

    return successful;
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
