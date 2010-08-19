/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static char const rcsid[] = "@(#) $Id: htmldecode.c,v 1.9 2008/01/09 06:49:37 danielk1977 Exp $";


#include "html.h"
#include <ctype.h>

/*
 *---------------------------------------------------------------------------
 *
 * readUriEncodedByte --
 *
 *     This function is part of the implementation of the 
 *
 * Results:
 *     Returns a string containing the versions of the *.c files used
 *     to build the library
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
unsigned char readUriEncodedByte(unsigned char **pzIn){
    unsigned char *zIn = *pzIn;
    unsigned char c;

    do {
        c = *(zIn++); 
    } while (c == ' ' || c == '\n' || c == '\t');

    if (c == '%') {
        char c1 = *(zIn++);
        char c2 = *(zIn++);

        if (c1 >= '0' && c1 <= '9')      c = (c1 - '0');
        else if (c1 >= 'A' && c1 <= 'F') c = (c1 - 'A');
        else if (c1 >= 'a' && c1 <= 'f') c = (c1 - 'a');
        else return 0;
        c = c << 4;

        if (c2 >= '0' && c2 <= '9')      c += (c2 - '0');
        else if (c2 >= 'A' && c2 <= 'F') c += (c2 - 'A' + 10);
        else if (c2 >= 'a' && c2 <= 'f') c += (c2 - 'a' + 10);
        else return 0;
    }

    *pzIn = zIn;

    return c;
}

int read6bits(unsigned char **pzIn){
#if 0
    char const z64[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#endif

    int map[256] = { 
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 0  */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 16 */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, 62, -1, -1, -1, 63,  /* 32 */
    52, 53, 54, 55, 56, 57, 58, 59,   60, 61, -1, -1, -1, -1, -1, -1,  /* 48 */
    -1,  0,  1,  2,  3,  4,  5,  6,    7,  8,  9, 10, 11, 12, 13, 14,  /* 64 */
    15, 16, 17, 18, 19, 20, 21, 22,   23, 24, 25, -1, -1, -1, -1, -1,  /* 80 */
    -1, 26, 27, 28, 29, 30, 31, 32,   33, 34, 35, 36, 37, 38, 39, 40,  /* 96 */
    41, 42, 43, 44, 45, 46, 47, 48,   49, 50, 51, -1, -1, -1, -1, -1   /* 112 */

    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 128 */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1
    };
    unsigned char c;

    c = readUriEncodedByte(pzIn);
    return map[c];
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDecode --
 *
 *         ::tkhtml::decode ?-base64? DATA
 *
 *     This command is designed to help scripts process "data:" URIs. It
 *     is completely separate from the html widget. 
 *
 * Results:
 *     Returns the decoded data.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlDecode(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    unsigned char *zOut;
    int jj;

    Tcl_Obj *pData;
    int nData;
    unsigned char *zData;
    int is64 = 0;

    if (objc != 3 && objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?-base64? DATA");
        return TCL_ERROR;
    }
    pData = objv[objc - 1];
    is64 = (objc == 3);

    zData = (unsigned char *)Tcl_GetStringFromObj(pData, &nData);
    zOut = (unsigned char *)HtmlAlloc("temp", nData);
    jj = 0;

    if (is64) {
        while (1) {
            int a = read6bits(&zData);
            int b = read6bits(&zData);
            int c = read6bits(&zData);
            int d = read6bits(&zData);
            int e = 0;
    
            if (a >= 0) e += a << 18;
            if (b >= 0) e += b << 12;
            if (c >= 0) e += c << 6;
            if (d >= 0) e += d;
    
            assert(jj < nData);
            if (b >= 0) zOut[jj++] = (e & 0x00FF0000) >> 16;
            assert(jj < nData);
            if (c >= 0) zOut[jj++] = (e & 0x0000FF00) >> 8;
            assert(jj < nData);
            if (d >= 0) zOut[jj++] = (e & 0x000000FF);
            if (d < 0) break;
        }
    } else {
        unsigned char c;
        while (0 != (c = readUriEncodedByte(&zData))) {
            zOut[jj++] = c;
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(zOut, jj));
    HtmlFree(zOut);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlEncode --
 *
 *         ::tkhtml::encode DATA
 *
 *         - _ . ! ~ * ' ( )
 *
 * Results:
 *     Returns the encoded data.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlEncode(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int map[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,    /* 0   */
        0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,    /* 16  */
        0, 1, 0, 0, 0, 0, 0, 1,   1, 1, 1, 0, 0, 1, 1, 0,    /* 32  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 0, 0, 0, 0, 0, 0,    /* 48  */

        0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,    /* 64  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,    /* 80  */
        0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,    /* 96  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0     /* 112 */
    };

    char hex[16] = { 
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };

    unsigned char *zOut;
    int iOut;

    int iIn;
    int nData;
    char *zData;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "DATA");
        return TCL_ERROR;
    }
    zData = Tcl_GetStringFromObj(objv[1], &nData);

    zOut = (unsigned char *)HtmlAlloc("temp", nData*3);
    iOut = 0;
    for(iIn = 0; iIn < nData; iIn++){
        char c = zData[iIn];
        if( !(c&0x80) && map[(int)c] ){
            zOut[iOut++] = c;
        } else {
            zOut[iOut++] = '%';
            zOut[iOut++] = hex[(int)((c>>4)&0x0F)];
            zOut[iOut++] = hex[(int)(c&0x0F)];
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(zOut, iOut));
    return TCL_OK;
}

static char * 
allocEscapedComponent(zInput, nInput, isQuery)
    const char *zInput;
    int nInput;
    int isQuery;
{
    int map[128] = { 
        0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,    /* 0   */
        0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,    /* 16  */
        0, 1, 0, 0, 1, 0, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,    /* 32  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 0, 1, 0, 0,    /* 48  */

        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,    /* 64  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,    /* 80  */
        0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,    /* 96  */
        1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0     /* 112 */
    };

    unsigned char *zEnd = (unsigned char *)&zInput[nInput];
    unsigned char *zCsr = (unsigned char *)zInput;
    char *zRes;
    char *zOut;

    zRes = (char *)HtmlAlloc("temp", 1+(nInput*3));
    zOut = (char *)zRes;

    for ( ; zCsr < zEnd; zCsr++) {
        if (*zCsr == '%' && (zEnd - zCsr) >= 3) {
            *(zOut++) = zCsr[0];
            *(zOut++) = zCsr[1];
            *(zOut++) = zCsr[2];
            zCsr += 2;
        } else if (isQuery && *zCsr == '?') {
            *(zOut++) = '?';
        } else if (*zCsr < 128 && map[*zCsr]) {
            *(zOut++) = zCsr[0];
        } else {
            int a = ((zCsr[0] & 0xF0) >> 4);
            int b = (zCsr[0] & 0x0F);
            *(zOut++) = '%';
            if (a < 10) {
                *(zOut++) = (unsigned char)a + '0';
            } else {
                *(zOut++) = (unsigned char)(a - 10) + 'A';
            }
            if (b < 10) {
                *(zOut++) = (unsigned char)b + '0';
            } else {
                *(zOut++) = (unsigned char)(b - 10) + 'A';
            }
        }
    }
    *zOut = '\0';
    assert((zOut - zRes) <= (1+(nInput*3)));

    return zRes;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlEscapeUriComponent --
 *
 *         ::tkhtml::escape_uri ?-query? STRING
 *
 * Results:
 *     Returns the decoded data.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlEscapeUriComponent(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    char *zRes;
    unsigned char *zCsr;
    int nIn;

    Tcl_Obj *pData;
    int isQuery;

    if (objc != 3 && objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?-query? URI-COMPONENT");
        return TCL_ERROR;
    }
    pData = objv[objc - 1];
    isQuery = (objc == 3);

    zCsr = (unsigned char *)Tcl_GetStringFromObj(pData, &nIn);
    zRes = allocEscapedComponent(zCsr, nIn, isQuery);

    Tcl_SetResult(interp, (char *)zRes, TCL_VOLATILE);
    HtmlFree(zRes);

    return TCL_OK;
}

/*
 * A parsed URI is held in an instance of the following structure.
 * Each member variable is either NULL or points to a nul-terminated 
 * string.
 *
 * The examples are from the URI 
 *
 *    http://192.168.1.1:8080/cgi-bin/printenv?name=xyzzy&addr=none#frag
 */
typedef struct Uri Uri;
struct Uri {
  char *zScheme;             /* Ex: "http" */
  char *zAuthority;          /* Ex: "192.168.1.1:8080" */
  char *zPath;               /* Ex: "/cgi-bin/printenv" */
  char *zQuery;              /* Ex: "name=xyzzy&addr=none" */
  char *zFragment;           /* Ex: "frag" */
};

#define ISALNUM(x) isalnum((unsigned char)(x))

static Uri *
objToUri(pObj)
    Tcl_Obj *pObj;
{
    int nInput;
    char *zInput;
    char *zOut;
    Uri *p;

    char *zCsr;

    zInput = Tcl_GetStringFromObj(pObj, &nInput);
    p = (Uri *)HtmlClearAlloc("::tkhtml::uri", sizeof(Uri) + nInput + 5);
    zOut = (char *)&p[1];

    /* Check if this URI includes a scheme. It includes a scheme if the
     * first character that is not a letter, number or '.' is a ':'.
     */
    zCsr = zInput;
    while (ISALNUM(*zCsr) || *zCsr == '.') zCsr++;
    if (*zCsr == ':') {
        /* There is a scheme. */
        int nScheme = zCsr - zInput;
        p->zScheme = zOut;
        memcpy(zOut, zInput, nScheme);
        zOut[nScheme] = '\0';
        zOut = &zOut[nScheme+1];
        zInput = &zCsr[1];
    }

    /* If there is now a "//", then the next bit is the authority. */
    if (zInput[0] == '/' && zInput[1] == '/') {
        int nAuthority;
        zInput = &zInput[2];
        zCsr = zInput;
        while (*zCsr && *zCsr != '/') zCsr++;
        nAuthority = zCsr - zInput;
        p->zAuthority = zOut;
        memcpy(zOut, zInput, nAuthority);
        zOut[nAuthority] = '\0';
        zOut = &zOut[nAuthority+1];
        zInput = zCsr;
    }

    /* Everything from this point until the first '?' or "#" is the path. */
    zCsr = zInput;
    while (*zCsr && *zCsr != '?' && *zCsr != '#') zCsr++;
    if (zCsr != zInput) {
        int nPath = zCsr - zInput;
        memcpy(zOut, zInput, nPath);
        p->zPath = zOut;
        zOut[nPath] = '\0';
        zOut = &zOut[nPath+1];
        zInput = zCsr;
    }

    /* The query */
    if (*zInput == '?') {
        int nQuery;
        zInput = &zInput[1];
        zCsr = zInput;
        while (*zCsr && *zCsr != '#') zCsr++;
        nQuery = zCsr - zInput;
        memcpy(zOut, zInput, nQuery);
        p->zQuery = zOut;
        zOut[nQuery] = '\0';
        zOut = &zOut[nQuery+1];
        zInput = zCsr;
    }

    /* The fragment */
    if (*zInput == '#') {
        int nFragment;
        zInput = &zInput[1];
        zCsr = zInput;
        while (*zCsr) zCsr++;
        nFragment = zCsr - zInput;
        memcpy(zOut, zInput, nFragment);
        p->zFragment = zOut;
        zOut[nFragment] = '\0';
        zOut = &zOut[nFragment+1];
    }

    assert(zOut - ((char *)&p[1]) <= (nInput + 5));
    return p;
}

static char *
combinePath(zOne, zTwo)
    const char *zOne;
    const char *zTwo;
{
    char *zRet;
    if (zTwo[0] == '/') {
        int nSpace = strlen(zTwo) + 1;
        zRet = HtmlAlloc("tmp", nSpace);
        strcpy(zRet, zTwo);
    } else if (!zOne) {
        int nSpace = strlen(zTwo) + 2;
        zRet = HtmlAlloc("tmp", nSpace);
        zRet[0] = '/';
        strcpy(&zRet[1], zTwo);
    } else {
        int nSpace;
        int nOne = 0;
        int ii = 0;
        while (zOne[ii]) {
            if (zOne[ii] == '/') {
                nOne = ii+1;
            }
            ii++;
        } 
        nSpace = strlen(zTwo) + nOne + 1;
        zRet = HtmlAlloc("tmp", nSpace);
        memcpy(zRet, zOne, nOne);
        strcpy(&zRet[nOne], zTwo);
    }

    return zRet;
}

static void
cleanPath(zPath)
    char *zPath;
{
    int nPath = strlen(zPath);
    int iIn;
    int iOut = 0;

    for(iIn = 0; iIn < nPath; iIn++){

        /* Replace any occurences of "//" with "/" */
        if (iIn <= (nPath-2) && zPath[iIn] == '/' && zPath[iIn+1] == '/') {
            continue;
        }
  
        /* Replace any occurences of "/./" with "/" */
        if( iIn <= (nPath-3) && 
            zPath[iIn] == '/' && zPath[iIn+1] == '.' && zPath[iIn+2] == '/'
        ){
            iIn++;
            continue;
        }
  
        /* Replace any occurences of "<path-component>/../" with "" */
        if (iOut > 0 && iIn <= (nPath - 4) && 
            zPath[iIn] == '/' && zPath[iIn+1] == '.' && 
            zPath[iIn+2] == '.' && zPath[iIn+3] == '/'
        ){
            iIn += 3;
            iOut--;
            for( ; iOut>0 && zPath[iOut-1]!='/'; iOut--);
            continue;
        }
  
        zPath[iOut++] = zPath[iIn];
    }
    zPath[iOut] = '\0';
}

static char *
makeUri(zScheme, zAuthority, zPath, zQuery, zFragment)
    char const *zScheme;
    char const *zAuthority;
    char const *zPath;
    char const *zQuery;
    char const *zFragment;
{
    char *zRes;
    int nSpace = 
        (zScheme ? strlen(zScheme) + 1 : 0) +
        (zAuthority ? strlen(zAuthority) + 2 : 0) +
        (zPath ? strlen(zPath) + 2 : 0) +
        (zQuery ? strlen(zQuery) + 1 : 0) +
        (zFragment ? strlen(zFragment) + 1 : 0) + 
        1
    ;
    zRes = HtmlAlloc("tmp", nSpace);
    sprintf(zRes, "%s%s%s%s%s%s%s%s%s",
        (zScheme ? zScheme : ""),
        (zScheme ? ":" : ""),
        (zAuthority ? "//" : ""),
        (zAuthority ? zAuthority : ""),
        (zPath ? zPath : ""),
        (zQuery ? "?" : ""),
        (zQuery ? zQuery : ""),
        (zFragment ? "#" : ""),
        (zFragment ? zFragment : "")
    );
    return zRes;
}

static char *
uriResolve(pBase, pObjRel)
    Uri *pBase;
    Tcl_Obj *pObjRel;
{
    char const *zScheme = pBase->zScheme;
    char const *zAuthority = pBase->zAuthority;
    char *zPath = pBase->zPath;
    char const *zQuery = pBase->zQuery;
    char const *zFragment = pBase->zFragment;

    char *zRes;
    Uri *pTmp = objToUri(pObjRel);

    if (pTmp->zScheme) {
        zScheme    = pTmp->zScheme;
        zAuthority = pTmp->zAuthority;
        zPath      = pTmp->zPath;
        zQuery     = pTmp->zQuery;
        zFragment  = pTmp->zFragment;
    }

    else if (pTmp->zAuthority) {
        zAuthority = pTmp->zAuthority;
        zPath      = pTmp->zPath;
        zQuery     = pTmp->zQuery;
        zFragment  = pTmp->zFragment;
    }

    else if (pTmp->zPath) {
        zPath      = combinePath(zPath, pTmp->zPath);
        zQuery     = pTmp->zQuery;
        zFragment  = pTmp->zFragment;
        cleanPath(zPath);
    }

    else if (pTmp->zQuery) {
        zQuery     = pTmp->zQuery;
        zFragment  = pTmp->zFragment;
    }

    else if (pTmp->zFragment) {
        zFragment  = pTmp->zFragment;
    }

    zRes = makeUri(zScheme, zAuthority, zPath, zQuery, zFragment);
    if (zPath != pBase->zPath && zPath != pTmp->zPath) {
        HtmlFree(zPath);
    }
    HtmlFree(pTmp);
    return zRes;
}

/*
 *---------------------------------------------------------------------------
 *
 * uriObjCmd --
 *
 *         $uri resolve URI
 *         $uri load URI
 *
 *         $uri scheme
 *         $uri authority
 *         $uri path
 *         $uri query
 *         $uri fragment
 *
 *         $uri destroy
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
uriObjCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The Uri data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Uri *p;

    enum URI_enum {
        URI_RESOLVE,
        URI_LOAD, 
        URI_GET,
        URI_NOFRAGMENT,
        URI_SCHEME, 
        URI_AUTHORITY, 
        URI_PATH, 
        URI_QUERY, 
        URI_FRAGMENT, 
        URI_DESTROY
    };
    int iChoice;

    static const struct UriCommand {
        const char *zCommand;
        enum URI_enum eSymbol;
        int nArg;
        const char *zUsage;
    } aSub[] = {
        {"resolve",   URI_RESOLVE,   1, "URI"},  
        {"load",      URI_LOAD,      1, "URI"},      
        {"get",             URI_GET,        0, ""},      
        {"get_no_fragment", URI_NOFRAGMENT, 0, ""},      
        {"scheme",    URI_SCHEME,    0, ""},      
        {"authority", URI_AUTHORITY, 0, ""},
        {"path",      URI_PATH,      0, ""},      
        {"query",     URI_QUERY,     0, ""},      
        {"fragment",  URI_FRAGMENT,  0, ""},      
        {"destroy",   URI_DESTROY,   0, ""},      
        {0, 0, 0}
    };
    p = (Uri *)clientData;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUB-COMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(interp, objv[1], aSub, 
            sizeof(struct UriCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }
    if (objc != 2+aSub[iChoice].nArg) {
        Tcl_WrongNumArgs(interp, 2, objv, aSub[iChoice].zUsage);
        return TCL_ERROR;
    }

#define TO_STRING_OBJ(z) Tcl_NewStringObj(((z)?(z):""), -1)

    switch (aSub[iChoice].eSymbol) {
        case URI_RESOLVE: {
            char *zRes = uriResolve(p, objv[2]);
            Tcl_SetObjResult(interp, Tcl_NewStringObj(zRes, -1));
            HtmlFree(zRes);
            break;
        }

        case URI_LOAD: {
            Tcl_CmdInfo info;
            Uri *pNew;
            char *zRes = uriResolve(p, objv[2]);
            Tcl_Obj *pObj = Tcl_NewStringObj(zRes, -1);
            HtmlFree(zRes);
            Tcl_IncrRefCount(pObj);
            pNew = objToUri(pObj);
            Tcl_DecrRefCount(pObj);
            Tcl_GetCommandInfo(interp, Tcl_GetString(objv[0]), &info);
            assert(info.objClientData == (ClientData)p);
            assert(info.deleteData == (ClientData)p);
            info.objClientData = (ClientData)pNew;
            info.deleteData = (ClientData)pNew;
            Tcl_SetCommandInfo(interp, Tcl_GetString(objv[0]), &info);
            HtmlFree(p);
            break;
        }

        case URI_GET: 
        case URI_NOFRAGMENT: {
            char *zRes = makeUri(p->zScheme, p->zAuthority, p->zPath, 
                p->zQuery, ((aSub[iChoice].eSymbol==URI_GET)?p->zFragment:0)
            );
            Tcl_SetObjResult(interp, Tcl_NewStringObj(zRes, -1));
            HtmlFree(zRes);
            break;
        }

        case URI_SCHEME: 
            Tcl_SetObjResult(interp, TO_STRING_OBJ(p->zScheme));
            break;
        case URI_AUTHORITY: 
            Tcl_SetObjResult(interp, TO_STRING_OBJ(p->zAuthority));
            break;
        case URI_PATH: 
            Tcl_SetObjResult(interp, TO_STRING_OBJ(p->zPath));
            break;
        case URI_QUERY: 
            Tcl_SetObjResult(interp, TO_STRING_OBJ(p->zQuery));
            break;
        case URI_FRAGMENT: 
            Tcl_SetObjResult(interp, TO_STRING_OBJ(p->zFragment));
            break;
        case URI_DESTROY:
            Tcl_DeleteCommand(interp, Tcl_GetString(objv[0]));
            break;
    }

    return TCL_OK;
}

static void 
uriObjDel(clientData)
    ClientData clientData;             /* The Uri data structure */
{
    HtmlFree(clientData);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCreateUri --
 *
 *     ::tkhtml::uri URI
 *
 *   STRING = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlCreateUri(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    static int iUriCounter = 1;
    Uri *p;
    char zBuf[64];

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "URI");
        return TCL_ERROR;
    }

    p = objToUri(objv[1]);
    sprintf(zBuf, "::tkhtml::uri%d", iUriCounter++);

    Tcl_CreateObjCommand(interp, zBuf, uriObjCmd, p, uriObjDel);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(zBuf, -1));
    return TCL_OK;
}
