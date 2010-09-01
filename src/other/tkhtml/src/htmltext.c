/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2006 Dan Kennedy.
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

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include "html.h"

#define ISNEWLINE(x) ((x) == '\n' || (x) == '\r')
#define ISTAB(x) ((x) == '\t')
#define ISSPACE(x) isspace((unsigned char)(x))

/*
 * This file implements the experimental [tag] widget method. The
 * following summarizes the interface supported:
 *
 *         html tag add TAGNAME FROM-NODE FROM-INDEX TO-NODE TO-INDEX
 *         html tag remove TAGNAME FROM-NODE FROM-INDEX TO-NODE TO-INDEX
 *         html tag delete TAGNAME
 *         html tag configure TAGNAME ?-fg COLOR? ?-bg COLOR?
 *
 * This file exports the following functions:
 *
 *     HtmlTranslateEscapes()
 *         Translates Html escapes (i.e. "&nbsp;").
 *
 *     HtmlTagAddRemoveCmd()
 *         Implementation of [pathName tag add] and [pathName tag remove]
 *
 *     HtmlTagDeleteCmd()
 *         Implementation of [pathName tag delete]
 *
 *     HtmlTagConfigureCmd()
 *         Implementation of [pathName tag configured]
 *
 *     HtmlTagCleanupNode()
 *     HtmlTagCleanupTree()
 *         Respectively called when an HtmlNode or HtmlTree structure is being
 *         deallocated to free outstanding tag related stuff.
 *
 *
 * Also:
 *
 *     HtmlTextTextCmd()
 *     HtmlTextBboxCmd()
 *     HtmlTextOffsetCmd()
 *     HtmlTextIndexCmd()
 */

/****************** Begin Escape Sequence Translator *************/

/*
** The next section of code implements routines used to translate
** the '&' escape sequences of SGML to individual characters.
** Examples:
**
**         &amp;          &
**         &lt;           <
**         &gt;           >
**         &nbsp;         nonbreakable space
*/

/* Each escape sequence is recorded as an instance of the following
** structure
*/
struct sgEsc {
    char *zName;              /* The name of this escape sequence. ex: "amp" */
    char value[8];            /* The value for this sequence. ex: "&" */
    struct sgEsc *pNext;      /* Next sequence with the same hash on zName */
};

/* The following is a table of all escape sequences.  Add new sequences
** by adding entries to this table.
*/
static struct sgEsc esc_sequences[] = {
    {"nbsp", "\xC2\xA0", 0},            /* Unicode code-point 160 */
    {"iexcl", "\xC2\xA1", 0},           /* Unicode code-point 161 */
    {"cent", "\xC2\xA2", 0},            /* Unicode code-point 162 */
    {"pound", "\xC2\xA3", 0},           /* Unicode code-point 163 */
    {"curren", "\xC2\xA4", 0},          /* Unicode code-point 164 */
    {"yen", "\xC2\xA5", 0},             /* Unicode code-point 165 */
    {"brvbar", "\xC2\xA6", 0},          /* Unicode code-point 166 */
    {"sect", "\xC2\xA7", 0},            /* Unicode code-point 167 */
    {"uml", "\xC2\xA8", 0},             /* Unicode code-point 168 */
    {"copy", "\xC2\xA9", 0},            /* Unicode code-point 169 */
    {"ordf", "\xC2\xAA", 0},            /* Unicode code-point 170 */
    {"laquo", "\xC2\xAB", 0},           /* Unicode code-point 171 */
    {"not", "\xC2\xAC", 0},             /* Unicode code-point 172 */
    {"shy", "\xC2\xAD", 0},             /* Unicode code-point 173 */
    {"reg", "\xC2\xAE", 0},             /* Unicode code-point 174 */
    {"macr", "\xC2\xAF", 0},            /* Unicode code-point 175 */
    {"deg", "\xC2\xB0", 0},             /* Unicode code-point 176 */
    {"plusmn", "\xC2\xB1", 0},          /* Unicode code-point 177 */
    {"sup2", "\xC2\xB2", 0},            /* Unicode code-point 178 */
    {"sup3", "\xC2\xB3", 0},            /* Unicode code-point 179 */
    {"acute", "\xC2\xB4", 0},           /* Unicode code-point 180 */
    {"micro", "\xC2\xB5", 0},           /* Unicode code-point 181 */
    {"para", "\xC2\xB6", 0},            /* Unicode code-point 182 */
    {"middot", "\xC2\xB7", 0},          /* Unicode code-point 183 */
    {"cedil", "\xC2\xB8", 0},           /* Unicode code-point 184 */
    {"sup1", "\xC2\xB9", 0},            /* Unicode code-point 185 */
    {"ordm", "\xC2\xBA", 0},            /* Unicode code-point 186 */
    {"raquo", "\xC2\xBB", 0},           /* Unicode code-point 187 */
    {"frac14", "\xC2\xBC", 0},          /* Unicode code-point 188 */
    {"frac12", "\xC2\xBD", 0},          /* Unicode code-point 189 */
    {"frac34", "\xC2\xBE", 0},          /* Unicode code-point 190 */
    {"iquest", "\xC2\xBF", 0},          /* Unicode code-point 191 */
    {"Agrave", "\xC3\x80", 0},          /* Unicode code-point 192 */
    {"Aacute", "\xC3\x81", 0},          /* Unicode code-point 193 */
    {"Acirc", "\xC3\x82", 0},           /* Unicode code-point 194 */
    {"Atilde", "\xC3\x83", 0},          /* Unicode code-point 195 */
    {"Auml", "\xC3\x84", 0},            /* Unicode code-point 196 */
    {"Aring", "\xC3\x85", 0},           /* Unicode code-point 197 */
    {"AElig", "\xC3\x86", 0},           /* Unicode code-point 198 */
    {"Ccedil", "\xC3\x87", 0},          /* Unicode code-point 199 */
    {"Egrave", "\xC3\x88", 0},          /* Unicode code-point 200 */
    {"Eacute", "\xC3\x89", 0},          /* Unicode code-point 201 */
    {"Ecirc", "\xC3\x8A", 0},           /* Unicode code-point 202 */
    {"Euml", "\xC3\x8B", 0},            /* Unicode code-point 203 */
    {"Igrave", "\xC3\x8C", 0},          /* Unicode code-point 204 */
    {"Iacute", "\xC3\x8D", 0},          /* Unicode code-point 205 */
    {"Icirc", "\xC3\x8E", 0},           /* Unicode code-point 206 */
    {"Iuml", "\xC3\x8F", 0},            /* Unicode code-point 207 */
    {"ETH", "\xC3\x90", 0},             /* Unicode code-point 208 */
    {"Ntilde", "\xC3\x91", 0},          /* Unicode code-point 209 */
    {"Ograve", "\xC3\x92", 0},          /* Unicode code-point 210 */
    {"Oacute", "\xC3\x93", 0},          /* Unicode code-point 211 */
    {"Ocirc", "\xC3\x94", 0},           /* Unicode code-point 212 */
    {"Otilde", "\xC3\x95", 0},          /* Unicode code-point 213 */
    {"Ouml", "\xC3\x96", 0},            /* Unicode code-point 214 */
    {"times", "\xC3\x97", 0},           /* Unicode code-point 215 */
    {"Oslash", "\xC3\x98", 0},          /* Unicode code-point 216 */
    {"Ugrave", "\xC3\x99", 0},          /* Unicode code-point 217 */
    {"Uacute", "\xC3\x9A", 0},          /* Unicode code-point 218 */
    {"Ucirc", "\xC3\x9B", 0},           /* Unicode code-point 219 */
    {"Uuml", "\xC3\x9C", 0},            /* Unicode code-point 220 */
    {"Yacute", "\xC3\x9D", 0},          /* Unicode code-point 221 */
    {"THORN", "\xC3\x9E", 0},           /* Unicode code-point 222 */
    {"szlig", "\xC3\x9F", 0},           /* Unicode code-point 223 */
    {"agrave", "\xC3\xA0", 0},          /* Unicode code-point 224 */
    {"aacute", "\xC3\xA1", 0},          /* Unicode code-point 225 */
    {"acirc", "\xC3\xA2", 0},           /* Unicode code-point 226 */
    {"atilde", "\xC3\xA3", 0},          /* Unicode code-point 227 */
    {"auml", "\xC3\xA4", 0},            /* Unicode code-point 228 */
    {"aring", "\xC3\xA5", 0},           /* Unicode code-point 229 */
    {"aelig", "\xC3\xA6", 0},           /* Unicode code-point 230 */
    {"ccedil", "\xC3\xA7", 0},          /* Unicode code-point 231 */
    {"egrave", "\xC3\xA8", 0},          /* Unicode code-point 232 */
    {"eacute", "\xC3\xA9", 0},          /* Unicode code-point 233 */
    {"ecirc", "\xC3\xAA", 0},           /* Unicode code-point 234 */
    {"euml", "\xC3\xAB", 0},            /* Unicode code-point 235 */
    {"igrave", "\xC3\xAC", 0},          /* Unicode code-point 236 */
    {"iacute", "\xC3\xAD", 0},          /* Unicode code-point 237 */
    {"icirc", "\xC3\xAE", 0},           /* Unicode code-point 238 */
    {"iuml", "\xC3\xAF", 0},            /* Unicode code-point 239 */
    {"eth", "\xC3\xB0", 0},             /* Unicode code-point 240 */
    {"ntilde", "\xC3\xB1", 0},          /* Unicode code-point 241 */
    {"ograve", "\xC3\xB2", 0},          /* Unicode code-point 242 */
    {"oacute", "\xC3\xB3", 0},          /* Unicode code-point 243 */
    {"ocirc", "\xC3\xB4", 0},           /* Unicode code-point 244 */
    {"otilde", "\xC3\xB5", 0},          /* Unicode code-point 245 */
    {"ouml", "\xC3\xB6", 0},            /* Unicode code-point 246 */
    {"divide", "\xC3\xB7", 0},          /* Unicode code-point 247 */
    {"oslash", "\xC3\xB8", 0},          /* Unicode code-point 248 */
    {"ugrave", "\xC3\xB9", 0},          /* Unicode code-point 249 */
    {"uacute", "\xC3\xBA", 0},          /* Unicode code-point 250 */
    {"ucirc", "\xC3\xBB", 0},           /* Unicode code-point 251 */
    {"uuml", "\xC3\xBC", 0},            /* Unicode code-point 252 */
    {"yacute", "\xC3\xBD", 0},          /* Unicode code-point 253 */
    {"thorn", "\xC3\xBE", 0},           /* Unicode code-point 254 */
    {"yuml", "\xC3\xBF", 0},            /* Unicode code-point 255 */
    {"quot", "\x22", 0},                /* Unicode code-point 34 */
    {"amp", "\x26", 0},                 /* Unicode code-point 38 */
    {"lt", "\x3C", 0},                  /* Unicode code-point 60 */
    {"gt", "\x3E", 0},                  /* Unicode code-point 62 */
    {"OElig", "\xC5\x92", 0},           /* Unicode code-point 338 */
    {"oelig", "\xC5\x93", 0},           /* Unicode code-point 339 */
    {"Scaron", "\xC5\xA0", 0},          /* Unicode code-point 352 */
    {"scaron", "\xC5\xA1", 0},          /* Unicode code-point 353 */
    {"Yuml", "\xC5\xB8", 0},            /* Unicode code-point 376 */
    {"circ", "\xCB\x86", 0},            /* Unicode code-point 710 */
    {"tilde", "\xCB\x9C", 0},           /* Unicode code-point 732 */
    {"ensp", "\xE2\x80\x82", 0},        /* Unicode code-point 8194 */
    {"emsp", "\xE2\x80\x83", 0},        /* Unicode code-point 8195 */
    {"thinsp", "\xE2\x80\x89", 0},      /* Unicode code-point 8201 */
    {"zwnj", "\xE2\x80\x8C", 0},        /* Unicode code-point 8204 */
    {"zwj", "\xE2\x80\x8D", 0},         /* Unicode code-point 8205 */
    {"lrm", "\xE2\x80\x8E", 0},         /* Unicode code-point 8206 */
    {"rlm", "\xE2\x80\x8F", 0},         /* Unicode code-point 8207 */
    {"ndash", "\xE2\x80\x93", 0},       /* Unicode code-point 8211 */
    {"mdash", "\xE2\x80\x94", 0},       /* Unicode code-point 8212 */
    {"lsquo", "\xE2\x80\x98", 0},       /* Unicode code-point 8216 */
    {"rsquo", "\xE2\x80\x99", 0},       /* Unicode code-point 8217 */
    {"sbquo", "\xE2\x80\x9A", 0},       /* Unicode code-point 8218 */
    {"ldquo", "\xE2\x80\x9C", 0},       /* Unicode code-point 8220 */
    {"rdquo", "\xE2\x80\x9D", 0},       /* Unicode code-point 8221 */
    {"bdquo", "\xE2\x80\x9E", 0},       /* Unicode code-point 8222 */
    {"dagger", "\xE2\x80\xA0", 0},      /* Unicode code-point 8224 */
    {"Dagger", "\xE2\x80\xA1", 0},      /* Unicode code-point 8225 */
    {"permil", "\xE2\x80\xB0", 0},      /* Unicode code-point 8240 */
    {"lsaquo", "\xE2\x80\xB9", 0},      /* Unicode code-point 8249 */
    {"rsaquo", "\xE2\x80\xBA", 0},      /* Unicode code-point 8250 */
    {"euro", "\xE2\x82\xAC", 0},        /* Unicode code-point 8364 */
    {"fnof", "\xC6\x92", 0},            /* Unicode code-point 402 */
    {"Alpha", "\xCE\x91", 0},           /* Unicode code-point 913 */
    {"Beta", "\xCE\x92", 0},            /* Unicode code-point 914 */
    {"Gamma", "\xCE\x93", 0},           /* Unicode code-point 915 */
    {"Delta", "\xCE\x94", 0},           /* Unicode code-point 916 */
    {"Epsilon", "\xCE\x95", 0},         /* Unicode code-point 917 */
    {"Zeta", "\xCE\x96", 0},            /* Unicode code-point 918 */
    {"Eta", "\xCE\x97", 0},             /* Unicode code-point 919 */
    {"Theta", "\xCE\x98", 0},           /* Unicode code-point 920 */
    {"Iota", "\xCE\x99", 0},            /* Unicode code-point 921 */
    {"Kappa", "\xCE\x9A", 0},           /* Unicode code-point 922 */
    {"Lambda", "\xCE\x9B", 0},          /* Unicode code-point 923 */
    {"Mu", "\xCE\x9C", 0},              /* Unicode code-point 924 */
    {"Nu", "\xCE\x9D", 0},              /* Unicode code-point 925 */
    {"Xi", "\xCE\x9E", 0},              /* Unicode code-point 926 */
    {"Omicron", "\xCE\x9F", 0},         /* Unicode code-point 927 */
    {"Pi", "\xCE\xA0", 0},              /* Unicode code-point 928 */
    {"Rho", "\xCE\xA1", 0},             /* Unicode code-point 929 */
    {"Sigma", "\xCE\xA3", 0},           /* Unicode code-point 931 */
    {"Tau", "\xCE\xA4", 0},             /* Unicode code-point 932 */
    {"Upsilon", "\xCE\xA5", 0},         /* Unicode code-point 933 */
    {"Phi", "\xCE\xA6", 0},             /* Unicode code-point 934 */
    {"Chi", "\xCE\xA7", 0},             /* Unicode code-point 935 */
    {"Psi", "\xCE\xA8", 0},             /* Unicode code-point 936 */
    {"Omega", "\xCE\xA9", 0},           /* Unicode code-point 937 */
    {"alpha", "\xCE\xB1", 0},           /* Unicode code-point 945 */
    {"beta", "\xCE\xB2", 0},            /* Unicode code-point 946 */
    {"gamma", "\xCE\xB3", 0},           /* Unicode code-point 947 */
    {"delta", "\xCE\xB4", 0},           /* Unicode code-point 948 */
    {"epsilon", "\xCE\xB5", 0},         /* Unicode code-point 949 */
    {"zeta", "\xCE\xB6", 0},            /* Unicode code-point 950 */
    {"eta", "\xCE\xB7", 0},             /* Unicode code-point 951 */
    {"theta", "\xCE\xB8", 0},           /* Unicode code-point 952 */
    {"iota", "\xCE\xB9", 0},            /* Unicode code-point 953 */
    {"kappa", "\xCE\xBA", 0},           /* Unicode code-point 954 */
    {"lambda", "\xCE\xBB", 0},          /* Unicode code-point 955 */
    {"mu", "\xCE\xBC", 0},              /* Unicode code-point 956 */
    {"nu", "\xCE\xBD", 0},              /* Unicode code-point 957 */
    {"xi", "\xCE\xBE", 0},              /* Unicode code-point 958 */
    {"omicron", "\xCE\xBF", 0},         /* Unicode code-point 959 */
    {"pi", "\xCF\x80", 0},              /* Unicode code-point 960 */
    {"rho", "\xCF\x81", 0},             /* Unicode code-point 961 */
    {"sigmaf", "\xCF\x82", 0},          /* Unicode code-point 962 */
    {"sigma", "\xCF\x83", 0},           /* Unicode code-point 963 */
    {"tau", "\xCF\x84", 0},             /* Unicode code-point 964 */
    {"upsilon", "\xCF\x85", 0},         /* Unicode code-point 965 */
    {"phi", "\xCF\x86", 0},             /* Unicode code-point 966 */
    {"chi", "\xCF\x87", 0},             /* Unicode code-point 967 */
    {"psi", "\xCF\x88", 0},             /* Unicode code-point 968 */
    {"omega", "\xCF\x89", 0},           /* Unicode code-point 969 */
    {"thetasym", "\xCF\x91", 0},        /* Unicode code-point 977 */
    {"upsih", "\xCF\x92", 0},           /* Unicode code-point 978 */
    {"piv", "\xCF\x96", 0},             /* Unicode code-point 982 */
    {"bull", "\xE2\x80\xA2", 0},        /* Unicode code-point 8226 */
    {"hellip", "\xE2\x80\xA6", 0},      /* Unicode code-point 8230 */
    {"prime", "\xE2\x80\xB2", 0},       /* Unicode code-point 8242 */
    {"Prime", "\xE2\x80\xB3", 0},       /* Unicode code-point 8243 */
    {"oline", "\xE2\x80\xBE", 0},       /* Unicode code-point 8254 */
    {"frasl", "\xE2\x81\x84", 0},       /* Unicode code-point 8260 */
    {"weierp", "\xE2\x84\x98", 0},      /* Unicode code-point 8472 */
    {"image", "\xE2\x84\x91", 0},       /* Unicode code-point 8465 */
    {"real", "\xE2\x84\x9C", 0},        /* Unicode code-point 8476 */
    {"trade", "\xE2\x84\xA2", 0},       /* Unicode code-point 8482 */
    {"alefsym", "\xE2\x84\xB5", 0},     /* Unicode code-point 8501 */
    {"larr", "\xE2\x86\x90", 0},        /* Unicode code-point 8592 */
    {"uarr", "\xE2\x86\x91", 0},        /* Unicode code-point 8593 */
    {"rarr", "\xE2\x86\x92", 0},        /* Unicode code-point 8594 */
    {"darr", "\xE2\x86\x93", 0},        /* Unicode code-point 8595 */
    {"harr", "\xE2\x86\x94", 0},        /* Unicode code-point 8596 */
    {"crarr", "\xE2\x86\xB5", 0},       /* Unicode code-point 8629 */
    {"lArr", "\xE2\x87\x90", 0},        /* Unicode code-point 8656 */
    {"uArr", "\xE2\x87\x91", 0},        /* Unicode code-point 8657 */
    {"rArr", "\xE2\x87\x92", 0},        /* Unicode code-point 8658 */
    {"dArr", "\xE2\x87\x93", 0},        /* Unicode code-point 8659 */
    {"hArr", "\xE2\x87\x94", 0},        /* Unicode code-point 8660 */
    {"forall", "\xE2\x88\x80", 0},      /* Unicode code-point 8704 */
    {"part", "\xE2\x88\x82", 0},        /* Unicode code-point 8706 */
    {"exist", "\xE2\x88\x83", 0},       /* Unicode code-point 8707 */
    {"empty", "\xE2\x88\x85", 0},       /* Unicode code-point 8709 */
    {"nabla", "\xE2\x88\x87", 0},       /* Unicode code-point 8711 */
    {"isin", "\xE2\x88\x88", 0},        /* Unicode code-point 8712 */
    {"notin", "\xE2\x88\x89", 0},       /* Unicode code-point 8713 */
    {"ni", "\xE2\x88\x8B", 0},          /* Unicode code-point 8715 */
    {"prod", "\xE2\x88\x8F", 0},        /* Unicode code-point 8719 */
    {"sum", "\xE2\x88\x91", 0},         /* Unicode code-point 8721 */
    {"minus", "\xE2\x88\x92", 0},       /* Unicode code-point 8722 */
    {"lowast", "\xE2\x88\x97", 0},      /* Unicode code-point 8727 */
    {"radic", "\xE2\x88\x9A", 0},       /* Unicode code-point 8730 */
    {"prop", "\xE2\x88\x9D", 0},        /* Unicode code-point 8733 */
    {"infin", "\xE2\x88\x9E", 0},       /* Unicode code-point 8734 */
    {"ang", "\xE2\x88\xA0", 0},         /* Unicode code-point 8736 */
    {"and", "\xE2\x88\xA7", 0},         /* Unicode code-point 8743 */
    {"or", "\xE2\x88\xA8", 0},          /* Unicode code-point 8744 */
    {"cap", "\xE2\x88\xA9", 0},         /* Unicode code-point 8745 */
    {"cup", "\xE2\x88\xAA", 0},         /* Unicode code-point 8746 */
    {"int", "\xE2\x88\xAB", 0},         /* Unicode code-point 8747 */
    {"there4", "\xE2\x88\xB4", 0},      /* Unicode code-point 8756 */
    {"sim", "\xE2\x88\xBC", 0},         /* Unicode code-point 8764 */
    {"cong", "\xE2\x89\x85", 0},        /* Unicode code-point 8773 */
    {"asymp", "\xE2\x89\x88", 0},       /* Unicode code-point 8776 */
    {"ne", "\xE2\x89\xA0", 0},          /* Unicode code-point 8800 */
    {"equiv", "\xE2\x89\xA1", 0},       /* Unicode code-point 8801 */
    {"le", "\xE2\x89\xA4", 0},          /* Unicode code-point 8804 */
    {"ge", "\xE2\x89\xA5", 0},          /* Unicode code-point 8805 */
    {"sub", "\xE2\x8A\x82", 0},         /* Unicode code-point 8834 */
    {"sup", "\xE2\x8A\x83", 0},         /* Unicode code-point 8835 */
    {"nsub", "\xE2\x8A\x84", 0},        /* Unicode code-point 8836 */
    {"sube", "\xE2\x8A\x86", 0},        /* Unicode code-point 8838 */
    {"supe", "\xE2\x8A\x87", 0},        /* Unicode code-point 8839 */
    {"oplus", "\xE2\x8A\x95", 0},       /* Unicode code-point 8853 */
    {"otimes", "\xE2\x8A\x97", 0},      /* Unicode code-point 8855 */
    {"perp", "\xE2\x8A\xA5", 0},        /* Unicode code-point 8869 */
    {"sdot", "\xE2\x8B\x85", 0},        /* Unicode code-point 8901 */
    {"lceil", "\xE2\x8C\x88", 0},       /* Unicode code-point 8968 */
    {"rceil", "\xE2\x8C\x89", 0},       /* Unicode code-point 8969 */
    {"lfloor", "\xE2\x8C\x8A", 0},      /* Unicode code-point 8970 */
    {"rfloor", "\xE2\x8C\x8B", 0},      /* Unicode code-point 8971 */
    {"lang", "\xE2\x8C\xA9", 0},        /* Unicode code-point 9001 */
    {"rang", "\xE2\x8C\xAA", 0},        /* Unicode code-point 9002 */
    {"loz", "\xE2\x97\x8A", 0},         /* Unicode code-point 9674 */
    {"spades", "\xE2\x99\xA0", 0},      /* Unicode code-point 9824 */
    {"clubs", "\xE2\x99\xA3", 0},       /* Unicode code-point 9827 */
    {"hearts", "\xE2\x99\xA5", 0},      /* Unicode code-point 9829 */
    {"diams", "\xE2\x99\xA6", 0},       /* Unicode code-point 9830 */
  
    /* Non-standard. But very common. */
    {"quote", "\"", 0},
    {"apos", "'", 0},
};

/* The size of the handler hash table.  For best results this should
** be a prime number which is about the same size as the number of
** escape sequences known to the system. */
#define ESC_HASH_SIZE (sizeof(esc_sequences)/sizeof(esc_sequences[0])+7)

/* The hash table 
**
** If the name of an escape sequences hashes to the value H, then
** apEscHash[H] will point to a linked list of Esc structures, one of
** which will be the Esc structure for that escape sequence.
*/
static struct sgEsc *apEscHash[ESC_HASH_SIZE];

/* Hash a escape sequence name.  The value returned is an integer
** between 0 and ESC_HASH_SIZE-1, inclusive.
*/
static int
EscHash(zName)
    const char *zName;
{
    int h = 0;                         /* The hash value to be returned */
    char c;                            /* The next character in the name
                                        * being hashed */

    while ((c = *zName) != 0) {
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    else {
    }
    return h % ESC_HASH_SIZE;
}

#ifdef TEST

/* 
** Compute the longest and average collision chain length for the
** escape sequence hash table
*/
static void
EscHashStats(void)
{
    int i;
    int sum = 0;
    int max = 0;
    int cnt;
    int notempty = 0;
    struct sgEsc *p;

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[0]); i++) {
        cnt = 0;
        p = apEscHash[i];
        if (p)
            notempty++;
        while (p) {
            cnt++;
            p = p->pNext;
        }
        sum += cnt;
        if (cnt > max)
            max = cnt;
    }
    printf("Longest chain=%d  avg=%g  slots=%d  empty=%d (%g%%)\n",
           max, (double) sum / (double) notempty, i, i - notempty,
           100.0 * (i - notempty) / (double) i);
}
#endif

/* Initialize the escape sequence hash table
*/
static void
EscInit()
{
    int i;                             /* For looping thru the list of escape 
                                        * sequences */
    int h;                             /* The hash on a sequence */

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[i]); i++) {

/* #ifdef TCL_UTF_MAX */
#if 0
        {
            int c = esc_sequences[i].value[0];
            Tcl_UniCharToUtf(c, esc_sequences[i].value);
        }
#endif
        h = EscHash(esc_sequences[i].zName);
        esc_sequences[i].pNext = apEscHash[h];
        apEscHash[h] = &esc_sequences[i];
    }
#ifdef TEST
    EscHashStats();
#endif
}

/*
** This table translates the non-standard microsoft characters between
** 0x80 and 0x9f into plain ASCII so that the characters will be visible
** on Unix systems.  Care is taken to translate the characters
** into values less than 0x80, to avoid UTF-8 problems.
*/
#ifndef __WIN32__
static char acMsChar[] = {
    /*
     * 0x80 
     */ 'C',
    /*
     * 0x81 
     */ ' ',
    /*
     * 0x82 
     */ ',',
    /*
     * 0x83 
     */ 'f',
    /*
     * 0x84 
     */ '"',
    /*
     * 0x85 
     */ '.',
    /*
     * 0x86 
     */ '*',
    /*
     * 0x87 
     */ '*',
    /*
     * 0x88 
     */ '^',
    /*
     * 0x89 
     */ '%',
    /*
     * 0x8a 
     */ 'S',
    /*
     * 0x8b 
     */ '<',
    /*
     * 0x8c 
     */ 'O',
    /*
     * 0x8d 
     */ ' ',
    /*
     * 0x8e 
     */ 'Z',
    /*
     * 0x8f 
     */ ' ',
    /*
     * 0x90 
     */ ' ',
    /*
     * 0x91 
     */ '\'',
    /*
     * 0x92 
     */ '\'',
    /*
     * 0x93 
     */ '"',
    /*
     * 0x94 
     */ '"',
    /*
     * 0x95 
     */ '*',
    /*
     * 0x96 
     */ '-',
    /*
     * 0x97 
     */ '-',
    /*
     * 0x98 
     */ '~',
    /*
     * 0x99 
     */ '@',
    /*
     * 0x9a 
     */ 's',
    /*
     * 0x9b 
     */ '>',
    /*
     * 0x9c 
     */ 'o',
    /*
     * 0x9d 
     */ ' ',
    /*
     * 0x9e 
     */ 'z',
    /*
     * 0x9f 
     */ 'Y',
};
#endif

static int 
translateNumericEscape(z, pI)
    char *z;
    int *pI;          /* IN/OUT: Index into string z */
{
    char *zLocal = &z[*pI];
    int base = 10;
    int iRet = 10;

    if (*zLocal == 'x' || *zLocal == 'X') {
        /* Hexadecimal number */
        base = 16;
        zLocal++;
    }

    iRet = strtol(zLocal, &zLocal, base);

    /* Gobble up a trailing semi-colon */
    if (*zLocal == ';') {
        zLocal++;
    }

    *pI = (zLocal - z);
    return iRet;
}

/* Translate escape sequences in the string "z".  "z" is overwritten
** with the translated sequence.
**
** Unrecognized escape sequences are unaltered.
**
** Example:
**
**      input =    "AT&amp;T &gt MCI"
**      output =   "AT&T > MCI"
*/
void
HtmlTranslateEscapes(z)
    char *z;
{
    int from;                          /* Read characters from this position
                                        * in z[] */
    int to;                            /* Write characters into this position 
                                        * in z[] */
    int h;                             /* A hash on the escape sequence */
    struct sgEsc *p;                   /* For looping down the escape
                                        * sequence collision chain */
    static int isInit = 0;             /* True after initialization */

    from = to = 0;
    if (!isInit) {
        EscInit();
        isInit = 1;
    }
    while (z[from]) {
        if (z[from] == '&') {
            if (z[from + 1] == '#') {
                int i = from + 2;
                int v = translateNumericEscape(z, &i);

                /*
                 * On Unix systems, translate the non-standard microsoft **
                 * characters in the range of 0x80 to 0x9f into something **
                 * we can see. 
                 */
#ifndef __WIN32__
                if (v >= 0x80 && v < 0xa0) {
                    v = acMsChar[v & 0x1f];
                }
#endif
                /*
                 * Put the character in the output stream in place of ** the
                 * "&#000;".  How we do this depends on whether or ** not we
                 * are using UTF-8. 
                 */
#ifdef TCL_UTF_MAX
                {
                    int j, n;
                    char value[8];
                    n = Tcl_UniCharToUtf(v, value);
                    for (j = 0; j < n; j++) {
                        z[to++] = value[j];
                    }
                }
#else
                z[to++] = v;
#endif
                from = i;
            }
            else {
                int i = from + 1;
                int c;
                while (z[i] && isalnum(z[i])) {
                    i++;
                }
                c = z[i];
                z[i] = 0;
                h = EscHash(&z[from + 1]);
                p = apEscHash[h];
                while (p && strcmp(p->zName, &z[from + 1]) != 0) {
                    p = p->pNext;
                }
                z[i] = c;
                if (p) {
                    int j;
                    for (j = 0; p->value[j]; j++) {
                        z[to++] = p->value[j];
                    }
                    from = i;
                    if (c == ';') {
                        from++;
                    }
                }
                else {
                    z[to++] = z[from++];
                }
            }

            /*
             * On UNIX systems, look for the non-standard microsoft
             * characters ** between 0x80 and 0x9f and translate them into
             * printable ASCII ** codes.  Separate algorithms are required to 
             * do this for plain ** ascii and for utf-8. 
             */
#ifndef __WIN32__
#ifdef TCL_UTF_MAX
        }
        else if ((z[from] & 0x80) != 0) {
            Tcl_UniChar c;
            int n;
            n = Tcl_UtfToUniChar(&z[from], &c);
            if (c >= 0x80 && c < 0xa0) {
                z[to++] = acMsChar[c & 0x1f];
                from += n;
            }
            else {
                while (n--)
                    z[to++] = z[from++];
            }
#else /* if !defined(TCL_UTF_MAX) */
        }
        else if (((unsigned char) z[from]) >= 0x80
                 && ((unsigned char) z[from]) < 0xa0) {
            z[to++] = acMsChar[z[from++] & 0x1f];
#endif /* TCL_UTF_MAX */
#endif /* __WIN32__ */
        }
        else {
            z[to++] = z[from++];
        }
    }
    z[to] = 0;
}

static HtmlWidgetTag *
getWidgetTag(pTree, zTag, pIsNew)
    HtmlTree *pTree;
    const char *zTag;
    int *pIsNew;
{
    Tcl_HashEntry *pEntry;
    int isNew;
    HtmlWidgetTag *pTag;

    pEntry = Tcl_CreateHashEntry(&pTree->aTag, zTag, &isNew);
    if (isNew) {
        Tk_OptionTable otab = pTree->tagOptionTable;
        static Tk_OptionSpec ospec[] = {
            {TK_OPTION_COLOR, "-foreground", "", "", "white", -1, \
             Tk_Offset(HtmlWidgetTag, foreground), 0, 0, 0},
            {TK_OPTION_COLOR, "-background", "", "", "black", -1, \
             Tk_Offset(HtmlWidgetTag, background), 0, 0, 0},

            {TK_OPTION_SYNONYM, "-bg", 0, 0, 0, 0, -1, 0, "-background", 0},
            {TK_OPTION_SYNONYM, "-fg", 0, 0, 0, 0, -1, 0, "-foreground", 0},

            {TK_OPTION_END, 0, 0, 0, 0, 0, 0, 0, 0}
        };
        pTag = (HtmlWidgetTag *)HtmlClearAlloc("", sizeof(HtmlWidgetTag));
        Tcl_SetHashValue(pEntry, pTag);
        if (0 == otab) {
            pTree->tagOptionTable = Tk_CreateOptionTable(pTree->interp, ospec);
            otab = pTree->tagOptionTable;
            assert(otab);
        }
        Tk_InitOptions(pTree->interp, (char *)pTag, otab, pTree->tkwin);
        assert(pTag->foreground && pTag->background);
    } else {
        pTag = (HtmlWidgetTag *)Tcl_GetHashValue(pEntry);
    }

    if (pIsNew) {
        *pIsNew = isNew;
    }
    return pTag;
}

static HtmlNode * 
orderIndexPair(ppA, piA, ppB, piB)
    HtmlNode **ppA;
    int *piA;
    HtmlNode **ppB;
    int *piB;
{
    HtmlNode *pA;
    HtmlNode *pB;
    HtmlNode *pParent;
    int nDepthA = 0;
    int nDepthB = 0;
    int ii;

    int swap = 0;

    for(pA = HtmlNodeParent(*ppA); pA; pA = HtmlNodeParent(pA)) nDepthA++;
    for(pB = HtmlNodeParent(*ppB); pB; pB = HtmlNodeParent(pB)) nDepthB++;

    pA = *ppA;
    pB = *ppB;
    for(ii = 0; ii < (nDepthA - nDepthB); ii++) pA = HtmlNodeParent(pA);
    for(ii = 0; ii < (nDepthB - nDepthA); ii++) pB = HtmlNodeParent(pB);

    if (pA == pB) {
        if (nDepthA == nDepthB) {
            /* In this case *ppA and *ppB are the same node */
            swap = (*piA > *piB);
        } else {
            /* One of (*ppA, *ppB) is a descendant of the other */
            swap = (nDepthA > nDepthB);
        }
        pParent = pA;
    } else {
        while (HtmlNodeParent(pA) != HtmlNodeParent(pB)) {
            pA = HtmlNodeParent(pA);
            pB = HtmlNodeParent(pB);
            assert(pA && pB && pA != pB);
        }
        pParent = HtmlNodeParent(pA);
        for (ii = 0; ; ii++) {
            HtmlNode *pChild = HtmlNodeChild(pParent, ii);
            assert(ii < HtmlNodeNumChildren(pParent) && pChild);
            if (pChild == pA) break;
            if (pChild == pB) {
                swap = 1;
                break;
            }
        }
    }

    if (swap) {
        HtmlNode *p;
        int i;
        p = *ppB;
        *ppB = *ppA;
        *ppA = p;
        i = *piB;
        *piB = *piA;
        *piA = i;
    }

    return pParent;
}

/*
 *---------------------------------------------------------------------------
 *
 * removeTagFromNode --
 * 
 * Results:
 *     Returns non-zero if one or more characters of this node were tagged.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
removeTagFromNode(pTextNode, pTag)
    HtmlTextNode *pTextNode;
    HtmlWidgetTag *pTag;
{
    int eRet = 0;
    HtmlTaggedRegion *pTagged = pTextNode->pTagged;
    if (pTagged) { 
        HtmlTaggedRegion **pPtr = &pTextNode->pTagged;
        
        while (pTagged) {
            if (pTagged->pTag == pTag) {
                *pPtr = pTagged->pNext;
                HtmlFree(pTagged);
                eRet = 1;
            } else {
                pPtr = &pTagged->pNext;
            }
            pTagged = *pPtr;
        }
    }

#ifndef NDEBUG
    for (pTagged = pTextNode->pTagged; pTagged ; pTagged = pTagged->pNext) {
        assert(pTagged->pTag != pTag);
    }
#endif

    return eRet;
}

static HtmlTaggedRegion *
findTagInNode(pTextNode, pTag, ppPtr)
    HtmlTextNode *pTextNode;
    HtmlWidgetTag *pTag;
    HtmlTaggedRegion ***ppPtr;
{
    HtmlTaggedRegion *pTagged;
    HtmlTaggedRegion **pPtr = &pTextNode->pTagged;
    for (pTagged = pTextNode->pTagged; pTagged; pTagged = pTagged->pNext) {
        if (pTagged->pTag == pTag) {
            *ppPtr = pPtr;
            return pTagged;
        }
        pPtr = &pTagged->pNext;
    }
    *ppPtr = pPtr;
    return 0;
}

typedef struct TagOpData TagOpData;
struct TagOpData {
    HtmlNode *pFrom;
    int iFrom;
    HtmlNode *pTo;
    int iTo;
    int eSeenFrom;          /* True after pFrom has been traversed */
    HtmlWidgetTag *pTag;

    int isAdd;              /* True for [add] false for [remove] */

    HtmlNode *pFirst;
    HtmlNode *pLast;
    int iFirst;
    int iLast;
};

#define OVERLAP_NONE     1
#define OVERLAP_SUPER    2
#define OVERLAP_SUB      3
#define OVERLAP_FROM     4
#define OVERLAP_TO       5
#define OVERLAP_EXACT    6
static int
getOverlap(pTagged, iFrom, iTo)
    HtmlTaggedRegion *pTagged;
    int iFrom;
    int iTo;
{
    assert(iFrom <= iTo);
    assert(pTagged->iFrom <= pTagged->iTo);

    if (iFrom == pTagged->iFrom && iTo == pTagged->iTo) {
        return OVERLAP_EXACT;
    }
    if (iFrom <= pTagged->iFrom && iTo >= pTagged->iTo) {
        return OVERLAP_SUPER;
    }
    if (iFrom >= pTagged->iFrom && iTo <= pTagged->iTo) {
        return OVERLAP_SUB;
    }
    if (iFrom > pTagged->iTo || iTo < pTagged->iFrom) {
        return OVERLAP_NONE;
    }
    if (iFrom > pTagged->iFrom) {
        assert(iFrom <= pTagged->iTo);
        assert(iTo > pTagged->iTo);
        return OVERLAP_TO;
    }
    assert(iTo >= pTagged->iFrom);
    assert(iTo < pTagged->iTo);
    assert(iFrom < pTagged->iFrom);
    return OVERLAP_FROM;
}


static int
tagAddRemoveCallback(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    TagOpData *pData = (TagOpData *)clientData;
    HtmlTextNode *pTextNode = HtmlNodeAsText(pNode);

    if (pNode == pData->pFrom) {
        assert(0 == pData->eSeenFrom);
        pData->eSeenFrom = 1;
    }

    if (pTextNode && pData->eSeenFrom) {
        HtmlTaggedRegion *pTagged;
        HtmlTaggedRegion **pPtr;
        int iFrom = 0;
        int iTo = 1000000;
        if (pNode == pData->pFrom) iFrom = pData->iFrom;
        if (pNode == pData->pTo) iTo = pData->iTo;

        assert(iFrom <= iTo);

        pTagged = findTagInNode(pTextNode, pData->pTag, &pPtr);
        assert(*pPtr == pTagged);

        switch (pData->isAdd) {
            case HTML_TAG_ADD:
                while (pTagged && pTagged->pTag == pData->pTag) {
                    int e = getOverlap(pTagged, iFrom, iTo);
                    pPtr = &pTagged->pNext;
                    if (e != OVERLAP_NONE) {
                        if (0 == pData->pFirst) {
                            if (e == OVERLAP_SUPER || e == OVERLAP_FROM) {
                                pData->pFirst = pNode;
                                pData->iFirst = iFrom;
                            } else if (e == OVERLAP_TO) {
                                pData->pFirst = pNode;
                                pData->iFirst = pTagged->iTo;
                            }
                        }
                        if (e == OVERLAP_TO || e == OVERLAP_SUPER) {
                            pData->pLast = pNode;
                            pData->iLast = iTo;
                        } if (e == OVERLAP_FROM) {
                            pData->pLast = pNode;
                            pData->iLast = pTagged->iFrom;
                        }
                        pTagged->iFrom = MIN(pTagged->iFrom, iFrom);
                        pTagged->iTo = MAX(pTagged->iTo, iTo);
                        break;
                    }
                    pTagged = *pPtr;
                }
                if (!pTagged || pTagged->pTag != pData->pTag) {
                    HtmlTaggedRegion *pNew = (HtmlTaggedRegion *)
                        HtmlClearAlloc("", sizeof(HtmlTaggedRegion));
                    pNew->iFrom = iFrom;
                    pNew->iTo = iTo;
                    pNew->pNext = pTagged;
                    pNew->pTag = pData->pTag;

                    if (!pData->pFirst) {
                        pData->pFirst = pNode;
                        pData->iFirst = iFrom;
                    }
                    pData->pLast = pNode;
                    pData->iLast = iTo;

                    *pPtr = pNew;
                }

                break;

            case HTML_TAG_REMOVE:
                while (pTagged && pTagged->pTag == pData->pTag) {
                    int eOverlap = getOverlap(pTagged, iFrom, iTo);

                    switch (eOverlap) {
                        case OVERLAP_EXACT:
                        case OVERLAP_SUPER: {
                            /* Delete the whole list entry */
                            *pPtr = pTagged->pNext;
                            HtmlFree(pTagged);
                            break;
                        };
                            
                        case OVERLAP_TO:
                            pTagged->iTo = iFrom;
                            pPtr = &pTagged->pNext;
                            break;
                        case OVERLAP_FROM:
                            pTagged->iFrom = iTo;
                            pPtr = &pTagged->pNext;
                            break;

                        case OVERLAP_NONE:
                            /* Do nothing */
                            pPtr = &pTagged->pNext;
                            break;

                        case OVERLAP_SUB: {
                            HtmlTaggedRegion *pNew = (HtmlTaggedRegion *)
                                HtmlClearAlloc("", sizeof(HtmlTaggedRegion));
                            pNew->iFrom = iTo;
                            pNew->iTo = pTagged->iTo;
                            pNew->pTag = pData->pTag;
                            pNew->pNext = pTagged->pNext;
                            pTagged->pNext = pNew;
                            pTagged->iTo = iFrom;
                            pPtr = &pNew->pNext;
                            break;
                        }
                    }
                    pTagged = *pPtr;
                }
                break;
        }
    }

    if (pNode == pData->pTo) {
        return HTML_WALK_ABANDON;
    }
    return HTML_WALK_DESCEND;
}

int 
HtmlTagAddRemoveCmd(clientData, interp, objc, objv, isAdd)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
    int isAdd;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlNode *pParent;
    HtmlWidgetTag *pTag;

    TagOpData sData;
    memset(&sData, 0, sizeof(TagOpData));

    assert(isAdd == HTML_TAG_REMOVE || isAdd == HTML_TAG_ADD);

    if (objc != 8) {
        Tcl_WrongNumArgs(interp, 3, objv, 
            "TAGNAME FROM-NODE FROM-INDEX TO-NODE TO-INDEX"
        );
        return TCL_ERROR;
    }
    if (
        0 == (sData.pFrom=HtmlNodeGetPointer(pTree, Tcl_GetString(objv[4]))) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[5], &sData.iFrom) ||
        0 == (sData.pTo=HtmlNodeGetPointer(pTree, Tcl_GetString(objv[6]))) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[7], &sData.iTo)
    ) {
        return TCL_ERROR;
    }

    /* If either node is an orphan node, throw a Tcl exception. */
    if (HtmlNodeIsOrphan(sData.pFrom)) {
        Tcl_AppendResult(interp, Tcl_GetString(objv[4]), " is an orphan", 0);
        return TCL_ERROR;
    }
    if (HtmlNodeIsOrphan(sData.pTo)) {
        Tcl_AppendResult(interp, Tcl_GetString(objv[6]), " is an orphan", 0);
        return TCL_ERROR;
    }

    pTag = getWidgetTag(pTree, Tcl_GetString(objv[3]), 0);
    sData.pTag = pTag;
    sData.isAdd = isAdd;

    pParent = orderIndexPair(&sData.pFrom,&sData.iFrom,&sData.pTo,&sData.iTo);
    HtmlWalkTree(pTree, pParent, tagAddRemoveCallback, &sData);

    if (isAdd == HTML_TAG_REMOVE) {
        HtmlWidgetDamageText(pTree, 
            sData.pFrom, sData.iFrom,
            sData.pTo, sData.iTo
        );
    } else if (sData.pFirst) {
        assert(sData.pLast);
        HtmlWidgetDamageText(pTree, 
            sData.pFirst, sData.iFirst,
            sData.pLast, sData.iLast
        );
    }

    return TCL_OK;
}

int 
HtmlTagConfigureCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tk_OptionTable otab;
    HtmlWidgetTag *pTag;
    Tk_Window win = pTree->tkwin;
    int isNew;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAGNAME ?options?");
        return TCL_ERROR;
    }

    pTag = getWidgetTag(pTree, Tcl_GetString(objv[3]), &isNew);
    otab = pTree->tagOptionTable;
    assert(otab);
    Tk_SetOptions(interp, (char *)pTag, otab, objc - 4, &objv[4], win, 0, 0);

    if (!isNew) {
        /* Redraw the whole viewport. Todo: Update only the tagged regions */
        HtmlCallbackDamage(pTree, 0, 0, 1000000, 1000000);
    }

    return TCL_OK;
}

struct TagDeleteContext {
    HtmlWidgetTag *pTag; 
    int nOcc;
};
typedef struct TagDeleteContext TagDeleteContext;

static int
tagDeleteCallback(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlTextNode *pTextNode = HtmlNodeAsText(pNode);
    if (pTextNode) {
        TagDeleteContext *p = (TagDeleteContext *)clientData;
        p->nOcc += removeTagFromNode(pTextNode, p->pTag);
    }
    return HTML_WALK_DESCEND;
}

int 
HtmlTagDeleteCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    const char *zTag;
    Tcl_HashEntry *pEntry;
    HtmlTree *pTree = (HtmlTree *)clientData;
    TagDeleteContext context = {0, 0};

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAGNAME");
        return TCL_ERROR;
    }

    zTag = Tcl_GetString(objv[3]);
    pEntry = Tcl_FindHashEntry(&pTree->aTag, zTag);
    if (pEntry) {
        HtmlWidgetTag *pTag = (HtmlWidgetTag *)Tcl_GetHashValue(pEntry);
        context.pTag = pTag;
        HtmlWalkTree(pTree, 0, tagDeleteCallback, (ClientData)&context);
        HtmlFree(pTag);
        Tcl_DeleteHashEntry(pEntry);
    }

    /* Redraw the whole viewport. Todo: Update only the required regions */
    if (context.nOcc) {
        HtmlCallbackDamage(pTree, 0, 0, 1000000, 1000000);
    }

    return TCL_OK;
}

void
HtmlTagCleanupNode(pTextNode)
    HtmlTextNode *pTextNode;
{
    HtmlTaggedRegion *pTagged = pTextNode->pTagged;
    while (pTagged) {
        HtmlTaggedRegion *pNext = pTagged->pNext;
        HtmlFree(pTagged);
        pTagged = pNext;
    }
    pTextNode->pTagged = 0;
}

void
HtmlTagCleanupTree(pTree)
    HtmlTree *pTree;
{
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;
    pEntry = Tcl_FirstHashEntry(&pTree->aTag, &search);
    for ( ; pEntry; pEntry = Tcl_NextHashEntry(&search)) {
        HtmlWidgetTag *pTag = (HtmlWidgetTag *)Tcl_GetHashValue(pEntry);
        Tk_FreeConfigOptions((char *)pTag, pTree->tagOptionTable, pTree->tkwin);
        HtmlFree(pTag);
    }
    Tcl_DeleteHashTable(&pTree->aTag);
}


/*
 * The following two structs are used together to create a data-structure 
 * to store the text-representation of the document.
 *
 */
typedef struct HtmlTextMapping HtmlTextMapping;
struct HtmlTextMapping {
    HtmlTextNode *pTextNode;
    int iStrIndex;             /* Character offset in HtmlText.pObj */
    int iNodeIndex;            /* Byte offset in HtmlTextNode.zText */
    HtmlTextMapping *pNext;
};
struct HtmlText {
    Tcl_Obj *pObj;
    HtmlTextMapping *pMapping;
};

typedef struct HtmlTextInit HtmlTextInit;
struct HtmlTextInit {
    HtmlText *pText;
    int eState;
    int iIdx;
};

#define SEEN_TEXT  0
#define SEEN_SPACE 1
#define SEEN_BLOCK 2

static void
addTextMapping(pText, pTextNode, iNodeIndex, iStrIndex)
    HtmlText *pText;
    HtmlTextNode *pTextNode;
    int iNodeIndex;
    int iStrIndex;
{
    HtmlTextMapping *p;
    p = (HtmlTextMapping *)HtmlAlloc("HtmlTextMapping",sizeof(HtmlTextMapping));
    p->iStrIndex = iStrIndex;
    p->iNodeIndex = iNodeIndex;
    p->pTextNode = pTextNode;
    p->pNext = pText->pMapping;
    pText->pMapping = p;
}

static void
initHtmlText_TextNode(pTree, pTextNode, pInit)
    HtmlTree *pTree;
    HtmlTextNode *pTextNode;
    HtmlTextInit *pInit;
{
    HtmlNode *pNode = &pTextNode->node;
    int isPre = (HtmlNodeComputedValues(pNode)->eWhitespace == CSS_CONST_PRE);

    HtmlTextIter sIter;

    if (pInit->eState == SEEN_BLOCK) {
        Tcl_AppendToObj(pInit->pText->pObj, "\n", 1);
        pInit->iIdx++;
    }
    for (
        HtmlTextIterFirst(pTextNode, &sIter);
        HtmlTextIterIsValid(&sIter);
        HtmlTextIterNext(&sIter)
    ) {
        int eType = HtmlTextIterType(&sIter);
        int nData = HtmlTextIterLength(&sIter);
        char const * zData = HtmlTextIterData(&sIter);

        switch (eType) {
            case HTML_TEXT_TOKEN_NEWLINE:
            case HTML_TEXT_TOKEN_SPACE:
                if (isPre) {
                    int ii;
                    const char *zWhite;
                    zWhite = (eType==HTML_TEXT_TOKEN_SPACE ? " " : "\n");
                    for (ii = 0; ii < nData; ii++) {
                        Tcl_AppendToObj(pInit->pText->pObj, zWhite, 1);
                    }
                    pInit->iIdx += nData;
                    pInit->eState = SEEN_TEXT;
                } else {
                    pInit->eState = MAX(pInit->eState, SEEN_SPACE);
                }
                break;

            case HTML_TEXT_TOKEN_TEXT:
                if (pInit->iIdx > 0 && pInit->eState == SEEN_SPACE) {
                    Tcl_AppendToObj(pInit->pText->pObj, " ", 1);
                    pInit->iIdx++;
                }

                addTextMapping(pTree->pText, 
                    pTextNode, (zData - pTextNode->zText), pInit->iIdx
                );
                Tcl_AppendToObj(pInit->pText->pObj, zData, nData);
                pInit->eState = SEEN_TEXT;
                assert(nData >= 0);
                pInit->iIdx += Tcl_NumUtfChars(zData, nData);
                break;

            default:
                assert(!"Bad return value from HtmlTextIterType()");
        }
    }
}

static void
initHtmlText_Elem(pTree, pElem, pInit)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
    HtmlTextInit *pInit;
{
    HtmlNode *pNode = &pElem->node;
    int eDisplay = HtmlNodeComputedValues(pNode)->eDisplay; 
    int ii;

    /* If the element has "display:none" or a replacement window, do
     * not consider any text children to be part of the text
     * rendering of the document.
     */
    if (
        (eDisplay == CSS_CONST_NONE) ||
        (pElem->pReplacement && pElem->pReplacement->win)
    ) {
        return;
    }

    if (eDisplay != CSS_CONST_INLINE) {
        pInit->eState = SEEN_BLOCK;
    }

    for (ii = 0; ii < HtmlNodeNumChildren(pNode); ii++) {
        HtmlNode *p = HtmlNodeChild(pNode, ii);
        if (HtmlNodeIsText(p)) {
            initHtmlText_TextNode(pTree, HtmlNodeAsText(p), pInit);
        } else {
            initHtmlText_Elem(pTree, HtmlNodeAsElement(p), pInit);
        }
    }

    if (eDisplay != CSS_CONST_INLINE) {
        pInit->eState = SEEN_BLOCK;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * initHtmlText --
 * 
 *     This function is called to initialise the HtmlText data structure 
 *     at HtmlTree.pText. If the data structure is already initialised
 *     this function is a no-op.
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
initHtmlText(pTree)
    HtmlTree *pTree;
{
    if (!pTree->pText) {
        HtmlTextInit sInit;
        HtmlCallbackForce(pTree);
        pTree->pText = (HtmlText *)HtmlClearAlloc(0, sizeof(HtmlText));
        memset(&sInit, 0, sizeof(HtmlTextInit));
        sInit.pText = pTree->pText;
        sInit.pText->pObj = Tcl_NewObj();
        Tcl_IncrRefCount(sInit.pText->pObj);
        initHtmlText_Elem(pTree, HtmlNodeAsElement(pTree->pRoot), &sInit);
        Tcl_AppendToObj(sInit.pText->pObj, "\n", 1);
    }
}

void 
HtmlTextInvalidate(pTree)
    HtmlTree *pTree;
{
    if (pTree->pText) {
        HtmlText *pText = pTree->pText;
        HtmlTextMapping *pMapping = pText->pMapping;

        Tcl_DecrRefCount(pTree->pText->pObj);
        while (pMapping) {
            HtmlTextMapping *pNext = pMapping->pNext;
            HtmlFree(pMapping);
            pMapping = pNext;
        }
        HtmlFree(pTree->pText);
        pTree->pText = 0;
    }
}

int
HtmlTextTextCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 3, objv, "");
        return TCL_ERROR;
    }
    initHtmlText(pTree);
    Tcl_SetObjResult(interp, pTree->pText->pObj);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTextIndexCmd --
 * 
 *         $html text index OFFSET ?OFFSET? ?OFFSET?
 *
 *     The argument $OFFSET is an offset into the string returned
 *     by [$html text text]. This Tcl command returns a list of two
 *     elements - the node and node index corresponding to the 
 *     equivalent point in the document tree.
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
HtmlTextIndexCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int ii;
    Tcl_Obj *p = Tcl_NewObj();

    HtmlTextMapping *pMap = 0;
    int iPrev = 0;
 
    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "OFFSET ?OFFSET? ...");
        return TCL_ERROR;
    }

    initHtmlText(pTree);
    for (ii = objc-1; ii >= 3; ii--) {
        int iIndex;
        if (Tcl_GetIntFromObj(interp, objv[ii], &iIndex)) {
            return TCL_ERROR;
        }
        if (pMap == 0 || iIndex > iPrev) {
            pMap = pTree->pText->pMapping;
        }
        for ( ; pMap; pMap = pMap->pNext) {
            assert(!pMap->pNext || pMap->iStrIndex >= pMap->pNext->iStrIndex);
            if (pMap->iStrIndex <= iIndex || !pMap->pNext) {
                int iNodeIdx = pMap->iNodeIndex; 
                Tcl_Obj *apObj[2];

                int nExtra = iIndex - pMap->iStrIndex;
                char *zExtra = &(pMap->pTextNode->zText[iNodeIdx]);
                iNodeIdx += (Tcl_UtfAtIndex(zExtra, nExtra) - zExtra);

                apObj[0] = HtmlNodeCommand(pTree, &pMap->pTextNode->node);
                apObj[1] = Tcl_NewIntObj(iNodeIdx);
                Tcl_ListObjReplace(0, p, 0, 0, 2, apObj);
                break;
            }
        }
        iPrev = iIndex;
    }

    Tcl_SetObjResult(interp, p);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTextOffsetCmd --
 * 
 *         $html text index NODE INDEX
 *
 *     Given the supplied node/index pair, return the corresponding offset
 *     in the text representation of the document.
 * 
 *     The INDEX parameter is in bytes. The returned value is in characters.
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
HtmlTextOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTextMapping *pMap;

    /* C interpretations of arguments passed to the Tcl command */
    HtmlNode *pNode;
    HtmlTextNode *pTextNode;
    int iIndex;

    /* Return value for the Tcl command. Anything less than 0 results
     * in an empty string being returned.
     */
    int iRet = -1;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 3, objv, "NODE INDEX");
        return TCL_ERROR;
    }
    if (
        0 == (pNode = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[3]))) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &iIndex)
    ) {
        return TCL_ERROR;
    }
    if (!(pTextNode = HtmlNodeAsText(pNode))) {
        const char *zNode = Tcl_GetString(objv[3]);
        Tcl_AppendResult(interp, zNode, " is not a text node", 0);
        return TCL_ERROR;
    }

    initHtmlText(pTree);
    for (pMap = pTree->pText->pMapping; pMap; pMap = pMap->pNext) {
        if (pMap->pTextNode == pTextNode && pMap->iNodeIndex <= iIndex) {
            char *zExtra = &pTextNode->zText[pMap->iNodeIndex];
            int nExtra = iIndex - pMap->iNodeIndex;
            iRet = pMap->iStrIndex + Tcl_NumUtfChars(zExtra, nExtra);
            break;
        }
    }

    if (iRet >= 0) {
        Tcl_SetObjResult(interp, Tcl_NewIntObj(iRet));
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTextBboxCmd --
 * 
 *         $html text bbox NODE1 INDEX1 NODE2 INDEX2
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
HtmlTextBboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlNode *pFrom;
    HtmlNode *pTo;
    int iFrom;
    int iTo;

    int iTop, iLeft, iBottom, iRight;

    if (objc != 7) {
        Tcl_WrongNumArgs(interp, 3, objv, 
            "FROM-NODE FROM-INDEX TO-NODE TO-INDEX"
        );
        return TCL_ERROR;
    }
    if (
        0 == (pFrom=HtmlNodeGetPointer(pTree, Tcl_GetString(objv[3]))) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &iFrom) ||
        0 == (pTo=HtmlNodeGetPointer(pTree, Tcl_GetString(objv[5]))) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[6], &iTo)
    ) {
        return TCL_ERROR;
    }
    orderIndexPair(&pFrom, &iFrom, &pTo, &iTo);

    HtmlWidgetBboxText(pTree, pFrom, iFrom, pTo, iTo, 
        &iTop, &iLeft, &iBottom, &iRight
    );
    if (iTop < iBottom && iLeft < iRight) {
        Tcl_Obj *pRes = Tcl_NewObj();
        Tcl_ListObjAppendElement(0, pRes, Tcl_NewIntObj(iLeft));
        Tcl_ListObjAppendElement(0, pRes, Tcl_NewIntObj(iTop));
        Tcl_ListObjAppendElement(0, pRes, Tcl_NewIntObj(iRight));
        Tcl_ListObjAppendElement(0, pRes, Tcl_NewIntObj(iBottom));
        Tcl_SetObjResult(interp, pRes);
    }

    return TCL_OK;
}

/*
 * NOTES ON INTERNALS OF HtmlTextNode:
 * 
 *     The internals of the HtmlTextNode structure should be accessed 
 *     via the following API:
 *
 *         HtmlTextNew()
 *         HtmlTextFree()
 *
 *         HtmlTextIterFirst
 *         HtmlTextIterIsValid
 *         HtmlTextIterNext
 *
 *         HtmlTextIterType
 *         HtmlTextIterLength
 *         HtmlTextIterData
 *
 *         HtmlNodeIsWhitespace
 *
 *         HtmlTextCombine()
 *     
 *     An HtmlTextNode object stores it's text in two parts:
 *
 *         * A character string with SGML escape sequences replaced their
 *           UTF-8 equivalents and each block of whitespace replaced by
 *           a single space character.
 *
 *         * An ordered list of tokens. Each token one of the following:
 *           + An unwrappable block of text (i.e. a word),
 *           + One or more newline characters, or
 *           + One or more space characters.
 *
 *           Each token is represented by an instance of struct 
 *           HtmlTextToken. It has 1 byte type and a 1 byte unsigned 
 *           length indicating the number of bytes, spaces or 
 *           newlines represented by the token.
 *
 *     The following block of text:
 *
 *         "abcde   <NEWLINE>&gt;&lt;<NEWLINE><NEWLINE>hello"
 *
 *     Is represented by the data structures:
 *
 *         HtmlTextNode.aToken = {
 *              {TEXT, 5}, {SPACE, 3}, {NEWLINE, 1}, 
 *              {TEXT, 2}, {NEWLINE 2},
 *              {TEXT, 5},
 *              {END, <unused>},
 *         }
 *         HtmlTextNode.zText  = "abcde <> hello"
 *
 *     Storing the text in this format means that we are well-placed
 *     to rationalise a good percentage calls to Tk_DrawChars() etc. in 
 *     other parts of the widget code.
 *
 *     Blocks of contiguous space or newline characters longer than 255
 *     characters (the maximum value of HtmlTextToken.n) are stored
 *     as two or more contiguous SPACE tokens. i.e. 650 contiguous spaces
 *     require the following three tokens in the aToken array:
 *
 *         {SPACE, 255}, {SPACE, 255}, {SPACE, 140}
 *
 *     Blocks of non-whitespace longer than 255 characters are trickier.
 *     They always require exactly three tokens to be added to the array.
 *     For a 650 byte block of text the following three tokens:
 *
 *         {LONGTEXT, {650 >> 16} & 0xFF}, 
 *         {LONGTEXT, {650 >>  8} & 0xFF}, 
 *         {LONGTEXT, {650 >>  0} & 0xFF}
 *
 *     If a node consists entirely of white-space, then the HtmlTextNode.zText
 *     string is zero bytes in length. In this case HtmlTextNode.zText is
 *     set to NULL, to speed up checking if the node consists entirely
 *     of whitespace.
 *
 *     Todo: It's tempting to use single byte tokens, instead of two. Three
 *     bits for the type and five for the length. On the other hand,
 *     premature optimization.....
 */
struct HtmlTextToken {
    unsigned char n;
    unsigned char eType;
};

/* Return true if the argument is a unicode codpoint that should be handled
 * as a 'cjk' character.
 */
#define ISCJK(ii) ((ii)>=0x3000 && (ii)<=0x9fff)

/*
 *---------------------------------------------------------------------------
 *
 * utf8Read --
 * 
 *     Decode a single UTF8 character from the buffer pointed to by z.
 *
 * Results:
 *     Unicode codepoint of read character, or 0 if the end of the buffer
 *     has been reached.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_UniChar utf8Read(
  const unsigned char *z,         /* first byte of utf-8 character */
  const unsigned char *zTerm,     /* pretend this byte is 0x00 */
  const unsigned char **pzNext    /* write first byte past utf-8 char here */
){
    static const unsigned char UtfTrans[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
    };

    unsigned int c = 0;
    if (zTerm>z) {
        c = (unsigned int)*z;
        if ((c&0xC0)==0xC0) {
            const unsigned char *zCsr = &z[1];
            c = UtfTrans[c-0xC0];
            while (zCsr!=zTerm && ((*zCsr)&0xC0)==0x80){
                c = (c << 6) + ((*zCsr)&0x3F);
                zCsr++;
            }
            *pzNext = zCsr;
        } else {
            *pzNext = &z[1];
        }
    } else {
      *pzNext = zTerm;
    }
  
    return c;
}

/*
 *---------------------------------------------------------------------------
 *
 * tokenLength --
 * 
 *     Argument zToken points at the start of a text token (a non-whitespace
 *     character). This function returns the number of bytes in the token.
 *     zEnd points to 1 byte passed the end of the buffer - reading *zEnd
 *     would be a seg-fault.
 *
 * Results:
 *     Length of token at zToken in bytes.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tokenLength(zToken, zEnd)
    const unsigned char *zToken;
    const unsigned char *zEnd;
{
    Tcl_UniChar iChar = 0;
    const unsigned char *zCsr = zToken;
    const unsigned char *zNext = zCsr;

    iChar = utf8Read(zCsr, zEnd, &zNext);
    while (iChar && (iChar >= 256 || !ISSPACE(iChar)) && !ISCJK(iChar)){
      zCsr = zNext;
      iChar = utf8Read(zCsr, zEnd, &zNext);
    }

    return ((zCsr==zToken)?zNext:zCsr)-zToken;
}

/*
 *---------------------------------------------------------------------------
 *
 * populateTextNode --
 * 
 *     This function is called to tokenize a block of document text into 
 *     an HtmlTextNode structure. It is a helper function for HtmlTextNew().
 *
 *     This function is designed to be called twice for each block of text
 *     parsed to an HtmlTextNode. The first time, it determines the space
 *     (number of tokens and number of bytes of text) required for the
 *     the new HtmlTextNode. The caller then allocates this space and
 *     calls the function a second time to populate the new structure.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static void
populateTextNode(n, z, pText, pnToken, pnText)
    int n;                     /* Length of input text */
    char const *z;             /* Input text */
    HtmlTextNode *pText;       /* OUT: The structure to populate (or NULL) */
    int *pnToken;              /* OUT: Number of tokens required (or used) */
    int *pnText;               /* OUT: Bytes of text required (or used) */
{
    char const *zCsr = z;
    char const *zStop = &z[n];

    /* A running count of the number of tokens and bytes of text storage
     * required to store the parsed form of the input text.
     */
    int nToken = 0;
    int nText = 0;

    /* This variable is used to expand tabs. */
    int iCol = 0;

    int isPrevTokenText = 0;

    while (zCsr < zStop) {
        unsigned char c = (int)(*zCsr);
        char const *zStart = zCsr;

        if (ISSPACE(c)) {

            int nSpace = 0;                    /* Eventual token length */
            int eType = HTML_TEXT_TOKEN_SPACE; /* Eventual token type */

            if (ISNEWLINE(c)) {
                eType = HTML_TEXT_TOKEN_NEWLINE;
            }

            do {
                /* If a tab character, this is equivalent to adding between
                 * one and eight spaces, depending on the current value of
                 * variable iCol (see comments above variable declaration). 
                 */
                if (ISTAB(*zCsr)) nSpace += (7 - (iCol%8));

                /* The sequence CR (carraige return) followed by LF (line 
                 * feed) counts as a single newline. If either is encountered
                 * alone, this is also a single newline. LF followed by CR
                 * is two newlines.
                 */
                if (zCsr[0] == '\r' && &zCsr[1] < zStop && zCsr[1] == '\n') {
                    zCsr++;
                }

                nSpace++;
                zCsr++;
                iCol += nSpace;
            } while (
                zCsr < zStop && nSpace < (255 - 8) && ISSPACE(*zCsr) && (
                    (eType == HTML_TEXT_TOKEN_NEWLINE && ISNEWLINE(*zCsr)) ||
                    (eType == HTML_TEXT_TOKEN_SPACE && !ISNEWLINE(*zCsr))
                )
            );

            if (eType == HTML_TEXT_TOKEN_NEWLINE) {
                iCol = 0;
            }

            assert(nSpace <= 255);
            if (pText) {
                pText->aToken[nToken].n = nSpace;
                pText->aToken[nToken].eType = eType;
            }
            nToken++;

            /* If the previous token was text, add a single space character
             * to the HtmlTextNode.zText buffer. Otherwise, add nothing
             * to the text buffer.
             */
            if (isPrevTokenText) {
                if (pText) {
                    pText->zText[nText] = ' ';
                }
                nText++;
                isPrevTokenText = 0;
            }
        } else {

            /* This block sets nThisText to the number of bytes (not 
             * characters) in the token starting at zStart. zCsr is
             * left pointing to the byte immediately after the token.
             */
            int nThisText = tokenLength(
                (unsigned char *)zCsr, (unsigned char *)zStop
            );
            assert(zCsr == zStart);
            assert(nThisText>0);
            zCsr = &zCsr[nThisText];

            if (nThisText > 255) {
                if (pText) {
                    pText->aToken[nToken].eType = HTML_TEXT_TOKEN_LONGTEXT;
                    pText->aToken[nToken+1].eType = HTML_TEXT_TOKEN_LONGTEXT;
                    pText->aToken[nToken+2].eType = HTML_TEXT_TOKEN_LONGTEXT;
                    pText->aToken[nToken].n = ((nThisText >> 16) & 0x000000FF);
                    pText->aToken[nToken+1].n = ((nThisText >> 8) & 0x000000FF);
                    pText->aToken[nToken+2].n = (nThisText & 0x000000FF);
                    memcpy(&pText->zText[nText], zStart, nThisText);
                }
                nToken += 3;
            } else {
                if (pText) {
                    pText->aToken[nToken].eType = HTML_TEXT_TOKEN_TEXT;
                    pText->aToken[nToken].n = nThisText;
                    memcpy(&pText->zText[nText], zStart, nThisText);
                }
                nToken++;
            }

            nText += nThisText;
            isPrevTokenText = 1;
            iCol += nThisText;
        }
    }

    /* Add the terminator token */
    if (pText) {
        pText->aToken[nToken].eType = HTML_TEXT_TOKEN_END;
    }
    nToken++;

    if (pnToken) *pnToken = nToken;
    if (pnText) *pnText = nText;
}

void
HtmlTextSet(pText, n, z, isTrimEnd, isTrimStart)
    HtmlTextNode *pText;
    int n;
    const char *z;
    int isTrimEnd;
    int isTrimStart;
{
    char *z2;
    HtmlTextToken *pFinal;

    int nText = 0;
    int nToken = 0;
    int nAlloc;                /* Number of bytes allocated */

    if (pText->aToken) {
        HtmlFree(pText->aToken);
    }

    /* Make a temporary copy of the text and translate any embedded html 
     * escape characters (i.e. "&nbsp;"). Todo: Avoid this copy by changing
     * populateTextNode() so that it deals with escapes.
     */
    z2 = (char *)HtmlAlloc("temp", n + 1);
    memcpy(z2, z, n);
    z2[n] = '\0';
    HtmlTranslateEscapes(z2);

    /* Figure out how much space is required for this HtmlTextNode. */
    populateTextNode(strlen(z2), z2, 0, &nToken, &nText);
    assert(nText >= 0 && nToken > 0);

    /* Allocate space for HtmlTextNode.aToken and HtmlTextNode.zText */
    nAlloc = nText + (nToken * sizeof(HtmlTextToken));
    pText->aToken = (HtmlTextToken *)HtmlClearAlloc("TextNode.aToken", nAlloc);
    if (nText > 0) {
        pText->zText = (char *)&pText->aToken[nToken];
    } else {
        /* If the node is all white-space, set HtmlTextNode.zText to NULL */
        pText->zText = 0;
    }

    /* Populate the HtmlTextNode.aToken and zText arrays. */
    populateTextNode(strlen(z2), z2, pText, 0, 0);
    HtmlFree(z2);

    assert(pText->aToken[nToken-1].eType == HTML_TEXT_TOKEN_END);
    pFinal = &pText->aToken[nToken-2];
    if (isTrimEnd && pFinal->eType == HTML_TEXT_TOKEN_NEWLINE) {
        pFinal->n--;
        if( pFinal->n==0 ){
            pFinal->eType = HTML_TEXT_TOKEN_END;
            nToken--;
        }
    }
    if (isTrimStart && pText->aToken[0].eType == HTML_TEXT_TOKEN_NEWLINE) {
        memmove(pText->aToken, &pText->aToken[1], sizeof(HtmlTextToken)*nToken);
    }

#ifndef NDEBUG
    /* This assert() block checks the following:
     *
     *     1) If there is nothing but white-space in this node, then
     *        HtmlTextNode.zText is set to NULL.
     *     2) If there are any text elements in the node HtmlTextNode.zText 
     *        is not set to NULL.
     *
     * In other words, the node is either white-space or not. This test is
     * included because I am paranoid the optimized HtmlNodeIsWhitespace() 
     * test (pText->zText==0) will malfunction one day.
     */
    if (1) {
        int haveText = 0;
        HtmlTextIter sIter;
        HtmlTextIterFirst(pText, &sIter);
        for ( ; HtmlTextIterIsValid(&sIter); HtmlTextIterNext(&sIter)) {
            if (HtmlTextIterType(&sIter) == HTML_TEXT_TOKEN_TEXT) {
                haveText = 1;
            }
        }
        assert(
            (!haveText && pText->zText == 0) ||        /* white-space */
            (haveText && pText->zText)                 /* not white-space */
        );
    }
#endif
}

HtmlTextNode *
HtmlTextNew(n, z, isTrimEnd, isTrimStart)
    int n;
    const char *z;
    int isTrimEnd;
    int isTrimStart;
{
    HtmlTextNode *pText;

    /* Allocate space for the HtmlTextNode. */ 
    pText = HtmlNew(HtmlTextNode);

    HtmlTextSet(pText, n, z, isTrimEnd, isTrimStart);
    return pText;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTextCombine --
 * 
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
#if 0
static HtmlTextNode *
HtmlTextCombine(p1, p2)
    HtmlTextNode *p1;
    HtmlTextNode *p2;
{
    /* HtmlTextNode *pNew; */
    return 0;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTextFree --
 * 
 *     Free a text-node structure allocated by HtmlTextNew().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Text-node p is deleted.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlTextFree(p)
    HtmlTextNode *p;
{
    HtmlFree(p);
}

void
HtmlTextIterFirst(pTextNode, pTextIter)
    HtmlTextNode *pTextNode;
    HtmlTextIter *pTextIter;
{
    pTextIter->pTextNode = pTextNode;
    pTextIter->iText = 0;
    pTextIter->iToken = 0;
}

int
HtmlTextIterIsValid(pTextIter)
    HtmlTextIter *pTextIter;
{
    HtmlTextToken *p = &pTextIter->pTextNode->aToken[pTextIter->iToken];
    return (p->eType != HTML_TEXT_TOKEN_END) ? 1 : 0;
}

void
HtmlTextIterNext(pTextIter)
    HtmlTextIter *pTextIter;
{
    HtmlTextToken *p = &pTextIter->pTextNode->aToken[pTextIter->iToken];
    int eType = p[0].eType;
    int eNext = p[1].eType;

    assert(eType != HTML_TEXT_TOKEN_END);

    if (eType == HTML_TEXT_TOKEN_TEXT) {
        pTextIter->iText += p->n;
    }
    else if (eType == HTML_TEXT_TOKEN_LONGTEXT) {
        int n = (p[0].n << 16) + (p[1].n << 8) + p[2].n;
        pTextIter->iText += n;
        pTextIter->iToken += 2;
    }

    if ((eType == HTML_TEXT_TOKEN_TEXT || eType == HTML_TEXT_TOKEN_LONGTEXT) &&
        (eNext != HTML_TEXT_TOKEN_TEXT && eNext != HTML_TEXT_TOKEN_LONGTEXT)
    ) {
        pTextIter->iText++;
    }

    pTextIter->iToken++;
}

int
HtmlTextIterIsLast(pTextIter)
    HtmlTextIter *pTextIter;
{
    HtmlTextIter sIter;
    memcpy(&sIter, pTextIter, sizeof(HtmlTextIter));
    HtmlTextIterNext(&sIter);
    return !HtmlTextIterIsValid(&sIter);
}

int 
HtmlTextIterType(pTextIter)
    HtmlTextIter *pTextIter;
{
    HtmlTextToken *p = &pTextIter->pTextNode->aToken[pTextIter->iToken];
    if (p->eType == HTML_TEXT_TOKEN_LONGTEXT) return HTML_TEXT_TOKEN_TEXT;
    return (int)(p->eType);
}

int 
HtmlTextIterLength(pTextIter)
    HtmlTextIter *pTextIter;
{
    HtmlTextToken *p = &pTextIter->pTextNode->aToken[pTextIter->iToken];
    if (p->eType == HTML_TEXT_TOKEN_LONGTEXT) {
        int n = (p[0].n << 16) + (p[1].n << 8) + p[2].n;
        return n;
    }
    return (int)(p->n);
}

const char *
HtmlTextIterData(pTextIter)
    HtmlTextIter *pTextIter;
{
    return (const char *)(&pTextIter->pTextNode->zText[pTextIter->iText]);
}

