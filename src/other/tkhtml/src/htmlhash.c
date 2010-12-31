/*
 * htmlhash.c --
 *
 *     This file contains code to extend the Tcl hash mechanism with some
 *     extra key-types used by the widget:
 *
 *         * case-insensitive strings
 *         * HtmlFontKey structures
 *         * HtmlComputedValues structures
 * 
 *     The code for case-insensitive strings was copied from the Tcl core code
 *     for regular string hashes and modified only slightly.
 *
 *----------------------------------------------------------------------------
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
 * COPYRIGHT:
 */
static const char rcsid[] = "$Id$";

#include <tcl.h>
/* #include <strings.h> */
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "html.h"
#include "htmlprop.h"

/*
 *---------------------------------------------------------------------------
 *
 * compareCaseInsensitiveKey --
 *
 *     The compare function for the case-insensitive string hash. Compare a
 *     new key to the key of an existing hash-entry.
 *
 * Results:
 *     True if the two keys are the same, false if not.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
compareCaseInsensitiveKey(keyPtr, hPtr)
    VOID *keyPtr;               /* New key to compare. */
    Tcl_HashEntry *hPtr;        /* Existing key to compare. */
{   
    CONST char *p1 = (CONST char *) keyPtr;
    CONST char *p2 = (CONST char *) hPtr->key.string;

    return !stricmp(p1, p2);
}

/*
 *---------------------------------------------------------------------------
 *
 * hashCaseInsensitiveKey --
 *
 *     Generate a 4-byte hash of the NULL-terminated string pointed to by 
 *     keyPtr. The hash is case-insensitive.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static unsigned int 
hashCaseInsensitiveKey(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key from which to compute hash value. */
{
    CONST char *string = (CONST char *) keyPtr;
    unsigned int result;
    int c;

    result = 0;

    for (c=*string++ ; c ; c=*string++) {
        result += (result<<3) + tolower(c);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * allocCaseInsensitiveEntry --
 *
 *     Allocate enough space for a Tcl_HashEntry and associated string key.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_HashEntry * 
allocCaseInsensitiveEntry(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key to store in the hash table entry. */
{
    const char *string = (CONST char *) keyPtr;
    char *pCsr;
    Tcl_HashEntry *hPtr;
    unsigned int size;

    size = sizeof(Tcl_HashEntry) + strlen(string) + 1 - sizeof(hPtr->key);
    if (size < sizeof(Tcl_HashEntry)) {
        size = sizeof(Tcl_HashEntry);
    }
    hPtr = (Tcl_HashEntry *) HtmlAlloc("allocCaseInsensitiveEntry()", size);
    strcpy(hPtr->key.string, string);

    for (pCsr = hPtr->key.string; *pCsr; pCsr++) {
        if( *pCsr > 0 ) *pCsr = tolower(*pCsr);
    }

    return hPtr;
}

static void
freeCaseInsensitiveEntry(hPtr)
    Tcl_HashEntry *hPtr;
{
    HtmlFree(hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCaseInsenstiveHashType --
 *
 *     Return a pointer to the hash key type for case-insensitive string
 *     hashes. This can be used to initialize a hash table as follows:
 *
 *         Tcl_HashTable hash;
 *         Tcl_HashKeyType *pCase = HtmlCaseInsenstiveHashType();
 *         Tcl_InitCustomHashTable(&hash, TCL_CUSTOM_TYPE_KEYS, pCase);
 *
 * Results:
 *     Pointer to hash_key_type (see above).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_HashKeyType *
HtmlCaseInsenstiveHashType() 
{
    /*
     * Hash key type for case-insensitive hash.
     */
    static Tcl_HashKeyType hash_key_type = {
        TCL_HASH_KEY_TYPE_VERSION,          /* version */
        0,                                  /* flags */
        hashCaseInsensitiveKey,             /* hashKeyProc */
        compareCaseInsensitiveKey,          /* compareKeysProc */
        allocCaseInsensitiveEntry,          /* allocEntryProc */
        freeCaseInsensitiveEntry            /* freeEntryProc */
    };
    return &hash_key_type;
}

/*
 *---------------------------------------------------------------------------
 *
 * hashFontKey --
 *
 *     Generate a 4-byte hash of the NULL-terminated string pointed to by 
 *     keyPtr. The hash is case-insensitive.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static unsigned int 
hashFontKey(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key from which to compute hash value. */
{
    HtmlFontKey *pKey = (HtmlFontKey *) keyPtr;
    CONST char *zFontFamily = pKey->zFontFamily;
    unsigned int result = 0;
    int c;

    for (c=*zFontFamily++ ; c ; c=*zFontFamily++) {
        result += (result<<3) + c;
    }
    result += (result<<3) + pKey->iFontSize;
    result += (result<<1) + (pKey->isItalic?1:0);
    result += (result<<1) + (pKey->isBold?1:0);

    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * compareFontKey --
 *
 *     The compare function for the font-key hash. Compare a new key to the key
 *     of an existing hash-entry.
 *
 * Results:
 *     True if the two keys are the same, false if not.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
compareFontKey(keyPtr, hPtr)
    VOID *keyPtr;               /* New key to compare. */
    Tcl_HashEntry *hPtr;        /* Existing key to compare. */
{   
    HtmlFontKey *p1 = (HtmlFontKey *) keyPtr;
    HtmlFontKey *p2 = (HtmlFontKey *) hPtr->key.string;

    return ((
        p1->iFontSize != p2->iFontSize ||
        p1->isItalic != p2->isItalic ||
        p1->isBold != p2->isBold ||
        strcmp(p1->zFontFamily, p2->zFontFamily)
    ) ? 0 : 1);
}


/*
 *---------------------------------------------------------------------------
 *
 * allocFontEntry --
 *
 *     Allocate enough space for a Tcl_HashEntry and an HtmlFontKey key.
 *
 * Results:
 *     Pointer to allocated TclHashEntry structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_HashEntry * 
allocFontEntry(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key to store in the hash table entry. */
{
    HtmlFontKey *pKey = (HtmlFontKey *)keyPtr;
    unsigned int size;
    Tcl_HashEntry *hPtr;
    HtmlFontKey *pStoredKey;

    assert(pKey->zFontFamily);
    size = (
        sizeof(Tcl_HashEntry) - sizeof(hPtr->key) +
        strlen(pKey->zFontFamily) + 1 +
        sizeof(HtmlFontKey)
    );
    assert(size >= sizeof(Tcl_HashEntry));

    hPtr = (Tcl_HashEntry *) HtmlAlloc("allocFontEntry()", size);
    pStoredKey = (HtmlFontKey *)(hPtr->key.string);
    pStoredKey->iFontSize = pKey->iFontSize;
    pStoredKey->isItalic = pKey->isItalic;
    pStoredKey->isBold = pKey->isBold;
    pStoredKey->zFontFamily = (char *)(&pStoredKey[1]);
    strcpy((char *)pStoredKey->zFontFamily, pKey->zFontFamily);

    return hPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlFontKeyHashType --
 *
 *     Return a pointer to the hash key type for font-key hashes. The key-type
 *     for the hash-table is HtmlFontKey (see htmlprop.h). This can be used to
 *     initialize a hash table as follows:
 *
 *         Tcl_HashTable hash;
 *         Tcl_HashKeyType *pFontKey = HtmlFontKeyHashType();
 *         Tcl_InitCustomHashTable(&hash, TCL_CUSTOM_TYPE_KEYS, pFontKey);
 *
 * Results:
 *     Pointer to hash_key_type (see above).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_HashKeyType * 
HtmlFontKeyHashType() 
{
    /*
     * Hash key type for font-key hash.
     */
    static Tcl_HashKeyType hash_key_type = {
        TCL_HASH_KEY_TYPE_VERSION,          /* version */
        0,                                  /* flags */
        hashFontKey,                        /* hashKeyProc */
        compareFontKey,                     /* compareKeysProc */
        allocFontEntry,                     /* allocEntryProc */
        freeCaseInsensitiveEntry            /* freeEntryProc */
    };
    return &hash_key_type;
}

/*
 *---------------------------------------------------------------------------
 *
 * hashValuesKey --
 *
 *     Generate a 4-byte hash of the HtmlComputedValues object pointed to by
 *     keyPtr. All fields of the structure apart from 'nRef' may be used by
 *     this function.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static unsigned int 
hashValuesKey(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key from which to compute hash value. */
{
    HtmlComputedValues *p= (HtmlComputedValues *)keyPtr;
    unsigned int result = 0;

    /* Do not include the first two fields - nRef and imZoomedBackgroundImage */
    unsigned char *pInt = (unsigned char *)(&p->mask);

    /* Hash the remaining bytes of the structure */
    while (pInt < (unsigned char *)&p[1]) {
      result += (result << 3) + *pInt;
      pInt++;
    }

    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * compareValuesKey --
 *
 *     The compare function for the property-values hash. Compare a new key to
 *     the key of an existing hash-entry.
 *
 * Results:
 *     True if the two keys are the same, false if not.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
compareValuesKey(keyPtr, hPtr)
    VOID *keyPtr;               /* New key to compare. */
    Tcl_HashEntry *hPtr;        /* Existing key to compare. */
{   
    unsigned char *p1 = (unsigned char *) keyPtr;
    unsigned char *p2 = (unsigned char *) hPtr->key.string;

    static const int N = Tk_Offset(HtmlComputedValues, mask); 
    static const int nBytes = 
        sizeof(HtmlComputedValues) - Tk_Offset(HtmlComputedValues, mask);


    /* Do not compare the first field - nRef */
    p1 += N;
    p2 += N;

    return (0 == memcmp(p1, p2, nBytes));
}

static void
freeValuesEntry(hPtr)
    Tcl_HashEntry *hPtr;
{
    HtmlFree(hPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * allocValuesEntry --
 *
 *     Allocate enough space for a Tcl_HashEntry and an HtmlComputedValues 
 *     key.
 *
 * Results:
 *     Pointer to allocated TclHashEntry structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_HashEntry * 
allocValuesEntry(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key to store in the hash table entry. */
{
    HtmlComputedValues *pKey = (HtmlComputedValues *)keyPtr;
    HtmlComputedValues *pStoredKey;
    unsigned int size;
    Tcl_HashEntry *hPtr;

    size = (
        sizeof(HtmlComputedValues) +
        sizeof(Tcl_HashEntry) - 
        sizeof(hPtr->key)
    );
    assert(size >= sizeof(Tcl_HashEntry));

    hPtr = (Tcl_HashEntry *) HtmlAlloc("allocValuesEntry()", size);
    pStoredKey = (HtmlComputedValues *)(hPtr->key.string);
    memcpy(pStoredKey, pKey, sizeof(HtmlComputedValues));

    return hPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesHashType --
 *
 *     Return a pointer to the hash key type for property-values hashes. The
 *     key-type for the hash-table is HtmlFontKey (see htmlprop.h). This can be
 *     used to initialize a hash table as follows:
 *
 *         Tcl_HashTable hash;
 *         Tcl_HashKeyType *pFontKey = HtmlFontKeyHashType();
 *         Tcl_InitCustomHashTable(&hash, TCL_CUSTOM_TYPE_KEYS, pFontKey);
 *
 * Results:
 *     Pointer to hash_key_type (see above).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_HashKeyType * HtmlComputedValuesHashType() 
{
    /*
     * Hash key type for property-values hash.
     */
    static Tcl_HashKeyType hash_key_type = {
        TCL_HASH_KEY_TYPE_VERSION,          /* version */
        0,                                  /* flags */
        hashValuesKey,                      /* hashKeyProc */
        compareValuesKey,                   /* compareKeysProc */
        allocValuesEntry,                   /* allocEntryProc */
        freeValuesEntry                     /* freeEntryProc */
    };
    return &hash_key_type;
}

