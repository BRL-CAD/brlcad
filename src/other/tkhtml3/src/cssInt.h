/*
 * cssInt.h - 
 *
 *     This header defines the internal structures and functions
 *     used internally by the tkhtml CSS module. 
 *
 *----------------------------------------------------------------------------
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
 *     * Neither the name of the Eolas Technologies Inc. nor the names of its
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

#ifndef __CSSINT_H__
#define __CSSINT_H__

#include "css.h"
#include <tcl.h>

typedef struct CssSelector CssSelector;
typedef struct CssRule CssRule;
typedef struct CssParse CssParse;
typedef struct CssToken CssToken;
typedef struct CssPriority CssPriority;
typedef struct CssProperties CssProperties;

typedef unsigned char u8;
typedef unsigned int u32;

/*
 * Ways in which simple selectors and pseudo-classes can be chained 
 * together to form complex selectors.
 */
#define CSS_SELECTORCHAIN_DESCENDANT     1    /* eg. "a b" */
#define CSS_SELECTORCHAIN_CHILD          2    /* eg. "a > b" */
#define CSS_SELECTORCHAIN_ADJACENT       3    /* eg. "a + b" */

/*
 * Simple selector types.
 */
#define CSS_SELECTOR_UNIVERSAL           4    /* eg. "*" */
#define CSS_SELECTOR_TYPE                5
#define CSS_SELECTOR_ATTR                7
#define CSS_SELECTOR_ATTRVALUE           8
#define CSS_SELECTOR_ATTRLISTVALUE       9
#define CSS_SELECTOR_ATTRHYPHEN          10
#define CSS_SELECTOR_CLASS               34   /* eg. ".classname"   */
#define CSS_SELECTOR_ID                  35   /* eg. "#idname"      */

/*
** Psuedo-classes
*/
#define CSS_PSEUDOCLASS_LANG             11
#define CSS_PSEUDOCLASS_FIRSTCHILD       12
#define CSS_PSEUDOCLASS_LASTCHILD        13
#define CSS_PSEUDOCLASS_LINK             14
#define CSS_PSEUDOCLASS_VISITED          15
#define CSS_PSEUDOCLASS_ACTIVE           16
#define CSS_PSEUDOCLASS_HOVER            17
#define CSS_PSEUDOCLASS_FOCUS            18

/*
** Pseudo-elements.
*/
#define CSS_PSEUDOELEMENT_FIRSTLINE      19
#define CSS_PSEUDOELEMENT_FIRSTLETTER    20
#define CSS_PSEUDOELEMENT_BEFORE         21
#define CSS_PSEUDOELEMENT_AFTER          22

/*
** CSS media types.
*/
#define CSS_MEDIA_ALL          23
#define CSS_MEDIA_AURAL        24
#define CSS_MEDIA_BRAILLE      25
#define CSS_MEDIA_EMBOSSED     26
#define CSS_MEDIA_HANDHELD     27
#define CSS_MEDIA_PRINT        28
#define CSS_MEDIA_PROJECTION   29
#define CSS_MEDIA_SCREEN       30
#define CSS_MEDIA_TTY          31
#define CSS_MEDIA_TV           32

#define CSS_SELECTOR_NEVERMATCH 33


/*
 * Before they are passed to the lemon-generated parser, the tokenizer
 * splits the output into tokens of the following type. Refer to function
 * cssGetToken() in css.c for details.
 */
struct CssToken {
    const char *z;
    int n;
};

/*
 * A CSS selector is stored as a linked list of the CssSelector structure.
 * The first element in the list is the rightmost simple-selector in the 
 * selector text. For example, the selector "h1 h2 > p" (match elements of
 * type <p> that is a child of an <h2> that is a descendant of an <h1>)
 * is stored as [p]->[h2]->[h1].
 *
 * See the function selectorTest() in css.c for details of how this is
 * used.
 */
struct CssSelector {
    u8 isDynamic;     /* True if this selector is dynamic */
    u8 eSelector;     /* CSS_SELECTOR* or CSS_PSEUDO* value */
    char *zAttr;      /* The attribute queried, if any. */
    char *zValue;     /* The value tested for, if any. */
    CssSelector *pNext;  /* Next simple-selector in chain */
};

/*
** A collection of CSS2 properties and values.
*/
struct CssPropertySet {
    int n;
    struct CssPropertySetItem {
        int eProp;
        CssProperty *pProp;
    } *a;
};

struct CssProperties {
    int nRule;
    CssRule **apRule;
};

struct CssRule {
    CssPriority *pPriority;  /* Pointer to the priority of source stylesheet */
    int specificity;         /* Specificity of the selector */
    int iRule;               /* Rule-number within source style sheet */
    CssSelector *pSelector;  /* The selector-chain for this rule */
    int freePropertySets;          /* True to delete pPropertySet */
    int freeSelector;              /* True to delete pSelector */
    CssPropertySet *pPropertySet;  /* Property values for the rule. */
    CssRule *pNext;                /* Next rule in this list. */
};

/*
 * A linked list of the following structures is stored in
 * CssStyleSheet.pPriority.
 *
 * Each time a call is made to [<widget> style] to add a new stylesheet to
 * the configuration, two instances of this structure are allocated using
 * HtmlAlloc() and inserted into the list. The CssPriority.origin and
 * CssPriority.pIdTail variables are set to the origin and id-tail of the
 * new stylesheet (based on parsing the stylesheet-id) in both instances.
 * In one instance the CssPriority.important flag is set to true, in the
 * other false.
 *
 * The list is kept in order from highest priority to lowest priority based
 * on the values of CssPriority.important, CssPriority.origin and
 * CssPriority.pIdTail. Each time the list is reorganized, the values of
 * CssPriority.iPriority are set such that higher priority list members
 * have lower values of CssPriority.iPriority.
 *
 * Each CssRule structure has a pointer to it's associated CssPriority
 * structure.
 */
struct CssPriority {
    int important;           /* True if !IMPORTANT flag is set */
    int origin;              /* One of CSS_ORIGIN_AGENT, _AUTHOR or _USER */ 
    Tcl_Obj *pIdTail;        /* Tail of the stylesheet id */
    int iPriority;
    CssPriority *pNext;      /* Linked list pointer */
};

/*
 * A style-sheet contains zero or more rules. Depending on the nature of
 * the selector for the rule, it is either stored in a linked list starting
 * at CssStyleSheet.pUniversalRules, or in a linked list stored in the
 * hash table CssStyleSheet.rules. The CssStyleSheet.rules hash is indexed
 * by the tag type of the elements that the rule applies to.
 *
 * For example, the rule "H1 {text-decoration: bold}" is stored in a linked
 * list accessible by looking up "h1" in the rules hash table.
 */
struct CssStyleSheet {
    int nSyntaxErr;           /* Number of syntax errors during parsing */
    CssPriority *pPriority;

    CssRule *pUniversalRules;  /* Rules that do not belong to any other list */

    CssRule *pAfterRules;      /* Rules that end in :after */
    CssRule *pBeforeRules;     /* Rules that end in :before */

    Tcl_HashTable aByTag;      /* Rule lists by tag (string keys) */
    Tcl_HashTable aByClass;    /* Rule lists by class (string keys) */
    Tcl_HashTable aById;       /* Rule lists by id (string keys) */
};

/*
 * A single instance of this object is used for each parse. After the parse
 * is finished it is no longer required, the permanent record of the parsed
 * stylesheet is built up in CssParse.pStyle.
 */
struct CssParse {
    CssStyleSheet *pStyle;

    CssSelector *pSelector;         /* Selector currently being parsed */
    int nXtra;
    CssSelector **apXtraSelector;   /* Selectors also waiting for prop set. */

    CssPropertySet *pPropertySet;   /* Declarations being parsed. */
    CssPropertySet *pImportant;     /* !IMPORTANT declarations. */

    CssPriority *pPriority1;
    CssPriority *pPriority2;

    int iNextRule;                  /* iRule value for next rule */

    /* The parser sets the isIgnore flag to true when it enters an @media {}
     * block that does *not* apply, and sets it back to false when it exits the
     * @media block.
     */
    int isIgnore;                   /* True to ignore new elements */

    /* In the body of a stylesheet @import directives must be ignored. */
    int isBody;                     /* True once we are in the body */

    int origin;
    Tcl_Obj *pStyleId;
    Tcl_Obj *pImportCmd;            /* Script to invoke for @import */
    Tcl_Obj *pUrlCmd;               /* Script to invoke for url() */
    Tcl_Obj *pErrorLog;             /* In non-zero, store syntax errors here */
    Tcl_Interp *interp;             /* Interpreter to invoke pImportCmd */
    HtmlTree *pTree;                /* Tree used to determine if quirks mode */
};

/*
 * These functions are called by the lemon-generated parser (see
 * cssparse.y). They add rules to the stylesheet.
 */
void HtmlCssDeclaration(CssParse *, CssToken *, CssToken *, int);
void HtmlCssSelector(CssParse *, int, CssToken *, CssToken *);
void HtmlCssRule(CssParse *, int);
void HtmlCssSelectorComma(CssParse *pParse);
void HtmlCssImport(CssParse *pParse, CssToken *);

/* Test if a selector matches a node */
int HtmlCssSelectorTest(CssSelector *, HtmlNode *, int);

void HtmlCssAddDynamic(HtmlElementNode *, CssSelector *, int);

/* Append the string representation of the supplied selector to the object. */
void HtmlCssSelectorToString(CssSelector *, Tcl_Obj *);

int HtmlCssTclNodeDynamics(Tcl_Interp *, HtmlNode *);

int HtmlCssSelectorParse(HtmlTree *, int, const char *, CssStyleSheet **);

enum CssTokenType {
    CT_SPACE,    CT_LP,           CT_RP,        CT_RRP,        CT_LSP,
    CT_RSP,      CT_SEMICOLON,    CT_COMMA,     CT_COLON,      CT_PLUS,
    CT_DOT,      CT_HASH,         CT_EQUALS,    CT_TILDE,      CT_PIPE,
    CT_AT,       CT_BANG,         CT_STRING,    CT_LRP,        CT_GT,
    CT_STAR,     CT_SLASH,        CT_IDENT,     CT_FUNCTION,

    CT_SGML_OPEN, CT_SGML_CLOSE, CT_SYNTAX_ERROR, CT_EOF
};
typedef enum CssTokenType CssTokenType;

void HtmlCssRunParser(const char *, int, CssParse *);
void HtmlCssRunStyleParser(const char *, int, CssParse *);
CssTokenType HtmlCssGetToken(const char *, int, int *);

#endif /* __CSS_H__ */

