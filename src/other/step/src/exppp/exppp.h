extern int exppp_nesting_indent;	/* default nesting indent */
extern int exppp_continuation_indent;	/* default nesting indent for */
					/* continuation lines */
extern int exppp_linelength;		/* leave some slop for closing */
					/* parens.  \n is not included in */
					/* this count either */
extern int exppp_rmpp;			/* if true, create rmpp */
extern int exppp_alphabetize;		/* if true, alphabetize */
extern int exppp_terse;			/* don't describe action to stdout */
extern int exppp_reference_info;	/* if true, add commentary */
					/* about where things came from */
extern int exppp_preserve_comments;	/* if true, preserve comments where */
					/* possible */
extern char *exppp_output_filename;	/* force output filename */
extern int exppp_output_filename_reset;	/* if true, force output filename */

void EXPRESSout(Express e);

void ENTITYout(Entity e);
void EXPRout(Expression expr);
void FUNCout(Function f);
void PROCout(Procedure p);
void RULEout(Rule r);
char *SCHEMAout(Schema s);
void SCHEMAref_out(Schema s);
void STMTout(Statement s);
void TYPEout(Type t);
void TYPEhead_out(Type t);
void TYPEbody_out(Type t);
void WHEREout(Linked_List w);

char *REFto_string(Dictionary refdict,Linked_List reflist,char *type,int level);
char *ENTITYto_string(Entity e);
char *SUBTYPEto_string(Expression e);
char *EXPRto_string(Expression expr);
char *FUNCto_string(Function f);
char *PROCto_string(Procedure p);
char *RULEto_string(Rule r);
char *SCHEMAref_to_string(Schema s);
char *STMTto_string(Statement s);
char *TYPEto_string(Type t);
char *TYPEhead_to_string(Type t);
char *TYPEbody_to_string(Type t);
char *WHEREto_string(Linked_List w);

int REFto_buffer(Dictionary refdict,Linked_List reflist,char *type,int level,char *buffer,int length);
int ENTITYto_buffer(Entity e,char *buffer,int length);
int EXPRto_buffer(Expression e,char *buffer,int length);
int FUNCto_buffer(Function e,char *buffer,int length);
int PROCto_buffer(Procedure e,char *buffer,int length);
int RULEto_buffer(Rule e,char *buffer,int length);
int SCHEMAref_to_buffer(Schema s,char *buffer,int length);
int STMTto_buffer(Statement s,char *buffer,int length);
int TYPEto_buffer(Type t,char *buffer,int length);
int TYPEhead_to_buffer(Type t,char *buffer,int length);
int TYPEbody_to_buffer(Type t,char *buffer,int length);
int WHEREto_buffer(Linked_List w,char *buffer,int length);
