	#include "../tab/tokens.h"
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

