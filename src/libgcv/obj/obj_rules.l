%{
#include <stdio.h>
#include <string.h>
#include "obj_parser.h"
#include "obj_grammar.h"
%}

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
	      	      printf("entering id_list_state\n");
                      return GROUP;
                    }
                    
{object}            {
                      return OBJECT;
                    }

{smooth}            {
                      return SMOOTH;
                    }

{integer}           {
                      yylval.integer = atoi(yytext);
                      return INTEGER;
                    }

{real}              {
                      yylval.real = atof(yytext);
                      return FLOAT;
                    }

{usemtl}            {
                      BEGIN(id_state);
	      	      printf("entering id_state\n");
                      return USEMTL;
                    }

{mtllib}            {
	              BEGIN(id_list_state);
	      	      printf("entering id_list_state\n");
                      return MTLLIB;
                    }

{usemap}            {
                      BEGIN(toggle_id_state);
	      	      printf("entering toggle_id_state\n");
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
                      return SHADOW_OBJ;
                    }

{trace_obj}         {
                      return TRACE_OBJ;
                    }

{on}                {
                      return ON;
                    }

{off}               {
                      return OFF;
                    }
{v_reference}       {
		      if (obj_split_reference(yytext,yylval.reference))
	                 return V_REFERENCE;
                    }
{v_tv_reference}    {
		      if (obj_split_reference(yytext,yylval.reference))
                         return TV_REFERENCE;
                      return 0;
                    }
{v_nt_reference}    {
		      if (obj_split_reference(yytext,yylval.reference))
                         return NV_REFERENCE;
                      return 0;
                    }
{v_tnv_reference_list} {
		      if (obj_split_reference(yytext,yylval.reference))
                         return TNV_REFERENCE;
                      return 0;
                    }
{id}		    {
		         return ID;
                    }
{wspace}            { }

{comment}|{newline} {
                      return '\n';
                    }

.                   { return yytext[0]; }


<id_state>{

{id}                {
                      // Keywords are valid identifiers here
                      // Goto initial state after single token
                      //working_string = yytext;
                      BEGIN(INITIAL);
	      	      printf("entering INITIAL state\n");
                      return ID;
                    }

{wspace}            { }

{comment}|{newline} {
                      // Goto initial state when we hit newline
                      BEGIN(INITIAL);
	      	      printf("entering INITIAL state\n");
                      return '\n';
                    }

.                   { return yytext[0]; }
}

<toggle_id_state>{

{off}               {
                      // off is a valid token, not an id
                      BEGIN(INITIAL);                      
	      	      printf("entering INITIAL state\n");
                      return OFF;
                    }

{id}                {
                      // Keywords are valid identifiers here
                      // Goto initial state after single token
                      //working_string = yytext;
                      BEGIN(INITIAL);
	      	      printf("entering INITIAL state\n");
                      return ID;
                    }

{wspace}            { }

{comment}|{newline} {
                      // Goto initial state when we hit newline
                      BEGIN(INITIAL);
	      	      printf("entering INITIAL state\n");
                      return '\n';
                    }

.                   { return yytext[0]; }
}

<id_list_state>{

{id}                {
                      // Keywords are valid identifiers here
                      //working_string = yytext;
                      return ID;
                    }

{wspace}            { }

{comment}|{newline} {
                      // Goto initial state when we hit newline
                      BEGIN(INITIAL);
	      	      printf("entering INITIAL state\n");
                      return '\n';
                    }

.                   { return yytext[0]; }
}



%%

int obj_split_reference(const char *s, int val[3])
 {
   memset(val,sizeof(int)*3,0);
 
   char *endptr;
   val[0] = strtol(s,&endptr,0);
   if(*endptr == 0)
     return 1;
 
   if(*endptr != '/')
     return 0;
   ++endptr;
 
   val[1] = strtol(endptr,&endptr,0);
   if(*endptr == 0)
     return 1;
 
   if(*endptr != '/')
     return 0;
   ++endptr;
 
   val[2] = strtol(endptr,&endptr,0);
 
   return *endptr == 0;
}

