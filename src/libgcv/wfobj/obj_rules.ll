/*                   O B J _ R U L E S . L L
 * BRL-CAD
 *
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
%{
/*                   O B J _ R U L E S . L L
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

#include "bio.h"

#include "obj_parser_state.h"

#include <sys/types.h>

#include "obj_grammar.h"

#include <stdlib.h>
#include <limits.h>

/* increase token limits of at&t and mk2 lex */
#undef YYLMAX
#define YYLMAX 4096

/* increase token limits of pclex (2x) */
#undef F_BUFSIZE
#define F_BUFSIZE 2048

#if NO_POSIX_READ
#define YY_INPUT(buf, result, max_size) \
errno=0; \
while ((result = fread(buf, 1, max_size, yyin))==0 && ferror(yyin)) { \
    if (errno != EINTR) { \
	YY_FATAL_ERROR("input in flex scanner failed"); \
	break; \
    } \
    errno=0; \
    clearerr(yyin); \
}
#endif

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
%}

%option full never-interactive
%option warn nodefault noyywrap nounput
%option reentrant bison-bridge

%x id_state
%x toggle_id_state
%x id_list_state

vertex      v
t_vertex    vt
n_vertex    vn
point       p
line        l
face        f
group       g
object      o
smooth      s
integer     [+-]?([[:digit:]]+)
dseq        ([[:digit:]]+)
dseq_opt    ([[:digit:]]*)
frac        (({dseq_opt}"."{dseq})|{dseq}".")
exp         ([eE][+-]?{dseq})
exp_opt     ({exp}?)
fsuff       [flFL]
fsuff_opt   ({fsuff}?)
hpref       (0[xX])
hdseq       ([[:xdigit:]]+)
hdseq_opt   ([[:xdigit:]]*)
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

%%

{vertex}            { return VERTEX; }
{t_vertex}          { return T_VERTEX; }
{n_vertex}          { return N_VERTEX; }
{point}             { return POINT; }
{line}              { return LINE; }
{face}              { return FACE; }
{group}             {
    BEGIN(id_list_state);
    return GROUP;
}

{object}            {
    BEGIN(id_state);
    return OBJECT;
}

{smooth}            {
    return SMOOTH;
}

{integer}           {
    yylval->integer = atoi(yytext);
    return INTEGER;
}

{real}              {
    yylval->real = atof(yytext);
    return FLOAT;
}

{usemtl}            {
    BEGIN(id_state);
    return USEMTL;
}

{mtllib}            {
    BEGIN(id_list_state);
    return MTLLIB;
}

{usemap}            {
    BEGIN(toggle_id_state);
    return USEMAP;
}

{maplib}            {
    BEGIN(id_list_state);
    return MAPLIB;
}

{bevel}             {
    return BEVEL;
}

{c_interp}          {
    return C_INTERP;
}

{d_interp}          {
    return D_INTERP;
}

{lod}               {
    return LOD;
}

{shadow_obj}        {
    BEGIN(id_state);
    return SHADOW_OBJ;
}

{trace_obj}         {
    BEGIN(id_state);
    return TRACE_OBJ;
}

{on}                {
    return ON;
}

{off}               {
    return OFF;
}

{v_reference}       {
    if (detail::split_reference(yytext, yylval->reference))
	return V_REFERENCE;
    return 0;
}
{v_tv_reference}    {
    if (detail::split_reference(yytext, yylval->reference))
	return TV_REFERENCE;
    return 0;
}
{v_nt_reference}    {
    if (detail::split_reference(yytext, yylval->reference))
	return NV_REFERENCE;
    return 0;
}
{v_tnv_reference_list} {
    if (detail::split_reference(yytext, yylval->reference))
	return TNV_REFERENCE;
    return 0;
}

{wspace}            { }

{comment}|{newline} {
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    return '\n';
}

.                   { return yytext[0]; }


<id_state>{

{id}                {
    // Keywords are valid identifiers here
    // Goto initial state after single token
    detail::get_state(yyextra).working_string = yytext;
    BEGIN(INITIAL);
    return ID;
}

{wspace}            { }

{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    return '\n';
}

.                   { return yytext[0]; }
}


<toggle_id_state>{

{off}               {
    // off is a valid token, not an id
    BEGIN(INITIAL);
    return OFF;
}

{id}                {
    // Keywords are valid identifiers here
    // Goto initial state after single token
    detail::get_state(yyextra).working_string = yytext;
    BEGIN(INITIAL);
    return ID;
}

{wspace}            { }

{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    return '\n';
}

.                   { return yytext[0]; }
}


<id_list_state>{

{id}                {
    // Keywords are valid identifiers here
    detail::get_state(yyextra).working_string = yytext;
    return ID;
}

{wspace}            { }

{comment}|{newline} {
    // Goto initial state when we hit newline
    ++(detail::get_state(yyextra).file_stack.back().lineno);
    BEGIN(INITIAL);
    return '\n';
}

.                   { return yytext[0]; }
}


%%

namespace arl {
namespace obj_parser {
namespace detail {

bool split_reference(const char *s, int val[3])
{
    memset(val, sizeof(int) * 3, 0);

    char *endptr;
    val[0] = strtol(s, &endptr, 0);

    if (*endptr == 0) {
	return true;
    }

    if (*endptr != '/') {
	return false;
    }

    ++endptr;

    val[1] = strtol(endptr, &endptr, 0);

    if (*endptr == 0) {
	return true;
    }

    if (*endptr != '/') {
	return false;
    }

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
