/*
 * bltWinUtil.c --
 *
 *	This module contains WIN32 routines not included in the Tcl/Tk
 *	libraries.
 *
 * Copyright 1998 by Bell Labs Innovations for Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that the
 * copyright notice and warranty disclaimer appear in supporting documentation,
 * and that the names of Lucent Technologies any of their entities not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this software,
 * including all implied warranties of merchantability and fitness.  In no event
 * shall Lucent Technologies be liable for any special, indirect or
 * consequential damages or any damages whatsoever resulting from loss of use,
 * data or profits, whether in an action of contract, negligence or other
 * tortuous action, arising out of or in connection with the use or performance
 * of this software.
 *
 */

#include <bltInt.h>

double
drand48(void)
{
    return (double) rand() / (double)RAND_MAX;
}

void
srand48(long int seed)
{
    srand(seed);
}

int
Blt_GetPlatformId(void)
{
    static int platformId = 0;

    if (platformId == 0) {
	OSVERSIONINFO opsysInfo;

	opsysInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx(&opsysInfo)) {
	    platformId = opsysInfo.dwPlatformId;
	}
    }
    return platformId;
}

char *
Blt_LastError(void)
{
    static char buffer[1024];
    int length;

    FormatMessage(
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	GetLastError(),
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	/* Default language */
	buffer,
	1024,
	NULL);
    length = strlen(buffer);
    if (buffer[length - 2] == '\r') {
	buffer[length - 2] = '\0';
    }
    return buffer;
}

