%{
  #include <stdlib.h>
  #include <stdio.h>
  #include <stddef.h>
  #include <string.h>
  #include "obj_parser.h"

int yydebug = 1;

extern FILE *yyin;
extern int yylex();
extern char *yytext;

/* lex/yacc definitions */

void obj_parser_error()
{
	printf("obj_parser_error\n");
}

void yyerror(const char *str)
{
        fprintf(stderr,"error: %s\n",str);
}
 
int yywrap()
{
        return 1;
} 

%}

%token <real> FLOAT
%token <integer> INTEGER
%token <reference> V_REFERENCE
%token <reference> TV_REFERENCE
%token <reference> NV_REFERENCE
%token <reference> TNV_REFERENCE

%token ID
%token VERTEX
%token T_VERTEX
%token N_VERTEX
%token POINT
%token LINE
%token FACE
%token GROUP
%token SMOOTH
%token OBJECT
%token USEMTL
%token MTLLIB
%token USEMAP
%token MAPLIB
%token BEVEL
%token C_INTERP
%token D_INTERP
%token LOD
%token SHADOW_OBJ
%token TRACE_OBJ
%token ON
%token OFF

%type <real> coord
%type <toggle> toggle
%type <index> p_v_reference_list
%type <index> l_v_reference_list
%type <index> l_tv_reference_list
%type <index> f_v_reference_list
%type <index> f_tv_reference_list
%type <index> f_nv_reference_list
%type <index> f_tnv_reference_list
%%

statement_list: '\n'
  | statement '\n'
  | statement_list '\n'
  | statement_list statement '\n'
  ;
  
statement: vertex
  | t_vertex
  | n_vertex
  | point
  | line
  | face
  | group
  | smooth
  | object
  | usemtl
  | mtllib
  | usemap
  | maplib
  | shadow_obj
  | trace_obj
  | bevel
  | c_interp
  | d_interp
  | lod
  ;

coord: FLOAT { $$ = $1; }
  | INTEGER { $$ = $1; }
  ;

vertex
  : VERTEX coord coord coord
    {
      obj_add_vertex(0, $2, $3, $4);
    }
  | VERTEX coord coord coord coord
    {
      printf("\tVERTEX: %f,%f,%f,%f\n", $2,$3,$4,$5);
    }
  ;

t_vertex
  : T_VERTEX coord
    {
      obj_add_vertex(1, $2, 0, 0);
    }
  | T_VERTEX coord coord
    {
      obj_add_vertex(1, $2, $3, 0);
    }
  | T_VERTEX coord coord coord
    {
      obj_add_vertex(1, $2, $3, $4);
    }
  ;

n_vertex
  : N_VERTEX coord coord coord
    {
      obj_add_vertex(2, $2, $3, $4);
    }
  ;

p_v_reference_list
  : INTEGER
    {
      printf("\tpvrl INTEGER: %d\n", $1);
    }
  | V_REFERENCE
    {
      printf("\tpvrl V_REFERENCE\n");
    }
  | p_v_reference_list INTEGER
    {
      printf("\tpvrl p_v_reference_list INTEGER: %d\n", $2);
    }
  | p_v_reference_list V_REFERENCE
    {
      printf("\tpvrl p_v_reference_list V_REFERENCE\n");
    }
  ;

l_v_reference_list
  : INTEGER
    {
      printf("\tlvrl INTEGER: %d\n", $1);
    }
  | V_REFERENCE
    {
      printf("\tlvrl V_REFERENCE\n");
    }
  | l_v_reference_list INTEGER
    {
      printf("\tlvrl l_v_reference_list INTEGER: %d\n", $2);
    }
  | l_v_reference_list V_REFERENCE
    {
      printf("\tlvrl l_v_reference_list V_REFERENCE");
    }
 ;

l_tv_reference_list
  : TV_REFERENCE
    {
      printf("\tltvrl TV_REFERENCE\n");
    }
  | l_tv_reference_list TV_REFERENCE
    {
      printf("\tltvrl l_tv_reference_list TV_REFERENCE\n");
    }
  ;

f_v_reference_list
  : INTEGER
    {
      printf("\tfvrl INTEGER %d\n", $1);
    }
  | V_REFERENCE
    {
      printf("\tfvrl V_REFERENCE\n");
    }
  | f_v_reference_list INTEGER
    {
      printf("\tfvrl f_v_reference_list INTEGER: %d\n", $2);
    }
  | f_v_reference_list V_REFERENCE
    {
      printf("\tfvrl f_v_reference_list V_REFERENCE\n");
    }
  ;

f_tv_reference_list
  : TV_REFERENCE
    {
      printf("\tftvrl TV_REFERENCE\n");
    }
  | f_tv_reference_list TV_REFERENCE
    {
      printf("\tftvrl f_tv_reference_list TV_REFERENCE");
    }
  ;

f_nv_reference_list
  : NV_REFERENCE
    {
      printf("\tfnvrl NV_REFERENCE: %d,%d\n", $1[0],$1[2]);
    }
  | f_nv_reference_list NV_REFERENCE
    {
      printf("\tfnvrl f_nv_reference_list NV_REFERENCE: %d,%d\n",$2[0],$2[2]);
    }
  ;

f_tnv_reference_list
  : TNV_REFERENCE
    {
      printf("\tftnvrl TNV_REFERENCE\n");
    }
  | f_tnv_reference_list TNV_REFERENCE
    {
      printf("\tftnvrl f_tnv_reference_list TNV_REFERENCE\n");
    }
  ;

point
  : POINT p_v_reference_list
    {
      printf("\tPOINT f_tnv_reference_list\n");
    }
  ;

line
  : LINE l_v_reference_list
    {
      printf("\tLINE l_v_reference_list\n");
    }
  |
    LINE l_tv_reference_list
    {
      printf("\tLINE l_tv_reference_list\n");
    }
  ;
  
face
  : FACE f_v_reference_list
    {
      printf("\tFACE f_v_reference_list\n");
    }
  | FACE f_tv_reference_list
    {
      printf("\tFACE f_tv_reference_list\n");
    }
  | FACE f_nv_reference_list
    {
      printf("\tFACE f_nv_reference_list\n");
    }
  | FACE f_tnv_reference_list
    {
      printf("\tFACE f_tnv_reference_list\n");
    }
  ;

group: GROUP id_list
    {
      printf("\tGROUP id_list\n");
    }
  ;

smooth: SMOOTH INTEGER
    {
      if($2 < 0) {
        obj_parser_error();
        YYERROR;
      }
      printf("\tSMOOTH INTEGER: %d\t\n", $2);
    }
  | SMOOTH OFF
    {
      printf("\tSMOOTH OFF\n");
    }
  ;
  
object: OBJECT ID
    {
      printf("\tOBJECT ID: %s\n", yytext);
    }
  ;

usemtl: USEMTL ID
    {
      printf("\tUSEMTL ID: %s\n", yytext);
    }
  ;

mtllib: MTLLIB id_list
    {
      printf("\tMTLLIB id_list\n");
    }
  ;

usemap: USEMAP ID
    {
      printf("\tUSEMAP ID %s\n", yytext);
    }
  | USEMAP OFF
    {
      printf("\tUSEMAP OFF\n");
    }
  ;

maplib: MAPLIB id_list
    {
      printf("\tMAPLIB id_list\n");
    }
  ;
  
shadow_obj: SHADOW_OBJ ID
    {
      printf("\tSHADOW_OBJ ID: %s\n", yytext);
    }
  ;

trace_obj: TRACE_OBJ ID
    {
      printf("\tTRACE_OBJ ID: %s\n", yytext);
    }
  ;

bevel: BEVEL toggle
    {
      printf("\tBEVEL toggle\n");
    }
  ;

c_interp: C_INTERP toggle
    {
      printf("\tC_INTERP toggle\n");
    }
  ;

d_interp: D_INTERP toggle
    {
      printf("\tD_INTERP toggle\n");
    }
  ;

lod: LOD INTEGER
    {
      if(!($2 >= 0 && $2 <= 100)) {
        obj_parser_error();
        YYERROR;
      }
      
      printf("\tLOD INTEGER: %s\n", $2);
    }
  ;
  
id_list: ID
    {
      printf("\til ID: %s\n", yytext);
    }
  | id_list ID
    {
      printf("\til id_list ID\n");
    }
  ;

toggle: ON
    {
      printf("1\n");
      $$ = 1;
    }
  | OFF
    {
      printf("0\n");
      $$ = 0;
    }
  ;
 
%%
