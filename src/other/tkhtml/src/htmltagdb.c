
/*
 * htmltagdb.c ---
 *
 *     This file implements the interface used by other modules to the
 *     HtmlMarkupMap array. Right now this is partially here, and partially
 *     in htmlparse.c. But the idea is that it should all be here soon.
 *
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
static const char rcsid[] = "$Id: htmltagdb.c,v 1.11 2007/11/11 11:00:48 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>

/*
 * Public interface to code in this file:
 *
 *     HtmlMarkupName()
 *     HtmlMarkupFlags()
 *     HtmlMarkup()
 */

extern HtmlTokenMap HtmlMarkupMap[];


/* The hash table for HTML markup names.
**
** If an HTML markup name hashes to H, then apMap[H] will point to
** a linked list of sgMap structure, one of which will describe the
** the particular markup (if it exists.)
*/
static HtmlTokenMap *apMap[HTML_MARKUP_HASH_SIZE];

/* Hash a markup name
**
** HTML markup is case insensitive, so this function will give the
** same hash regardless of the case of the markup name.
**
** The value returned is an integer between 0 and HTML_MARKUP_HASH_SIZE-1,
** inclusive.
*/
static int
HtmlHash(htmlPtr, zName)
    void *htmlPtr;
    const char *zName;
{
    int h = 0;
    char c;
    while ((c = *zName) != 0) {
        if (isupper(c)) {
            c = tolower(c);
        }
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    return h % HTML_MARKUP_HASH_SIZE;
}

/*
** Convert a string to all lower-case letters.
*/
static void
ToLower(z)
    char *z;
{
    while (*z) {
        if (isupper(*z))
            *z = tolower(*z);
        z++;
    }
}

static int 
textContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Space || tag == Html_Text) {
        return TAG_OK;
    }
    return TAG_CLOSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkup --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlTokenMap *
HtmlMarkup(markup)
    int markup;
{
    if (markup == Html_Text || markup == Html_Space) {
        static HtmlTokenMap textmapentry = {
            "text",
            Html_Text,
            HTMLTAG_INLINE,
            textContent,
            0
        };
        return &textmapentry;
    } else if (markup > 0) {
        int i = markup-Html_A;
        assert(i<HTML_MARKUP_COUNT);
        return &HtmlMarkupMap[i];
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupFlags --
 *
 * Results:
 *     Return the 'flags' value associated with Html markup tag 'markup'.
 *     The flags value is a bitmask comprised of the HTMLTAG_xxx symbols
 *     defined in html.h.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Html_u8 
HtmlMarkupFlags(markup)
    int markup;
{
    int i = markup-Html_A;
    if (i>=0 && i<HTML_MARKUP_COUNT){
        return HtmlMarkupMap[i].flags;
    }

    /* Regular text behaves as an inline element. */
    if( markup==Html_Text || markup==Html_Space ){
        return HTMLTAG_INLINE;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupName --
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
HtmlMarkupName(markup)
    int markup;
{
    int i = markup-Html_A;
    if (i>=0 && i<HTML_MARKUP_COUNT){
        return HtmlMarkupMap[i].zName;
    }

    if( markup==Html_Text || markup==Html_Space ){
        return "";
    }

    return "unknown";
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlHashLookup --
 *
 *     Look up an HTML tag name in the hash-table.
 *
 * Results: 
 *     Return the corresponding HtmlTokenMap if the tag name is recognized,
 *     or NULL otherwise.
 *
 * Side effects:
 *     May initialise the hash table from the autogenerated array
 *     in htmltokens.c (generated from tokenlist.txt).
 *
 *---------------------------------------------------------------------------
 */
HtmlTokenMap * 
HtmlHashLookup(htmlPtr, zType)
    void *htmlPtr;
    const char *zType;          /* Null terminated tag name. eg. "br" */
{
    HtmlTokenMap *pMap;         /* For searching the markup name hash table */
    int h;                      /* The hash on zType */
    char buf[256];
    HtmlHashInit(htmlPtr, 0);

    h = HtmlHash(htmlPtr, zType);
    for (pMap = apMap[h]; pMap; pMap = pMap->pCollide) {
        if (stricmp(pMap->zName, zType) == 0) {
            return pMap;
        }
    }
    strncpy(buf, zType, 255);
    buf[255] = 0;

    return NULL;
}

/* Initialize the escape sequence hash table
*/
void
HtmlHashInit(htmlPtr, start)
    void *htmlPtr;
    int start;
{
    static int isInit = 0;

    int i;         /* For looping thru the list of markup names */
    int h;         /* The hash on a markup name */

    if (isInit) return;

    for (i = start; i < HTML_MARKUP_COUNT; i++) {
        h = HtmlHash(htmlPtr, HtmlMarkupMap[i].zName);
        HtmlMarkupMap[i].pCollide = apMap[h];
        apMap[h] = &HtmlMarkupMap[i];
    }
#ifdef TEST
    HtmlHashStats(htmlPtr);
#endif

    isInit = 1;
}


HtmlAttributes *
HtmlAttributesNew(argc, argv, arglen, doEscape)
    int argc;
    char const **argv;
    int *arglen;
    int doEscape;
{
    HtmlAttributes *pMarkup = 0;

    if (argc > 1) {
        int nByte;
        int j;
        char *zBuf;

        int nAttr = argc / 2;

        nByte = sizeof(HtmlAttributes);
        for (j = 0; j < argc; j++) {
            nByte += arglen[j] + 1;
        }
        nByte += sizeof(struct HtmlAttribute) * (argc - 1);

        pMarkup = (HtmlAttributes *)HtmlAlloc("HtmlAttributes", nByte);
        pMarkup->nAttr = nAttr;
        zBuf = (char *)(&pMarkup->a[nAttr]);

        for (j=0; j < nAttr; j++) {
            int idx = (j * 2);

            pMarkup->a[j].zName = zBuf;
            memcpy(zBuf, argv[idx], arglen[idx]);
            zBuf[arglen[idx]] = '\0';
            if (doEscape) {
                HtmlTranslateEscapes(zBuf);
                ToLower(zBuf);
            }
            zBuf += (arglen[idx] + 1);

            pMarkup->a[j].zValue = zBuf;
            memcpy(zBuf, argv[idx+1], arglen[idx+1]);
            zBuf[arglen[idx+1]] = '\0';
            if (doEscape) HtmlTranslateEscapes(zBuf);
            zBuf += (arglen[idx+1] + 1);
        }
    }

    return pMarkup;
}

/*
** Convert a markup name into a type integer
*/
int
HtmlNameToType(htmlPtr, zType)
    void *htmlPtr;
    char *zType;
{
    HtmlTokenMap *pMap = HtmlHashLookup(htmlPtr, zType);
    return pMap ? pMap->type : Html_Unknown;
}

/*
** Convert a type into a symbolic name
*/
const char *
HtmlTypeToName(htmlPtr, eTag)
    void *htmlPtr;
    int eTag;
{
    if (eTag >= Html_A && eTag < Html_TypeCount) {
        HtmlTokenMap *pMap = &HtmlMarkupMap[eTag - Html_A];
        return pMap->zName;
    }
    else {
        return "???";
    }
}

