
/*
 * css.h --
 *  
 *     This header file contains the interface used by other modules to
 *     access the CSS parsing/cascade module.
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
 */

#ifndef __CSS_H__
#define __CSS_H__

#include <tcl.h>
#include "cssprop.h"

/*
 * This header file contains the public interface to the CSS2 parsing 
 * module. The module parses CSS2 and implements the cascade algorithm.
 * It does not interpret property values, except for short-cut properties, 
 * which are translated to the equivalent canonical properties.
 *
 * This module uses a fairly complicated object-oriented interface. The 
 * objects have the following roles:
 *
 * CssProperty:      A single property value.
 *
 * CssProperties:    A collection of CSS2 properties. Can be queried for the
 *                   value or presence of a specific property.
 *
 * CssStyleSheet:    The parsed representation of a style-sheet document or
 *                   an HTML style attribute (i.e. from a tag like: 
 *                   '<h1 style="font : italic">').
 */
typedef struct CssStyleSheet CssStyleSheet;
typedef struct CssProperty CssProperty;
typedef struct CssDynamic CssDynamic;

typedef struct CssPropertySet CssPropertySet;

/* Include html.h after we define our opaque types, because it includes
 * structures that contain pointers to them.
 */
#include "html.h"

#define CSS_TYPE_EM           1            /* Value in 'rVal' */
#define CSS_TYPE_PX           2            /* Value in 'rVal' */
#define CSS_TYPE_PT           3            /* Value in 'rVal' */
#define CSS_TYPE_PC           4            /* Value in 'rVal' */
#define CSS_TYPE_EX           5            /* Value in 'rVal' */

/* Physical units. */
#define CSS_TYPE_CENTIMETER   6            /* Value in 'rVal */
#define CSS_TYPE_INCH         7            /* Value in 'rVal */
#define CSS_TYPE_MILLIMETER   8            /* Value in 'rVal */

#define CSS_TYPE_PERCENT      9            /* Value in 'rVal' */
#define CSS_TYPE_FLOAT       10            /* Value in 'rVal' */

#define CSS_TYPE_STRING      11            /* Value in 'zVal' */
#define CSS_TYPE_NONE        12            /* No value */

/* Function notation */
#define CSS_TYPE_TCL         13            /* Value in 'zVal' */
#define CSS_TYPE_URL         14            /* Value in 'zVal' */
#define CSS_TYPE_ATTR        15            /* Value in 'zVal' */
#define CSS_TYPE_COUNTER     16            /* Value in 'zVal' */
#define CSS_TYPE_COUNTERS    17            /* Value in 'zVal' */

#define CSS_TYPE_RAW         18 

#define CSS_TYPE_LIST        19            /* Used for 'content' property */


/*
 * A single CSS property is represented by an instance of the following
 * struct. The actual value is stored in one of the primitives inside
 * the union. The eType field is set to one of the CSS_TYPE_* constants
 * below. 
 */
struct CssProperty {
    int eType;
    union {
        char *zVal;
        double rVal;
        void *p;
    } v;
};

/*
 * Retrieve the string value of a CSS property. This works with all
 * internally consistent CssProperty objects, regardless of the
 * CssProperty.eType value.
 */
CONST char *HtmlCssPropertyGetString(CssProperty *pProp);

/*
 * Create a property from a value string (i.e. "20ex" or "yellow", 
 * not "h1 {font:large}").
 */
CssProperty *HtmlCssStringToProperty(CONST char *z, int n);

/*
 * This is used to split up a white-space seperated list.
 */
const char *HtmlCssGetNextListItem(const char *z, int n, int *pN);

const char *HtmlCssGetNextCommaListItem(const char *z, int n, int *pN);

/*
 * Functions to parse stylesheet and style data into CssStyleSheet objects.
 *
 * A CssStyleSheet object is created from either the text of a stylesheet
 * document (function HtmlCssParse()) or the value of a style="..." 
 * HTML attribute (function tkhtmlCssParseStyle()). The primary use
 * of a CssStyleSheet object is to incorporate it into a CssCascade
 * object.
 *
 * HtmlCssStyleSheetSyntaxErrs() returns the number of syntax errors
 * that occured while parsing the stylesheet or style attribute.
 *
 * tkhtmlCssStyleSheetFree() frees the memory used to store a stylesheet
 * object internally.
 */
int HtmlCssParse(Tcl_Obj *, int, Tcl_Obj *, Tcl_Obj *, CssStyleSheet **);
int HtmlCssStyleSheetSyntaxErrs(CssStyleSheet *);
void HtmlCssStyleSheetFree(CssStyleSheet *);

/* Values to pass as the second argument ("origin") of HtmlCssParse() */
#define CSS_ORIGIN_AGENT  1
#define CSS_ORIGIN_USER   2
#define CSS_ORIGIN_AUTHOR 3

/*
 * Function to apply a stylesheet to a document node.
 */
void HtmlCssStyleSheetApply(HtmlTree *, HtmlNode *);
void HtmlCssStyleSheetGenerated(HtmlTree *, HtmlElementNode *);
void HtmlCssStyleGenerateContent(HtmlTree *, HtmlElementNode *, int);

/*
 * Functions to interface with inline style information (in HTML, 
 * the "style" attribute).
 */
int  HtmlCssInlineParse(HtmlTree *, int, CONST char *, CssPropertySet **);
void HtmlCssInlineFree(CssPropertySet *);
int HtmlCssInlineQuery(Tcl_Interp *, CssPropertySet *, Tcl_Obj *);

/*
  CssProperty *HtmlCssPropertiesGet(CssProperties *, int, int*, int*);
*/

Tcl_ObjCmdProc HtmlCssStyleReport;

void HtmlCssCheckDynamic(HtmlTree *);
void HtmlCssFreeDynamics(HtmlElementNode *);
int  HtmlCssTclNodeDynamics(Tcl_Interp *, HtmlNode *);

/* The interface to the csssearch.c module. This module is responsible
 * for implementation of the [$widget search] command.
 */
int HtmlCssSearchInit(HtmlTree *);
int HtmlCssSearchShutdown(HtmlTree *);
int HtmlCssSearchInvalidateCache(HtmlTree *);
Tcl_ObjCmdProc HtmlCssSearch;

#if 0

/* Future interface for :before and :after pseudo-elements. Need this to 
 * handle the <br> tag most elegantly.
 */
CssProperties *HtmlCssPropertiesGetBefore(CssProperties *);
CssProperties *HtmlCssPropertiesGetAfter(CssProperties *);

#endif

#endif
