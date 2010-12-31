/*
 * Copyright (c) 2005 Dan Kennedy.
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
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
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
static const char rcsid[] = "$Id: css.c,v 1.139 2007/12/16 11:57:43 danielk1977 Exp $";

#define LOG if (pTree->options.logcmd)

/*
 *    The CSS "cascade":
 *
 *        1. Find all declarations that apply to the element/property in
 *           question.  Declarations apply if the selector matches the
 *           element in question. If no declarations apply, the inherited
 *           value is used. If there is no inherited value (this is the
 *           case for the 'HTML' element and for properties that do not
 *           inherit), the initial value is used. 
 *       
 *        2. Sort the declarations by explicit weight: declarations marked
 *           '!important' carry more weight than unmarked (normal)
 *           declarations.  
 * 
 *        3. Sort by origin: the author's style sheets override the
 *           reader's style sheet which override the UA's default values.
 *           An imported style sheet has the same origin as the style sheet
 *           from which it is imported. 
 *
 *        4. Sort by specificity of selector: more specific selectors will
 *           override more general ones. To find the specificity, count the
 *           number of ID attributes in the selector (a), the number of
 *           CLASS attributes in the selector (b), and the number of tag
 *           names in the selector (c). Concatenating the three numbers (in
 *           a number system with a large base) gives the specificity. Some
 *           examples:
 *
 *        5. Sort by order specified: if two rules have the same weight,
 *           the latter specified wins. Rules in imported style sheets are
 *           considered to be before any rules in the style sheet itself.
 */

#include <stdlib.h>
#include <tcl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "cssInt.h"
#include "html.h"

/*
 * Macros to trace code in this file. Set to non-zero to activate trace
 * output on stdout.
 */
#define TRACE_PARSER_CALLS 0

static int cssParse(HtmlTree*,int,CONST char*,int,int,Tcl_Obj*,Tcl_Obj*,Tcl_Obj*,Tcl_Obj*,CssStyleSheet**);

/*
 *---------------------------------------------------------------------------
 *
 * constantToString --
 *
 *     Transform an integer constant to it's string representation (for
 *     debugging). All of the constants defined in cssInt.h that start with
 *     CSS_* are supported.
 *
 * Results:
 *     Pointer to a static string.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if TRACE_PARSER_CALLS
static const char *constantToString(int c){
    switch( c ){
        case CSS_SELECTORCHAIN_DESCENDANT: 
            return "CSS_SELECTORCHAIN_DESCENDANT";
        case CSS_SELECTORCHAIN_CHILD: 
            return "CSS_SELECTORCHAIN_CHILD";
        case CSS_SELECTORCHAIN_ADJACENT: 
            return "CSS_SELECTORCHAIN_ADJACENT";
        case CSS_SELECTOR_UNIVERSAL: 
            return "CSS_SELECTOR_UNIVERSAL";
        case CSS_SELECTOR_TYPE: 
            return "CSS_SELECTOR_TYPE";
        case CSS_SELECTOR_ATTR: 
            return "CSS_SELECTOR_ATTR";
        case CSS_SELECTOR_ATTRVALUE: 
            return "CSS_SELECTOR_ATTRVALUE";
        case CSS_SELECTOR_ATTRLISTVALUE: 
            return "CSS_SELECTOR_ATTRLISTVALUE";
        case CSS_SELECTOR_CLASS: 
            return "CSS_SELECTOR_CLASS";
        case CSS_SELECTOR_ID: 
            return "CSS_SELECTOR_ID";
        case CSS_SELECTOR_ATTRHYPHEN: 
            return "CSS_SELECTOR_ATTRHYPHEN";
        case CSS_PSEUDOCLASS_LANG: 
            return "CSS_PSEUDOCLASS_LANG";
        case CSS_PSEUDOCLASS_FIRSTCHILD: 
            return "CSS_PSEUDOCLASS_FIRSTCHILD";
        case CSS_PSEUDOCLASS_LINK:  
            return "CSS_PSEUDOCLASS_LINK";
        case CSS_PSEUDOCLASS_VISITED: 
            return "CSS_PSEUDOCLASS_VISITED";
        case CSS_PSEUDOCLASS_ACTIVE:  
            return "CSS_PSEUDOCLASS_ACTIVE";
        case CSS_PSEUDOCLASS_HOVER: 
            return "CSS_PSEUDOCLASS_HOVER";
        case CSS_PSEUDOCLASS_FOCUS: 
            return "CSS_PSEUDOCLASS_FOCUS";
        case CSS_PSEUDOELEMENT_FIRSTLINE: 
            return "CSS_PSEUDOELEMENT_FIRSTLINE";
        case CSS_PSEUDOELEMENT_FIRSTLETTER: 
            return "CSS_PSEUDOELEMENT_FIRSTLETTER";
        case CSS_PSEUDOELEMENT_BEFORE: 
            return "CSS_PSEUDOELEMENT_BEFORE";
        case CSS_PSEUDOELEMENT_AFTER: 
            return "CSS_PSEUDOELEMENT_AFTER";
        case CSS_MEDIA_ALL: 
            return "CSS_MEDIA_ALL";
        case CSS_MEDIA_AURAL: 
            return "CSS_MEDIA_AURAL";
        case CSS_MEDIA_BRAILLE: 
            return "CSS_MEDIA_BRAILLE";
        case CSS_MEDIA_EMBOSSED: 
            return "CSS_MEDIA_EMBOSSED";
        case CSS_MEDIA_HANDHELD: 
            return "CSS_MEDIA_HANDHELD";
        case CSS_MEDIA_PRINT: 
            return "CSS_MEDIA_PRINT";
        case CSS_MEDIA_PROJECTION: 
            return "CSS_MEDIA_PROJECTION";
        case CSS_MEDIA_SCREEN: 
            return "CSS_MEDIA_SCREEN";
        case CSS_MEDIA_TTY: 
            return "CSS_MEDIA_TTY";
        case CSS_MEDIA_TV: 
            return "CSS_MEDIA_TV";
    }
    return "unknown";
}
#endif

/*--------------------------------------------------------------------------
 *
 * dequote --
 *
 *     This function is used to dequote a CSS string or identifier value. 
 * 
 *     Argument z is a pointer to a buffer containing a possibly quoted,
 *     null-terminated string. If it is quoted, then this function overwrites
 *     the buffer with the unquoted version. CSS strings may be quoted with
 *     single or double quotes. The quote character may occur within a string
 *     only if it is escaped with a "\" character.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     May modify the contents of buffer pointed to by z.
 *
 *--------------------------------------------------------------------------
 */
static int 
dequote(z)
    char *z;
{
    int rc = 1;

    static char hexvalue[128] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x00-0x0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x10-0x1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x20-0x2F */
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, /* 0x30-0x3F */
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x40-0x4F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x50-0x5F */
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x60-0x6F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  /* 0x70-0x7F */
    };

    /* printf("IN: %s\n", z); */

    if (z) {
        int i;
        char *zOut = z;
        int n = strlen(z);
        char q;

        /* Trim white-space from the start and end of the input buffer. */
        while( n>0 && isspace((unsigned char)z[0]) ){
            z++;
            n--;
        }
        while( n>0 && isspace((unsigned char)z[n-1]) ){
            n--;
        }
  
	/* Figure out if there is a quote character (" or ').  If there is one,
         * strip it from the start of the string before proceeding. 
         */
        q = z[0];
        if (q != '\'' && q != '"') {
            rc = 0;
            q = '\0';
        }
        if (n > 1 && z[n - 1] == q && z[n - 2] != '\\') {
            n--;
        }

        for (i = (q ? 1 : 0); i < n; i++) {
            unsigned char o = z[i];
            if (o == '\\') {
                unsigned int ch = 0;
                int ii;
                o = z[i+1];
                for (ii = 0; isxdigit(o) && ii < 6; ii++) {
                    assert(hexvalue[o] >=0 && hexvalue[o] <= 15);
                    ch = (ch << 4) + hexvalue[o];
                    o = z[(++i)+1];
                }
                if (ch > 0) {
                    int inc = Tcl_UniCharToUtf(ch, zOut);
                    zOut += inc;
     
		    /* Ignore a single white-space character after a
                     * hexadecimal escape.
                     */
                    if (isspace((unsigned char)(z[i + 1]))) i++;
                }
          
                /* Inside a string, if an escape character occurs just before
                 * a newline, ignore that newline. 
                 */
                if (ii == 0 && o == '\n') i++;
            } else {
                *zOut++ = o;
            }
        }
        *zOut = 0;
    }

    return rc;
    /* printf("OUT: %s\n", z); */
}


/*--------------------------------------------------------------------------
 *
 * tokenToString --
 *
 *     This function returns a null-terminated string (allocated by HtmlAlloc)
 *     populated with the contents of the supplied token.
 *
 * Results:
 *     Null-terminated string. Caller is responsible for calling HtmlFree()
 *     on it.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static char *tokenToString(CssToken *pToken){
    char *zRet;
    if( !pToken || pToken->n<=0 ){
         return 0;
    }
    zRet = (char *)HtmlAlloc("tokenToString()", pToken->n+1);
    memcpy(zRet, pToken->z, pToken->n);
    zRet[pToken->n] = '\0';
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * tokenToReal --
 *
 *     Try to convert the token pToken to a floating point number. The
 *     conversion is considered successful if pToken starts with anything
 *     that looks like a floating point number. "0.0px" and "0px" convert
 *     successfully, "px" does not. Return non-zero if the conversion is
 *     succcesful, else zero.
 *
 *     Write the floating point number to *pVal. Write the number of bytes
 *     convereted to *pLen.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int tokenToReal(pToken, pLen, pVal)
    CssToken *pToken;
    int *pLen;
    double *pVal;
{
    char zBuf[100];
    char *zEnd;
    if (pToken->n>99) {
        return 0;
    }

    strncpy(zBuf, pToken->z, pToken->n);
    zBuf[pToken->n] = '\0';

    /* Do not parse "NaN" or "-NaN" as a number. */
    if (zBuf[0] == 'N' || (zBuf[0] == '-' && zBuf[1] == 'N')) {
        return 0;
    }

    *pVal = strtod(zBuf, &zEnd);
    if (zEnd!=zBuf) {
        *pLen = (zEnd-zBuf);
        return 1;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyIsLength --
 *
 *     Return true if the property passed as the first argument is a length
 *     property. (i.e. pixels, em, percentage etc.)
 *
 *     Usually, a number with no units is not considered a length. There
 *     are two exceptions:
 *
 *       * When the number is 0, and
 *       * When quirks mode is enabled (in this case a number with no 
 *         units is treated as a pixel quantity).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
propertyIsLength(pParse, pProp)
    CssParse *pParse;
    CssProperty *pProp;
{
    switch (pProp->eType) {
        case CSS_TYPE_EM:
        case CSS_TYPE_PX:
        case CSS_TYPE_PT:
        case CSS_TYPE_PC:
        case CSS_TYPE_EX:
        case CSS_TYPE_CENTIMETER:
        case CSS_TYPE_INCH:
        case CSS_TYPE_MILLIMETER:
            return 1;

        case CSS_TYPE_FLOAT:
            return ((
                INTEGER(pProp->v.rVal) == 0 || 
                pParse->pTree->options.mode == HTML_MODE_QUIRKS
            ) ? 1 : 0);
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * rgbToColor --
 *
 *     This function is invoked when a property value of the form 
 *     "rgb(rrr, ggg, bbb)" is encountered. This routine interprets the
 *     property and transforms it to a string of the form #AAAAAA which can
 *     be understood as a color by Tk.
 *
 *     The first argument, zOut, points to a buffer of not less than 8
 *     bytes. The output string and it's NULL terminator is written here.
 *     The second parameter, zRgb, points to the first byte past the '('
 *     character of the original property value. The final parameter, nRgb,
 *     is the number of bytes between the '(' and ')' character of the
 *     original property.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Writes exactly 8 bytes to zOut.
 *
 *---------------------------------------------------------------------------
 */
static void 
rgbToColor(zOut, zRgb, nRgb)
    char *zOut;
    CONST char *zRgb;
    int nRgb;
{
    CONST char *z = zRgb;
    CONST char *zEnd = zRgb+nRgb;
    int n = 0;

    int aN[3] = {0, 0, 0};
    CssToken aToken[3];

    int ii;
    for (ii = 0; ii < 3; ii++){
        aToken[ii].z = HtmlCssGetNextCommaListItem(z, zEnd - z, &aToken[ii].n);
        z = &(aToken[ii].z[aToken[ii].n]);
    }
    if (!aToken[0].z || !aToken[1].z || !aToken[2].z ||
        !aToken[0].n || !aToken[1].n || !aToken[2].n
    ) {
        goto bad_color;
    }

    if (aToken[0].z[aToken[0].n-1]  == '%') {
        for (ii = 0; ii < 3; ii++){
            char *zEnd;
            double percent;
            
            if (aToken[ii].z[aToken[ii].n-1]  != '%') {
                goto bad_color;
            }
            percent = strtod(aToken[ii].z, &zEnd);
            if ((zEnd - aToken[ii].z) != (aToken[ii].n-1)) {
                goto bad_color;
            }
            aN[ii] = (percent * 255) / 100;
        }
    } else {
        for (ii = 0; ii < 3; ii++){
            char *zEnd;
            aN[ii] = strtol(aToken[ii].z, &zEnd, 0);
            if ((zEnd - aToken[ii].z) != aToken[ii].n) {
                goto bad_color;
            }
        }
    }
    for (ii = 0; ii < 3; ii++){
        aN[ii] = MIN(MAX(aN[ii], 0), 255);
    }

    n = sprintf(zOut, "#%.2x%.2x%.2x", aN[0], aN[1], aN[2]);
    assert(n==7);
    return;

  bad_color:
    zOut[0] = '\0';
    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * doUrlCmd --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
doUrlCmd(pParse, zArg, nArg)
    CssParse *pParse;
    CONST char *zArg;
    int nArg;
{
    const int eval_flags = TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL;
    char *zCopy = HtmlAlloc("temp", nArg + 1);
    Tcl_Obj *pCopy;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pParse->pUrlCmd);

    memcpy(zCopy, zArg, nArg);
    zCopy[nArg] = '\0';
    dequote(zCopy);
    pCopy = Tcl_NewStringObj(zCopy, -1);

    Tcl_IncrRefCount(pScript);
    Tcl_ListObjAppendElement(0, pScript, pCopy);
    Tcl_EvalObjEx(pParse->interp, pScript, eval_flags);
    Tcl_DecrRefCount(pScript);

    HtmlFree(zCopy);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tokenToProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *
tokenToProperty(pParse, pToken)
    CssParse *pParse;
    CssToken *pToken;
{
    struct LengthFormat {
        int type;
        int len;
        char *zUnit;
    } lengths[] = {
        {CSS_TYPE_EM,         2, "em"},
        {CSS_TYPE_EX,         2, "ex"},
        {CSS_TYPE_PX,         2, "px"},
        {CSS_TYPE_PT,         2, "pt"},
        {CSS_TYPE_PERCENT,    1, "%"},
        {CSS_TYPE_FLOAT,      0, ""},
        {CSS_TYPE_CENTIMETER, 2, "cm"},
        {CSS_TYPE_MILLIMETER, 2, "mm"},
        {CSS_TYPE_INCH,       2, "in"},
        {CSS_TYPE_PC,         2, "pc"},
    };

    struct FunctionFormat {
        int type;
        int len;
        char *zFunc;
    } functions[] = {
        {CSS_TYPE_TCL,      3, "tcl"},
        {CSS_TYPE_URL,      3, "url"},
        {CSS_TYPE_ATTR,     4, "attr"},
        {CSS_TYPE_COUNTER,  7, "counter"},
        {CSS_TYPE_COUNTERS, 8, "counters"},
        {-1,                3, "rgb"},
    };

    CssProperty *pProp = 0;
    int i;
    double realval;       /* Real value, if token can be converted to float */
    int reallen;          /* Bytes of token converted to realval */

    CONST char *z = pToken->z;
    int n = pToken->n;

    /* Check if this is a length. It is a length if the token consists
     * of a floating point number followed by one of the units enumerated
     * above in the array of LengthFormat structs.
     */
    if (tokenToReal(pToken, &reallen, &realval)) {
        for (i=0; i<(sizeof(lengths)/sizeof(lengths[0])); i++) {
            CONST char *zTokenUnit = &z[reallen];
            if ((n-reallen)==lengths[i].len &&
                    0==strnicmp(zTokenUnit, lengths[i].zUnit, lengths[i].len)) {
                pProp = HtmlNew(CssProperty);
                pProp->eType = lengths[i].type;
                pProp->v.rVal = realval;
                break;
            }
        }
    }

    /* Check if this is a function call. A function call is anything that
     * begins with one or more alphabetic characters and a '(' character.
     * The last character of the token must be ')'.
     */
    if (z[n-1]==')') {
        for (i=0; i<n && isalpha((int)z[i]); i++);
        if (i<n && i>0 && z[i]=='(') {
            int l = i;
            int nFunc = sizeof(functions)/sizeof(struct FunctionFormat);
            for (i=0; pProp==0 && i<nFunc; i++) {
                const char *zFunc = functions[i].zFunc;
                if (l==functions[i].len && 0==strnicmp(zFunc, z, l)) {
                    char CONST *zArg;
                    int nArg;

                    zArg = &z[l+1];
                    nArg = (n-l-2); /* len(token)-len(func)-len('(')-len(')') */

                    while (*zArg == ' ' && nArg > 0) {
                      zArg++;
                      nArg--;
                    }
                    while (zArg[nArg-1] == ' ' && nArg > 0) {
                      nArg--;
                    }

                    if (
                        functions[i].type == CSS_TYPE_URL &&
                        pParse &&
                        pParse->pUrlCmd
                    ) {
                        doUrlCmd(pParse, zArg, nArg);
                        zArg = Tcl_GetStringResult(pParse->interp);
                        nArg = strlen(zArg);
                    }

                    if (functions[i].type==-1) {
                        /* -1 means this is an RGB value. Transform to a
                         * color string that Tcl can understand before
			 * storing it in the properties database. The color
			 * string will be 7 characters long exactly.
                         */
                        int nAlloc = sizeof(CssProperty) + 7 + 1;
                        pProp = (CssProperty *)HtmlAlloc("CssProperty", nAlloc);
                        pProp->eType = CSS_TYPE_RAW;
                        pProp->v.zVal = (char *)&pProp[1];
                        rgbToColor(pProp->v.zVal, zArg, nArg);
                    } else {
                        int nAlloc = sizeof(CssProperty) + nArg + 1;
                        pProp = (CssProperty *)HtmlAlloc("CssProperty", nAlloc);
                        pProp->eType = functions[i].type;
                        pProp->v.zVal = (char *)&pProp[1];
                        strncpy(pProp->v.zVal, zArg, nArg);
                        pProp->v.zVal[nArg] = '\0';
                    }

                    if (pProp->eType == CSS_TYPE_URL) {
                        dequote(pProp->v.zVal);
                    }
                    break;
                }
            }
        }
    }

    /* Security: Do not allow a stylesheet other than the default to
     * contain tcl() scripts. Otherwise remote stylesheets could execute
     * arbitrary scripts in the web browser.
     */
    if (
        (pProp && pProp->eType == CSS_TYPE_TCL) && 
        (!pParse->pPriority1 || pParse->pPriority1->iPriority != 1)
    ) {
        HtmlFree(pProp);
        return 0;
    }

    /* Finally, treat the property as a generic string. v.zVal will point at
     * a NULL-terminated copy of the string. The eType field is set to
     * either CSS_TYPE_STRING, CSS_TYPE_RAW, or one of the symbols in 
     * cssprop.h (i.e. CSS_TYPE_BLOCK).
     */
    if (!pProp) {
        int eType;
        int nAlloc = sizeof(CssProperty) + n + 1;
        pProp = (CssProperty *)HtmlAlloc("CssProperty", nAlloc);
        pProp->v.zVal = (char *)&pProp[1];
        memcpy(pProp->v.zVal, z, n);
        pProp->v.zVal[n] = '\0';

        if (z[0] == '"' || z[0] == '\'') {
            dequote(pProp->v.zVal);
            pProp->eType = CSS_TYPE_STRING;
        } else {
            dequote(pProp->v.zVal);
            eType = HtmlCssConstantLookup(-1, pProp->v.zVal);
            if (eType <= 0) {
                pProp->eType = CSS_TYPE_RAW;
            } else {
                pProp->eType = eType;
            }
        }
    }
    return pProp;
}

/*
 *---------------------------------------------------------------------------
 *
 * textToProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *
textToProperty(pParse, z, n)
    CssParse *pParse;
    CONST char *z;
    int n;
{
    CssToken token;
    token.n = (n < 0) ? strlen(z): n;
    token.z = z;
    return tokenToProperty(pParse, &token);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssStringToProperty --
 *
 *     This is an externally available interface to convert the property
 *     value string pointed to by z, length n, to a property object. The
 *     caller should call HtmlFree() on the return value when it has finished
 *     with it.
 *
 * Results:
 *     Allocated CssProperty object.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CssProperty *HtmlCssStringToProperty(z, n)
    CONST char *z; 
    int n;
{
    CssToken sToken;
    if (n<0) {
        n = strlen(z);
    }
    sToken.z = z;
    sToken.n = n;
    return tokenToProperty(0, &sToken);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssPropertyGetString --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST char *
HtmlCssPropertyGetString(pProp)
    CssProperty *pProp;
{
    if (pProp) {
        int eType = pProp->eType;
        if (eType == CSS_TYPE_STRING || eType == CSS_TYPE_RAW ||
            (eType >= CSS_CONST_MIN_CONSTANT && eType <= CSS_CONST_MAX_CONSTANT)
        ) {
            return pProp->v.zVal;
        }
    }
    return 0;
}

/*--------------------------------------------------------------------------
 *
 * propertySetNew --
 *
 *     Allocate a new (empty) property set. The caller should eventually
 *     delete the property set using propertySetFree().
 *
 * Results:
 *     An empty property set.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static CssPropertySet *
propertySetNew()
{
    CssPropertySet *p = HtmlNew(CssPropertySet);
    return p;
}

/*--------------------------------------------------------------------------
 *
 * propertySetGet --
 *
 *     Retrieve CSS property 'i' if present in the property-set. 
 *
 * Results:
 *
 *     Return NULL if the property is not present, or a pointer to the
 *     value if it is.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static CssProperty *
propertySetGet(p, i)
    CssPropertySet *p;         /* Property set */
    int i;                     /* Property id (i.e CSS_PROPERTY_WIDTH) */
{
    int j;
    assert( i<128 && i>=0 );

    for (j = 0; j < p->n; j++) {
        if (i == p->a[j].eProp) {
            return p->a[j].pProp;
        }
    }

    return 0;
}

/*--------------------------------------------------------------------------
 *
 * propertySetAdd --
 *
 *     Insert or replace a value into a property set.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void 
propertySetAdd(p, i, v)
    CssPropertySet *p;         /* Property set. */
    int i;                     /* Property id (i.e CSS_PROPERTY_WIDTH). */
    CssProperty *v;            /* Value for property. */
{
    int nBytes;

    assert( i<128 && i>=0 );
    assert(!p->a || p->n > 0);

    /* Note: We used to avoid inserting duplicate properties into a
    ** single property set. But that led to errors with CSS like:
    **
    **     <selector> {
    **       padding: 3em;
    **       padding: -3em;
    **     }
    **
    ** The code in htmlprop.c needs to see both declarations, as the
    ** second one is ignored because the value is illegal. TODO: It
    ** would be better if we could verify the legality of the value
    ** here.
    */

    nBytes = (p->n + 1) * sizeof(struct CssPropertySetItem);
    p->a = (struct CssPropertySetItem *)HtmlRealloc(
        "CssPropertySet.a", (char *)p->a, nBytes
    );
    p->a[p->n].pProp = v;
    p->a[p->n].eProp = i;
    p->n++;
}


static void 
propertyFree(CssProperty *p){
  if (p && p->eType == CSS_TYPE_LIST) {
    int ii;
    CssProperty **apProp = (CssProperty **)p->v.p;
    for (ii = 0; apProp[ii]; ii++) {
        propertyFree(apProp[ii]);
    }
  }
  HtmlFree(p);
}

/*--------------------------------------------------------------------------
 *
 * propertySetFree --
 *
 *     Delete a property set and it's contents.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void 
propertySetFree(CssPropertySet *p){
    int i;
    if( !p ) return;
    for (i = 0; i < p->n; i++) {
        propertyFree(p->a[i].pProp);
    }
    HtmlFree(p->a);
    HtmlFree(p);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyDup --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *propertyDup(pProp)
    CssProperty *pProp;
{
    CssProperty *pRet;
    const char *z = HtmlCssPropertyGetString(pProp);
    int n = sizeof(CssProperty);

    if (z) n += (strlen(z) + 1);
    pRet = (CssProperty *)HtmlAlloc("CssProperty", n);
    memcpy(pRet, pProp, sizeof(CssProperty));
    if (z) {
        pRet->v.zVal = (char *)(&pRet[1]);
        strcpy(pRet->v.zVal, z);
    }
    return pRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertySetAddShortcutBorder --
 *
 *     Devolve the shortcut property 'border' into it's sub-properties and
 *     add the results to a CssPropertySet.
 *
 *     A 'border' property consists of a white-space seperated list of at
 *     most three quantities. Any of the three quantities may be omitted,
 *     but the order remains the same.
 *
 *     [<border-width>] [<border-style>] [<border-color>]
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void propertySetAddShortcutBorder(pParse, p, prop, v)
    CssParse *pParse;
    CssPropertySet *p;         /* Property set. */
    int prop;
    CssToken *v;               /* Value for property. */
{
    CONST char *z = v->z;
    CONST char *zEnd = z + v->n;
    int i;

    CssProperty *pBorderColor = 0;
    CssProperty *pBorderStyle = 0;
    CssProperty *pBorderWidth = 0;

    int aWidth[] = {
        CSS_PROPERTY_BORDER_TOP_WIDTH,
        CSS_PROPERTY_BORDER_RIGHT_WIDTH,
        CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
        CSS_PROPERTY_BORDER_LEFT_WIDTH,
    };
    int aStyle[] = {
        CSS_PROPERTY_BORDER_TOP_STYLE,
        CSS_PROPERTY_BORDER_RIGHT_STYLE,
        CSS_PROPERTY_BORDER_BOTTOM_STYLE,
        CSS_PROPERTY_BORDER_LEFT_STYLE,
    };
    int aColor[] = {
        CSS_PROPERTY_BORDER_TOP_COLOR,
        CSS_PROPERTY_BORDER_RIGHT_COLOR,
        CSS_PROPERTY_BORDER_BOTTOM_COLOR,
        CSS_PROPERTY_BORDER_LEFT_COLOR,
    };

    int iOffset = 0;        /* Offset in aWidth[], aStyle[], aColor[] */
    int nProp = 1;          /* Number of properties to set */
    switch (prop) {
        case CSS_SHORTCUTPROPERTY_BORDER:
            nProp = 4;
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_TOP:
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_RIGHT:
            iOffset = 1;
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_BOTTOM:
            iOffset = 2;
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_LEFT:
            iOffset = 3;
            break;
        default:
            assert(0);
    }

    while (z) {
        int n;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
        if (z) {
            CssToken token;
            CssProperty *pProp;
            int eType;

            token.z = z;
            token.n = n;
            pProp = tokenToProperty(0, &token);
            eType = pProp->eType;

            if (propertyIsLength(pParse, pProp) || eType == CSS_CONST_THIN || 
                eType == CSS_CONST_THICK        || eType == CSS_CONST_MEDIUM
            ) {
                if (pBorderWidth) {
                    HtmlFree(pProp);
                    goto parse_error;
                }
                pBorderWidth = pProp;
            } else if (
                eType == CSS_CONST_NONE   || eType == CSS_CONST_HIDDEN ||
                eType == CSS_CONST_DOTTED || eType == CSS_CONST_DASHED ||
                eType == CSS_CONST_SOLID  || eType == CSS_CONST_DOUBLE ||
                eType == CSS_CONST_GROOVE || eType == CSS_CONST_RIDGE  ||
                eType == CSS_CONST_OUTSET || eType == CSS_CONST_INSET 
            ) {
                if (pBorderStyle) {
                    HtmlFree(pProp);
                    goto parse_error;
                }
                pBorderStyle = pProp;
            } else {
                if (pBorderColor) {
                    HtmlFree(pProp);
                    goto parse_error;
                }
                pBorderColor = pProp;
            }
            z += n;
        }
    }

    if (!pBorderColor) {
        pBorderColor = HtmlCssStringToProperty("-tkhtml-no-color", -1);
    }
    if (!pBorderWidth) {
        pBorderWidth = HtmlCssStringToProperty("medium", -1);
    }
    if (!pBorderStyle) {
        pBorderStyle = HtmlCssStringToProperty("none", -1);
    }

    for (i = iOffset; i < iOffset+nProp; i++) {
        CssProperty *pC = pBorderColor;
        CssProperty *pW = pBorderWidth;
        CssProperty *pS = pBorderStyle;
        if (i != iOffset) {
            pC = propertyDup(pC);
            pW = propertyDup(pW);
            pS = propertyDup(pS);
        }
        propertySetAdd(p, aColor[i], pC);
        propertySetAdd(p, aWidth[i], pW);
        propertySetAdd(p, aStyle[i], pS);
    }
    return;

  parse_error:
    HtmlFree(pBorderStyle);
    HtmlFree(pBorderColor);
    HtmlFree(pBorderWidth);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyTransformBgPosition --
 *
 *     This function is called on values that might be assigned to the
 *     'background-position' property. If the property contains one of the
 *     following constants, it is overwritten with the corresponding percentage
 *     value before this function returns.
 *
 *         'top'    -> 0%
 *         'left'   -> 0%
 *         'bottom' -> 100%
 *         'right'  -> 100%
 *         'center' -> 50%
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
propertyTransformBgPosition(pProp)
    CssProperty *pProp;
{
    if (pProp) {
        double rVal;
        switch (pProp->eType) {
            case CSS_CONST_RIGHT:
            case CSS_CONST_BOTTOM:
                rVal = 100.0; 
                break;
    
            case CSS_CONST_CENTER:
                rVal = 50.0; 
                break;
     
            case CSS_CONST_TOP:
            case CSS_CONST_LEFT:
                rVal = 0.0; 
                break;
    
            default: 
                return;
        }
    
        pProp->eType = CSS_TYPE_PERCENT;
        pProp->v.rVal = rVal;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * shortcutBackground --
 *
 *     This function is called to handle the short cut property 'background'.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
shortcutBackground(pParse, p, v)
    CssParse *pParse;
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'background' property */
{
    CONST char *z= v->z;
    CONST char *zEnd = z + v->n;
    int nProp = 0;
    int ii;

    CssProperty *pColor = 0;
    CssProperty *pImage = 0;
    CssProperty *pRepeat = 0;
    CssProperty *pAttachment = 0;
    CssProperty *pPositionX = 0;
    CssProperty *pPositionY = 0;

    CssProperty *apProp[6];
    while (z && nProp<6) {
        int n;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
        if (z) {
            CssToken token;
            token.z = z;
            token.n = n;
            apProp[nProp] = tokenToProperty(pParse, &token);
            nProp++;
            assert(n>0);
            z += n;
        }
    }

    for (ii = 0; ii < nProp; ii++) {
        CssProperty *pProp = apProp[ii];
        if (propertyIsLength(pParse, pProp)) {
            if (!pPositionX) pPositionX = pProp;
            else if(!pPositionY) pPositionY = pProp;
            else goto error_out;
        } else {
            switch (pProp->eType) {
                case CSS_CONST_SCROLL:
                case CSS_CONST_FIXED:
                    if (pAttachment) goto error_out;
                    pAttachment = pProp;
                    break;

                case CSS_CONST_REPEAT:
                case CSS_CONST_NO_REPEAT:
                case CSS_CONST_REPEAT_X:
                case CSS_CONST_REPEAT_Y:
                    if (pRepeat) goto error_out;
                    pRepeat = pProp;
                    break;

                case CSS_TYPE_URL:
                case CSS_CONST_NONE:
                    if (pImage) goto error_out;
                    pImage = pProp;
                    break;

                case CSS_CONST_TOP:
                case CSS_CONST_BOTTOM:
                case CSS_CONST_RIGHT:
                case CSS_CONST_LEFT:
                case CSS_CONST_CENTER:
                case CSS_TYPE_PERCENT:
                    if (!pPositionX) pPositionX = pProp;
                    else if(!pPositionY) pPositionY = pProp;
                    else goto error_out;
                    break;

                default:
                    if (pColor) goto error_out;
                    pColor = pProp;
                    break;
            }
        }
    }

#if 0
    if (
        pPositionX && pPositionY && 
        ((pPositionX->eType == CSS_TYPE_PERCENT) ? 1 : 0) !=
        ((pPositionY->eType == CSS_TYPE_PERCENT) ? 1 : 0)
    ) {
        goto error_out;
    }
#endif

    /*
     * From CSS2 description of the 'background-position' property:
     */
    if (
        (pPositionX && (
            pPositionX->eType == CSS_CONST_TOP ||
            pPositionX->eType == CSS_CONST_BOTTOM
        )) ||
        (pPositionY && (
            pPositionY->eType == CSS_CONST_RIGHT ||
            pPositionY->eType == CSS_CONST_LEFT
        ))
    ) {
        CssProperty *pTmp = pPositionX;
        pPositionX = pPositionY;
        pPositionY = pTmp;
    }
    if ((pPositionX && (
            pPositionX->eType == CSS_CONST_TOP ||
            pPositionX->eType == CSS_CONST_BOTTOM)) || 
        (pPositionY && (
            pPositionY->eType == CSS_CONST_RIGHT ||
            pPositionY->eType == CSS_CONST_LEFT)) 
    ) {
        goto error_out;
    }
    propertyTransformBgPosition(pPositionX);
    propertyTransformBgPosition(pPositionY);
    if (!pPositionX && !pPositionY) { 
      pPositionX = HtmlCssStringToProperty("0%", 2);
      pPositionY = HtmlCssStringToProperty("0%", 2);
    }
    if (!pPositionX) { pPositionX = HtmlCssStringToProperty("50%", 3); }
    if (!pPositionY) { pPositionY = HtmlCssStringToProperty("50%", 3); }
 
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_IMAGE, 
        pImage ? pImage : HtmlCssStringToProperty("none", 4)
    );
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_ATTACHMENT, 
        pAttachment ? pAttachment : HtmlCssStringToProperty("scroll", 5)
    );
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_COLOR, 
        pColor ? pColor : HtmlCssStringToProperty("transparent", 11)
    );
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_REPEAT, 
        pRepeat ? pRepeat : HtmlCssStringToProperty("repeat", 6)
    );
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_POSITION_X, pPositionX);
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_POSITION_Y, pPositionY);

    return;

error_out:
    for (ii = 0; ii < nProp; ii++) {
        HtmlFree(apProp[ii]);
    }
}
/*
 *---------------------------------------------------------------------------
 *
 * shortcutListStyle --
 *
 * 	[ <'list-style-type'> || <'list-style-position'> ||
 * 	    <'list-style-image'> ] | inherit
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
shortcutListStyle(pParse, p, v)
    CssParse *pParse;
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'list-style' property */
{
    CONST char *z= v->z;
    CONST char *zEnd = z + v->n;

    CssProperty *pType = 0;
    CssProperty *pPosition = 0;
    CssProperty *pImage = 0;
    CssProperty *pProp = 0;

    while (z) {
        int n;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
        if (z) {
            pProp = textToProperty(pParse, z, n);
            switch (pProp->eType) {
                case CSS_CONST_INHERIT:
                    if (pType || pPosition || pImage) {
                        goto bad_parse;
                    }
                    pType = pProp;
                    pPosition = propertyDup(pProp);
                    pImage = propertyDup(pProp);
                    z = 0;
                    break;

                case CSS_CONST_INSIDE:
                case CSS_CONST_OUTSIDE:
                    if (pPosition) goto bad_parse;
                    pPosition = pProp;
                    break;

                case CSS_TYPE_URL:
                case CSS_TYPE_STRING:
                case CSS_TYPE_RAW:
                    if (pImage) goto bad_parse;
                    pImage = pProp;
                    break;

                case CSS_CONST_DISC:
                case CSS_CONST_CIRCLE:
                case CSS_CONST_SQUARE:
                case CSS_CONST_NONE:
                case CSS_CONST_DECIMAL:
                case CSS_CONST_LOWER_ALPHA:
                case CSS_CONST_UPPER_ALPHA:
                case CSS_CONST_LOWER_ROMAN:
                case CSS_CONST_UPPER_ROMAN:
                    if (pType) goto bad_parse;
                    pType = pProp;
                    break;

                default:
                    goto bad_parse;
            }
            if (z) {
                z += n;
            }
        }
    }

    propertySetAdd(p, CSS_PROPERTY_LIST_STYLE_TYPE, pType);
    propertySetAdd(p, CSS_PROPERTY_LIST_STYLE_POSITION, pPosition);
    propertySetAdd(p, CSS_PROPERTY_LIST_STYLE_IMAGE, pImage);

    return;

bad_parse:
    if (pProp) HtmlFree(pProp);
    if (pImage) HtmlFree(pImage);
    if (pPosition) HtmlFree(pPosition);
    if (pType) HtmlFree(pType);
}

static void
propertySetAddList(pParse, eProp, p, v)
    CssParse *pParse;
    int eProp;
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'content' property */
{
    CssProperty *pProp;
    CssProperty **apProp;
    int nAlloc;
    int n;

    int nElem = 0;
    const char *z = v->z;
    const char *zEnd = &z[v->n];

    /* Count the elements in the list. */
    z = HtmlCssGetNextListItem(z, zEnd-z, &n);
    while (z) {
        z = &z[n];
        nElem++;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
    }

    nAlloc = sizeof(CssProperty) + (nElem + 1) * sizeof(CssProperty *);
    pProp = (CssProperty *)HtmlAlloc("CssProperty", nAlloc);
    pProp->v.p = &(pProp[1]);
    pProp->eType = CSS_TYPE_LIST;
    apProp = (CssProperty **)(pProp->v.p);
    apProp[nElem] = 0;

    z = HtmlCssGetNextListItem(v->z, zEnd - v->z, &n);
    nElem = 0;
    while (z) {
        CssToken token;
        token.z = z;
        token.n = n;
        apProp[nElem] = tokenToProperty(pParse, &token);
        z = &z[n];
        nElem++;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
    }
    assert(apProp[nElem] == 0);

    propertySetAdd(p, eProp, pProp);
}

/*
 *---------------------------------------------------------------------------
 *
 * getNextFontFamily --
 *
 *     This function is used for splitting up a font-family list.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CONST char *
getNextFontFamily(zList, nList, pzNext)
    CONST char *zList;
    int nList;
    CONST char **pzNext;
{
    CssToken token;
    int t;
    int nToken = 0;
    int nElem = 0;
    char *zRet;

    while( 
        (CT_EOF != (t = HtmlCssGetToken(&zList[nElem], nList-nElem, &nToken))) && 
        (t != CT_COMMA) 
    ){
      nElem += nToken;
    }
    token.z = zList;
    token.n = nElem;
    
    *pzNext = &zList[nElem];
    if( t == CT_COMMA ){
        *pzNext = &zList[nElem + 1];
    }

    zRet = tokenToString(&token);
    dequote(zRet);
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * textToFontFamilyProperty --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *
textToFontFamilyProperty(pParse, zText, nText)
    CssParse *pParse;          /* Parse context */
    const char *zText;
    int nText;
{
    const char *zFamily = 0;

    Tcl_HashTable *aFamily = &pParse->pTree->aFontFamilies;
    const char *zCsr = zText;
    const char *zEnd = &zText[nText];

    while (zCsr < zEnd) {
        Tcl_HashEntry *pEntry;
        zFamily = getNextFontFamily(zCsr, zEnd-zCsr, &zCsr);
        pEntry = Tcl_FindHashEntry(aFamily, zFamily);
        HtmlFree(zFamily);
        if (pEntry) {
            zFamily = Tcl_GetHashValue(pEntry);
            if (!zFamily) {
                zFamily = Tcl_GetHashKey(aFamily, pEntry);
            } 
            break;
        }
        zFamily = 0;
    }

    return textToProperty(0, (zFamily ? zFamily : "Helvetica"), -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertySetAddFontFamily --
 *
 *     Add a 'font-family' property value to a property set.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
propertySetAddFontFamily(pParse, p, v)
    CssParse *pParse;          /* Parse context */
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'background' property */
{
    CssProperty *pProp = textToFontFamilyProperty(pParse, v->z, v->n);
    propertySetAdd(p, CSS_PROPERTY_FONT_FAMILY, pProp);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertySetAddShortcutFont --
 *
 *     Tkhtml3 supports two syntax for the 'font' short-hand property:
 *
 *       font ::= inherit
 *
 *       font ::= [ <font-style> || <font-variant> || <font-weight> ]? 
 *                <font-size> [ / <line-height> ]? <font-family>
 *
 *     According to CSS2.1, it should also be possible to use the following:
 *
 *       font ::= caption
 *       font ::= icon
 *       font ::= menu
 *       font ::= message-box
 *       font ::= small-caption
 *       font ::= status-bar
 *
 *     But Tkhtml3 does not support these yet.
 *     
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
propertySetAddShortcutFont(pParse, p, v)
    CssParse *pParse;
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'background' property */
{
    CONST char *z= v->z;
    CONST char *zEnd = z + v->n;

    static const CssToken normal = { "normal", CSS_CONST_NORMAL };

    CssProperty *pStyle = 0;
    CssProperty *pVariant = 0;
    CssProperty *pWeight = 0;
    CssProperty *pSize = 0;
    CssProperty *pLineHeight = 0;
    CssProperty *pFamily = 0;

    CssProperty *pProp = 0;
    while (z) {
        int n;
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
        if (z) {
            pProp = textToProperty(0, z, n);
            switch (pProp->eType) {
                case CSS_CONST_INHERIT:
                    if (pStyle || pVariant || pWeight || 
                            pSize || pLineHeight || pFamily
                    ) {
                        goto bad_parse;
                    }
                    pStyle = pProp;
                    pVariant = propertyDup(pProp);
                    pWeight = propertyDup(pProp);
                    pSize = propertyDup(pProp);
                    pLineHeight = propertyDup(pProp);
                    pFamily = propertyDup(pProp);
                    z = 0;
                    break;

                case CSS_CONST_NORMAL:
                    HtmlFree(pProp);
                    break;
    
                case CSS_CONST_ITALIC:
                case CSS_CONST_OBLIQUE:
                    if (pStyle) goto bad_parse;
                    pStyle = pProp;
                    break;
    
                case CSS_CONST_BOLD:
                case CSS_CONST_BOLDER:
                case CSS_CONST_LIGHTER:
                case CSS_TYPE_FLOAT:
                    if (pWeight) goto bad_parse;
                    pWeight = pProp;
                    break;
    
                case CSS_CONST_SMALL_CAPS:
                    if (pVariant) goto bad_parse;
                    pVariant = pProp;
                    break;

                default: {
                    int hasLineHeight = 0;
                    if (pProp->eType == CSS_TYPE_STRING || 
                        pProp->eType == CSS_TYPE_RAW
                    ) {
                        int j;
                        for (j = 0; j < n && z[j] != '/'; j++);
                        if (j == n) goto bad_parse;
                        HtmlFree(pProp);
                        n = j;
                        pProp = textToProperty(0, z, j);
                    } 
                    pSize = pProp;
                    pProp = 0;
                    z += n;
                    while (isspace(*z) || *z == '/') {
                        if (*z == '/') hasLineHeight = 1;
                        z++;
                    }
    
                    if (hasLineHeight) {
                        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
                        pLineHeight = textToProperty(0, z, n);
                        z += n;
                    } 
                    z = HtmlCssGetNextListItem(z, zEnd-z, &n);
                    if (!z) goto bad_parse;
                    pFamily = textToFontFamilyProperty(pParse, z, zEnd-z);
                    z = 0;
               }
            }

            if (z) {
                z += n;
            }
        }
    }
    pProp = 0;

    /* Both the font-family and size must be specified. */
    if (!pFamily || !pSize) goto bad_parse;

    /* Everything else defaults to "normal". */
    if (!pStyle) pStyle = tokenToProperty(pParse, &normal);
    if (!pVariant) pVariant = tokenToProperty(pParse, &normal);
    if (!pWeight) pWeight = tokenToProperty(pParse, &normal);
    if (!pLineHeight) pLineHeight = tokenToProperty(pParse, &normal);

    propertySetAdd(p, CSS_PROPERTY_FONT_STYLE, pStyle);
    propertySetAdd(p, CSS_PROPERTY_FONT_VARIANT, pVariant);
    propertySetAdd(p, CSS_PROPERTY_FONT_WEIGHT, pWeight);
    propertySetAdd(p, CSS_PROPERTY_FONT_SIZE, pSize);
    propertySetAdd(p, CSS_PROPERTY_FONT_FAMILY, pFamily);
    propertySetAdd(p, CSS_PROPERTY_LINE_HEIGHT, pLineHeight);

    return;

bad_parse:
    if (pProp) HtmlFree(pProp);
    if (pStyle) HtmlFree(pStyle);
    if (pVariant) HtmlFree(pVariant);
    if (pWeight) HtmlFree(pWeight);
    if (pSize) HtmlFree(pSize);
    if (pFamily) HtmlFree(pFamily);
    if (pLineHeight) HtmlFree(pLineHeight);
}


/*
 *---------------------------------------------------------------------------
 *
 * tokenToPropertyList --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
tokenToPropertyList(pToken, apProp, nMax) 
    CssToken *pToken;
    CssProperty **apProp;
    int nMax;
{
    CONST char *z= pToken->z;
    CONST char *zEnd = z + pToken->n;
    int nProp = 0;
    int ii;

    while (z) {
        int nBytes;
        z = HtmlCssGetNextListItem(z, zEnd-z, &nBytes);
        if (z) {
            CssToken token;
            if (nProp == nMax) {
                /* Fail: To many items in list. */
                goto parse_failed;
            }
            token.z = z;
            token.n = nBytes;
            apProp[nProp++] = tokenToProperty(0, &token);
            z += nBytes;
        }
    }

    return nProp;

  parse_failed:
    for (ii = 0; ii < nProp; ii++) {
        HtmlFree(apProp[ii]);
    }
    
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * shortcutBackgroundPosition --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
shortcutBackgroundPosition(pParse, p, v)
    CssParse *pParse;          /* Parse context */
    CssPropertySet *p;         /* Property set */
    CssToken *v;               /* Value for 'background' property */
{
    int nProp;
    CssProperty *apProp[2];

    nProp = tokenToPropertyList(v, apProp, 2);
    if (nProp == 0) {
        /* Parse error. Ignore this. */
        return;
    }
    assert(nProp == 1 || nProp == 2);

    if (nProp == 1) {
        if (
            apProp[0]->eType == CSS_TYPE_PERCENT || 
            apProp[0]->eType == CSS_CONST_LEFT || 
            apProp[0]->eType == CSS_CONST_RIGHT || 
            apProp[0]->eType == CSS_CONST_CENTER || 
            propertyIsLength(pParse, apProp[0])
        ) {
            apProp[1] = HtmlCssStringToProperty("50%", 3);
        } else if (
            apProp[0]->eType == CSS_CONST_TOP || 
            apProp[0]->eType == CSS_CONST_BOTTOM
        ) {
            apProp[1] = apProp[0];
            apProp[0] = HtmlCssStringToProperty("50%", 3);
        } else {
            /* Parse error. Ignore this. */
            HtmlFree(apProp[0]);
            return;
        }
    }

    else {
        int eType0 = apProp[0]->eType;
        int eType1 = apProp[1]->eType;

        if ((
                (eType0 == CSS_CONST_TOP || eType0 == CSS_CONST_BOTTOM) && (
                    eType1 == CSS_CONST_LEFT || 
                    eType1 == CSS_CONST_RIGHT || 
                    eType1 == CSS_CONST_CENTER
                )
            ) || (
                (eType1 == CSS_CONST_LEFT || eType1 == CSS_CONST_RIGHT) && (
                    eType0 == CSS_CONST_TOP || 
                    eType0 == CSS_CONST_BOTTOM || 
                    eType0 == CSS_CONST_CENTER
                )
            )
        ) {
            CssProperty *pProp = apProp[0];
            apProp[0] = apProp[1];
            apProp[1] = pProp;
        }

        if (
            apProp[0]->eType == CSS_CONST_TOP     || 
            apProp[0]->eType == CSS_CONST_BOTTOM  ||
            apProp[1]->eType == CSS_CONST_LEFT    || 
            apProp[1]->eType == CSS_CONST_RIGHT
        ) {
            /* Parse error. Ignore this. */
            HtmlFree(apProp[0]);
            HtmlFree(apProp[1]);
            return;
        }
    }

    propertyTransformBgPosition(apProp[0]);
    propertyTransformBgPosition(apProp[1]);
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_POSITION_X, apProp[0]);
    propertySetAdd(p, CSS_PROPERTY_BACKGROUND_POSITION_Y, apProp[1]);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertySetAddShortcutBorderColor --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void propertySetAddShortcutBorderColor(p, prop, v)
    CssPropertySet *p;         /* Property set. */
    int prop;
    CssToken *v;               /* Value for property. */
{
    CONST char *z= v->z;
    CONST char *zEnd = z + v->n;
    int n;

    int i = 0;                 /* Index of apProp to read next color in to */
    CssProperty *apProp[4];    /* Array of color properties */
    memset(apProp, 0, sizeof(CssProperty *)*4);

    while (z && i<4) {
        z = HtmlCssGetNextListItem(z, zEnd-z, &n);
        if (z) {
            CssToken token;
            token.z = z;
            token.n = n;
            apProp[i] = tokenToProperty(0, &token);
            i++;
            assert(n>0);
            z += n;
        }
    }

    switch (i) {
        case 1:
            apProp[1] = propertyDup(apProp[0]);
            apProp[2] = propertyDup(apProp[0]);
            apProp[3] = propertyDup(apProp[0]);
            break;
        case 2:
            apProp[2] = propertyDup(apProp[0]);
            apProp[3] = propertyDup(apProp[1]);
            break;
        case 3:
            apProp[3] = propertyDup(apProp[1]);
            break;
        case 4:
            break;
        default:
            return;
    }

    switch (prop) {
        case CSS_SHORTCUTPROPERTY_BORDER_COLOR:
            propertySetAdd(p, CSS_PROPERTY_BORDER_TOP_COLOR, apProp[0]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_RIGHT_COLOR, apProp[1]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_BOTTOM_COLOR, apProp[2]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_LEFT_COLOR, apProp[3]);
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_STYLE:
            propertySetAdd(p, CSS_PROPERTY_BORDER_TOP_STYLE, apProp[0]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_RIGHT_STYLE, apProp[1]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_BOTTOM_STYLE, apProp[2]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_LEFT_STYLE, apProp[3]);
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_WIDTH:
            propertySetAdd(p, CSS_PROPERTY_BORDER_TOP_WIDTH, apProp[0]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_RIGHT_WIDTH, apProp[1]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_BOTTOM_WIDTH, apProp[2]);
            propertySetAdd(p, CSS_PROPERTY_BORDER_LEFT_WIDTH, apProp[3]);
            break;
        case CSS_SHORTCUTPROPERTY_PADDING:
            propertySetAdd(p, CSS_PROPERTY_PADDING_TOP, apProp[0]);
            propertySetAdd(p, CSS_PROPERTY_PADDING_RIGHT, apProp[1]);
            propertySetAdd(p, CSS_PROPERTY_PADDING_BOTTOM, apProp[2]);
            propertySetAdd(p, CSS_PROPERTY_PADDING_LEFT, apProp[3]);
            break;
        case CSS_SHORTCUTPROPERTY_MARGIN:
            propertySetAdd(p, CSS_PROPERTY_MARGIN_TOP, apProp[0]);
            propertySetAdd(p, CSS_PROPERTY_MARGIN_RIGHT, apProp[1]);
            propertySetAdd(p, CSS_PROPERTY_MARGIN_BOTTOM, apProp[2]);
            propertySetAdd(p, CSS_PROPERTY_MARGIN_LEFT, apProp[3]);
            break;
    }
}

/*--------------------------------------------------------------------------
 *
 * selectorFree --
 *
 *     Delete a linked list of CssSelector structs, including the 
 *     CssSelector.zValue and CssSelector.zAttr fields.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void selectorFree(pSelector)
    CssSelector *pSelector;
{
    if( !pSelector ) return;
    selectorFree(pSelector->pNext);
    HtmlFree(pSelector->zValue);
    HtmlFree(pSelector->zAttr);
    HtmlFree(pSelector);
}

/*
 *---------------------------------------------------------------------------
 *
 * comparePriority --
 *
 *     Compare stylesheet priorities *pLeft and *pRight, returning less
 *     than, equal to or greater than zero if *pLeft is less than, equal
 *     to, or greater than pRight. i.e the expression:
 *
 *         (*pLeft - *pRight)
 *
 *     Note that this comparison follows the rules of CSS2, not CSS1.
 *
 * Results:
 *     Result of comparison (see above).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
comparePriority(pLeft, pRight)
    CssPriority *pLeft;
    CssPriority *pRight;
{
    int aNormal[3] = {CSS_ORIGIN_AUTHOR, CSS_ORIGIN_USER, CSS_ORIGIN_AGENT};
    int aImportant[3] = {CSS_ORIGIN_USER, CSS_ORIGIN_AUTHOR, CSS_ORIGIN_AGENT};
    int *a = 0;

    if (pLeft->important && pRight->important) {
        a = aImportant;
    }
    if (!pLeft->important && !pRight->important) {
        a = aNormal;
    }

    /* If a is set then the 'important' flags are both set or both cleared.
     * This means we have to look at the origin (and possibly style-id)
     * values.
     */
    if (a) { 
        int i;
        if (pLeft->origin == pRight->origin) {
            CONST char *zLeft = Tcl_GetString(pLeft->pIdTail);
            CONST char *zRight = Tcl_GetString(pRight->pIdTail);
            return strcmp(zLeft, zRight);
        }
        for (i = 0; i < 3; i++) {
            if (pLeft->origin == a[i]) return 1;
            if (pRight->origin == a[i]) return -1;
        }
        assert(!"Impossible");
    }

    /* One 'important' flag is set and the other cleared. The highest
     * priority is therefore the one with the 'important' flag set.
     */
    return pLeft->important ? 1 : -1;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * newCssPriority --
 *
 *     Add a new entry to the CssStyleSheet.pPriority list with values
 *     origin, pIdTail and important. Update the CssPriority.iPriority
 *     variable for all list members that require it.
 *
 *     See comments above CssPriority struct in cssInt.h for details.
 *
 *     The pointer to pIdList is copied and the reference count increased
 *     by one. It is decreased when the new list entry is deleted (along
 *     with the rest of the stylesheet).
 *
 * Results:
 *     Pointer to new list entry.
 *
 * Side effects:
 *     Modifies linked-list pStyle->pPriority. 
 *
 *---------------------------------------------------------------------------
 */
static CssPriority * 
newCssPriority(pStyle, origin, pIdTail, important)
    CssStyleSheet *pStyle;
    int origin;
    Tcl_Obj *pIdTail;
    int important;
{
    CssPriority *pNew;      /* New list entry */

    pNew = HtmlNew(CssPriority);
    pNew->origin = origin;
    pNew->important = important;
    pNew->pIdTail = pIdTail;
    Tcl_IncrRefCount(pIdTail);

    switch (origin) {
        case CSS_ORIGIN_AGENT:
            pNew->iPriority = 1;
            break;
        case CSS_ORIGIN_USER:
            if (important) {
                pNew->iPriority = 5;
            } else {
                pNew->iPriority = 2;
            }
            break;
        case CSS_ORIGIN_AUTHOR:
            if (important) {
                pNew->iPriority = 4;
            } else {
                pNew->iPriority = 3;
            }
            break;
        default:
            assert(!"Impossible");
    }

    pNew->pNext = pStyle->pPriority;
    pStyle->pPriority = pNew;

    return pNew;
}

/*
 *---------------------------------------------------------------------------
 *
 * cssParse --
 *
 *     This routine does the work of parsing stylesheets or style
 *     attributes on behalf of HtmlCssParse() and HtmlCssInlineParse()
 *     respectively. 
 * 
 *     The first two argument identify the length of, and the text to be
 *     parsed.
 *
 *     The third argument is true if z points to the text of a style
 *     attribute, i.e: "color:red ; margin:0.4em". If false, then z points
 *     to a complete stylesheet document, i.e. "H1 {text-size: 1.2em}". 
 *     The stylesheet produced when parsing a style is the same as 
 *     "* {<style text>}".
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
cssParse(
pTree, n, z, isStyle, origin, pStyleId, pImportCmd, pUrlCmd, pErrorVar, ppStyle)
    HtmlTree *pTree;
    int n;                       /* Size of z in bytes */
    CONST char *z;               /* Text of attribute/document */
    int isStyle;                 /* True if this is a style attribute */
    int origin;                  /* CSS_ORIGIN_* value */
    Tcl_Obj *pStyleId;           /* Second and later parts of stylesheet id */
    Tcl_Obj *pImportCmd;         /* Command to invoke to process @import */
    Tcl_Obj *pUrlCmd;            /* Command to invoke to translate url() */
    Tcl_Obj *pErrorVar;          /* Name of error-log variable */
    CssStyleSheet **ppStyle;     /* IN/OUT: Stylesheet to append to   */
{
    CssParse sParse;
    int ii;

    memset(&sParse, 0, sizeof(CssParse));
    sParse.origin = origin;
    sParse.pStyleId = pStyleId;
    sParse.pImportCmd = pImportCmd;
    sParse.pUrlCmd = pUrlCmd;
    sParse.interp = (pTree ? pTree->interp : 0);
    sParse.pTree = pTree;
    if (pErrorVar) {
        sParse.pErrorLog = Tcl_NewObj();
        Tcl_IncrRefCount(sParse.pErrorLog);
    }

    if( n<0 ){
        n = strlen(z);
    }

    /* If *ppStyle is NULL, then create a new CssStyleSheet object. If it
     * is not zero, then append the rules from the new stylesheet document
     * to the existing object.
     */
    if (0==*ppStyle) {
        sParse.pStyle = HtmlNew(CssStyleSheet);
        
        /* If pStyleId is not NULL, then initialise the hash-tables */
        if (pStyleId) {
            Tcl_InitHashTable(&sParse.pStyle->aByTag, TCL_STRING_KEYS);
            Tcl_InitHashTable(&sParse.pStyle->aByClass, TCL_STRING_KEYS);
            Tcl_InitHashTable(&sParse.pStyle->aById, TCL_STRING_KEYS);
        }
    } else {
        sParse.pStyle = *ppStyle;
    }

    /* If this is a stylesheet, not a style attribute, add the priority
     * entries for both regular and "!important" properties for this
     * stylesheet to the priority list.
     *
     * TODO: Sometimes people specify "!important" inside of style 
     * attributes. Tkhtml currently ignores this. Not sure if that's 
     * right or not...
     */
    if (pStyleId) {
        sParse.pPriority1 = newCssPriority(sParse.pStyle, origin, pStyleId, 0);
        sParse.pPriority2 = newCssPriority(sParse.pStyle, origin, pStyleId, 1);
    }

    if (isStyle) {
        HtmlCssRunStyleParser(z, n, &sParse);
    } else {
        HtmlCssRunParser(z, n, &sParse);
    }

    *ppStyle = sParse.pStyle;

    /* Clean up anything left in sParse */
    selectorFree(sParse.pSelector);
    for (ii = 0; ii < sParse.nXtra; ii++) {
        selectorFree(sParse.apXtraSelector[ii]);
    }
    propertySetFree(sParse.pPropertySet);
    propertySetFree(sParse.pImportant);

    if (pErrorVar) {
        Tcl_ObjSetVar2(pTree->interp, pErrorVar, 0, sParse.pErrorLog, 0);
        Tcl_DecrRefCount(sParse.pErrorLog);
    }

    return 0;
}

int 
HtmlCssSelectorParse(pTree, n, z, ppStyle)
    HtmlTree *pTree;
    int n;
    const char *z;
    CssStyleSheet **ppStyle;
{
    return cssParse(pTree, n, z, 0, 0, 0, 0, 0, 0, ppStyle);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleParse --
 *
 *     Compile a stylesheet document from text and add it to the widget.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlStyleParse(pTree, pStyleText, pId, pImportCmd, pUrlCmd, pErrorVar)
    HtmlTree *pTree;
    Tcl_Obj *pStyleText;
    Tcl_Obj *pId;
    Tcl_Obj *pImportCmd;
    Tcl_Obj *pUrlCmd;
    Tcl_Obj *pErrorVar;
{
    int origin = 0;
    Tcl_Obj *pStyleId = 0;
    CONST char *zId;
    CONST char *zStyleText;
    int nStyleText;

    /* Parse up the stylesheet id. It must begin with one of the strings
     * "agent", "user" or "author". After that it may contain any text.
     */
    zId = Tcl_GetString(pId);
    if (0==strncmp("agent", zId, 5)) {
        origin = CSS_ORIGIN_AGENT;
        pStyleId = Tcl_NewStringObj(&zId[5], -1);
    }
    else if (0==strncmp("user", zId, 4)) {
        origin = CSS_ORIGIN_USER;
        pStyleId = Tcl_NewStringObj(&zId[4], -1);
    }
    else if (0==strncmp("author", zId, 5)) {
        origin = CSS_ORIGIN_AUTHOR;
        pStyleId = Tcl_NewStringObj(&zId[6], -1);
    }
    if (!pStyleId) {
        Tcl_AppendResult(pTree->interp, "Bad style-sheet-id: ", zId, 0);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(pStyleId);

    /* If there is already a stylesheet in pTree->pStyle, then this call will
     * parse the stylesheet text in pStyleText and append rules to the
     * existing stylesheet. If pTree->pStyle is NULL, then a new stylesheet is
     * created. Within Tkhtml, each document only ever has a single stylesheet
     * object, possibly created by combining text from multiple stylesheet
     * documents.
     */
    zStyleText = Tcl_GetStringFromObj(pStyleText, &nStyleText);
    cssParse(
        pTree,
        nStyleText, zStyleText,            /* Stylesheet text */
        0,                                 /* This is not a style attribute */
        origin,                            /* Origin - CSS_ORIGIN_XXX */
        pStyleId,                          /* Rest of -id option */
        pImportCmd,                        /* How to handle @import */
        pUrlCmd,                           /* How to handle url() */
        pErrorVar,                         /* Variable to store errors in */
        &pTree->pStyle                     /* CssStylesheet to update/create */
    );

    Tcl_DecrRefCount(pStyleId);
    return TCL_OK;
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssInlineParse --
 *
 *     Parse the style attribute value pointed to by z, length n bytes. See
 *     comments above cssParse() for more detail.
 *
 * Results:
 *
 *     Returns a CssStyleSheet pointer, written to *ppStyle.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
int 
HtmlCssInlineParse(
    HtmlTree *pTree,
    int n,
    const char *z,
    CssPropertySet **ppPropertySet
){
    CssStyleSheet *pStyle = 0;
    assert(ppPropertySet && !(*ppPropertySet));
    cssParse(pTree, n, z, 1, 0, 0, 0, 0, 0, &pStyle);

    if (pStyle) {
        if (pStyle->pUniversalRules) {
            assert(!pStyle->pUniversalRules->pNext);
            *ppPropertySet = pStyle->pUniversalRules->pPropertySet;
            pStyle->pUniversalRules->pPropertySet = 0;
        }
        assert(!pStyle->pPriority);
        HtmlCssStyleSheetFree(pStyle);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * ruleFree --
 *
 *     Free the CssRule object pRule.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ruleFree(pRule)
    CssRule *pRule;
{
    if (pRule) {
        if (pRule->freeSelector) {
            selectorFree(pRule->pSelector);
        }
        if (pRule->freePropertySets) {
            propertySetFree(pRule->pPropertySet);
        }
        HtmlFree(pRule);
    }
}

static void
freeRulesList(ppList)
    CssRule **ppList;
{
    CssRule *pRule = *ppList;
    while (pRule) { 
        CssRule *pNext = pRule->pNext;
        ruleFree(pRule);
        pRule = pNext;
    }
    *ppList = 0;
}

static void
freeRulesHash(pHash)
    Tcl_HashTable *pHash;
{
    Tcl_HashSearch search;
    Tcl_HashEntry *pEntry;
  
    for (
        pEntry = Tcl_FirstHashEntry(pHash, &search); 
        pEntry; 
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        CssRule *pRule = (CssRule *)Tcl_GetHashValue(pEntry);
        freeRulesList(&pRule);
    }
    Tcl_DeleteHashTable(pHash);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssStyleSheetFree --
 *
 *     Delete the internal representation of the stylesheet configuration.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCssStyleSheetFree(pStyle)
    CssStyleSheet *pStyle;
{
    if (pStyle) {
        CssPriority *pPriority;

        /* Free the universal rules list */
        freeRulesList(&pStyle->pUniversalRules); 
        freeRulesList(&pStyle->pAfterRules); 
        freeRulesList(&pStyle->pBeforeRules); 
        freeRulesHash(&pStyle->aByTag); 
        freeRulesHash(&pStyle->aByClass); 
        freeRulesHash(&pStyle->aById); 

        /* Free the priorities list */
        pPriority = pStyle->pPriority;
        while (pPriority) {
            CssPriority *pNext = pPriority->pNext;
            Tcl_DecrRefCount(pPriority->pIdTail);
            HtmlFree(pPriority);
            pPriority = pNext;
        }

        HtmlFree(pStyle);
    }
}

/*
** Return the number of syntax errors that occured while parsing the
** style-sheet.
*/
int HtmlCssStyleSheetSyntaxErrs(CssStyleSheet *pStyle){
    return pStyle->nSyntaxErr;
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssDeclaration --
 *
 *     This function is called by the CSS parser when it parses a property 
 *     declaration (i.e "<property> : <expression>").
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void
HtmlCssDeclaration(pParse, pProp, pExpr, isImportant)
    CssParse *pParse;
    CssToken *pProp;
    CssToken *pExpr;
    int isImportant;         /* True if the !IMPORTANT symbol was seen */
{
    int prop; 
    CssPropertySet **ppPropertySet;
    char zBuf[128];

    /* Do nothing if the isIgnore flag is set */
    if (pParse->isIgnore) return;

#if TRACE_PARSER_CALLS
    printf("HtmlCssDeclaration(%p, \"%.*s\", \"%.*s\", %d)\n", 
        pParse,
        pProp?pProp->n:0, pProp?pProp->z:"", 
        pExpr?pExpr->n:0, pExpr?pExpr->z:"",
        isImportant
    );
#endif

    if (pParse->pStyleId == 0) {
        isImportant = 0;
    }

    /* Resolve the property name. If we don't recognize it, then ignore the
     * declaration (CSS2 spec says to do this - besides, what else could we
     * do?).
     */
#if 0
    prop = HtmlCssPropertyLookup(pProp->n, pProp->z);
#else
    if (pProp->n > 127) pProp->n = 127;
    memcpy(zBuf, pProp->z, pProp->n);
    zBuf[pProp->n] = '\0';
    dequote(zBuf);
    prop = HtmlCssPropertyLookup(-1, zBuf);
#endif
    if( prop<0 ) return;

    if (isImportant) {
        ppPropertySet = &pParse->pImportant;
    } else {
        ppPropertySet = &pParse->pPropertySet;
    }
    if( !*ppPropertySet ){
        *ppPropertySet = propertySetNew();
    }

    switch (prop) {
        case CSS_SHORTCUTPROPERTY_BORDER:
        case CSS_SHORTCUTPROPERTY_BORDER_LEFT:
        case CSS_SHORTCUTPROPERTY_BORDER_RIGHT:
        case CSS_SHORTCUTPROPERTY_BORDER_TOP:
        case CSS_SHORTCUTPROPERTY_BORDER_BOTTOM:
            propertySetAddShortcutBorder(pParse, *ppPropertySet, prop, pExpr);
            break;
        case CSS_SHORTCUTPROPERTY_BORDER_COLOR:
        case CSS_SHORTCUTPROPERTY_BORDER_STYLE:
        case CSS_SHORTCUTPROPERTY_BORDER_WIDTH:
        case CSS_SHORTCUTPROPERTY_PADDING:
        case CSS_SHORTCUTPROPERTY_MARGIN:
            propertySetAddShortcutBorderColor(*ppPropertySet, prop, pExpr);
            break;
        case CSS_SHORTCUTPROPERTY_BACKGROUND:
            shortcutBackground(pParse, *ppPropertySet, pExpr);
            break;
        case CSS_SHORTCUTPROPERTY_BACKGROUND_POSITION:
            shortcutBackgroundPosition(pParse, *ppPropertySet, pExpr);
            break;
        case CSS_SHORTCUTPROPERTY_FONT:
            propertySetAddShortcutFont(pParse, *ppPropertySet, pExpr);
            break;
        case CSS_PROPERTY_FONT_FAMILY:
            propertySetAddFontFamily(pParse, *ppPropertySet, pExpr);
            break;
        case CSS_SHORTCUTPROPERTY_LIST_STYLE:
            shortcutListStyle(pParse, *ppPropertySet, pExpr);
            break;
        case CSS_PROPERTY_CONTENT:
        case CSS_PROPERTY_COUNTER_INCREMENT:
        case CSS_PROPERTY_COUNTER_RESET:
            propertySetAddList(pParse, prop, *ppPropertySet, pExpr);
            break;
        default:
            propertySetAdd(*ppPropertySet, prop, tokenToProperty(pParse,pExpr));
    }
}



/*--------------------------------------------------------------------------
 *
 * HtmlCssSelector --
 *
 *     This is called whenever a simple selector is parsed. 
 *     i.e. "H1" or ":before".
 *
 *     A CssSelector struct is allocated and added to the beginning of the
 *     linked list at pParse->pSelector;
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void HtmlCssSelector(pParse, stype, pAttr, pValue)
    CssParse *pParse; 
    int stype;               /* One of the CSS_SELECTOR_TYPE_XXX values */
    CssToken *pAttr; 
    CssToken *pValue;
{
    CssSelector *pSelector;

    /* Do nothing if the isIgnore flag is set */
    if (pParse->isIgnore) return;

#if TRACE_PARSER_CALLS
    /* I used this to make sure the parser was passing the components of
     * selectors to this function in the correct order. Once this was 
     * verified, it is not particularly useful trace output. But we'll leave
     * it here for the time being in case something comes up.
     */
    printf("HtmlCssSelector(%p, %s, \"%.*s\", \"%.*s\")\n", 
        pParse, constantToString(stype), 
        pAttr?pAttr->n:0, pAttr?pAttr->z:"", 
        pValue?pValue->n:0, pValue?pValue->z:""
    );
#endif

    pSelector = HtmlNew(CssSelector);
    pSelector->eSelector = stype;
    pSelector->zValue = tokenToString(pValue);
    pSelector->zAttr = tokenToString(pAttr);
    pSelector->pNext = pParse->pSelector;
    pSelector->isDynamic = (
        (pSelector->pNext && pSelector->pNext->isDynamic) ||
#if 0
        (stype == CSS_PSEUDOCLASS_LINK)                   ||
        (stype == CSS_PSEUDOCLASS_VISITED)                ||
#endif
        (stype == CSS_PSEUDOCLASS_HOVER)                  ||
        (stype == CSS_PSEUDOCLASS_FOCUS)                  ||
        (stype == CSS_PSEUDOCLASS_ACTIVE)
    ) ? 1 : 0;
    pParse->pSelector = pSelector;
    dequote(pSelector->zValue);

    /* Tag names are case-insensitive - fold to lower case */
    if( stype==CSS_SELECTOR_TYPE ){
        assert(pSelector->zValue);
        Tcl_UtfToLower(pSelector->zValue);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ruleCompare --
 *
 *     Compare the priority of two rule objects. Return greater than zero
 *     if the priority of pLeft is higher, zero if the two rules have the
 *     same priority, and negative if pRight has higher priority. i.e.:
 *
 *         PRIORITY(pLeft) - PRIORITY(pRight)
 *
 *     This function is used to determine the order of rules in the
 *     rules CssRule.pNext linked list.
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
ruleCompare(CssRule *pLeft, CssRule *pRight) {
    int res = 0;

    assert(pLeft && pRight);
    assert(
        (pLeft->pPriority && pRight->pPriority) ||  
        (!pLeft->pPriority && !pRight->pPriority)
    );
    if( !pLeft->pPriority ) return 0;

    res = pLeft->pPriority->iPriority - pRight->pPriority->iPriority;
    if (res == 0) {
        /* But here we want (left - right), because specificity is higher
         * for more specific rules.
         */
        res = pLeft->specificity - pRight->specificity;

        if (res == 0) {
            CONST char *zLeft = Tcl_GetString(pLeft->pPriority->pIdTail);
            CONST char *zRight = Tcl_GetString(pRight->pPriority->pIdTail);
            res = strcmp(zLeft, zRight);

            if (res == 0) {
                /* If we get here, the rules have the same specificity,
                 * come from the same stylesheet and both either have the
                 * !IMPORTANT flag set or cleared. In this case the higher
                 * priority rule is the one that appeared later in the 
                 * source stylesheet.
                 */
		res = pLeft->iRule - pRight->iRule;
            }
        }
    }
    return res;
}

static void
insertRule(ppList, pRule) 
    CssRule **ppList;
    CssRule *pRule;
{
    if (!*ppList || ruleCompare(*ppList, pRule) <= 0) {
        /* If the default list is currently empty, or the rule being
         * added has higher priority than the first rule in the list,
         * our rule becomes the new head of the list.
         */
        pRule->pNext = *ppList;
        *ppList = pRule;
    } else {
        /* Otherwise insert the new rule into the list, ordered by
         * priority. If there exists another rule with the same
         * priority, then this rule is inserted into the list *before*
         * it. This is because when rules are of equal priority, the
         * latter specified wins.
         */
        CssRule *pR = *ppList;
        while (pR->pNext && ruleCompare(pR->pNext, pRule)>0 ) {
            pR = pR->pNext;
        }
        pRule->pNext = pR->pNext;
        pR->pNext = pRule;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * cssSelectorPropertySetPair --
 *
 *     A rule has just been parsed with selector pSelector and properties
 *     pPropertySet. This function creates a CssRule object to link the two
 *     together and inserts the new rule into the CssStyleSheet structure.
 *
 *     The caller should not free resources associated with pSelector or
 *     pPropertySet after this function returns, they are now linked into
 *     the stylesheet object.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define FREE_SELECTOR    0x00000001
#define FREE_PROPERTYSET 0x00000002
#define FREE_BOTH        0x00000003
static void 
cssSelectorPropertySetPair(pParse, pSelector, pPropertySet, freeWhat)
    CssParse *pParse;
    CssSelector *pSelector;
    CssPropertySet *pPropertySet;
    unsigned int freeWhat;
{
    int spec = 0;
    CssSelector *pS = 0;
    CssStyleSheet *pStyle = pParse->pStyle;
    CssRule *pRule = HtmlNew(CssRule);

    assert(pPropertySet && pPropertySet->n > 0);

    if (freeWhat & FREE_PROPERTYSET) {
        pRule->freePropertySets = 1;
    }
    if (freeWhat & FREE_SELECTOR) {
        pRule->freeSelector = 1;
    }

    /* Calculate the specificity of the rules. We use the following
     * formala:
     *
     *     Specificity = (number of id selectors)        * 10000 +
     *                   (number of attribute selectors) * 100   +
     *                   (number of pseudo classes)      * 100   +
     *                   (number of type selectors)
     *
     * Todo: There are (at least) two bugs here:
     *     1. A rule with 100 type selectors has greater specificity than a
     *        rule with a single attribute selector. This probably isn't a
     *        problem.
     *     2. A selector of the form '[id="hello"]' has the same
     *        specificity as the selector '.hello'. This is pretty obscure,
     *        but could come up.
     * 
     * See section 6.4, "The cascade", of CSS2 documentation for details on
     * selector specificity.
     */
    for (pS=pSelector; pS; pS = pS->pNext) {
         switch (pS->eSelector) {
             case CSS_SELECTOR_TYPE:
                 spec += 1;
                 break;

             case CSS_SELECTOR_ID:
                 spec += 10000;
                 break;

             case CSS_SELECTOR_ATTR:
             case CSS_SELECTOR_ATTRVALUE:
             case CSS_SELECTOR_ATTRLISTVALUE:
             case CSS_SELECTOR_ATTRHYPHEN:
                 spec += 100;
                 break;

             case CSS_SELECTOR_CLASS:
             case CSS_PSEUDOCLASS_LANG:
             case CSS_PSEUDOCLASS_FIRSTCHILD:
             case CSS_PSEUDOCLASS_LINK:
             case CSS_PSEUDOCLASS_VISITED:
             case CSS_PSEUDOCLASS_ACTIVE:
             case CSS_PSEUDOCLASS_HOVER:
             case CSS_PSEUDOCLASS_FOCUS:
                 spec += 100;
                 break;
         }
    }
    pRule->specificity = spec;
    assert(
        pPropertySet == pParse->pPropertySet || 
        pPropertySet == pParse->pImportant
    );
    if (pParse->pPropertySet == pPropertySet) {
        pRule->pPriority = pParse->pPriority1;
    } else {
        pRule->pPriority = pParse->pPriority2;
    }
    pRule->iRule = pParse->iNextRule++;

    /* Insert the rule into it's list. */
    if (pParse->pStyleId) {
        pS = pSelector;

        while (pS->pNext && (
                pS->eSelector == CSS_SELECTOR_ATTR ||
                pS->eSelector == CSS_SELECTOR_ATTRVALUE ||
                pS->eSelector == CSS_SELECTOR_ATTRLISTVALUE ||
                pS->eSelector == CSS_SELECTOR_ATTRHYPHEN ||
                pS->eSelector == CSS_PSEUDOCLASS_ACTIVE ||
                pS->eSelector == CSS_PSEUDOCLASS_HOVER ||
                pS->eSelector == CSS_PSEUDOCLASS_FOCUS ||
                pS->eSelector == CSS_PSEUDOCLASS_LINK ||
                pS->eSelector == CSS_PSEUDOCLASS_VISITED
            )
        ) {
            pS = pS->pNext;
        }

        switch (pS->eSelector) {

            case CSS_PSEUDOELEMENT_AFTER:
                insertRule(&pStyle->pAfterRules, pRule);
                break;

            case CSS_PSEUDOELEMENT_BEFORE:
                insertRule(&pStyle->pBeforeRules, pRule);
                break;
    
            case CSS_SELECTOR_ID:
            case CSS_SELECTOR_CLASS:
            case CSS_SELECTOR_TYPE: {
                int newentry;
                Tcl_HashTable *pTab;
                Tcl_HashEntry *p;
                CssRule *pList = 0;

                pTab = &pStyle->aByTag;
                switch (pS->eSelector) {
                    case CSS_SELECTOR_ID:    pTab = &pStyle->aById; break;
                    case CSS_SELECTOR_CLASS: pTab = &pStyle->aByClass; break;
                    case CSS_SELECTOR_TYPE:  pTab = &pStyle->aByTag; break;
                }

                p = Tcl_CreateHashEntry(pTab, pS->zValue, &newentry);
                if (!newentry) { 
                    pList = (CssRule *)Tcl_GetHashValue(p); 
                }
                insertRule(&pList, pRule);
                Tcl_SetHashValue(p, pList);
                break;
            }
    
            default:
                insertRule(&pStyle->pUniversalRules, pRule);
                break;
        }
    } else {
        insertRule(&pStyle->pUniversalRules, pRule);
    }

    pRule->pSelector = pSelector;
    pRule->pPropertySet = pPropertySet;
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssRule --
 *
 *     This is called when the parser has parsed an entire rule.
 *
 * Results:
 *     None.
 *
 * Side effects:
 * 
 *     If the parse was successful, then add the rule to the stylesheet.
 *     If unsuccessful, delete anything that was built up by calls to 
 *     HtmlCssDeclaration() or HtmlCssSelector().
 *
 *--------------------------------------------------------------------------
 */
void HtmlCssRule(pParse, success)
    CssParse *pParse;
    int success;
{
    CssSelector *pSelector = pParse->pSelector;
    CssPropertySet *pPropertySet = pParse->pPropertySet;
    CssPropertySet *pImportant = pParse->pImportant;
    CssSelector **apXtraSelector = pParse->apXtraSelector;
    int nXtra = pParse->nXtra;
    int i;

#if TRACE_PARSER_CALLS
    printf("HtmlCssRule(%p, %d)\n", pParse, success);
#endif

    if (pPropertySet && pPropertySet->n == 0) {
        propertySetFree(pPropertySet);
        pPropertySet = 0;
    }
    if (pImportant && pImportant->n == 0) {
        propertySetFree(pImportant);
        pImportant = 0;
    }

    if (
        success && 
        !pParse->isIgnore && 
        pSelector && 
        (pPropertySet || pImportant)
    ) {

        if (pPropertySet) {
            unsigned int flags = FREE_BOTH;
            cssSelectorPropertySetPair(pParse, pSelector, pPropertySet, flags);
            for (i = 0; i < nXtra; i++){
                unsigned int flags2 = FREE_SELECTOR;
                CssSelector *pS = apXtraSelector[i];
                cssSelectorPropertySetPair(pParse, pS, pPropertySet, flags2);
            }
        }

        if (pImportant) {
            unsigned int flags = (pPropertySet ? FREE_PROPERTYSET : FREE_BOTH);
            cssSelectorPropertySetPair(pParse, pSelector, pImportant, flags);
            for (i = 0; i < nXtra; i++){
                unsigned int flags2 = (pPropertySet ? 0 : FREE_SELECTOR);
                CssSelector *pS = apXtraSelector[i];
                cssSelectorPropertySetPair(pParse, pS, pImportant, flags2);
            }
        }

    } else {
        /* Some sort of a parse error has occured. We won't be including
         * this rule, so just free these structs so we don't leak memory.
         */ 
        selectorFree(pSelector);
        propertySetFree(pPropertySet);
        propertySetFree(pImportant);
        for (i = 0; i < nXtra; i++){
            selectorFree(apXtraSelector[i]);
        }
    }

    pParse->pSelector = 0;
    pParse->pPropertySet = 0;
    pParse->pImportant = 0;
    pParse->apXtraSelector = 0;
    pParse->nXtra = 0;

    if( apXtraSelector ){
        HtmlFree(apXtraSelector);
    }
}

/*--------------------------------------------------------------------------
 *
 * attrTest --
 *
 *     Test if an attribute value matches a string. The three modes of 
 *     comparing attribute values specified in CSS are supported.
 *
 * Results:
 *     Non-zero is returned if the match is true.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static int attrTest(eType, zString, zAttr)
    u8 eType;
    const char *zString;
    const char *zAttr;
{
    if (!zAttr) {
        return 0;
    }

    switch( eType ){
        /* True if the specified attribute exists */
        case CSS_SELECTOR_ATTR:
            return (zAttr?1:0);

        /* True if the specified attribute exists and the value matches
         * the string exactly.
         */
        case CSS_SELECTOR_ATTRVALUE:
            return ((zAttr && 0==stricmp(zAttr, zString))?1:0);

	/* Treat the attribute value (if it exists) as a space seperated list.
         * Return true if zString exists in the list.
         */
        case CSS_SELECTOR_ATTRLISTVALUE: {
            const char *pAttr = zAttr;
            int nAttr;
            int nString = strlen(zString);
            while ((pAttr=HtmlCssGetNextListItem(pAttr, strlen(pAttr), &nAttr))) {
                if (nString==nAttr && 0==strnicmp(pAttr, zString, nAttr)) {
                    return 1;
                }
                pAttr += nAttr;
            }
            return 0;
        }

        /* True if the attribute exists and matches zString up to the
         * first '-' character in the attribute value.
         */
        case CSS_SELECTOR_ATTRHYPHEN: {
            char *pHyphen = strchr(zAttr, '-');
            if( pHyphen && 0==strnicmp(zAttr, zString, pHyphen-zAttr) ){
                return 1;
            }
            return 0;
        }
    }

    assert(!"Impossible");
    return 0;
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssSelectorTest --
 *
 *     Test if a selector matches a document node.
 *
 * Results:
 *     Non-zero is returned if the Selector does match the node.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
#define N_TYPE(x)        HtmlNodeTagName(x)
#define N_ATTR(x,y)      HtmlNodeAttr(x,y)
#define N_PARENT(x)      HtmlNodeParent(x)
#define N_NUMCHILDREN(x) HtmlNodeNumChildren(x)
#define N_CHILD(x,y)     HtmlNodeChild(x,y)
int 
HtmlCssSelectorTest(pSelector, pNode, dynamic_true)
    CssSelector *pSelector;
    HtmlNode *pNode;
    int dynamic_true;
{
    CssSelector *p = pSelector;
    HtmlNode *x = pNode;

    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
    assert(pElem);

    while( p && x ){
        pElem = HtmlNodeAsElement(x);

        switch( p->eSelector ){
            case CSS_SELECTOR_UNIVERSAL:
                break;

            case CSS_SELECTOR_TYPE:
                assert(x->zTag || HtmlNodeIsText(x));
                if( HtmlNodeIsText(x) || strcmp(x->zTag, p->zValue) ) return 0;
                break;

            case CSS_SELECTOR_CLASS: {
                const char *zClass = p->zValue;
                const char *zAttr = N_ATTR(x, "class");
                if( !attrTest(CSS_SELECTOR_ATTRLISTVALUE, zClass, zAttr) ){
                    return 0;
                }
                break;
            }

            case CSS_SELECTOR_ID: {
                const char *zId = p->zValue;
                const char *zAttr = N_ATTR(x, "id");
                if( !attrTest(CSS_SELECTOR_ATTRVALUE, zId, zAttr) ){
                    return 0;
                }
                break;
            }

            case CSS_SELECTOR_ATTR:
            case CSS_SELECTOR_ATTRVALUE:
            case CSS_SELECTOR_ATTRLISTVALUE:
            case CSS_SELECTOR_ATTRHYPHEN:
                if( !attrTest(p->eSelector, p->zValue, N_ATTR(x,p->zAttr)) ){
                    return 0;
                }
                break;

            case CSS_SELECTORCHAIN_DESCENDANT: {
                HtmlNode *pParent = N_PARENT(x);
                CssSelector *pNext = p->pNext;
                while (pParent) {
                    if (HtmlCssSelectorTest(pNext, pParent, dynamic_true)) {
                        return 1;
                    }
                    pParent = N_PARENT(pParent);
                }
                return 0;
            }
            case CSS_SELECTORCHAIN_CHILD:
                x = N_PARENT(x);
                break;
            case CSS_SELECTORCHAIN_ADJACENT: {
                HtmlNode *pParent = N_PARENT(x);
                int i;

                if (
                    !pParent || 
                    ((HtmlElementNode *)pParent)->pBefore == x ||
                    ((HtmlElementNode *)pParent)->pAfter == x 
                ) {
                    return 0;
                }

                /* Search for the nearest left-hand sibling that is not
                 * white-space. If no such sibling exists, set x==0 (this
                 * will cause the selector-match to fail). If the sibling
                 * does exist, set x to point at it.
                 */
                for (i = 0; N_CHILD(pParent, i) != x; i++);
                i--;
                do {
                    x = N_CHILD(pParent, i);
                    i--;
                } while (i >= 0 && HtmlNodeIsWhitespace(x));
                if (i < 0) return 0;

                break;
            }

            case CSS_PSEUDOCLASS_FIRSTCHILD: {
                /* :first-child selector matches if x is the left-most child
                 * of it's parent, not including white-space nodes. */
                HtmlNode *pParent = N_PARENT(x);
                int i;
                if (!pParent) return 0;
                for (i = 0; i < N_NUMCHILDREN(pParent); i++) {
                    HtmlNode *pChild = N_CHILD(pParent, i);
                    if (pChild == x) break;
                    if (!HtmlNodeIsWhitespace(pChild)) return 0;
                }
                assert(i < N_NUMCHILDREN(pParent));
                break;
            }
            case CSS_PSEUDOCLASS_LASTCHILD: {
                /* :last-child selector matches if x is the right-most child
                 * of it's parent, not including white-space nodes. */
                HtmlNode *pParent = N_PARENT(x);
                int i;
                if (!pParent) return 0;
                for (i = N_NUMCHILDREN(pParent) - 1; i >= 0; i--) {
                    HtmlNode *pChild = N_CHILD(pParent, i);
                    if (pChild == x) break;
                    if (!HtmlNodeIsWhitespace(pChild)) return 0;
                }
                assert(i >= 0);
                break;
            }
                
            case CSS_PSEUDOCLASS_LANG:
                return 0;


            case CSS_PSEUDOELEMENT_FIRSTLINE:
            case CSS_PSEUDOELEMENT_FIRSTLETTER:
                return 0;

            case CSS_PSEUDOELEMENT_BEFORE:
            case CSS_PSEUDOELEMENT_AFTER:
                break;

            case CSS_PSEUDOCLASS_ACTIVE:
                if (dynamic_true || (pElem->flags & HTML_DYNAMIC_ACTIVE)) break;
                return 0;
            case CSS_PSEUDOCLASS_HOVER:
                if (dynamic_true || (pElem->flags & HTML_DYNAMIC_HOVER)) break;
                return 0;
            case CSS_PSEUDOCLASS_FOCUS:
                if (dynamic_true || (pElem->flags & HTML_DYNAMIC_FOCUS)) break;
                return 0;
            case CSS_PSEUDOCLASS_LINK:
                if (pElem->flags & HTML_DYNAMIC_LINK) break;
                return 0;
            case CSS_PSEUDOCLASS_VISITED:
                if (pElem->flags & HTML_DYNAMIC_VISITED) break;
                return 0;

            case CSS_SELECTOR_NEVERMATCH:
                return 0;

            default:
                assert(!"Impossible");
        }
        p = p->pNext;
    }

    return (x && !p)?1:0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssInlineFree --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCssInlineFree(pPropertySet)
    CssPropertySet *pPropertySet;
{
    propertySetFree(pPropertySet);
}

/*
 *---------------------------------------------------------------------------
 *
 * propertySetToPropertyValues --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static void 
propertySetToPropertyValues(p, aPropDone, pSet)
    HtmlComputedValuesCreator *p;
    int *aPropDone;
    CssPropertySet *pSet;
{
    int i;
    assert(pSet);

    for (i = pSet->n - 1; i >= 0; i--) {
        int eProp = pSet->a[i].eProp;
	/* eProp may be greater than MAX_PROPERTY if it stores a composite
	 * property that Tkhtml doesn't handle. In this case just ignore it.
         */
	if (eProp <= CSS_PROPERTY_MAX_PROPERTY && 0 == aPropDone[eProp]) {
            if (0 == HtmlComputedValuesSet(p, eProp, pSet->a[i].pProp)) {
                aPropDone[eProp] = 1;
            }
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ruleToPropertyValues --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ruleToPropertyValues(p, aPropDone, pRule)
    HtmlComputedValuesCreator *p;
    int *aPropDone;
    CssRule *pRule;
{
    propertySetToPropertyValues(p, aPropDone, pRule->pPropertySet);
}

/*
 *---------------------------------------------------------------------------
 *
 * ruleToPropertyValues --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
overrideToPropertyValues(p, aPropDone, pOverride)
    HtmlComputedValuesCreator *p;
    int *aPropDone;
    Tcl_Obj *pOverride;
{
    Tcl_Obj **apObj = 0;
    int nObj = 0;
    int ii;

    if (!pOverride) return;
    Tcl_ListObjGetElements(0, pOverride, &nObj, &apObj);

    for (ii = 0; ii < (nObj - 1); ii += 2) { 
        int eProp;
        const char *zProp;
        int nProp;

        zProp = Tcl_GetStringFromObj(apObj[ii], &nProp);
        eProp = HtmlCssPropertyLookup(nProp, zProp);

	if (eProp <= CSS_PROPERTY_MAX_PROPERTY && 0 == aPropDone[eProp]) {
            const char *zVal = Tcl_GetString(apObj[ii + 1]);
            CssProperty *pProp = HtmlCssStringToProperty(zVal, -1);
            if (0 == HtmlComputedValuesSet(p, eProp, pProp)) {
                aPropDone[eProp] = 1;
            }
            HtmlComputedValuesFreeProperty(p, pProp);
        }
    }
}

/*--------------------------------------------------------------------------
 *
 * applyRule --
 *
 *     Test the selector of pRule against node pNode. If there is a match,
 *     add the rules properties to the computed values being accumulated in
 *     pCreator.
 *
 * Results:
 *
 *     The value returned is true if the selector matched, or false otherwise.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
static int 
applyRule(pTree, pNode, pRule, aPropDone, pzIfMatch, pCreator)
    HtmlTree *pTree;
    HtmlNode *pNode;
    CssRule *pRule;
    int *aPropDone;
    char **pzIfMatch;
    HtmlComputedValuesCreator *pCreator;
{
    /* Test if the selector matches the node. Variable isMatch is set to
     * true if the selector matches, or false otherwise. 
     */
    CssSelector *pSelector = pRule->pSelector;
    int isMatch = HtmlCssSelectorTest(pSelector, pNode, 0);

    /* There is a match. Log some output for debugging. */
    LOG {
        CssPriority *pPriority = pRule->pPriority;
        Tcl_Obj *pS = Tcl_NewObj();
        Tcl_IncrRefCount(pS);
        HtmlCssSelectorToString(pSelector, pS);
        HtmlLog(pTree, "STYLEENGINE", "%s %s (%s)"
            " from \"%s%s\"",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            (isMatch ? "matches" : "nomatch"),
            Tcl_GetString(pS),
            pPriority->origin == CSS_ORIGIN_AUTHOR ? "author" :
            pPriority->origin == CSS_ORIGIN_AGENT ? "agent" : "user",
            Tcl_GetString(pPriority->pIdTail)
        );
        Tcl_DecrRefCount(pS);
    }
    if (isMatch) {

        if (pzIfMatch) {
            HtmlComputedValuesInit(pTree, pNode, pNode, pCreator);
            pCreator->pzContent = pzIfMatch;
        }
  
        /* Copy the properties from the rule into the computed values set. */
        ruleToPropertyValues(pCreator, aPropDone, pRule);
    }

    assert(isMatch == 0 || isMatch == 1);
    return isMatch;
}

/*--------------------------------------------------------------------------
 *
 * nextRule --
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
static CssRule *
nextRule(apRule, n)
    CssRule **apRule;
    int n;
{
    CssRule **ppRule = 0;
    CssRule *pRet = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (apRule[i] && (ppRule == 0 || ruleCompare(apRule[i], *ppRule) > 0)) {
            ppRule = &apRule[i];
        }
    }

    if (ppRule) {
        pRet = *ppRule;
        *ppRule = (*ppRule)->pNext;
    }

    return pRet;
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssStyleSheetApply --
 *
 *     It is assumed that pNode->pStyle contains the stylesheet parsed from
 *     any HTML style attribute attached to the node.  Once this function
 *     returns, the HtmlNode.pPropertyValues variable points to the
 *     structure containing the computed values applied to the node.
 *
 *     NOTE: There are two hard-coded limits in this function:
 *         1) No element may be a member of more than 126 classes.  
 *         2) No class name may be longer than 128 bytes (includes null term).
 *
 * Results:
 *
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void 
HtmlCssStyleSheetApply(pTree, pNode)
    HtmlTree *pTree; 
    HtmlNode *pNode; 
{

    /* The two hard coded constants mentioned above */
    #define MAX_CLASSES    126
    #define MAX_CLASS_NAME 128

    CssStyleSheet *pStyle = pTree->pStyle;    /* Stylesheet config */
    CssRule *pRule;                           /* Iterator variable */

    /* Boolean: set after considering the inline-style information */
    int isStyleDone = 0;

    HtmlComputedValuesCreator sCreator;

    /* The array aPropDone is large enough to contain an entry for each
     * property recognized by the CSS parser (approx 110, includes many that
     * Tkhtml does not use). After a property value is successfully written
     * into sCreator, the matching aPropDone entry is set to true.
     */
    int aPropDone[CSS_PROPERTY_MAX_PROPERTY + 1];

    Tcl_HashEntry *pEntry;
    char const *zClassAttr;            /* Value of node "class" attribute */
    char const *zIdAttr;               /* Value of node "id" attribute */

    CssRule *apRule[MAX_CLASSES + 2];  /* Array of applicable rules lists. */
    int npRule;

    int nSelectorMatch = 0;
    int nSelectorTest = 0;

    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
    assert(pElem);

    /* The universal rules list applies to all nodes */
    apRule[0] = pStyle->pUniversalRules;
    npRule = 1;

    /* Find the applicable "by-tag" rules list, if any. */
    pEntry = Tcl_FindHashEntry(&pStyle->aByTag, pNode->zTag);
    if (pEntry) {
        apRule[npRule++] = Tcl_GetHashValue(pEntry);
    }

    /* Find a rules list for the element id, if any */
    zIdAttr = HtmlNodeAttr(pNode, "id");
    if (zIdAttr) {
        pEntry = Tcl_FindHashEntry(&pStyle->aById, zIdAttr);
        if (pEntry) {
            apRule[npRule++] = (CssRule *)Tcl_GetHashValue(pEntry);
        }
    }

    /* Find a rules list for each class the element belongs to */
    zClassAttr = HtmlNodeAttr(pNode, "class");
    if (zClassAttr) {
        int nClass;
        char const *zClass = zClassAttr;
        char zTerm[MAX_CLASS_NAME];

        while (
            npRule < (MAX_CLASSES + 2) &&
            (zClass = HtmlCssGetNextListItem(zClass, strlen(zClass), &nClass))
        ) {
            strncpy(zTerm, zClass, MIN(MAX_CLASS_NAME, nClass));
            zTerm[MIN(MAX_CLASS_NAME - 1, nClass)] = '\0';
            zClass += nClass;

            pEntry = Tcl_FindHashEntry(&pStyle->aByClass, zTerm);
            if (pEntry) {
                apRule[npRule++] = (CssRule *)Tcl_GetHashValue(pEntry);
            }
        }
    }
    

    /* Initialise aPropDone and sCreator */
    HtmlComputedValuesInit(pTree, pNode, 0, &sCreator);
    memset(aPropDone, 0, sizeof(aPropDone));
    assert(sizeof(aPropDone) == sizeof(int) * (CSS_PROPERTY_MAX_PROPERTY+1));

    /* Before considering the stylesheet configure or any style attribute,
     * parse the properties from the override list in HtmlNode.pOverride.
     * These properties were set directly by the script and have a higher
     * priority than anything else.
     */
    overrideToPropertyValues(&sCreator, aPropDone, pElem->pOverride);

    /* Loop through the list of CSS rules in the stylesheet. Rules that occur
     * earlier in the list have a higher priority than those that occur later.
     */
    for (
        pRule = nextRule(apRule, npRule); 
        pRule; 
        pRule = nextRule(apRule, npRule)
    ) {
        CssPriority *pPriority = pRule->pPriority;
        CssSelector *pSelector = pRule->pSelector;

        nSelectorTest++;

        /* The contents of the "style" attribute, if one exists, are handled
         * after the important rules but before anything else. This is because:
         * 
	 *     (a) CSS 2.1, in section 6.4.3 says that a style attribute has
	 *         the maximum possible specificity, and
	 *     (b) Tkhtml assumes the style attribute resides on the author
	 *         stylesheet, with no !important flag - hence, according to
	 *         section 6.4.1 it is handled just after the !important stuff.
         */
        if (!isStyleDone && !pPriority->important) {
            isStyleDone = 1;
            if (pElem->pStyle) {
                propertySetToPropertyValues(&sCreator,aPropDone,pElem->pStyle);
            }
        }

        /* If the selector is a match for our node, apply the rule properties */
        nSelectorMatch += 
                applyRule(pTree, pNode, pRule, aPropDone, 0, &sCreator);

        if (
            pSelector->isDynamic &&
            HtmlCssSelectorTest(pSelector, pNode, 1)
        ) {
            HtmlCssAddDynamic(pElem, pSelector, 0);
        }
    }

    if (!isStyleDone && pElem->pStyle) {
        propertySetToPropertyValues(&sCreator, aPropDone, pElem->pStyle);
    }

    LOG {
       HtmlLog(pTree, "STYLEENGINE", "%s matched %d/%d selectors",
           Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
           nSelectorMatch, nSelectorTest
       );
    }

    /* Call HtmlComputedValuesFinish() to finish creating the
     * HtmlComputedValues structure.
     */
    pElem->pPropertyValues = HtmlComputedValuesFinish(&sCreator);
}

/*--------------------------------------------------------------------------
 *
 * generateContentText --
 *
 *     Argument zContent points to a nul-terminated string containing
 *     a value assigned to the 'content' property. This function allocates 
 *     and returns an HtmlTextNode structure populated with text
 *     based on the 'content' property.
 *
 * Results:
 *
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
static HtmlTextNode *
generateContentText(pTree, zContent)
    HtmlTree *pTree;
    const char *zContent;
{
    HtmlTextNode *pTextNode = HtmlTextNew(strlen(zContent), zContent, 0, 0);
    return pTextNode;
}

/*--------------------------------------------------------------------------
 *
 * generatedContent --
 *
 * Results:
 *
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
static void 
generatedContent(pTree, pNode, pCssRule, ppNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
    CssRule *pCssRule;        /* List of rules including :after or :before */
    HtmlNode **ppNode;
{
    CssRule *pRule;                                 /* Iterator variable */
    int have = 0;

    int aPropDone[CSS_PROPERTY_MAX_PROPERTY + 1];
    HtmlComputedValuesCreator sCreator;

    HtmlComputedValues *pValues = 0;
    char *zContent = 0;

    memset(aPropDone, 0, sizeof(aPropDone));

    sCreator.pzContent = &zContent;
    for (pRule = pCssRule; pRule; pRule = pRule->pNext) {
        char **pz = (have ? 0 : (&zContent));
        int isMatch = applyRule(pTree, pNode, pRule, aPropDone, pz, &sCreator);
        if (isMatch) have = 1;
    }
    if (have) {
        pValues = HtmlComputedValuesFinish(&sCreator);
    } else {
        assert(zContent == 0);
        return;
    }

    *ppNode = (HtmlNode *)HtmlNew(HtmlElementNode);
    ((HtmlElementNode *)(*ppNode))->pPropertyValues = pValues;

    if (zContent) {
        /* If a value was specified for the 'content' property, create
         * a text node also.
         */
        HtmlTextNode *pTextNode = generateContentText(pTree, zContent);
        int idx = HtmlNodeAddTextChild(*ppNode, pTextNode);
        HtmlNodeChild(*ppNode, idx)->iNode = HTML_NODE_GENERATED;
        HtmlFree(zContent);
    }
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssStyleSheetGenerated --
 *
 *     Retrieve the value of a specified property from a CssProperties
 *     object, or NULL if the property is not defined.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void HtmlCssStyleGenerateContent(pTree, pElem, isBefore)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
    int isBefore;
{
    CssStyleSheet *pStyle = pTree->pStyle;    /* Stylesheet config */
    HtmlNode *pNode = (HtmlNode *)pElem;
    if (isBefore) {
        generatedContent(pTree, pNode, pStyle->pBeforeRules, &pElem->pBefore);
    } else {
        generatedContent(pTree, pNode, pStyle->pAfterRules, &pElem->pAfter);
    }
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssPropertiesGet --
 *
 *     Retrieve the value of a specified property from a CssProperties
 *     object, or NULL if the property is not defined.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
CssProperty *
HtmlCssPropertiesGet(pProperties, prop, pSheetnum, pSpec)
    CssProperties * pProperties; 
    int prop;
    int *pSheetnum;
    int *pSpec;
{
    CssProperty *zRet = 0;
    if (pProperties) {
        int i;
        for (i=0; i<pProperties->nRule && !zRet; i++){
            CssPropertySet *pPropertySet = pProperties->apRule[i]->pPropertySet;
            zRet = propertySetGet(pPropertySet, prop);
            if (zRet) {
                if (pSheetnum)  {
                    *pSheetnum = pProperties->apRule[i]->pPriority->origin;
                }
                if (pSpec) {
                    *pSpec = pProperties->apRule[i]->specificity;
                }
            }
        }
    }
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssSelectorComma --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlCssSelectorComma(pParse)
    CssParse *pParse;
{
    int n = (pParse->nXtra + 1) * sizeof(CssSelector *);

    /* Do nothing if the isIgnore flag is set */
    if (pParse->isIgnore) return;

    pParse->apXtraSelector = (CssSelector **)HtmlRealloc(
           "CssParse.apXtraSelector", (char *)pParse->apXtraSelector, n
    );
    pParse->apXtraSelector[pParse->nXtra] = pParse->pSelector;
    pParse->pSelector = 0;
    pParse->nXtra++;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssImport --
 *
 *     The parser calls this function when an @import directive is encountered.
 *     The pToken argument contains the specified URL.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May invoke the -importcmd script.
 *
 *---------------------------------------------------------------------------
 */
void HtmlCssImport(pParse, pToken)
    CssParse *pParse;
    CssToken *pToken;
{
    Tcl_Obj *pEval = pParse->pImportCmd;

    /* Do nothing if the isIgnore or isBody flags are set */
    if (pParse->isBody) return;

    if (pEval) {
        Tcl_Interp *interp = pParse->interp;
        CssProperty *p = tokenToProperty(pParse, pToken);
        CONST char *zUrl = p->v.zVal;

        switch (p->eType) {
            case CSS_TYPE_URL:
                break;
            case CSS_TYPE_RAW:
            case CSS_TYPE_STRING:
                if (pParse && pParse->pUrlCmd) {
                    doUrlCmd(pParse, zUrl, strlen(zUrl));
                    zUrl = Tcl_GetStringResult(pParse->interp);
                }
                break;
            default:
                return;
        }

        pEval = Tcl_DuplicateObj(pEval);
        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewStringObj(zUrl, -1));
        Tcl_EvalObjEx(interp, pEval, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        Tcl_DecrRefCount(pEval);
        HtmlFree(p);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssSelectorToString --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
void
HtmlCssSelectorToString(pSelector, pObj)
    CssSelector *pSelector;
    Tcl_Obj *pObj;
{
    char *z = 0;
    if (!pSelector) return;

    if (pSelector->pNext) {
        HtmlCssSelectorToString(pSelector->pNext, pObj);
    }
 
    switch (pSelector->eSelector) {
        case CSS_SELECTORCHAIN_DESCENDANT:         z = " ";       break;
        case CSS_SELECTORCHAIN_CHILD:              z = " > ";     break;
        case CSS_SELECTORCHAIN_ADJACENT:           z = " + ";     break;
        case CSS_SELECTOR_UNIVERSAL:               z = "*";       break;
        case CSS_PSEUDOCLASS_LANG:                 z = ":lang";         break;
        case CSS_PSEUDOCLASS_FIRSTCHILD:           z = ":first-child";  break;
        case CSS_PSEUDOCLASS_LASTCHILD:            z = ":last-child";   break;
        case CSS_PSEUDOCLASS_LINK:                 z = ":link";         break;
        case CSS_PSEUDOCLASS_VISITED:              z = ":visited";      break;
        case CSS_PSEUDOCLASS_ACTIVE:               z = ":active";       break;
        case CSS_PSEUDOCLASS_HOVER:                z = ":hover";        break;
        case CSS_PSEUDOCLASS_FOCUS:                z = ":focus";        break;
        case CSS_PSEUDOELEMENT_FIRSTLINE:          z = ":first-line";   break;
        case CSS_PSEUDOELEMENT_FIRSTLETTER:        z = ":first-letter"; break;
        case CSS_PSEUDOELEMENT_BEFORE:             z = ":before";       break;
        case CSS_PSEUDOELEMENT_AFTER:              z = ":after";        break;

        case CSS_SELECTOR_TYPE:
            z = pSelector->zValue;
            break;

        case CSS_SELECTOR_CLASS: 
            Tcl_AppendStringsToObj(pObj, ".", pSelector->zValue, NULL);
            break;

        case CSS_SELECTOR_ID: 
            Tcl_AppendStringsToObj(pObj, "#", pSelector->zValue, NULL);
            break;

        case CSS_SELECTOR_ATTR: 
            Tcl_AppendStringsToObj(pObj, "[", pSelector->zAttr, "]", NULL);
            break;
           
        case CSS_SELECTOR_ATTRVALUE: 
            Tcl_AppendStringsToObj(pObj, 
                "[", pSelector->zAttr, "=\"", pSelector->zValue, "\"]", NULL);
            break;

        case CSS_SELECTOR_ATTRLISTVALUE: 
            Tcl_AppendStringsToObj(pObj, 
                "[", pSelector->zAttr, "~=\"", pSelector->zValue, "\"]", NULL);
            break;
        case CSS_SELECTOR_ATTRHYPHEN: 
            Tcl_AppendStringsToObj(pObj, 
                "[", pSelector->zAttr, "|=\"", pSelector->zValue, "\"]", NULL);
            break;

        case CSS_SELECTOR_NEVERMATCH: 
            Tcl_AppendStringsToObj(pObj, "NEVERMATCH", NULL);
            break;

        default:
            assert(!"Unknown CSS_SELECTOR_XXX value in HtmlSelectorToString()");
    }

    if (z) Tcl_AppendToObj(pObj, z, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * rulelistReport --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static void
rulelistReport(pRule, pObj, pN)
    CssRule *pRule;
    Tcl_Obj *pObj;
    int *pN;
{
    CssRule *p;
    for (p = pRule; p; p = p->pNext) {
        int i;

        (*pN)++;

        if (pRule->pSelector->isDynamic) {
            Tcl_AppendStringsToObj(pObj, 
                "<tr><td style=\"background:lightgrey\">", NULL
            );
        } else {
            Tcl_AppendStringsToObj(pObj, "<tr><td>", NULL);
        }

        HtmlCssSelectorToString(p->pSelector, pObj);
        Tcl_AppendStringsToObj(pObj, "</td><td><ul>", NULL);

        for (i = 0; i < p->pPropertySet->n; i++) {
            CssProperty *pProp = p->pPropertySet->a[i].pProp;
            if (pProp) {
                char *zFree = 0;
                int eProp = p->pPropertySet->a[i].eProp;
                Tcl_AppendStringsToObj(pObj, 
                    "<li>", HtmlCssPropertyToString(eProp), ": ", 
                    HtmlPropertyToString(pProp, &zFree), NULL
                );
                HtmlFree(zFree);
            }
        }

        Tcl_AppendStringsToObj(pObj, "</ul></td></tr>", NULL);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssStyleReport --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
int
HtmlCssStyleReport(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    CssStyleSheet *pStyle = pTree->pStyle;

    int nUniversal = 0;
    int nByTag = 0;
    int nByClass = 0;
    int nById = 0;
    int nAfter = 0;
    int nBefore = 0;

    CssRule *pRule;
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;

    Tcl_Obj *pAfter;
    Tcl_Obj *pBefore;
    Tcl_Obj *pUniversal;

    Tcl_Obj *pByTag;
    Tcl_Obj *pByClass;
    Tcl_Obj *pById;

    Tcl_Obj *pReport;

    pUniversal = Tcl_NewObj();
    Tcl_IncrRefCount(pUniversal);
    Tcl_AppendStringsToObj(pUniversal, 
        "<h1>Universal Rules</h1>",
        "<table border=1>", NULL
    );
    rulelistReport(pStyle->pUniversalRules, pUniversal, &nUniversal);
    Tcl_AppendStringsToObj(pUniversal, "</table>", NULL);

    pAfter = Tcl_NewObj();
    Tcl_IncrRefCount(pAfter);
    Tcl_AppendStringsToObj(pAfter, 
        "<h1>After Rules</h1>",
        "<table border=1>", NULL
    );
    rulelistReport(pStyle->pAfterRules, pAfter, &nAfter);
    Tcl_AppendStringsToObj(pAfter, "</table>", NULL);

    pBefore = Tcl_NewObj();
    Tcl_IncrRefCount(pBefore);
    Tcl_AppendStringsToObj(pBefore, 
        "<h1>Before Rules</h1>",
        "<table border=1>", NULL
    );
    rulelistReport(pStyle->pBeforeRules, pBefore, &nBefore);
    Tcl_AppendStringsToObj(pBefore, "</table>", NULL);

    pByTag = Tcl_NewObj();
    Tcl_IncrRefCount(pByTag);
    Tcl_AppendStringsToObj(pByTag, 
        "<h1>By Tag Rules</h1>",
        "<table border=1>", NULL
    );
    for (
        pEntry = Tcl_FirstHashEntry(&pStyle->aByTag, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        pRule = (CssRule *)Tcl_GetHashValue(pEntry);
        rulelistReport(pRule, pByTag, &nByTag);
    }
    Tcl_AppendStringsToObj(pByTag, "</table>", NULL);

    pByClass = Tcl_NewObj();
    Tcl_IncrRefCount(pByClass);
    Tcl_AppendStringsToObj(pByClass, 
        "<h1>By Class Rules</h1>",
        "<table border=1>", NULL
    );
    for (
        pEntry = Tcl_FirstHashEntry(&pStyle->aByClass, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        pRule = (CssRule *)Tcl_GetHashValue(pEntry);
        rulelistReport(pRule, pByClass, &nByClass);
    }
    Tcl_AppendStringsToObj(pByClass, "</table>", NULL);

    pById = Tcl_NewObj();
    Tcl_IncrRefCount(pById);
    Tcl_AppendStringsToObj(pById, 
        "<h1>By Id Rules</h1>",
        "<table border=1>", NULL
    );
    for (
        pEntry = Tcl_FirstHashEntry(&pStyle->aById, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        pRule = (CssRule *)Tcl_GetHashValue(pEntry);
        rulelistReport(pRule, pById, &nById);
    }
    Tcl_AppendStringsToObj(pById, "</table>", NULL);

    pReport = Tcl_NewObj();
    Tcl_IncrRefCount(pReport);

    Tcl_AppendStringsToObj(pReport, 
        "<div><ul>", 
        "<li>Universal rules list: ", NULL
    );
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nUniversal));

    Tcl_AppendStringsToObj(pReport, "<li>By tag rules lists: ", NULL);
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nByTag));

    Tcl_AppendStringsToObj(pReport, "<li>By class rules lists: ", NULL);
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nByClass));

    Tcl_AppendStringsToObj(pReport, "<li>By id rules lists: ", NULL);
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nById));

    Tcl_AppendStringsToObj(pReport, "<li>:before rules lists: ", NULL);
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nBefore));

    Tcl_AppendStringsToObj(pReport, "<li>:after rules lists: ", NULL);
    Tcl_AppendObjToObj(pReport, Tcl_NewIntObj(nAfter));
    Tcl_AppendStringsToObj(pReport, "</ul></div>", NULL);

    Tcl_AppendObjToObj(pReport, pUniversal);
    Tcl_AppendObjToObj(pReport, pByTag);
    Tcl_AppendObjToObj(pReport, pByClass);
    Tcl_AppendObjToObj(pReport, pById);
    Tcl_AppendObjToObj(pReport, pBefore);
    Tcl_AppendObjToObj(pReport, pAfter);

    Tcl_SetObjResult(interp, pReport);
    Tcl_DecrRefCount(pReport);
    Tcl_DecrRefCount(pUniversal);
    Tcl_DecrRefCount(pByTag);
    Tcl_DecrRefCount(pByClass);
    Tcl_DecrRefCount(pById);
      
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ruleQsortCompare --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
ruleQsortCompare(const void *pLeft, const void *pRight)
{
    CssRule *pL = *(CssRule **)pLeft;
    CssRule *pR = *(CssRule **)pRight;

    return ruleCompare(pL, pR);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssStyleConfigDump --
 *
 *         widget styleconfig
 *
 *     This function contains the implementation of the Tcl widget command
 *     "styleconfig". This is intended to be used by Tkhtml regression tests 
 *     to test that stylesheets are correctly parsed, and that rules are placed
 *     in correct priority order.
 *
 *         {SELECTOR PROPERTIES ORIGIN}
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
int
HtmlCssStyleConfigDump(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
#define MAX_RULES 8096
    HtmlTree *pTree = (HtmlTree *)clientData;
    CssStyleSheet *pStyle = pTree->pStyle;
    Tcl_HashTable *apTable[3];

    CssRule *pRule;
    CssRule *apRule[MAX_RULES];
    Tcl_Obj *pRet;
    int nRule = 0;
    int jj = 0;

    for (pRule = pStyle->pUniversalRules; pRule; pRule = pRule->pNext) {
        if (nRule < MAX_RULES) {
            apRule[nRule++] = pRule;
        }
    }

    apTable[0] = &pStyle->aByTag;
    apTable[1] = &pStyle->aById;
    apTable[2] = &pStyle->aByClass;
    for (jj = 0; jj < 3; jj++) {
        Tcl_HashEntry *pEntry;
        Tcl_HashSearch search;
        for (pEntry = Tcl_FirstHashEntry(apTable[jj], &search);
             pEntry;
             pEntry = Tcl_NextHashEntry(&search)
        ) {
            pRule = (CssRule *)Tcl_GetHashValue(pEntry);
            for ( ; pRule; pRule = pRule->pNext) {
                if (nRule < MAX_RULES) {
                    apRule[nRule++] = pRule;
                }
            }
        }
    }

    qsort(apRule, nRule, sizeof(CssRule *), ruleQsortCompare);

    pRet = Tcl_NewObj();
    for (jj = 0; jj < nRule; jj++) {
        CssPriority *pPri = apRule[jj]->pPriority;
        Tcl_Obj *pList = Tcl_NewObj();
        Tcl_Obj *p;
        char zBuf[256];
        int ii;
        int isRequireSemi = 0;
        pRule = apRule[jj];

        p = Tcl_NewObj();
        HtmlCssSelectorToString(pRule->pSelector, p);
        Tcl_ListObjAppendElement(0, pList, p);
        
        p = Tcl_NewObj();
        for (ii = 0; ii < pRule->pPropertySet->n; ii++) {
            CssProperty *pProp = pRule->pPropertySet->a[ii].pProp;
            if (pProp) {
                int eProp = pRule->pPropertySet->a[ii].eProp;
                char *zPropVal;
                char *zFree = 0;
                if (isRequireSemi) {
                    Tcl_AppendToObj(p, "; ", 2);
                }
                zPropVal = HtmlPropertyToString(pProp, &zFree);
                Tcl_AppendToObj(p, HtmlCssPropertyToString(eProp), -1);
                Tcl_AppendToObj(p, ":", 1);
                Tcl_AppendToObj(p, zPropVal, -1);
                isRequireSemi = 1;
                if (zFree) HtmlFree(zFree);
            }
        }
        Tcl_ListObjAppendElement(0, pList, p);

        snprintf(zBuf, 255, "%s%s%s", 
            (pPri->origin == CSS_ORIGIN_AUTHOR) ? "author" :
            (pPri->origin == CSS_ORIGIN_AGENT) ? "agent" :
            (pPri->origin == CSS_ORIGIN_USER) ? "user" : "N/A",
            Tcl_GetString(pPri->pIdTail),
            pPri->important ? " (!important)" : ""
        );
        zBuf[255] = '\0';
        Tcl_ListObjAppendElement(0, pList, Tcl_NewStringObj(zBuf, -1));

        Tcl_ListObjAppendElement(0, pRet, pList);
    }
 
    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssInlineQuery --
 *
 *     This function is called to process a query of a nodes inline style,
 *     using the following Tcl syntax:
 *
 *         $node property -inline ?PROPERTY-NAME?
 *
 *     The third argument to this function is the PROPERTY-NAME parameter
 *     if present, or NULL otherwise.
 *
 * Results:
 *     Standard Tcl result (TCL_OK or TCL_ERROR).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlCssInlineQuery(interp, pPropertySet, pArg)
    Tcl_Interp *interp;
    CssPropertySet *pPropertySet;
    Tcl_Obj *pArg;
{
    if (pPropertySet) {
        int ii;
      
        if (pArg) {
            char *zArg;
            int nArg;
            int eProp;
    
            zArg = Tcl_GetStringFromObj(pArg, &nArg);
            eProp = HtmlCssPropertyLookup(nArg, zArg);
            if (eProp < 0) {
                Tcl_AppendResult(interp, "No such property: ", zArg, 0);
                return TCL_ERROR;
            }

            for (ii = 0; ii < pPropertySet->n; ii++) {
                if (pPropertySet->a[ii].eProp == eProp) {
                    char *zFree = 0;
                    char *zProp = HtmlPropertyToString(
                        pPropertySet->a[ii].pProp, &zFree
                    );
                    Tcl_SetResult(interp, zProp, TCL_VOLATILE);
                    HtmlFree(zFree);
                }
            }
        } else {
            Tcl_Obj *pRet = Tcl_NewObj();
            for (ii = 0; ii < pPropertySet->n; ii++) {
                char *zFree = 0;
                char *zProp = HtmlPropertyToString(
                    pPropertySet->a[ii].pProp, &zFree
                );
                Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(
                    HtmlCssPropertyToString(pPropertySet->a[ii].eProp), -1
                ));
                Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(zProp, -1));
                HtmlFree(zFree);
            }
            Tcl_SetObjResult(interp, pRet);
        }
    }
   
    return TCL_OK;
}


