/******************************************************************************
 *
 * Project:  Shapelib
 * Purpose:  Default implementation of file io based on stdio.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see COPYING).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 */

#include "common.h"
#include "shapefil.h"

#include <assert.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SHPAPI_UTF8_HOOKS
#   ifdef SHPAPI_WINDOWS
#       define WIN32_LEAN_AND_MEAN
#       define NOMINMAX
#       include <windows.h>
#       pragma comment(lib, "kernel32.lib")
#   endif
#endif

static SAFile SADFOpen(const char *pszFilename, const char *pszAccess) {
    return (SAFile)fopen(pszFilename, pszAccess);
}

static SAOffset SADFRead(void *p, SAOffset size, SAOffset nmemb, SAFile file) {
    return (SAOffset)fread( p, (size_t)size, (size_t)nmemb, (FILE *)file);
}

static SAOffset SADFWrite(void *p, SAOffset size, SAOffset nmemb, SAFile file) {
    return (SAOffset)fwrite(p, (size_t)size, (size_t)nmemb, (FILE *)file);
}

static SAOffset SADFSeek(SAFile file, SAOffset offset, int whence) {
    return (SAOffset)fseek((FILE *) file, (long) offset, whence);
}

static SAOffset SADFTell(SAFile file) {
    return (SAOffset)ftell((FILE *)file);
}

static int SADFFlush(SAFile file) {
    return fflush((FILE *) file);
}

static int SADFClose(SAFile file) {
    return fclose((FILE *) file);
}

static int SADRemove(const char *filename) {
    return remove(filename);
}

static void SADError(const char *message) {
    fprintf(stderr, "%s\n", message);
}

void SASetupDefaultHooks(SAHooks *psHooks) {
    psHooks->FOpen   = SADFOpen;
    psHooks->FRead   = SADFRead;
    psHooks->FWrite  = SADFWrite;
    psHooks->FSeek   = SADFSeek;
    psHooks->FTell   = SADFTell;
    psHooks->FFlush  = SADFFlush;
    psHooks->FClose  = SADFClose;
    psHooks->Remove  = SADRemove;

    psHooks->Error   = SADError;
    psHooks->Atof    = atof;
}

#ifdef SHPAPI_WINDOWS

const wchar_t* Utf8ToWideChar(const char *pszFilename) {
    const int nMulti = strlen(pszFilename) + 1;
    const int nWide = MultiByteToWideChar( CP_UTF8, 0, pszFilename, nMulti, 0, 0);
    if (nWide == 0) {
        return NULL;
    }
    wchar_t *pwszFileName = (wchar_t*) malloc(nWide * sizeof(wchar_t));
    if (pwszFileName == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, pszFilename, nMulti, pwszFileName, nWide) == 0) {
        free(pwszFileName);
        return NULL;
    }
    return pwszFileName;
}

/************************************************************************/
/*                           SAUtf8WFOpen                               */
/************************************************************************/

SAFile SAUtf8WFOpen(const char *pszFilename, const char *pszAccess) {
    SAFile file = NULL;
    wchar_t *pwszFileName = Utf8ToWideChar( pszFilename );
    wchar_t *pwszAccess = Utf8ToWideChar( pszAccess );
    if (pwszFileName != NULL && pwszAccess != NULL) {
        file = (SAFile) _wfopen( pwszFileName, pwszAccess );
    }
    free(pwszFileName);
    free(pwszAccess);
    return file;
}

int SAUtf8WRemove(const char *pszFilename) {
    wchar_t *pwszFileName = Utf8ToWideChar(pszFilename);
    int rc = -1;
    if (pwszFileName != NULL) {
        rc = _wremove(pwszFileName);
    }
    free(pwszFileName);
    return rc;
}

#endif

#ifdef SHPAPI_UTF8_HOOKS
#ifndef SHPAPI_WINDOWS
#  error "no implementations of UTF-8 hooks available for this platform"
#endif

void SASetupUtf8Hooks(SAHooks *psHooks) {
    psHooks->FOpen   = SAUtf8WFOpen;
    psHooks->Remove  = SAUtf8WRemove;
    psHooks->FRead   = SADFRead;
    psHooks->FWrite  = SADFWrite;
    psHooks->FSeek   = SADFSeek;
    psHooks->FTell   = SADFTell;
    psHooks->FFlush  = SADFFlush;
    psHooks->FClose  = SADFClose;

    psHooks->Error   = SADError;
    psHooks->Atof    = atof;
}
#endif
