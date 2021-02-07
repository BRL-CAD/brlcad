/*                          K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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

#include "common.h"

/* system headers */
#include <signal.h>
#include "bio.h"
#ifndef HAVE_KILL
#  include <TlHelp32.h>
#endif

/* bu headers */
#include "bu/process.h"
#include "./process.h"

/* c99 doesn't declare these */
#if defined(HAVE_KILL) && !defined(__cplusplus)
extern int kill(pid_t, int);
#endif


#ifdef HAVE_KILL

HIDDEN int
_bu_terminate(int process)
{
    int successful = 0;

    /* kill process and all children (negative pid, sysv extension) */
    successful = kill((pid_t)-process, SIGKILL);
    /* kill() returns 0 for success */
    successful = !successful;

    return successful;
}

#else /* !HAVE_KILL */

HIDDEN int
_bu_terminate(int process)
{
    int successful = 0;
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32 = {0};

    pe32.dwSize = sizeof(PROCESSENTRY32);
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
	return successful;
    }

    if (!Process32First(hProcessSnap, &pe32)) {
	CloseHandle(hProcessSnap);
	return successful;
    }

    /* First, find and kill the children */
    do {
	if (pe32.th32ParentProcessID == (DWORD)process) {
#if 1
	    _bu_terminate((int)pe32.th32ProcessID);
#else
	    hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pe32.th32ProcessID);
	    if (hProcess != NULL) {
		TerminateProcess(hProcess, 0);
		CloseHandle(hProcess);
	    }
#endif
	}
    } while(Process32Next(hProcessSnap, &pe32));

    /* Finally, kill the parent */
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, (DWORD)process);
    if (hProcess != NULL) {
	successful = TerminateProcess(hProcess, BU_MSVC_ABORT_EXIT);
	CloseHandle(hProcess);
    }

    CloseHandle(hProcessSnap);
    return successful;
}

#endif


int
bu_terminate(int process)
{
    return _bu_terminate(process);
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
