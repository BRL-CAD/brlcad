%{
/*
**	Scanner for base
*/

#include	"std.h"

extern	int	more;
%}

%%

"$"			{	return 1;	}
"[\."			{	return 2;	}
".\]"			{	return -2;	}
"\\f(as"		{	return 3;	}
"\\f(CW"		{	return 3;	}
"\\fP"			{	return -3;	}
"\\fR"			{	return -3;	}
".(p"			{	return 3;	}
".)p"			{	return -3;	}
".PS"			{	return 4;	}
".PE"			{	return -4;	}
.|\n			{	return 0;	}

%%

yywrap()
{
	more = FALSE;
	return 1;
}
