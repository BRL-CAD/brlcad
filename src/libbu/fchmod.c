/*                        F C H M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#ifndef HAVE_FCHMOD
/* headers for the GetFileNameFromHandle() function below */
#  include <tchar.h>
#  include <string.h>
#  include <psapi.h>
#  include <strsafe.h>
#endif

#include "bu.h"


/* c99 doesn't declare these */
#ifndef  fileno
extern int fileno(FILE*);
#endif

#ifdef HAVE_FCHMOD
extern int fchmod(int, mode_t);
#else

/* Presumably Windows, pulled from MSDN sample code */
int
GetFileNameFromHandle(HANDLE hFile, char filepath[])
{
    int bSuccess = 0;
    TCHAR pszFilename[MAXPATHLEN+1];
    char filename[MAXPATHLEN+1];
    HANDLE hFileMap;

    /* Get the file size. */
    DWORD dwFileSizeHi = 0;
    DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);

    if (dwFileSizeLo == 0 && dwFileSizeHi == 0) {
	_tprintf(TEXT("Cannot map a file with a length of zero.\n"));
	return FALSE;
    }

    /* Create a file mapping object. */
    hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);

    if (hFileMap) {
	/* Create a file mapping to get the file name. */
	void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);

	if (pMem) {
	    if (GetMappedFileName (GetCurrentProcess(), pMem, pszFilename, MAXPATHLEN)) {

		/* Translate path with device name to drive letters. */
		TCHAR szTemp[MAXPATHLEN+1];
		szTemp[0] = '\0';

		if (GetLogicalDriveStrings(MAXPATHLEN, szTemp)) {
		    TCHAR szName[MAXPATHLEN];
		    TCHAR szDrive[3] = TEXT(" :");
		    int bFound = 0;
		    TCHAR* p = szTemp;

		    do {
			/* Copy the drive letter to the template string */
			*szDrive = *p;

			/* Look up each device name */
			if (QueryDosDevice(szDrive, szName, MAXPATHLEN)) {
			    size_t uNameLen = _tcslen(szName);

			    if (uNameLen < MAXPATHLEN) {
				bFound = _tcsnicmp(pszFilename, szName, uNameLen) == 0;

				if (bFound && *(pszFilename + uNameLen) == _T('\\')) {
				    /* Reconstruct pszFilename using szTempFile */
				    /* Replace device path with DOS path */
				    TCHAR szTempFile[MAXPATHLEN];
				    StringCchPrintf(szTempFile, MAXPATHLEN, TEXT("%s%s"), szDrive, pszFilename+uNameLen);
				    StringCchCopyN(pszFilename, MAXPATHLEN+1, szTempFile, _tcslen(szTempFile));
				}
			    }
			}

			/* Go to the next NULL character. */
			while (*p++);
		    } while (!bFound && *p)
			; /* end of string */
		}
	    }
	    bSuccess = TRUE;
	    UnmapViewOfFile(pMem);
	}

	CloseHandle(hFileMap);
    }
    if (sizeof(TCHAR) == sizeof(wchar_t)) {
	wcstombs(filename, pszFilename, MAXPATHLEN);
	bu_strlcpy(filepath, filename, MAXPATHLEN);
    } else {
	bu_strlcpy(filepath, pszFilename, MAXPATHLEN);
    }
    return(bSuccess);
}
#endif


int
bu_fchmod(FILE *fp,
	  unsigned long pmode)
{
    if (UNLIKELY(!fp)) {
	return 0;
    }

#ifdef HAVE_FCHMOD
    return fchmod(fileno(fp), (mode_t)pmode);
#else
    /* Presumably Windows, so get dirty.  We can call chmod() instead
     * of fchmod(), but that means we need to know the file name.
     * This isn't portably knowable, but Windows provides a roundabout
     * way to figure it out.
     *
     * If we were willing to limit ourselves to Windows 2000 or 7+, we
     * could call GetModuleFileNameEx() but there are reports that
     * it's rather unreliable.
     */
    {
	char filepath[MAXPATHLEN+1];
	int fd = fileno(fp);
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	GetFileNameFromHandle(h, filepath);
	return chmod(filepath, pmode);
    }
#endif
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
