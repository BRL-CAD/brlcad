/*                        S C R I P T . L
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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
%option nounput
%{
/*                        S C R I P T . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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

#include "../tab/tokens.h"

/* quell flex stupidities */
#ifndef YY_STACK_USED
#  define YY_STACK_USED 0
#endif
#ifndef YY_ALWAYS_INTERACTIVE
#  define YY_ALWAYS_INTERACTIVE 0
#endif
#ifndef YY_NEVER_INTERACTIVE
#  define YY_NEVER_INTERACTIVE 0
#endif
#ifndef YY_MAIN
#  define YY_MAIN 0
#endif
#ifndef YY_NO_INPUT
#  define YY_NO_INPUT 1
#endif

/* flex is being stupid.  need to check fwrite() return value to quell warning. */
#ifndef ECHO
#  define ECHO  if (fwrite( yytext, yyleng, 1, yyout ) != 1) perror("fwrite")
#endif

%}

D	[0-9]
A	[a-zA-Z]
L	[a-zA-Z0-9_\/\.\(\)\[\]\{\}]

%%

-?{D}*\.{D}+e[\+-]{D}+	return FLOAT;
-?{D}*\.{D}+	return FLOAT;
-?0[Xx]{D}+	return INT;
-?0{D}+		return INT;
-?{D}+		return INT;
^!.*		return SHELL;
#.*		return COMMENT;
start		return START;
viewsize	return VIEWSIZE;
eye_pt		return EYEPT;
lookat_pt	return LOOKATPT;
viewrot		return VIEWROT;
orientation	return ORIENTATION;
end		return END;
tree		return TREE;
multiview	return MULTIVIEW;
anim		return ANIM;
matrix		return MATRIX;
rmul		return RMUL;
lmul		return LMUL;
rboth		return RBOTH;
rstack		return RSTACK;
rarc		return RARC;
clean		return CLEAN;
set		return SET;
width		return WIDTH;
height		return HEIGHT;
perspective	return PERSPECTIVE;
angle		return ANGLE;
ae		return AE;
opt		return OPT;
{A}[a-zA-Z0-9_]*	return STRING;
\/{A}{L}*	return PATH;
{A}{L}*		return PATH;
;		return SEMI;
[ \t\n]		;

%%

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
