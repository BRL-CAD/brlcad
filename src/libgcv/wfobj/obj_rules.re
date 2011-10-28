/*                   O B J _ R U L E S . R E
 * Copyright (c) 2010-2011 United States Government as represented by
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>

#include "bu.h"
#include "bio.h"
#include "obj_grammar_decls.h"
#include "obj_parser_state.h"
#include "obj_rules.h"
#include "re2c_utils.h"

/* increase token limits of at&t and mk2 lex */
#undef YYLMAX
#define YYLMAX 4096

/* increase token limits of pclex (2x) */
#undef F_BUFSIZE
#define F_BUFSIZE 2048

namespace arl {
namespace obj_parser {
namespace detail {

/**
 *  convenience routines
 */
template<typename T>
inline static objCombinedState::parser_state_type &get_state(T scanner)
{
    return static_cast<objCombinedState*>(scanner)->parser_state;
}

/**
 *  Local functions
 */
static bool split_reference(const char *s, int val[3]);

} /* namespace detail */
} /* namespace obj_parser */
} /* namespace arl */

using namespace arl::obj_parser;

void *obj_parser_get_extra(yyscan_t scanner)
{
    return scanner->extra;
}

void obj_parser_set_extra(yyscan_t scanner, void *extra)
{
    scanner->extra = extra;
}

void obj_parser_lex_destroy(yyscan_t scanner)
{
    scannerFree(scanner);
}

int obj_parser_lex(YYSTYPE *yylval, yyscan_t scanner)
{
    using arl::obj_parser::detail::objCombinedState;

    void *yyextra = static_cast<objCombinedState*>(scanner->extra);

BEGIN_RE2C(scanner, cursor)

/*!re2c
re2c:define:YYCTYPE = char;
re2c:define:YYCURSOR = cursor;
re2c:yyfill:enable = 0;
re2c:condenumprefix = "";
re2c:define:YYSETCONDITION = BEGIN;

vertex      v
t_vertex    vt
n_vertex    vn
point       p
line        l
face        f
group       g
object      o
smooth      s
integer     [+-]?([0-9]+)
dseq        ([0-9]+)
dseq_opt    ([0-9]*)
frac        (({dseq_opt}"."{dseq})|{dseq}".")
exp         ([eE][+-]?{dseq})
exp_opt     ({exp}?)
fsuff       [flFL]
fsuff_opt   ({fsuff}?)
hpref       ('0'[xX])
hdseq       ([0-9]+)
hdseq_opt   ([0-9]*)
hfrac       (({hdseq_opt}"."{hdseq})|({hdseq}"."))
bexp        ([pP][+-]?{dseq})
dfc         (({frac}{exp_opt}{fsuff_opt})|({dseq}{exp}{fsuff_opt}))
hfc         (({hpref}{hfrac}{bexp}{fsuff_opt})|({hpref}{hdseq}{bexp}{fsuff_opt}))
real        [+-]?({dfc}|{hfc})
usemtl      usemtl
mtllib      mtllib
usemap      usemap
maplib      maplib
bevel       bevel
c_interp    c_interp
d_interp    d_interp
lod         lod
shadow_obj  shadow_obj
trace_obj   trace_obj
off         off
on          on
v_reference {integer}"/""/"?
v_tv_reference {integer}"/"{integer}
v_nt_reference {integer}"//"{integer}
v_tnv_reference_list {integer}"/"{integer}"/"{integer}

wspace      [ \t]
id          [!-~]+
newline     ["\r\n""\n"]
comment     "#"[^"\r\n""\n"]*{newline}

<INITIAL>{vertex}            { RETURN(VERTEX); }
<INITIAL>{t_vertex}          { RETURN(T_VERTEX); }
<INITIAL>{n_vertex}          { RETURN(N_VERTEX); }
<INITIAL>{point}             { RETURN(POINT); }
<INITIAL>{line}              { RETURN(LINE); }
<INITIAL>{face}              { RETURN(FACE); }
<INITIAL>{group}             {
    BEGIN(id_list_state);
    RETURN(GROUP);
}

<INITIAL>{object}            {
    BEGIN(id_state);
    RETURN(OBJECT);
}

<INITIAL>{smooth}            {
    RETURN(SMOOTH);
}

<INITIAL>{integer}           {
    yylval->integer = atoi(yytext);
    RETURN(INTEGER);
}

<INITIAL>{real}              {
    yylval->real = atof(yytext);
    RETURN(FLOAT);
}

<INITIAL>{usemtl}            {
    BEGIN(id_state);
    RETURN(USEMTL);
}

<INITIAL>{mtllib}            {
    BEGIN(id_list_state);
    RETURN(MTLLIB);
}

<INITIAL>{usemap}            {
    BEGIN(toggle_id_state);
    RETURN(USEMAP);
}

<INITIAL>{maplib}            {
    BEGIN(id_list_state);
    RETURN(MAPLIB);
}

<INITIAL>{bevel}             {
    RETURN(BEVEL);
}

<INITIAL>{c_interp}          {
    RETURN(C_INTERP);
}

<INITIAL>{d_interp}          {
    RETURN(D_INTERP);
}

<INITIAL>{lod}               {
    RETURN(LOD);
}

<INITIAL>{shadow_obj}        {
    BEGIN(id_state);
    RETURN(SHADOW_OBJ);
}

<INITIAL>{trace_obj}         {
    BEGIN(id_state);
    RETURN(TRACE_OBJ);
}

<INITIAL>{on}                {
    RETURN(ON);
}

<INITIAL>{off}               {
    RETURN(OFF);
}

<INITIAL>{v_reference}       {
    if (detail::split_reference(yytext, yylval->reference))
	RETURN(V_REFERENCE);
    RETURN(YYEOF);
}
<INITIAL>{v_tv_reference}    {
    if (detail::split_reference(yytext, yylval->reference))
	RETURN(TV_REFERENCE);
    RETURN(YYEOF);
}
<INITIAL>{v_nt_reference}    {
    if (detail::split_reference(yytext, yylval->reference))
	RETURN(NV_REFERENCE);
    RETURN(YYEOF);
}
<INITIAL>{v_tnv_reference_list} {
    if (detail::split_reference(yytext, yylval->reference))
	RETURN(TNV_REFERENCE);
    RETURN(YYEOF);
}

<INITIAL>{wspace}            { IGNORE_TOKEN; }

<INITIAL>{comment}|{newline} {
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    RETURN(EOL);
}

<INITIAL>. { IGNORE_TOKEN; }

<id_state>{id}                {
    // Keywords are valid identifiers here
    // Goto initial state after single token
    detail::get_state(yyextra).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    BEGIN(INITIAL);
    RETURN(ID);
}

<id_state>{wspace}            { IGNORE_TOKEN; }

<id_state>{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    RETURN(EOL);
}

<id_state>. { IGNORE_TOKEN; }

<toggle_id_state>{off}               {
    // off is a valid token, not an id
    BEGIN(INITIAL);
    RETURN(OFF);
}

<toggle_id_state>{id}                {
    // Keywords are valid identifiers here
    // Goto initial state after single token
    detail::get_state(yyextra).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    BEGIN(INITIAL);
    RETURN(ID);
}

<toggle_id_state>{wspace}            { IGNORE_TOKEN; }

<toggle_id_state>{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    RETURN(EOL);
}

<toggle_id_state>. { IGNORE_TOKEN; }

<id_list_state>{id}                {
    // Keywords are valid identifiers here
    detail::get_state(yyextra).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    RETURN(ID);
}

<id_list_state>{wspace}            { IGNORE_TOKEN }

<id_list_state>{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    RETURN(EOL);
}

<id_list_state>. { IGNORE_TOKEN; }

*/
END_RE2C

} /* scan */

namespace arl {
namespace obj_parser {
namespace detail {

bool split_reference(const char *s, int val[3])
{
    memset(val, sizeof(int)*3, 0);

    char *endptr;
    val[0] = strtol(s, &endptr, 0);
    if (*endptr == 0)
	return true;

    if (*endptr != '/')
	return false;
    ++endptr;

    val[1] = strtol(endptr, &endptr, 0);
    if (*endptr == 0)
	return true;

    if (*endptr != '/')
	return false;
    ++endptr;

    val[2] = strtol(endptr, &endptr, 0);

    return (*endptr == 0);
}

} /* namespace detail */
} /* namespace obj_parser */
} /* namespace arl */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
