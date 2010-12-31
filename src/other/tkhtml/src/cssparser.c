/*
 * Copyright (c) 2007 Dan Kennedy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of this software nor the names of its 
 *       contributors may be used to endorse or promote products 
 *       derived from this software without specific prior written 
 *       permission.
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
static const char rcsid[] = "$Id: cssparser.c,v 1.8 2008/01/19 06:08:13 danielk1977 Exp $";

#include <ctype.h>
#include <assert.h>

#include "cssInt.h"

#define ISSPACE(x) isspace((unsigned char)(x))

typedef struct CssInput CssInput;
struct CssInput {
    /* Current token */
    CssTokenType eToken;
    char *zToken;
    int nToken;

    /* Input text */
    char *zInput;       /* Input text (CSS document) */
    int nInput;         /* Number of bytes allocated at zInput */
    int iInput;         /* Offset of next token in zInput */
};


#if 0
static void inputPrintToken(pInput)
    CssInput *pInput;
{
    switch (pInput->eToken) {
        case CT_SPACE: printf("CT_SPACE"); break;
        case CT_LP: printf("CT_LP"); break;
        case CT_RP: printf("CT_RP"); break;
        case CT_RRP: printf("CT_RRP"); break;
        case CT_LSP: printf("CT_LSP"); break;

        case CT_RSP: printf("CT_RSP"); break;
        case CT_SEMICOLON: printf("CT_SEMICOLON"); break;
        case CT_COMMA: printf("CT_COMMA"); break;
        case CT_COLON: printf("CT_COLON"); break;
        case CT_PLUS: printf("CT_PLUS"); break;

        case CT_DOT: printf("CT_DOT"); break;
        case CT_HASH: printf("CT_HASH"); break;
        case CT_EQUALS: printf("CT_EQUALS"); break;
        case CT_TILDE: printf("CT_TILDE"); break;
        case CT_PIPE: printf("CT_PIPE"); break;

        case CT_AT: printf("CT_AT"); break;
        case CT_BANG: printf("CT_BANG"); break;
        case CT_STRING: printf("CT_STRING"); break;
        case CT_LRP: printf("CT_LRP"); break;
        case CT_GT: printf("CT_GT"); break;

        case CT_STAR: printf("CT_STAR"); break;
        case CT_SLASH: printf("CT_SLASH"); break;
        case CT_IDENT: printf("CT_IDENT"); break;

        case CT_SGML_OPEN: printf("CT_SGML_OPEN"); break;
        case CT_SGML_CLOSE: printf("CT_SGML_CLOSE"); break;
        case CT_SYNTAX_ERROR: printf("CT_SYNTAX_ERROR"); break;
        case CT_FUNCTION: printf("CT_FUNCTION"); break;
        case CT_EOF: printf("CT_EOF"); break;
    }
    printf(" (%d) \"%.*s\"\n", 
        pInput->nToken, pInput->nToken, pInput->zToken
    );
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * inputDiscardComment --
 *
 *     Discard any comment from the current point in the input string 
 *     *pInput.
 *
 * Results: 
 *     Return 1 if a comment was found, or 0 otherwise.
 *
 * Side effects:
 *     May consume data from *pInput.
 *
 *---------------------------------------------------------------------------
 */
static int inputDiscardComment(pInput)
    CssInput *pInput;
{
    const char *z = &pInput->zInput[pInput->iInput];
    int n = pInput->nInput - pInput->iInput;

    if (n > 1 && z[0] == '/' && z[1] == '*') {
        int i;
        for (i = 4; i <= n && (z[i-1] != '/' || z[i-2] != '*'); i++) {
        }
        pInput->iInput += i;

        /* If the previous token returned was CT_SPACE, then ignore any 
         * white-space that occurs immediately after a comment. This is
         * so the parser can assume there will never be two CT_SPACE 
         * tokens in a row.
         */
        if (pInput->eToken == CT_SPACE) {
            while (
                pInput->iInput < pInput->nInput &&
                ISSPACE(pInput->zInput[pInput->iInput])
            ) {
                pInput->iInput++;
            }
        }

        return 1;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * inputGetToken --
 *
 *     Access the type, text and length of the current input token.
 *
 * Results: 
 *     Return the type of the current input token.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssTokenType inputGetToken(pInput, pzToken, pnToken)
    CssInput *pInput;
    const char **pzToken;          /* OUTPUT: Pointer to token */
    int *pnToken;                  /* OUTPUT: Token length */
{
    if (pzToken) *pzToken = pInput->zToken;
    if (pnToken) *pnToken = pInput->nToken;
    return pInput->eToken;
}

/*
 *---------------------------------------------------------------------------
 *
 * inputNextToken --
 *
 *     Advance the input to the next token.
 *
 * Results: 
 *     Non-zero is returned if the end-of-file is reached. Otherwise zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int inputNextToken(pInput)
    CssInput *pInput;
{
    const char *z;
    int n;
    int nToken;
    CssTokenType eToken = CT_SYNTAX_ERROR;

    /* Discard any comments from the input stream */
    while (inputDiscardComment(pInput));

    z = &pInput->zInput[pInput->iInput];
    n = pInput->nInput - pInput->iInput;

    if (n <= 0) {
      pInput->eToken = CT_EOF;
      pInput->zToken = 0;
      pInput->nToken = 0;
      return 1;               /* 1 == EOF */
    }

    nToken = 1;
    switch( z[0] ){
        case ' ':
        case '\n':
        case '\r':
        case '\t': {
            /* Collapse any contiguous whitespace to a single token */
            int i;
            for (i = 1; ISSPACE((int)z[i]); i++) {
            }
            nToken = i;
            eToken = CT_SPACE;
            break;
        }
        case '{': eToken = CT_LP; break;
        case '}': eToken = CT_RP; break;
        case ')': eToken = CT_RRP; break;
        case '(': eToken = CT_LRP; break;
        case '[': eToken = CT_LSP; break;
        case ']': eToken = CT_RSP; break;
        case ';': eToken = CT_SEMICOLON; break;
        case ',': eToken = CT_COMMA; break;
        case ':': eToken = CT_COLON; break;
        case '+': eToken = CT_PLUS; break;
        case '>': eToken = CT_GT; break;
        case '*': eToken = CT_STAR; break;
        case '.': eToken = CT_DOT; break;
        case '#': eToken = CT_HASH; break;
        case '=': eToken = CT_EQUALS; break;
        case '~': eToken = CT_TILDE; break;
        case '|': eToken = CT_PIPE; break;
        case '@': eToken = CT_AT; break;
        case '!': eToken = CT_BANG; break;
        case '/': eToken = CT_SLASH; break;

        case '"': case '\'': {
            char delim = z[0];
            char c;
            int i;
            for(i=1; i<n; i++){
                c = z[i];
                if( c=='\\' ){
                    i++;
                }
                else if( c=='\n' ){
                    /* This is illegal. A CSS string cannot contain a new-line
                     * unless it is escaped by a '\' character. The correct
                     * response is to skip all the input parsed so far,
                     * then fall back to normal syntax-error parsing.
                     */
                    pInput->iInput += i;
                    goto bad_token;
                } 
                else if( c==delim ){
                    nToken = i+1; 
                    eToken = CT_STRING;
                    break;
                }
            }
            /* This is actually not a parse error. If an EOF occurs in the
             * middle of a string constant, the correct behaviour is to
             * act as though the string was closed with a " or ' character.
             */
            break;
        }

        case '<': {
            if (z[1] != '!' || z[2] != '-' || z[3] != '-') {
                goto parse_as_token;
            }
            eToken = CT_SGML_OPEN;
            nToken = 4;
            break;
        }
        case '-': {
            if (z[1] != '-' || z[2] != '>') {
                goto parse_as_token;
            }
            eToken = CT_SGML_CLOSE;
            nToken = 3;
            break;
        }

parse_as_token:
        default: {
                
            /* This must be either an identifier or a function. For the
            ** ASCII character range 0-127, the 'charmap' array is 1 for
            ** characters allowed in an identifier or function name, 0
            ** for characters not allowed. Allowed characters are a-z, 
	    ** 0-9, '-', '_', '%' and '\'. All unicode characters
            ** outside the ASCII range are allowed.
            */
            static u8 charmap[128] = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00-0x0F */
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10-0x1F */
                0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, /* 0x20-0x2F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, /* 0x30-0x3F */
                0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40-0x4F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, /* 0x50-0x5F */
                0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60-0x6F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0  /* 0x70-0x7F */
            };
            int i;
            int inHexEscape = 0;

            for(i=0; i<n && (z[i]<0 || charmap[(int)z[i]]); i++){
                if (z[i] == '\\' && z[i + 1]) i++;

                /* In CSS, a single white-space character that occurs after
                 * a hexadecimal escape sequence is ignored.
                 */
                if (isxdigit((u8)(z[i]))) {
                    if (i > 0 && z[i - 1] == '\\') inHexEscape = 1;
                    if (inHexEscape && isspace((u8)(z[i + 1]))) i++;
                } else {
                    inHexEscape = 0;
                }
            }

            if( i==0 ) goto bad_token;
            if( i<n && z[i]=='(' ){
                CssInput sInput;
                memset(&sInput, 0, sizeof(CssInput));
                sInput.zInput = (char *)(&z[i]);
                sInput.nInput = n - i;

                inputNextToken(&sInput);
                eToken = inputGetToken(&sInput, 0, 0);
                while (eToken != CT_RRP && eToken != CT_EOF) {
                    inputNextToken(&sInput);
                    eToken = inputGetToken(&sInput, 0, 0);
                }
                if( eToken!=CT_RRP ) goto bad_token;
                nToken = sInput.iInput + i;
                eToken = CT_FUNCTION;
            } else {
                nToken = i;
                eToken = CT_IDENT;
            }
        }
    }

    pInput->eToken = eToken;
    pInput->nToken = nToken;
    pInput->zToken = (char *)z;
    pInput->iInput += nToken;
    return 0;

bad_token:
    pInput->iInput++;
    pInput->eToken = CT_SYNTAX_ERROR;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * inputNextTokenIgnoreSpace --
 *
 *     Advance the input to the next non-whitespace token.
 *
 * Results: 
 *     Non-zero is returned if the end-of-file is reached. Otherwise zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int inputNextTokenIgnoreSpace(pInput)
    CssInput *pInput;
{
    int rc = inputNextToken(pInput);
    if (rc == 0 && CT_SPACE == inputGetToken(pInput, 0, 0)) {
        rc = inputNextToken(pInput);
    }
    assert(CT_SPACE != inputGetToken(pInput, 0, 0));
    return rc;
}


/*
 *---------------------------------------------------------------------------
 *
 * parseSyntaxError --
 *
 *     This function is called when a syntax-error is encountered parsing
 *     either an "at-rule" or a CSS rule (except within the body of the
 *     property declaration block, that is handled seperately by
 *     parseDeclarationError()).
 *
 *     If the error occured while parsing an at-rule, the isStopAtSemiColon
 *     argument is true. Otherwise false.
 *
 *     If isStopAtSemiColon is false, then tokens are discarded until the
 *     end of the next block or the end-of-file is encountered. If
 *     isStopAtSemiColon at semi-colon is true, then also stop if a 
 *     semi-colon character (i.e. ';') that is not inside a block is
 *     encountered.
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     Consumes data from *pInput.
 *
 *---------------------------------------------------------------------------
 */
static void parseSyntaxError(pInput, pParse, isStopAtSemiColon)
    CssInput *pInput;
    CssParse *pParse;
    int isStopAtSemiColon;        /* True if error occured parsing an @ rule */
{
    char *zToken;
    int nToken;
    CssTokenType eToken;

    int iNest = 0;

    int iErrorStart = 0;
    int iErrorLength = 0;

    iErrorStart = pInput->iInput;
    eToken = inputGetToken(pInput, &zToken, &nToken);

    while (
        eToken != CT_EOF && 
        (eToken != CT_SEMICOLON || iNest != 0 || !isStopAtSemiColon) &&
        (eToken != CT_RP        || iNest > 1)
    ) {
        if (eToken == CT_LP) iNest++;
        if (eToken == CT_RP) iNest--;
        inputNextToken(pInput);
        eToken = inputGetToken(pInput, 0, 0);
    }
    iErrorLength = pInput->iInput - iErrorStart;

    if (pParse->pErrorLog) {
        Tcl_Obj *pError = pParse->pErrorLog;
        Tcl_ListObjAppendElement(0, pError, Tcl_NewIntObj(iErrorStart));
        Tcl_ListObjAppendElement(0, pError, Tcl_NewIntObj(iErrorLength));
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * parseDeclarationError --
 *
 *     This function is called when a syntax-error is encountered within
 *     a declarationblock (see parseDeclarationBlock()).
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     Consumes data from *pInput.
 *
 *---------------------------------------------------------------------------
 */
static int parseDeclarationError(pInput, pParse)
    CssInput *pInput;
    CssParse *pParse;
{
    char *zToken;
    int nToken;
    CssTokenType eToken;

    int iNest = 0;

    int iErrorStart = 0;
    int iErrorLength = 0;

    iErrorStart = pInput->iInput;
    eToken = inputGetToken(pInput, &zToken, &nToken);

    while (
        eToken != CT_EOF && 
        (eToken != CT_SEMICOLON || iNest != 0) &&
        (eToken != CT_RP        || iNest > 0)
    ) {
        if (eToken == CT_LP) iNest++;
        if (eToken == CT_RP) iNest--;
        inputNextToken(pInput);
        eToken = inputGetToken(pInput, 0, 0);
    }
    iErrorLength = pInput->iInput - iErrorStart;
    inputNextToken(pInput);

    if (pParse->pErrorLog) {
        Tcl_Obj *pError = pParse->pErrorLog;
        Tcl_ListObjAppendElement(0, pError, Tcl_NewIntObj(iErrorStart));
        Tcl_ListObjAppendElement(0, pError, Tcl_NewIntObj(iErrorLength));
    }

    return ((eToken == CT_SEMICOLON) ? 0: 1);
}

#define safe_isdigit(c) (isdigit((unsigned char)(c)))

/*
 *---------------------------------------------------------------------------
 *
 * parseSelector --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int parseSelector(pInput, pParse)
    CssInput *pInput;
    CssParse *pParse;
{

    while (1) {
        const char *zToken;
        int nToken;
        CssTokenType eNext;
        CssTokenType eToken;

        eToken = inputGetToken(pInput, &zToken, &nToken);
        inputNextToken(pInput);
        eNext = inputGetToken(pInput, 0, 0);
    
        switch (eToken) {
            case CT_STAR:       /* Universal selector (section 5.3) */
                HtmlCssSelector(pParse, CSS_SELECTOR_UNIVERSAL, 0, 0);
                break;
    
            case CT_IDENT: {    /* Type selector (section 5.4) */
                CssToken tType;
                tType.z = zToken;
                tType.n = nToken;
                HtmlCssSelector(pParse, CSS_SELECTOR_TYPE, 0, &tType);
                break;
            }
    
            case CT_SPACE: {    /* Descendant selector (section 5.5) */
                /* Three possibilities here:
                 *
                 *     (a) white-space between selectors, indicating a 
                 *         descendant selector (i.e. "H1 P"), or
                 *
                 *     (b) white-space before a "+" or ">" selector, or
                 *
                 *     (c) white-space before the start of the properties 
                 *         block (i.e. " {color: red}").
                 *
                 * Ignore the white-space if it is either (b) or (c). Otherwise
                 * add a descendant selector to the parse context.
                 */
                if (eNext != CT_PLUS && eNext != CT_GT && 
                    eNext != CT_LP && eNext != CT_COMMA
                ) {
                    HtmlCssSelector(pParse, CSS_SELECTORCHAIN_DESCENDANT, 0, 0);
                }
                break;
            }
    
            case CT_GT: {    /* Child selector (section 5.5) */
                HtmlCssSelector(pParse, CSS_SELECTORCHAIN_CHILD, 0, 0);
                /* Ignore any white-space that occurs after a '>' */
                if (eNext == CT_SPACE) inputNextToken(pInput);
                break;
            }

            case CT_COLON: {
                struct _Pseudo {
                    char *z;
                    int eArg;
                    int twocolonsok;
                } aPseudo[] = {
                    {"first-child",  CSS_PSEUDOCLASS_FIRSTCHILD, 0}, 
                    {"last-child",   CSS_PSEUDOCLASS_LASTCHILD, 0}, 
                    {"link",         CSS_PSEUDOCLASS_LINK, 0}, 
                    {"visited",      CSS_PSEUDOCLASS_VISITED, 0}, 
                    {"active",       CSS_PSEUDOCLASS_ACTIVE, 0}, 
                    {"hover",        CSS_PSEUDOCLASS_HOVER, 0}, 
                    {"focus",        CSS_PSEUDOCLASS_FOCUS, 0}, 
                    {"lang",         CSS_PSEUDOCLASS_LANG, 0}, 

                    {"after",        CSS_PSEUDOELEMENT_AFTER, 1}, 
                    {"before",       CSS_PSEUDOELEMENT_BEFORE, 1}, 
                    {"first-line",   CSS_PSEUDOELEMENT_FIRSTLINE, 1}, 
                    {"first-letter", CSS_PSEUDOELEMENT_FIRSTLETTER, 1}, 
                    {0, 0}
                };
                int ii;
                int twocolons = 0;
                if (eNext == CT_COLON) {
                    twocolons = 1;
                    inputNextToken(pInput);
                    eNext = inputGetToken(pInput, 0, 0);
                }
                if (eNext != CT_IDENT) goto syntax_error;
                inputGetToken(pInput, &zToken, &nToken);
                for (ii = 0; aPseudo[ii].z; ii++) {
                    if (
                        nToken == strlen(aPseudo[ii].z) && 
                        0 == strncmp(zToken, aPseudo[ii].z, nToken) &&
                        aPseudo[ii].twocolonsok >= twocolons
                    ) break;
                }
                if (!aPseudo[ii].z) goto syntax_error;

                if (aPseudo[ii].eArg == CSS_PSEUDOCLASS_LANG){
                    /* TODO: Parse lang(...) */
                    goto syntax_error;
                } else {
                    HtmlCssSelector(pParse, aPseudo[ii].eArg, 0, 0);
                }
                inputNextToken(pInput);
                break;
            }

            case CT_PLUS: {    /* Child selector (section 5.7) */
                HtmlCssSelector(pParse, CSS_SELECTORCHAIN_ADJACENT, 0, 0);
                /* Ignore any white-space that occurs after a '+' */
                if (eNext == CT_SPACE) inputNextToken(pInput);
                break;
            }

            case CT_DOT: {    /* Class selector (section 5.8.3) */
                CssToken t;
                eToken = inputGetToken(pInput, &t.z, &t.n);

                /* Section 4.1.3 of CSS 2.1 says that class names, 
                 * id names and element names may not start with a digit
                 * or a hyphen followed by a digit.
                 */
                if (eToken != CT_IDENT || 
                    (t.z[0] == '-' && t.n > 1 && safe_isdigit(t.z[1])) ||
                    safe_isdigit(t.z[0])
                ) {
                    goto syntax_error;
                }
                HtmlCssSelector(pParse, CSS_SELECTOR_CLASS, 0, &t);
                inputNextToken(pInput);
                break;
            }

            case CT_HASH: {    /* Id selector (section 5.9) */
                CssToken t;
                eToken = inputGetToken(pInput, &t.z, &t.n);

                /* Section 4.1.3 of CSS 2.1 says that class names, 
                 * id names and element names may not start with a digit
                 * or a hyphen followed by a digit.
                 */
                if (eToken != CT_IDENT || 
                    (t.z[0] == '-' && t.n > 1 && safe_isdigit(t.z[1])) ||
                    safe_isdigit(t.z[0])
                ) {
                    goto syntax_error;
                }
                HtmlCssSelector(pParse, CSS_SELECTOR_ID, 0, &t);
                inputNextToken(pInput);
                break;
            }

            case CT_LSP: {    /* Attribute selector of some kind */
                CssToken t1;
                CssToken t2;
                if (eNext == CT_SPACE) inputNextToken(pInput);

                eToken = inputGetToken(pInput, &t1.z, &t1.n);
                if (eToken != CT_IDENT) goto syntax_error;

                inputNextToken(pInput);
                eToken = inputGetToken(pInput, 0, 0);
                if (eToken == CT_SPACE) {
                    inputNextToken(pInput);
                    eToken = inputGetToken(pInput, 0, 0);
                }

                if (eToken == CT_RSP) {
                    HtmlCssSelector(pParse, CSS_SELECTOR_ATTR, &t1, 0);
                } else if (
                        eToken == CT_TILDE || 
                        eToken == CT_PIPE || 
                        eToken == CT_EQUALS
                ) {
                    if (eToken == CT_TILDE || eToken == CT_PIPE) {
                         CssTokenType e;
                         inputNextToken(pInput);
                         e = inputGetToken(pInput, 0, 0);
                         if (e != CT_EQUALS) goto syntax_error;
                    }
                    inputNextToken(pInput);
                    if (CT_SPACE == inputGetToken(pInput, 0, 0)) {
                        inputNextToken(pInput);
                    }
                    eNext = inputGetToken(pInput, &t2.z, &t2.n);
                    if (eNext != CT_IDENT && eNext != CT_STRING) {
                        goto syntax_error;
                    }

                    inputNextToken(pInput);
                    if (CT_SPACE == inputGetToken(pInput, 0, 0)) {
                        inputNextToken(pInput);
                    }
                    eNext = inputGetToken(pInput, 0, 0);
                    if (eNext != CT_RSP) goto syntax_error;

                    HtmlCssSelector(pParse, (
                        (eToken == CT_TILDE) ? CSS_SELECTOR_ATTRLISTVALUE :
                        (eToken == CT_PIPE)  ? CSS_SELECTOR_ATTRHYPHEN :
                        CSS_SELECTOR_ATTRVALUE) , &t1, &t2
                    );
                } else {
                    goto syntax_error;
                }
                inputNextToken(pInput);
                break;
            }

            case CT_COMMA: {
	        if( !pParse->pSelector ){
                  goto syntax_error;
                }
                HtmlCssSelectorComma(pParse);
                if (CT_SPACE == eNext) inputNextToken(pInput);
                break;
            }

            case CT_LP: return 0;
            default: goto syntax_error;
        }
    }

  syntax_error:
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseDeclarationBlock --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int parseDeclarationBlock(CssInput *pInput, CssParse *pParse){
    while (1) {
        CssToken tProp;
        CssToken tVal;
        CssTokenType eToken;
        int isImportant = 0;

        /* Property name */
        if (inputGetToken(pInput, 0, 0) == CT_SPACE) inputNextToken(pInput);
        eToken = inputGetToken(pInput, &tProp.z, &tProp.n);
        if (eToken == CT_RP) return 0;
        if (eToken != CT_IDENT) goto syntax_error;

        /* Colon */
        inputNextTokenIgnoreSpace(pInput);
        eToken = inputGetToken(pInput, 0, 0);
        if (eToken != CT_COLON) goto syntax_error;

        /* Property value */
        inputNextTokenIgnoreSpace(pInput);
        eToken = inputGetToken(pInput, &tVal.z, 0);
        tVal.n = 0;
        while (
             eToken == CT_IDENT || 
             eToken == CT_STRING || 
             eToken == CT_COMMA || 
             eToken == CT_PLUS ||
             eToken == CT_FUNCTION ||
             eToken == CT_HASH ||
             eToken == CT_SLASH ||
             eToken == CT_DOT
        ) {
            char *z;
            int n;
            inputGetToken(pInput, &z, &n);
            tVal.n = (&z[n] - tVal.z);
            inputNextTokenIgnoreSpace(pInput);
            eToken = inputGetToken(pInput, 0, 0);
        }
        if (tVal.n == 0) goto syntax_error;

        /* Check for the !IMPORTANT symbol */
        if (eToken == CT_BANG) {
            char *z;
            int n;
            inputNextTokenIgnoreSpace(pInput);
            eToken = inputGetToken(pInput, &z, &n);
            if (n != 9 || 0 != strnicmp("important", z, 9)) {
                goto syntax_error;
            }
            isImportant = 1;
            inputNextTokenIgnoreSpace(pInput);
        }

        eToken = inputGetToken(pInput, 0, 0);
        if (eToken != CT_RP && eToken != CT_SEMICOLON && eToken != CT_EOF) {
            goto syntax_error;
        }
         
        HtmlCssDeclaration(pParse, &tProp, &tVal, isImportant);

        if (eToken == CT_RP || eToken == CT_EOF) return 0;
        inputNextTokenIgnoreSpace(pInput);

        continue;

      syntax_error:
        if (parseDeclarationError(pInput, pParse)) return 0;
    }

    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseMediaList --
 *
 *     Tkhtml3 uses rules suitable for the following media:
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int parseMediaList(pInput, pIsMatch)
    CssInput *pInput;
    int *pIsMatch;
{
    int media_ok = 0;

    while (1) {
        CssTokenType eToken;
        char * zToken;
        int nToken;
        eToken = inputGetToken(pInput, &zToken, &nToken);

        if (eToken != CT_IDENT) return 1;
        if ((nToken == 3 && strnicmp("all", zToken, nToken) == 0) ||
            (nToken == 6 && strnicmp("screen", zToken, nToken) == 0)
        ) {
           media_ok = 1;
        }

        inputNextTokenIgnoreSpace(pInput);
        if (CT_COMMA != inputGetToken(pInput, 0, 0)) break;

        inputNextTokenIgnoreSpace(pInput);
    }

    *pIsMatch = media_ok;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseAtRule --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int parseAtRule(CssInput *pInput, CssParse *pParse){
    char *zWord;
    int nWord;
    inputNextToken(pInput);
  
    /* According to CSS2.1, white-space after the '@' character is illegal */
    if (CT_IDENT != inputGetToken(pInput, &zWord, &nWord)) return 1;
  
    if (nWord == 6 && strnicmp("import", zWord, nWord) == 0) {
        CssTokenType eToken;
        CssToken tToken;
        int media_ok = 1;

	/* If we are already into the stylesheet "body", this is a 
         * syntax error 
         */
        if (pParse->isBody) {
            return 1;
        }
  
        inputNextTokenIgnoreSpace(pInput);
        eToken = inputGetToken(pInput, &tToken.z, &tToken.n);
        if (eToken != CT_STRING && eToken != CT_FUNCTION) {
            return 1;
        }
  
        inputNextTokenIgnoreSpace(pInput);
        eToken = inputGetToken(pInput, 0, 0);
        if (eToken != CT_SEMICOLON && eToken != CT_EOF) {
            if (parseMediaList(pInput, &media_ok)) return 1;
        }
  
        eToken = inputGetToken(pInput, 0, 0);
        if (eToken != CT_SEMICOLON && eToken != CT_EOF) return 1;
  
        if (media_ok) {
            HtmlCssImport(pParse, &tToken);
        }
    }
  
    else if (nWord == 5 && strnicmp("media", zWord, nWord) == 0) {
        int media_ok;
        pParse->isBody = 1;
        inputNextTokenIgnoreSpace(pInput);
        if (parseMediaList(pInput, &media_ok)) return 1;
        if (CT_LP != inputGetToken(pInput, 0, 0)) return 1;
        inputNextToken(pInput);
        if (!media_ok) {
            /* The media does not match. Skip tokens until the end of
             * the block.
             */
            int iNest = 1;
            while (
                (inputGetToken(pInput, 0, 0) != CT_EOF) &&
                (inputGetToken(pInput, 0, 0) != CT_RP || iNest != 1)
            ) {
                if (inputGetToken(pInput, 0, 0) == CT_LP) iNest++;
                if (inputGetToken(pInput, 0, 0) == CT_RP) iNest--;
                inputNextToken(pInput);
            }
        }
      
  /*
    }
    else if (nWord == 4 && strnicmp("page", zWord, nWord) == 0) {
  */
    }
    else if (nWord == 7 && strnicmp("charset", zWord, nWord) == 0) {
        CssTokenType eNext;
        do {
            inputNextTokenIgnoreSpace(pInput);
            eNext = inputGetToken(pInput, 0, 0);
        } while (eNext != CT_SEMICOLON && eNext != CT_EOF);
    } else {
        pParse->isBody = 1;
        return 1;
    }
  
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssRunParser --
 *
 *     Parse a stylesheet document.
 *
 *     Calls the following functions from css.c:
 *
 *         HtmlCssDeclaration
 *         HtmlCssSelectorComma
 *         HtmlCssSelector
 *         HtmlCssImport
 *         HtmlCssRule
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
void HtmlCssRunParser(zInput, nInput, pParse)
    const char *zInput;
    int nInput;
    CssParse *pParse;
{
    int eToken;
    CssInput sInput;
    memset(&sInput, 0, sizeof(CssInput));
    sInput.zInput = (char *)zInput;
    sInput.nInput = nInput;

    /* This function is the top-level parser. From this functions point
     * of view, a CSS stylesheet is made up of statements. Each statement
     * is either an at-rule or a declaration.
     */
    while (0 == inputNextTokenIgnoreSpace(&sInput)) {
        int isSyntaxError;

        eToken = inputGetToken(&sInput, 0, 0);

        if (eToken == CT_SGML_OPEN || eToken == CT_SGML_CLOSE) {
            isSyntaxError = 0;
        } else if (eToken == CT_RP) {
            isSyntaxError = 0;
        } else if (eToken == CT_AT) {
            isSyntaxError = parseAtRule(&sInput, pParse);
        } else {
            pParse->isBody = 1;
            isSyntaxError = parseSelector(&sInput, pParse);
            if (!isSyntaxError) {
                isSyntaxError = parseDeclarationBlock(&sInput, pParse);
            }
            HtmlCssRule(pParse, !isSyntaxError);
        }

        if (isSyntaxError) {
            parseSyntaxError(&sInput, pParse, (eToken==CT_AT));
        }
    }

}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssRunStyleParser --
 *
 *     Parse an inline style declaration.
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlCssRunStyleParser(zInput, nInput, pParse)
    const char *zInput;
    int nInput;
    CssParse *pParse;
{
    CssInput sInput;
    memset(&sInput, 0, sizeof(CssInput));
    sInput.zInput = (char *)zInput;
    sInput.nInput = nInput;

    HtmlCssSelector(pParse, CSS_SELECTOR_UNIVERSAL, 0, 0);
    parseDeclarationBlock(&sInput, pParse);
    HtmlCssRule(pParse, 1);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssGetToken --
 *
 *     Return the id of the next CSS token in the string pointed to by z,
 *     length n. The length of the token is written to *pLen. 0 is returned
 *     if there are no complete tokens remaining.
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CssTokenType HtmlCssGetToken(z, n, pLen)
    CONST char *z; 
    int n; 
    int *pLen;
{
    CssInput sInput;
    memset(&sInput, 0, sizeof(CssInput));
    sInput.zInput = (char *)z;
    sInput.nInput = n;

    inputNextToken(&sInput);
    *pLen = sInput.iInput;
    return sInput.eToken;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssGetNextListItem --
 *
 *     Return the first property from a space seperated list of properties.
 *
 *     The property list is stored in string zList, length nList.
 *
 *     A pointer to the first property is returned. The length of the first
 *     property is stored in *pN.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
const char *
HtmlCssGetNextListItem(zList, nList, pN)
    const char *zList;
    int nList;
    int *pN;
{
    char *zRet;
    int eToken;
    int eFirst;
    int nLen = 0;

    CssInput sInput;
    
    memset(&sInput, 0, sizeof(CssInput));
    sInput.zInput = (char *)zList;
    sInput.nInput = nList;

    inputNextTokenIgnoreSpace(&sInput);
    eFirst = inputGetToken(&sInput, &zRet, &nLen);
    *pN = nLen;
    if (eFirst == CT_EOF) {
        return 0;
    }
    if (eFirst == CT_STRING || eFirst == CT_FUNCTION) {
        return zRet;
    }

    nLen = 0;
    do {
        int n;
        inputGetToken(&sInput, 0, &n);
        nLen += n;
        inputNextToken(&sInput);
        eToken = inputGetToken(&sInput, 0, 0);
    } while (eToken != CT_SPACE && eToken != CT_EOF);

    *pN = nLen;
    assert(nLen <= nList);
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssGetNextListItem --
 *
 *     Return the next item in a comma seperated list of properties.
 *
 *     The property list is stored in string zList, length nList.
 *
 *     A pointer to the first property is returned. The length of the first
 *     property is stored in *pN.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
const char *
HtmlCssGetNextCommaListItem(zList, nList, pN)
    const char *zList;
    int nList;
    int *pN;
{
    char *zRet;
    int eToken;
    int nLen = 0;

    CssInput sInput;

    if (nList < 0) nList = strlen(zList);
    
    memset(&sInput, 0, sizeof(CssInput));
    sInput.zInput = (char *)zList;
    sInput.nInput = nList;

    inputNextTokenIgnoreSpace(&sInput);
    if (inputGetToken(&sInput, 0, 0) == CT_EOF) {
        *pN = 0;
        return 0;
    }
    if (inputGetToken(&sInput, &zRet, 0) == CT_COMMA) {
        inputNextTokenIgnoreSpace(&sInput);
        inputGetToken(&sInput, &zRet, 0);
    }

    do {
        int n;
        inputGetToken(&sInput, 0, &n);
        nLen += n;
        inputNextTokenIgnoreSpace(&sInput);
        eToken = inputGetToken(&sInput, 0, 0);
    } while (eToken != CT_COMMA && eToken != CT_EOF);

    *pN = nLen;
    return zRet;
}

