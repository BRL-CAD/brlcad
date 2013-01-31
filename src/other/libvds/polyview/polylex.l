%{
#include "polytok.h"
/* All our rules return, so define YY_BREAK to be null */
#define YY_BREAK
int pLineNo;

%}
%option noyywrap
DIG [0-9]
WS [ \t]

%%

("+"|"-")?{DIG}+		{ return INT; }
("+"|"-")?{DIG}+("."{DIG}+)?(e("+"|"-")?{DIG}+)?  { return FLOAT; } 
"v"|"V"				{ return VERT; }
"f"|"F"				{ return FACE; }
"vertices:"			{ return NUMVERT; }
"faces:"			{ return NUMFACE; }
{WS}+				/* Just eat it up! */ 
"\n"				{ pLineNo++; break; }
.				{ return UNKNOWN; } 
<<EOF>>				{ return PEOF; }

%%
