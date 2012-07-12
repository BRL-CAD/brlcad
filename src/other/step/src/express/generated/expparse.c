/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 2 "expparse.y"

#include <assert.h>
#include "token_type.h"
#include "parse_data.h"

int yyerrstatus = 0;
#define yyerrok (yyerrstatus = 0)

YYSTYPE yylval;

    /*
     * YACC grammar for Express parser.
     *
     * This software was developed by U.S. Government employees as part of
     * their official duties and is not subject to copyright.
     *
     * $Log: expparse.y,v $
     * Revision 1.23  1997/11/14 17:09:04  libes
     * allow multiple group references
     *
     * Revision 1.22  1997/01/21 19:35:43  dar
     * made C++ compatible
     *
     * Revision 1.21  1995/06/08  22:59:59  clark
     * bug fixes
     *
     * Revision 1.20  1995/04/08  21:06:07  clark
     * WHERE rule resolution bug fixes, take 2
     *
     * Revision 1.19  1995/04/08  20:54:18  clark
     * WHERE rule resolution bug fixes
     *
     * Revision 1.19  1995/04/08  20:49:08  clark
     * WHERE
     *
     * Revision 1.18  1995/04/05  13:55:40  clark
     * CADDETC preval
     *
     * Revision 1.17  1995/03/09  18:43:47  clark
     * various fixes for caddetc - before interface clause changes
     *
     * Revision 1.16  1994/11/29  20:55:58  clark
     * fix inline comment bug
     *
     * Revision 1.15  1994/11/22  18:32:39  clark
     * Part 11 IS; group reference
     *
     * Revision 1.14  1994/11/10  19:20:03  clark
     * Update to IS
     *
     * Revision 1.13  1994/05/11  19:50:00  libes
     * numerous fixes
     *
     * Revision 1.12  1993/10/15  18:47:26  libes
     * CADDETC certified
     *
     * Revision 1.10  1993/03/19  20:53:57  libes
     * one more, with feeling
     *
     * Revision 1.9  1993/03/19  20:39:51  libes
     * added unique to parameter types
     *
     * Revision 1.8  1993/02/16  03:17:22  libes
     * reorg'd alg bodies to not force artificial begin/ends
     * added flag to differentiate parameters in scopes
     * rewrote query to fix scope handling
     * added support for Where type
     *
     * Revision 1.7  1993/01/19  22:44:17  libes
     * *** empty log message ***
     *
     * Revision 1.6  1992/08/27  23:36:35  libes
     * created fifo for new schemas that are parsed
     * connected entity list to create of oneof
     *
     * Revision 1.5  1992/08/18  17:11:36  libes
     * rm'd extraneous error messages
     *
     * Revision 1.4  1992/06/08  18:05:20  libes
     * prettied up interface to print_objects_when_running
     *
     * Revision 1.3  1992/05/31  23:31:13  libes
     * implemented ALIAS resolution
     *
     * Revision 1.2  1992/05/31  08:30:54  libes
     * multiple files
     *
     * Revision 1.1  1992/05/28  03:52:25  libes
     * Initial revision
     */

#include "express/symbol.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/express.h"
#include "express/schema.h"
#include "express/entity.h"
#include "express/resolve.h"
#include "expscan.h"

    extern int print_objects_while_running;

    int tag_count;	/* use this to count tagged GENERIC types in the formal */
    /* argument lists.  Gross, but much easier to do it this */
    /* way then with the 'help' of yacc. */
    /* Set it to -1 to indicate that tags cannot be defined, */
    /* only used (outside of formal parameter list, i.e. for */
    /* return types).  Hey, as long as */
    /* there's a gross hack sitting around, we might as well */
    /* milk it for all it's worth!  -snc */

    Express yyexpresult;	/* hook to everything built by parser */

    Symbol *interface_schema;	/* schema of interest in use/ref clauses */
    void (*interface_func)();	/* func to attach rename clauses */

    /* record schemas found in a single parse here, allowing them to be */
    /* differentiated from other schemas parsed earlier */
    Linked_List PARSEnew_schemas;

    void SCANskip_to_end_schema(perplex_t scanner);

    int	yylineno;


    static void	yyerror(const char*, char *string);

    bool yyeof = false;

#define MAX_SCOPE_DEPTH	20	/* max number of scopes that can be nested */

    static struct scope {
        struct Scope_ *this_;
        char type;	/* one of OBJ_XXX */
        struct scope *pscope;	/* pointer back to most recent scope */
        /* that has a printable name - for better */
        /* error messages */
    } scopes[MAX_SCOPE_DEPTH], *scope;
#define CURRENT_SCOPE (scope->this_)
#define PREVIOUS_SCOPE ((scope-1)->this_)
#define CURRENT_SCHEMA (scope->this_->u.schema)
#define CURRENT_SCOPE_NAME		(OBJget_symbol(scope->pscope->this_,scope->pscope->type)->name)
#define CURRENT_SCOPE_TYPE_PRINTABLE	(OBJget_type(scope->pscope->type))

    /* ths = new scope to enter */
    /* sym = name of scope to enter into parent.  Some scopes (i.e., increment) */
    /*       are not named, in which case sym should be 0 */
    /*	 This is useful for when a diagnostic is printed, an earlier named */
    /* 	 scoped can be used */
    /* typ = type of scope */
#define PUSH_SCOPE(ths,sym,typ) \
	if (sym) DICTdefine(scope->this_->symbol_table,(sym)->name,(Generic)ths,sym,typ);\
	ths->superscope = scope->this_; \
	scope++;		\
	scope->type = typ;	\
	scope->pscope = (sym?scope:(scope-1)->pscope); \
	scope->this_ = ths; \
	if (sym) { \
		ths->symbol = *(sym); \
	}
#define POP_SCOPE() scope--

    /* PUSH_SCOPE_DUMMY just pushes the scope stack with nothing actually on it */
    /* Necessary for situations when a POP_SCOPE is unnecessary but inevitable */
#define PUSH_SCOPE_DUMMY() scope++

    /* normally the superscope is added by PUSH_SCOPE, but some things (types) */
    /* bother to get pushed so fix them this way */
#define SCOPEadd_super(ths) ths->superscope = scope->this_;

#define ERROR(code)	ERRORreport(code, yylineno)

void parserInitState()
{
    scope = scopes;
    /* no need to define scope->this */
    scope->this_ = yyexpresult;
    scope->pscope = scope;
    scope->type = OBJ_EXPRESS;
    yyexpresult->symbol.name = yyexpresult->u.express->filename;
    yyexpresult->symbol.filename = yyexpresult->u.express->filename;
    yyexpresult->symbol.line = 1;
}
#line 2466 "expparse.y"

static void
yyerror(const char *yytext, char *string)
{
    char buf[200];
    Symbol sym;

    strcpy(buf, string);

    if (yyeof) {
	strcat(buf, " at end of input");
    } else if (yytext[0] == 0) {
	strcat(buf, " at null character");
    } else if (yytext[0] < 040 || yytext[0] >= 0177) {
	sprintf(buf + strlen(buf), " before character 0%o", yytext[0]);
    } else {
	sprintf(buf + strlen(buf), " before `%s'", yytext);
    }

    sym.line = yylineno;
    sym.filename = current_filename;
    ERRORreport_with_symbol(ERROR_syntax, &sym, buf,
	CURRENT_SCOPE_TYPE_PRINTABLE, CURRENT_SCOPE_NAME);
}
#line 217 "expparse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned short int
#define YYNOCODE 280
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE  YYSTYPE 
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  struct qualifier yy46;
  Variable yy91;
  Op_Code yy126;
  struct entity_body yy176;
  Where yy234;
  struct subsuper_decl yy242;
  struct type_flags yy252;
  struct upper_lower yy253;
  Symbol* yy275;
  Type yy297;
  Case_Item yy321;
  Statement yy332;
  Linked_List yy371;
  struct type_either yy378;
  struct subtypes yy385;
  Expression yy401;
  Qualified_Attr* yy457;
  TypeBody yy477;
  Integer yy507;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define ParseARG_SDECL  parse_data_t parseData ;
#define ParseARG_PDECL , parse_data_t parseData 
#define ParseARG_FETCH  parse_data_t parseData  = yypParser->parseData 
#define ParseARG_STORE yypParser->parseData  = parseData 
#define YYNSTATE 655
#define YYNRULE 334
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero. */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
#define YY_ACTTAB_COUNT (2866)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    79,   80,  409,   69,   70,  228,  167,   73,   71,   72,
 /*    10 */    74,  250,   81,   76,   75,   16,   44,  593,  120,  553,
 /*    20 */   109,  491,  490,  396,  409,  609,  170,  533,   91,  612,
 /*    30 */   272,  607,  624,  557,  606,   47,  604,  608,   68,  543,
 /*    40 */   603,   46,  153,  158,  176,  629,  628,   59,   58,  305,
 /*    50 */    79,   80,  741,   62,  622,   89,  131,  251,  622,  623,
 /*    60 */   310,   15,   81,  621,  314,   16,   44,  621,  631,  616,
 /*    70 */   404,  403,   77,  396,  236,  643,  618,  617,  615,  614,
 /*    80 */   613,  529,  416,  184,  417,  180,  415,  532,   68,  395,
 /*    90 */   458,  313,  315,  114,  113,  629,  628,  114,  113,  305,
 /*   100 */    79,   80,  531,  566,  622,  537,  526,  246,  630,  623,
 /*   110 */    23,  579,   81,  621,  204,   16,   44,  168,  457,  616,
 /*   120 */   159,  787,  312,  396,   36,  569,  618,  617,  615,  614,
 /*   130 */   613,  410,  475,  462,  474,  477,  875,  473,   68,  395,
 /*   140 */   354,  476,  204,  114,  113,  629,  628,  122,  337,  169,
 /*   150 */    79,   80,  524,  528,  622,  475,  478,  474,  477,  623,
 /*   160 */   473,  319,   81,  621,  476,   16,   44,   62,  568,  616,
 /*   170 */   404,  403,   77,  396,  114,  113,  618,  617,  615,  614,
 /*   180 */   613,  475,  472,  474,  477,   35,  473,  459,   68,  395,
 /*   190 */   476,  404,  352,   77,  121,  629,  628,  122,  450,  386,
 /*   200 */    79,   80,   19,   26,  622,  475,  471,  474,  477,  623,
 /*   210 */   473,  787,   81,  621,  476,   16,   44,  366,  535,  616,
 /*   220 */   875,  224,  529,  316,  413,   92,  618,  617,  615,  614,
 /*   230 */   613,  413,  290,  320,  990,  119,  411,  626,   68,  395,
 /*   240 */   238,  635,  625,  654,  636,  629,  628,  637,  642,  641,
 /*   250 */   525,  640,  639,  108,  622,  392,  609,  144,  154,  623,
 /*   260 */   612,  282,  607,  621,  144,  606,  518,  604,  608,  616,
 /*   270 */   652,  603,   46,  153,  158,  534,  618,  617,  615,  614,
 /*   280 */   613,  599,  597,  600,  533,  595,  594,  598,  601,  395,
 /*   290 */   596,   69,   70,  529,  528,   73,   71,   72,   74,  552,
 /*   300 */    75,   76,   75,  522,  154,  609,  170,  249,  225,  612,
 /*   310 */   151,  607,  518,  557,  606,  622,  604,  608,  219,    5,
 /*   320 */   603,   46,  153,  158,  621,  504,  503,  502,  501,  500,
 /*   330 */   499,  498,  497,  496,  495,    2,  350,  116,  246,  559,
 /*   340 */   129,   22,  371,  369,  365,   41,  363,  341,  350,  360,
 /*   350 */   104,  154,   84,  453,   73,   71,   72,   74,  366,  518,
 /*   360 */    76,   75,  326,  410,  322,  530,  105,  339,    3,  393,
 /*   370 */   381,  504,  503,  502,  501,  500,  499,  498,  497,  496,
 /*   380 */   495,    2,  253,   41,   93,  339,  129,  246,   69,   70,
 /*   390 */   239,  252,   73,   71,   72,   74,  359,  154,   76,   75,
 /*   400 */   116,  136,  394,  388,  363,  518,  591,   65,  529,  125,
 /*   410 */   562,  453,   43,  443,    3,  223,  339,  518,  504,  503,
 /*   420 */   502,  501,  500,  499,  498,  497,  496,  495,    2,  366,
 /*   430 */   302,  171,  387,  129,  255,  442,  186,  609,  224,  187,
 /*   440 */   253,  612,  308,  607,  115,  348,  606,   42,  604,  608,
 /*   450 */    45,   41,  603,   46,  154,  157,  115,  481,   86,  136,
 /*   460 */   455,    3,  518,  568,  504,  503,  502,  501,  500,  499,
 /*   470 */   498,  497,  496,  495,    2,  306,  429,  579,  240,  129,
 /*   480 */   527,  165,   29,  163,  633,  247,  245,  586,  585,  244,
 /*   490 */   242,   69,   70,  529,  386,   73,   71,   72,   74,  358,
 /*   500 */   154,   76,   75,  289,  372,   10,  353,    3,  518,  590,
 /*   510 */   421,  652,   21,  183,  162,  161,  347,  560,  565,  246,
 /*   520 */   572,  504,  503,  502,  501,  500,  499,  498,  497,  496,
 /*   530 */   495,    2,  475,  470,  474,  477,  129,  473,  111,  303,
 /*   540 */    41,  476,  573,   41,  366,  356,  154,  233,  338,  579,
 /*   550 */   230,  134,   78,  248,  518,  175,  633,  247,  245,  586,
 /*   560 */   585,  244,  242,  234,    3,  528,  652,  504,  503,  502,
 /*   570 */   501,  500,  499,  498,  497,  496,  495,    2,  334,  529,
 /*   580 */   537,  410,  129,  560,  433,  357,  174,  173,  609,  311,
 /*   590 */   154,  494,  612,  151,  607,   39,  652,  606,  518,  604,
 /*   600 */   608,   39,  164,  603,   46,  153,  158,   87,  102,  385,
 /*   610 */     3,  319,  551,  504,  503,  502,  501,  500,  499,  498,
 /*   620 */   497,  496,  495,    2,  210,  333,   39,  384,  129,  366,
 /*   630 */   638,  635,  383,  203,  636,  493,  154,  637,  642,  641,
 /*   640 */   200,  640,  639,  217,  518,  475,  469,  474,  477,  567,
 /*   650 */   473,  528,  423,  301,  476,  130,    3,  504,  503,  502,
 /*   660 */   501,  500,  499,  498,  497,  496,  495,    2,  549,  487,
 /*   670 */   246,   27,  129,   14,  205,   12,  133,   39,   13,  579,
 /*   680 */   374,  355,  379,  248,  100,  175,  633,  247,  245,  586,
 /*   690 */   585,  244,  242,  561,   14,  205,  380,   24,  382,   13,
 /*   700 */     3,  331,  378,  504,  503,  502,  501,  500,  499,  498,
 /*   710 */   497,  496,  495,    2,  558,  366,  174,  173,  129,  172,
 /*   720 */    14,  205,  126,  232,  511,   13,  557,  336,  520,  381,
 /*   730 */    69,   70,  231,  216,   73,   71,   72,   74,  452,  373,
 /*   740 */    76,   75,  229,  226,  370,  139,    3,   14,  205,  368,
 /*   750 */   367,  110,   13,  106,  189,  364,  107,  523,  222,  650,
 /*   760 */   221,  361,  218,  213,  117,  651,  649,  648,   61,  652,
 /*   770 */   647,  646,  645,  644,  118,   32,  208,  529,    9,   20,
 /*   780 */   486,  485,  484,   18,  207,  351,  877,  288,   82,   25,
 /*   790 */   346,  647,  646,  645,  644,  118,  206,  468,   99,  202,
 /*   800 */    97,  160,   95,  335,    9,   20,  486,  485,  484,  340,
 /*   810 */   461,   94,  198,  199,  194,  193,  135,  647,  646,  645,
 /*   820 */   644,  118,  434,  190,  335,  328,  327,  426,  325,  185,
 /*   830 */   323,    9,   20,  486,  485,  484,  188,  424,  321,  181,
 /*   840 */   178,  177,    8,  449,  647,  646,  645,  644,  118,  528,
 /*   850 */   335,  123,   55,   53,   56,   49,   51,   50,   54,   57,
 /*   860 */    48,   52,  329,  197,   59,   58,   11,  634,  635,  653,
 /*   870 */    62,  636,  652,  143,  637,  642,  641,  335,  640,  639,
 /*   880 */   408,   60,   39,   55,   53,   56,   49,   51,   50,   54,
 /*   890 */    57,   48,   52,   63,  592,   59,   58,  632,   21,  587,
 /*   900 */   584,   62,  243,   55,   53,   56,   49,   51,   50,   54,
 /*   910 */    57,   48,   52,  366,  583,   59,   58,  475,  467,  474,
 /*   920 */   477,   62,  473,  241,   88,  576,  476,  582,  237,   37,
 /*   930 */   611,   85,  627,  570,   55,   53,   56,   49,   51,   50,
 /*   940 */    54,   57,   48,   52,  235,  140,   59,   58,  112,  377,
 /*   950 */   579,  544,   62,  141,   55,   53,   56,   49,   51,   50,
 /*   960 */    54,   57,   48,   52,  541,  371,   59,   58,   83,   69,
 /*   970 */    70,  550,   62,   73,   71,   72,   74,  538,  540,   76,
 /*   980 */    75,  605,  548,   55,   53,   56,   49,   51,   50,   54,
 /*   990 */    57,   48,   52,  539,  215,   59,   58,  547,  546,  545,
 /*  1000 */    27,   62,   28,  214,  536,  454,  209,  307,  619,  256,
 /*  1010 */   588,  521,   55,   53,   56,   49,   51,   50,   54,   57,
 /*  1020 */    48,   52,  516,  460,   59,   58,  446,  138,  101,  195,
 /*  1030 */    62,  475,  466,  474,  477,  610,  473,   98,  515,  514,
 /*  1040 */   476,   40,  124,   55,   53,   56,   49,   51,   50,   54,
 /*  1050 */    57,   48,   52,  513,  512,   59,   58,  444,  580,  635,
 /*  1060 */     4,   62,  636,  508,  506,  637,  642,  641,  505,  640,
 /*  1070 */   639,  581,    1,   55,   53,   56,   49,   51,   50,   54,
 /*  1080 */    57,   48,   52,  609,  196,   59,   58,  612,  265,  607,
 /*  1090 */   192,   62,  606,  304,  604,  608,  330,   17,  603,   46,
 /*  1100 */   153,  158,   38,  428,   55,   53,   56,   49,   51,   50,
 /*  1110 */    54,   57,   48,   52,  492,  488,   59,   58,  480,  430,
 /*  1120 */   439,  132,   62,  440,  438,  456,  441,  642,  641,  451,
 /*  1130 */   640,  639,  571,  436,   55,   53,   56,   49,   51,   50,
 /*  1140 */    54,   57,   48,   52,  254,  448,   59,   58,  447,  578,
 /*  1150 */   635,  445,   62,  636,  435,  432,  637,  642,  641,  437,
 /*  1160 */   640,  639,  431,  427,  425,  246,  142,  422,   55,   53,
 /*  1170 */    56,   49,   51,   50,   54,   57,   48,   52,  182,  324,
 /*  1180 */    59,   58,  420,  419,  191,  418,   62,  179,   55,   53,
 /*  1190 */    56,   49,   51,   50,   54,   57,   48,   52,  414,  412,
 /*  1200 */    59,   58,  407,   90,  390,  555,   62,  389,   55,   53,
 /*  1210 */    56,   49,   51,   50,   54,   57,   48,   52,  556,  542,
 /*  1220 */    59,   58,   14,  205,  554,  376,   62,   13,  375,  577,
 /*  1230 */   635,  507,  342,  636,  103,  212,  637,  642,  641,  343,
 /*  1240 */   640,  639,   96,    6,  344,   55,   53,   56,   49,   51,
 /*  1250 */    50,   54,   57,   48,   52,  345,   64,   59,   58,  137,
 /*  1260 */   602,  520,  620,   62,  564,  563,   67,  519,   31,  509,
 /*  1270 */    55,   53,   56,   49,   51,   50,   54,   57,   48,   52,
 /*  1280 */   482,   66,   59,   58,  479,  332,  575,  635,   62,  991,
 /*  1290 */   636,  991,  991,  637,  642,  641,  154,  640,  639,  991,
 /*  1300 */   991,  991,  991,  991,  518,  991,  483,   20,  486,  485,
 /*  1310 */   484,  991,  991,  991,  991,  991,  154,   30,  991,  647,
 /*  1320 */   646,  645,  644,  118,  518,  991,  391,  609,  991,  991,
 /*  1330 */   991,  612,  282,  607,  991,  991,  606,  991,  604,  608,
 /*  1340 */   991,  991,  603,   46,  153,  158,  991,  991,  475,  465,
 /*  1350 */   474,  477,  335,  473,  991,  652,  991,  476,  318,  991,
 /*  1360 */   991,  991,  991,  504,  503,  502,  501,  500,  499,  498,
 /*  1370 */   497,  496,  495,  517,  991,  991,  991,  991,  129,  991,
 /*  1380 */   991,  991,  991,  504,  503,  502,  501,  500,  499,  498,
 /*  1390 */   497,  496,  495,  489,  991,  574,  635,  991,  129,  636,
 /*  1400 */   991,  991,  637,  642,  641,  991,  640,  639,  991,  246,
 /*  1410 */   991,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*  1420 */    52,  309,  362,   59,   58,  475,  464,  474,  477,   62,
 /*  1430 */   473,  609,  991,  991,  476,  612,  280,  607,  991,   34,
 /*  1440 */   606,    7,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1450 */   991,  220,  622,  991,  991,  991,  991,  991,  991,  991,
 /*  1460 */   609,  621,   33,  991,  612,  589,  607,  991,  991,  606,
 /*  1470 */   317,  604,  608,  991,  991,  603,   46,  153,  158,  991,
 /*  1480 */   991,  991,  991,  991,  991,  991,  510,  991,  991,  991,
 /*  1490 */   128,  609,  166,  991,  991,  612,  270,  607,  652,  211,
 /*  1500 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1510 */   609,  991,  991,  246,  612,  269,  607,  991,  991,  606,
 /*  1520 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  475,
 /*  1530 */   463,  474,  477,  991,  473,  991,  991,  991,  476,  609,
 /*  1540 */   991,  991,  246,  612,  406,  607,  991,  991,  606,  991,
 /*  1550 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  991,
 /*  1560 */   991,  991,  991,  609,  991,  991,  991,  612,  405,  607,
 /*  1570 */   991,  991,  606,  246,  604,  608,  991,  991,  603,   46,
 /*  1580 */   153,  158,  475,  201,  474,  477,  991,  473,  991,  609,
 /*  1590 */   991,  476,  246,  612,  300,  607,  991,  991,  606,  991,
 /*  1600 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  991,
 /*  1610 */   991,  991,  991,  609,  991,  991,  991,  612,  299,  607,
 /*  1620 */   991,  246,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  1630 */   153,  158,  991,  991,  991,  609,  991,  991,  991,  612,
 /*  1640 */   298,  607,  991,  991,  606,  246,  604,  608,  991,  991,
 /*  1650 */   603,   46,  153,  158,  227,  635,  991,  991,  636,  991,
 /*  1660 */   991,  637,  642,  641,  991,  640,  639,  991,  991,  609,
 /*  1670 */   991,  246,  991,  612,  297,  607,  991,  991,  606,  991,
 /*  1680 */   604,  608,  991,  991,  603,   46,  153,  158,  475,  127,
 /*  1690 */   474,  477,  991,  473,  609,  246,  991,  476,  612,  296,
 /*  1700 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  1710 */    46,  153,  158,  991,  991,  991,  609,  246,  991,  991,
 /*  1720 */   612,  295,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  1730 */   991,  603,   46,  153,  158,  991,  991,  991,  991,  991,
 /*  1740 */   991,  991,  991,  991,  609,  991,  991,  991,  612,  294,
 /*  1750 */   607,  246,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  1760 */    46,  153,  158,  991,  991,  991,  609,  991,  991,  991,
 /*  1770 */   612,  293,  607,  991,  991,  606,  246,  604,  608,  991,
 /*  1780 */   991,  603,   46,  153,  158,  991,  991,  991,  991,  991,
 /*  1790 */   991,  991,  991,  991,  991,  991,  609,  991,  246,  991,
 /*  1800 */   612,  292,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  1810 */   991,  603,   46,  153,  158,  991,  991,  991,  991,  991,
 /*  1820 */   991,  991,  991,  991,  991,  609,  246,  991,  991,  612,
 /*  1830 */   291,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  1840 */   603,   46,  153,  158,  991,  991,  991,  609,  246,  991,
 /*  1850 */   991,  612,  281,  607,  991,  991,  606,  991,  604,  608,
 /*  1860 */   991,  991,  603,   46,  153,  158,  991,  991,  991,  991,
 /*  1870 */   991,  609,  991,  991,  991,  612,  268,  607,  246,  991,
 /*  1880 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1890 */   991,  991,  991,  609,  991,  991,  991,  612,  267,  607,
 /*  1900 */   991,  991,  606,  991,  604,  608,  991,  246,  603,   46,
 /*  1910 */   153,  158,  991,  991,  349,  635,  991,  991,  636,  991,
 /*  1920 */   991,  637,  642,  641,  991,  640,  639,  609,  991,  246,
 /*  1930 */   991,  612,  266,  607,  991,  991,  606,  991,  604,  608,
 /*  1940 */   991,  991,  603,   46,  153,  158,  991,  991,  991,  991,
 /*  1950 */   991,  991,  609,  246,  991,  991,  612,  279,  607,  991,
 /*  1960 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  153,
 /*  1970 */   158,  991,  991,  991,  609,  246,  991,  991,  612,  278,
 /*  1980 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  1990 */    46,  153,  158,  991,  991,  991,  991,  991,  991,  991,
 /*  2000 */   991,  991,  609,  991,  991,  991,  612,  264,  607,  246,
 /*  2010 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  153,
 /*  2020 */   158,  991,  991,  991,  609,  991,  991,  991,  612,  263,
 /*  2030 */   607,  991,  991,  606,  246,  604,  608,  991,  991,  603,
 /*  2040 */    46,  153,  158,  991,  991,  991,  991,  991,  991,  991,
 /*  2050 */   991,  991,  991,  991,  609,  991,  246,  991,  612,  262,
 /*  2060 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  2070 */    46,  153,  158,  991,  991,  991,  991,  991,  991,  991,
 /*  2080 */   991,  991,  991,  609,  246,  991,  991,  612,  261,  607,
 /*  2090 */   991,  991,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  2100 */   153,  158,  991,  991,  991,  609,  246,  991,  991,  612,
 /*  2110 */   277,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2120 */   603,   46,  153,  158,  991,  991,  991,  991,  991,  609,
 /*  2130 */   991,  991,  991,  612,  150,  607,  246,  991,  606,  991,
 /*  2140 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  991,
 /*  2150 */   991,  609,  991,  991,  991,  612,  149,  607,  991,  991,
 /*  2160 */   606,  991,  604,  608,  991,  246,  603,   46,  153,  158,
 /*  2170 */   991,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2180 */   991,  991,  991,  991,  991,  609,  991,  246,  991,  612,
 /*  2190 */   260,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2200 */   603,   46,  153,  158,  991,  991,  991,  991,  991,  991,
 /*  2210 */   609,  246,  991,  991,  612,  259,  607,  991,  991,  606,
 /*  2220 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  991,
 /*  2230 */   991,  991,  609,  246,  991,  991,  612,  258,  607,  991,
 /*  2240 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  153,
 /*  2250 */   158,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2260 */   609,  991,  991,  991,  612,  148,  607,  246,  991,  606,
 /*  2270 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  991,
 /*  2280 */   991,  991,  609,  991,  991,  991,  612,  276,  607,  991,
 /*  2290 */   991,  606,  246,  604,  608,  991,  991,  603,   46,  153,
 /*  2300 */   158,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2310 */   991,  991,  609,  991,  246,  991,  612,  257,  607,  991,
 /*  2320 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  153,
 /*  2330 */   158,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2340 */   991,  609,  246,  991,  991,  612,  275,  607,  991,  991,
 /*  2350 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  2360 */   991,  991,  991,  609,  246,  991,  991,  612,  274,  607,
 /*  2370 */   991,  991,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  2380 */   153,  158,  991,  991,  991,  991,  991,  609,  991,  991,
 /*  2390 */   991,  612,  273,  607,  246,  991,  606,  991,  604,  608,
 /*  2400 */   991,  991,  603,   46,  153,  158,  991,  991,  991,  609,
 /*  2410 */   309,  362,  991,  612,  147,  607,  991,  991,  606,  991,
 /*  2420 */   604,  608,  991,  246,  603,   46,  153,  158,   34,  991,
 /*  2430 */     7,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2440 */   220,  622,  991,  609,  991,  246,  991,  612,  271,  607,
 /*  2450 */   621,   33,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  2460 */   153,  158,  991,  991,  991,  991,  991,  991,  991,  246,
 /*  2470 */   991,  991,  991,  991,  991,  510,  991,  991,  991,  128,
 /*  2480 */   991,  166,  991,  609,  991,  991,  991,  612,  211,  607,
 /*  2490 */   991,  246,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  2500 */   283,  158,  991,  991,  991,  609,  991,  991,  991,  612,
 /*  2510 */   991,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2520 */   603,   46,  402,  158,  991,  246,  609,  991,  991,  991,
 /*  2530 */   612,  991,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  2540 */   991,  603,   46,  401,  158,  609,  991,  991,  991,  612,
 /*  2550 */   991,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2560 */   603,   46,  400,  158,  991,  246,  991,  609,  991,  991,
 /*  2570 */   991,  612,  991,  607,  991,  991,  606,  991,  604,  608,
 /*  2580 */   991,  991,  603,   46,  399,  158,  609,  246,  991,  991,
 /*  2590 */   612,  991,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  2600 */   991,  603,   46,  398,  158,  991,  609,  991,  246,  991,
 /*  2610 */   612,  991,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  2620 */   991,  603,   46,  397,  158,  609,  991,  246,  991,  612,
 /*  2630 */   991,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2640 */   603,   46,  287,  158,  609,  991,  991,  991,  612,  246,
 /*  2650 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  2660 */    46,  286,  158,  991,  991,  609,  991,  991,  246,  612,
 /*  2670 */   991,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2680 */   603,   46,  146,  158,  991,  991,  991,  609,  246,  991,
 /*  2690 */   991,  612,  991,  607,  991,  991,  606,  991,  604,  608,
 /*  2700 */   991,  991,  603,   46,  145,  158,  609,  246,  991,  991,
 /*  2710 */   612,  991,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  2720 */   991,  603,   46,  152,  158,  609,  246,  991,  991,  612,
 /*  2730 */   991,  607,  991,  991,  606,  991,  604,  608,  991,  991,
 /*  2740 */   603,   46,  284,  158,  609,  991,  991,  246,  612,  991,
 /*  2750 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  2760 */    46,  285,  158,  991,  609,  991,  991,  991,  612,  246,
 /*  2770 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  2780 */    46,  991,  156,  609,  991,  991,  991,  612,  246,  607,
 /*  2790 */   991,  991,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  2800 */   991,  155,  991,  991,  991,  991,  991,  246,  991,  991,
 /*  2810 */   991,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2820 */   991,  991,  991,  991,  991,  991,  246,  991,  991,  991,
 /*  2830 */   991,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2840 */   991,  991,  991,  991,  991,  991,  246,  991,  991,  991,
 /*  2850 */   991,  991,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2860 */   991,  991,  991,  991,  991,  246,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    11,   12,  128,   11,   12,   30,   31,   15,   16,   17,
 /*    10 */    18,   80,   23,   21,   22,   26,   27,   28,  177,  178,
 /*    20 */    27,  122,  123,   34,  128,  126,  185,   34,   30,  130,
 /*    30 */   131,  132,   28,  192,  135,   31,  137,  138,   49,  128,
 /*    40 */   141,  142,  143,  144,   33,   56,   57,   13,   14,  169,
 /*    50 */    11,   12,    0,   19,   65,   33,  182,  235,   65,   70,
 /*    60 */   161,  239,   23,   74,  170,   26,   27,   74,   29,   80,
 /*    70 */    24,   25,   26,   34,  163,  164,   87,   88,   89,   90,
 /*    80 */    91,  135,  260,  261,  262,  263,  264,   94,   49,  100,
 /*    90 */   166,  145,  181,   19,   20,   56,   57,   19,   20,  169,
 /*   100 */    11,   12,   28,  229,   65,  211,   28,  208,   29,   70,
 /*   110 */    31,   34,   23,   74,  190,   26,   27,   40,  166,   80,
 /*   120 */   168,   27,  176,   34,   30,  229,   87,   88,   89,   90,
 /*   130 */    91,   79,  211,  212,  213,  214,   27,  216,   49,  100,
 /*   140 */    51,  220,  190,   19,   20,   56,   57,  267,  268,   72,
 /*   150 */    11,   12,   28,  207,   65,  211,  212,  213,  214,   70,
 /*   160 */   216,  109,   23,   74,  220,   26,   27,   19,   34,   80,
 /*   170 */    24,   25,   26,   34,   19,   20,   87,   88,   89,   90,
 /*   180 */    91,  211,  212,  213,  214,   39,  216,   28,   49,  100,
 /*   190 */   220,   24,   25,   26,   60,   56,   57,  267,  268,   65,
 /*   200 */    11,   12,   30,   31,   65,  211,  212,  213,  214,   70,
 /*   210 */   216,   27,   23,   74,  220,   26,   27,  271,   28,   80,
 /*   220 */   111,   31,  135,   34,  241,  265,   87,   88,   89,   90,
 /*   230 */    91,  241,  145,  273,  251,  252,  253,   34,   49,  100,
 /*   240 */   210,  211,   34,  253,  214,   56,   57,  217,  218,  219,
 /*   250 */    28,  221,  222,   31,   65,  125,  126,  274,  127,   70,
 /*   260 */   130,  131,  132,   74,  274,  135,  135,  137,  138,   80,
 /*   270 */   111,  141,  142,  143,  144,  173,   87,   88,   89,   90,
 /*   280 */    91,    1,    2,    3,   34,    5,    6,    7,    8,  100,
 /*   290 */    10,   11,   12,  135,  207,   15,   16,   17,   18,  178,
 /*   300 */    22,   21,   22,  172,  127,  126,  185,  205,  206,  130,
 /*   310 */   131,  132,  135,  192,  135,   65,  137,  138,   77,   78,
 /*   320 */   141,  142,  143,  144,   74,  194,  195,  196,  197,  198,
 /*   330 */   199,  200,  201,  202,  203,  204,  135,   58,  208,   34,
 /*   340 */   209,  162,  113,  114,  115,   26,   62,   30,  135,  172,
 /*   350 */    33,  127,   33,   69,   15,   16,   17,   18,  271,  135,
 /*   360 */    21,   22,   83,   79,   85,  207,   30,   31,  237,   34,
 /*   370 */    65,  194,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   380 */   203,  204,   98,   26,   30,   31,  209,  208,   11,   12,
 /*   390 */    33,  107,   15,   16,   17,   18,  172,  127,   21,   22,
 /*   400 */    58,  117,   27,   66,   62,  135,   29,   30,  135,  127,
 /*   410 */   231,   69,   30,   28,  237,  133,   31,  135,  194,  195,
 /*   420 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  271,
 /*   430 */   184,   31,   95,  209,   92,   28,   28,  126,   31,   31,
 /*   440 */    98,  130,  172,  132,  243,  244,  135,   30,  137,  138,
 /*   450 */   101,   26,  141,  142,  127,  144,  243,  244,   33,  117,
 /*   460 */   167,  237,  135,   34,  194,  195,  196,  197,  198,  199,
 /*   470 */   200,  201,  202,  203,  204,   32,  230,   34,   33,  209,
 /*   480 */   207,   38,   27,   40,   41,   42,   43,   44,   45,   46,
 /*   490 */    47,   11,   12,  135,   65,   15,   16,   17,   18,  172,
 /*   500 */   127,   21,   22,  145,  124,  159,  160,  237,  135,   29,
 /*   510 */    28,  111,   27,   31,   71,   72,   73,  174,  175,  208,
 /*   520 */    66,  194,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   530 */   203,  204,  211,  212,  213,  214,  209,  216,  158,  170,
 /*   540 */    26,  220,   95,   26,  271,  172,  127,   33,  255,   34,
 /*   550 */    33,  276,  277,   38,  135,   40,   41,   42,   43,   44,
 /*   560 */    45,   46,   47,  179,  237,  207,  111,  194,  195,  196,
 /*   570 */   197,  198,  199,  200,  201,  202,  203,  204,   63,  135,
 /*   580 */   211,   79,  209,  174,  175,   34,   71,   72,  126,  145,
 /*   590 */   127,  172,  130,  131,  132,   26,  111,  135,  135,  137,
 /*   600 */   138,   26,   33,  141,  142,  143,  144,   33,   33,   25,
 /*   610 */   237,  109,  228,  194,  195,  196,  197,  198,  199,  200,
 /*   620 */   201,  202,  203,  204,  147,  110,   26,   34,  209,  271,
 /*   630 */   210,  211,   24,   33,  214,  172,  127,  217,  218,  219,
 /*   640 */   139,  221,  222,  156,  135,  211,  212,  213,  214,   34,
 /*   650 */   216,  207,  257,  258,  220,   30,  237,  194,  195,  196,
 /*   660 */   197,  198,  199,  200,  201,  202,  203,  204,  211,  134,
 /*   670 */   208,  120,  209,  148,  149,  150,  151,   26,  153,   34,
 /*   680 */   223,  172,   34,   38,   33,   40,   41,   42,   43,   44,
 /*   690 */    45,   46,   47,  231,  148,  149,   25,   39,   34,  153,
 /*   700 */   237,  155,   24,  194,  195,  196,  197,  198,  199,  200,
 /*   710 */   201,  202,  203,  204,   34,  271,   71,   72,  209,  185,
 /*   720 */   148,  149,   30,   33,  237,  153,  192,  155,  193,   65,
 /*   730 */    11,   12,   33,  256,   15,   16,   17,   18,  237,   36,
 /*   740 */    21,   22,   34,   61,   33,   27,  237,  148,  149,  115,
 /*   750 */    33,   27,  153,   27,  155,   33,   27,   34,   37,  234,
 /*   760 */    55,   34,   77,  104,   36,  240,  241,  242,   49,  111,
 /*   770 */   245,  246,  247,  248,  249,   39,  104,  135,  232,  233,
 /*   780 */   234,  235,  236,   30,   53,   34,  111,  145,   30,   39,
 /*   790 */    30,  245,  246,  247,  248,  249,   59,   34,   33,   33,
 /*   800 */    33,   33,   33,  278,  232,  233,  234,  235,  236,   34,
 /*   810 */    34,   30,   97,   93,  116,   33,   27,  245,  246,  247,
 /*   820 */   248,  249,    1,   68,  278,   34,   36,   27,   84,   34,
 /*   830 */    82,  232,  233,  234,  235,  236,  106,   34,   84,   34,
 /*   840 */   108,   34,  238,  270,  245,  246,  247,  248,  249,  207,
 /*   850 */   278,  269,    1,    2,    3,    4,    5,    6,    7,    8,
 /*   860 */     9,   10,  152,  154,   13,   14,  239,  210,  211,  237,
 /*   870 */    19,  214,  111,  237,  217,  218,  219,  278,  221,  222,
 /*   880 */   226,   30,   26,    1,    2,    3,    4,    5,    6,    7,
 /*   890 */     8,    9,   10,   27,  156,   13,   14,  140,   27,  140,
 /*   900 */   188,   19,  140,    1,    2,    3,    4,    5,    6,    7,
 /*   910 */     8,    9,   10,  271,   96,   13,   14,  211,  212,  213,
 /*   920 */   214,   19,  216,  140,  191,   95,  220,  188,  136,   39,
 /*   930 */    28,  191,   50,  237,    1,    2,    3,    4,    5,    6,
 /*   940 */     7,    8,    9,   10,  180,   86,   13,   14,   95,   34,
 /*   950 */    34,  237,   19,  183,    1,    2,    3,    4,    5,    6,
 /*   960 */     7,    8,    9,   10,   66,  113,   13,   14,  189,   11,
 /*   970 */    12,  228,   19,   15,   16,   17,   18,  173,  237,   21,
 /*   980 */    22,   28,  211,    1,    2,    3,    4,    5,    6,    7,
 /*   990 */     8,    9,   10,  237,  147,   13,   14,  211,  211,  211,
 /*  1000 */   120,   19,  118,  146,  211,   34,  146,  169,   50,  237,
 /*  1010 */    28,  237,    1,    2,    3,    4,    5,    6,    7,    8,
 /*  1020 */     9,   10,  237,   34,   13,   14,   34,  254,  191,  167,
 /*  1030 */    19,  211,  212,  213,  214,  102,  216,  191,  237,  237,
 /*  1040 */   220,   30,   27,    1,    2,    3,    4,    5,    6,    7,
 /*  1050 */     8,    9,   10,  237,  237,   13,   14,  171,  210,  211,
 /*  1060 */   237,   19,  214,  237,  237,  217,  218,  219,  237,  221,
 /*  1070 */   222,   29,  237,    1,    2,    3,    4,    5,    6,    7,
 /*  1080 */     8,    9,   10,  126,  272,   13,   14,  130,  131,  132,
 /*  1090 */    27,   19,  135,  169,  137,  138,  174,  119,  141,  142,
 /*  1100 */   143,  144,   30,  230,    1,    2,    3,    4,    5,    6,
 /*  1110 */     7,    8,    9,   10,  237,  237,   13,   14,  237,   34,
 /*  1120 */   211,   27,   19,  214,  215,  237,  217,  218,  219,  237,
 /*  1130 */   221,  222,   29,  224,    1,    2,    3,    4,    5,    6,
 /*  1140 */     7,    8,    9,   10,  237,  237,   13,   14,  237,  210,
 /*  1150 */   211,  237,   19,  214,  237,  237,  217,  218,  219,  250,
 /*  1160 */   221,  222,  237,  237,  237,  208,   33,  257,    1,    2,
 /*  1170 */     3,    4,    5,    6,    7,    8,    9,   10,  259,   34,
 /*  1180 */    13,   14,  237,  237,  275,  237,   19,  259,    1,    2,
 /*  1190 */     3,    4,    5,    6,    7,    8,    9,   10,  237,  237,
 /*  1200 */    13,   14,  227,  187,  227,  237,   19,  227,    1,    2,
 /*  1210 */     3,    4,    5,    6,    7,    8,    9,   10,  192,  128,
 /*  1220 */    13,   14,  148,  149,  237,  227,   19,  153,  227,  210,
 /*  1230 */   211,  237,  226,  214,  187,   28,  217,  218,  219,  226,
 /*  1240 */   221,  222,  187,   76,  226,    1,    2,    3,    4,    5,
 /*  1250 */     6,    7,    8,    9,   10,  226,  225,   13,   14,  237,
 /*  1260 */   193,  193,  266,   19,  237,  237,  186,  237,   81,  129,
 /*  1270 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*  1280 */   237,  186,   13,   14,   67,   34,  210,  211,   19,  279,
 /*  1290 */   214,  279,  279,  217,  218,  219,  127,  221,  222,  279,
 /*  1300 */   279,  279,  279,  279,  135,  279,  232,  233,  234,  235,
 /*  1310 */   236,  279,  279,  279,  279,  279,  127,   48,  279,  245,
 /*  1320 */   246,  247,  248,  249,  135,  279,  125,  126,  279,  279,
 /*  1330 */   279,  130,  131,  132,  279,  279,  135,  279,  137,  138,
 /*  1340 */   279,  279,  141,  142,  143,  144,  279,  279,  211,  212,
 /*  1350 */   213,  214,  278,  216,  279,  111,  279,  220,  157,  279,
 /*  1360 */   279,  279,  279,  194,  195,  196,  197,  198,  199,  200,
 /*  1370 */   201,  202,  203,  204,  279,  279,  279,  279,  209,  279,
 /*  1380 */   279,  279,  279,  194,  195,  196,  197,  198,  199,  200,
 /*  1390 */   201,  202,  203,  204,  279,  210,  211,  279,  209,  214,
 /*  1400 */   279,  279,  217,  218,  219,  279,  221,  222,  279,  208,
 /*  1410 */   279,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*  1420 */    10,   34,   35,   13,   14,  211,  212,  213,  214,   19,
 /*  1430 */   216,  126,  279,  279,  220,  130,  131,  132,  279,   52,
 /*  1440 */   135,   54,  137,  138,  279,  279,  141,  142,  143,  144,
 /*  1450 */   279,   64,   65,  279,  279,  279,  279,  279,  279,  279,
 /*  1460 */   126,   74,   75,  279,  130,  131,  132,  279,  279,  135,
 /*  1470 */   165,  137,  138,  279,  279,  141,  142,  143,  144,  279,
 /*  1480 */   279,  279,  279,  279,  279,  279,   99,  279,  279,  279,
 /*  1490 */   103,  126,  105,  279,  279,  130,  131,  132,  111,  112,
 /*  1500 */   135,  279,  137,  138,  279,  279,  141,  142,  143,  144,
 /*  1510 */   126,  279,  279,  208,  130,  131,  132,  279,  279,  135,
 /*  1520 */   279,  137,  138,  279,  279,  141,  142,  143,  144,  211,
 /*  1530 */   212,  213,  214,  279,  216,  279,  279,  279,  220,  126,
 /*  1540 */   279,  279,  208,  130,  131,  132,  279,  279,  135,  279,
 /*  1550 */   137,  138,  279,  279,  141,  142,  143,  144,  279,  279,
 /*  1560 */   279,  279,  279,  126,  279,  279,  279,  130,  131,  132,
 /*  1570 */   279,  279,  135,  208,  137,  138,  279,  279,  141,  142,
 /*  1580 */   143,  144,  211,  212,  213,  214,  279,  216,  279,  126,
 /*  1590 */   279,  220,  208,  130,  131,  132,  279,  279,  135,  279,
 /*  1600 */   137,  138,  279,  279,  141,  142,  143,  144,  279,  279,
 /*  1610 */   279,  279,  279,  126,  279,  279,  279,  130,  131,  132,
 /*  1620 */   279,  208,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  1630 */   143,  144,  279,  279,  279,  126,  279,  279,  279,  130,
 /*  1640 */   131,  132,  279,  279,  135,  208,  137,  138,  279,  279,
 /*  1650 */   141,  142,  143,  144,  210,  211,  279,  279,  214,  279,
 /*  1660 */   279,  217,  218,  219,  279,  221,  222,  279,  279,  126,
 /*  1670 */   279,  208,  279,  130,  131,  132,  279,  279,  135,  279,
 /*  1680 */   137,  138,  279,  279,  141,  142,  143,  144,  211,  212,
 /*  1690 */   213,  214,  279,  216,  126,  208,  279,  220,  130,  131,
 /*  1700 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  1710 */   142,  143,  144,  279,  279,  279,  126,  208,  279,  279,
 /*  1720 */   130,  131,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  1730 */   279,  141,  142,  143,  144,  279,  279,  279,  279,  279,
 /*  1740 */   279,  279,  279,  279,  126,  279,  279,  279,  130,  131,
 /*  1750 */   132,  208,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  1760 */   142,  143,  144,  279,  279,  279,  126,  279,  279,  279,
 /*  1770 */   130,  131,  132,  279,  279,  135,  208,  137,  138,  279,
 /*  1780 */   279,  141,  142,  143,  144,  279,  279,  279,  279,  279,
 /*  1790 */   279,  279,  279,  279,  279,  279,  126,  279,  208,  279,
 /*  1800 */   130,  131,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  1810 */   279,  141,  142,  143,  144,  279,  279,  279,  279,  279,
 /*  1820 */   279,  279,  279,  279,  279,  126,  208,  279,  279,  130,
 /*  1830 */   131,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  1840 */   141,  142,  143,  144,  279,  279,  279,  126,  208,  279,
 /*  1850 */   279,  130,  131,  132,  279,  279,  135,  279,  137,  138,
 /*  1860 */   279,  279,  141,  142,  143,  144,  279,  279,  279,  279,
 /*  1870 */   279,  126,  279,  279,  279,  130,  131,  132,  208,  279,
 /*  1880 */   135,  279,  137,  138,  279,  279,  141,  142,  143,  144,
 /*  1890 */   279,  279,  279,  126,  279,  279,  279,  130,  131,  132,
 /*  1900 */   279,  279,  135,  279,  137,  138,  279,  208,  141,  142,
 /*  1910 */   143,  144,  279,  279,  210,  211,  279,  279,  214,  279,
 /*  1920 */   279,  217,  218,  219,  279,  221,  222,  126,  279,  208,
 /*  1930 */   279,  130,  131,  132,  279,  279,  135,  279,  137,  138,
 /*  1940 */   279,  279,  141,  142,  143,  144,  279,  279,  279,  279,
 /*  1950 */   279,  279,  126,  208,  279,  279,  130,  131,  132,  279,
 /*  1960 */   279,  135,  279,  137,  138,  279,  279,  141,  142,  143,
 /*  1970 */   144,  279,  279,  279,  126,  208,  279,  279,  130,  131,
 /*  1980 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  1990 */   142,  143,  144,  279,  279,  279,  279,  279,  279,  279,
 /*  2000 */   279,  279,  126,  279,  279,  279,  130,  131,  132,  208,
 /*  2010 */   279,  135,  279,  137,  138,  279,  279,  141,  142,  143,
 /*  2020 */   144,  279,  279,  279,  126,  279,  279,  279,  130,  131,
 /*  2030 */   132,  279,  279,  135,  208,  137,  138,  279,  279,  141,
 /*  2040 */   142,  143,  144,  279,  279,  279,  279,  279,  279,  279,
 /*  2050 */   279,  279,  279,  279,  126,  279,  208,  279,  130,  131,
 /*  2060 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  2070 */   142,  143,  144,  279,  279,  279,  279,  279,  279,  279,
 /*  2080 */   279,  279,  279,  126,  208,  279,  279,  130,  131,  132,
 /*  2090 */   279,  279,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  2100 */   143,  144,  279,  279,  279,  126,  208,  279,  279,  130,
 /*  2110 */   131,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2120 */   141,  142,  143,  144,  279,  279,  279,  279,  279,  126,
 /*  2130 */   279,  279,  279,  130,  131,  132,  208,  279,  135,  279,
 /*  2140 */   137,  138,  279,  279,  141,  142,  143,  144,  279,  279,
 /*  2150 */   279,  126,  279,  279,  279,  130,  131,  132,  279,  279,
 /*  2160 */   135,  279,  137,  138,  279,  208,  141,  142,  143,  144,
 /*  2170 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2180 */   279,  279,  279,  279,  279,  126,  279,  208,  279,  130,
 /*  2190 */   131,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2200 */   141,  142,  143,  144,  279,  279,  279,  279,  279,  279,
 /*  2210 */   126,  208,  279,  279,  130,  131,  132,  279,  279,  135,
 /*  2220 */   279,  137,  138,  279,  279,  141,  142,  143,  144,  279,
 /*  2230 */   279,  279,  126,  208,  279,  279,  130,  131,  132,  279,
 /*  2240 */   279,  135,  279,  137,  138,  279,  279,  141,  142,  143,
 /*  2250 */   144,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2260 */   126,  279,  279,  279,  130,  131,  132,  208,  279,  135,
 /*  2270 */   279,  137,  138,  279,  279,  141,  142,  143,  144,  279,
 /*  2280 */   279,  279,  126,  279,  279,  279,  130,  131,  132,  279,
 /*  2290 */   279,  135,  208,  137,  138,  279,  279,  141,  142,  143,
 /*  2300 */   144,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2310 */   279,  279,  126,  279,  208,  279,  130,  131,  132,  279,
 /*  2320 */   279,  135,  279,  137,  138,  279,  279,  141,  142,  143,
 /*  2330 */   144,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2340 */   279,  126,  208,  279,  279,  130,  131,  132,  279,  279,
 /*  2350 */   135,  279,  137,  138,  279,  279,  141,  142,  143,  144,
 /*  2360 */   279,  279,  279,  126,  208,  279,  279,  130,  131,  132,
 /*  2370 */   279,  279,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  2380 */   143,  144,  279,  279,  279,  279,  279,  126,  279,  279,
 /*  2390 */   279,  130,  131,  132,  208,  279,  135,  279,  137,  138,
 /*  2400 */   279,  279,  141,  142,  143,  144,  279,  279,  279,  126,
 /*  2410 */    34,   35,  279,  130,  131,  132,  279,  279,  135,  279,
 /*  2420 */   137,  138,  279,  208,  141,  142,  143,  144,   52,  279,
 /*  2430 */    54,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2440 */    64,   65,  279,  126,  279,  208,  279,  130,  131,  132,
 /*  2450 */    74,   75,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  2460 */   143,  144,  279,  279,  279,  279,  279,  279,  279,  208,
 /*  2470 */   279,  279,  279,  279,  279,   99,  279,  279,  279,  103,
 /*  2480 */   279,  105,  279,  126,  279,  279,  279,  130,  112,  132,
 /*  2490 */   279,  208,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  2500 */   143,  144,  279,  279,  279,  126,  279,  279,  279,  130,
 /*  2510 */   279,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2520 */   141,  142,  143,  144,  279,  208,  126,  279,  279,  279,
 /*  2530 */   130,  279,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  2540 */   279,  141,  142,  143,  144,  126,  279,  279,  279,  130,
 /*  2550 */   279,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2560 */   141,  142,  143,  144,  279,  208,  279,  126,  279,  279,
 /*  2570 */   279,  130,  279,  132,  279,  279,  135,  279,  137,  138,
 /*  2580 */   279,  279,  141,  142,  143,  144,  126,  208,  279,  279,
 /*  2590 */   130,  279,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  2600 */   279,  141,  142,  143,  144,  279,  126,  279,  208,  279,
 /*  2610 */   130,  279,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  2620 */   279,  141,  142,  143,  144,  126,  279,  208,  279,  130,
 /*  2630 */   279,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2640 */   141,  142,  143,  144,  126,  279,  279,  279,  130,  208,
 /*  2650 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  2660 */   142,  143,  144,  279,  279,  126,  279,  279,  208,  130,
 /*  2670 */   279,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2680 */   141,  142,  143,  144,  279,  279,  279,  126,  208,  279,
 /*  2690 */   279,  130,  279,  132,  279,  279,  135,  279,  137,  138,
 /*  2700 */   279,  279,  141,  142,  143,  144,  126,  208,  279,  279,
 /*  2710 */   130,  279,  132,  279,  279,  135,  279,  137,  138,  279,
 /*  2720 */   279,  141,  142,  143,  144,  126,  208,  279,  279,  130,
 /*  2730 */   279,  132,  279,  279,  135,  279,  137,  138,  279,  279,
 /*  2740 */   141,  142,  143,  144,  126,  279,  279,  208,  130,  279,
 /*  2750 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  2760 */   142,  143,  144,  279,  126,  279,  279,  279,  130,  208,
 /*  2770 */   132,  279,  279,  135,  279,  137,  138,  279,  279,  141,
 /*  2780 */   142,  279,  144,  126,  279,  279,  279,  130,  208,  132,
 /*  2790 */   279,  279,  135,  279,  137,  138,  279,  279,  141,  142,
 /*  2800 */   279,  144,  279,  279,  279,  279,  279,  208,  279,  279,
 /*  2810 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2820 */   279,  279,  279,  279,  279,  279,  208,  279,  279,  279,
 /*  2830 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2840 */   279,  279,  279,  279,  279,  279,  208,  279,  279,  279,
 /*  2850 */   279,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2860 */   279,  279,  279,  279,  279,  208,
};
#define YY_SHIFT_USE_DFLT (-70)
#define YY_SHIFT_COUNT (410)
#define YY_SHIFT_MIN   (-69)
#define YY_SHIFT_MAX   (2376)
static const short yy_shift_ofst[] = {
 /*     0 */   502, 1387, 1387, 1387, 1387, 1387, 1387, 1387, 1387, 1387,
 /*    10 */    89,  284,  342,  342,  342,  284,   39,  189, 2376, 2376,
 /*    20 */   342,  -11,  189,  139,  139,  139,  139,  139,  139,  139,
 /*    30 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    40 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    50 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    60 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    70 */   139,  139,  139,  139,  139,  139,  139,  139,  515,  139,
 /*    80 */   139,  139,  645,  645,  645,  645,  645,  645,  645,  645,
 /*    90 */   645,  645,  279,  443,  443,  443,  443,  443,  443,  443,
 /*   100 */   443,  443,  443,  443,  443,  443,   -7,   -7,   -7,   -7,
 /*   110 */    -7,  134,  664,   -7,   -7,  250,  250,  250,  229,   52,
 /*   120 */   664,  429,  989,  989, 1217,  167,   77,  658,  551,  485,
 /*   130 */   305,  429, 1145, 1085,  978,  916, 1251, 1217, 1015,  916,
 /*   140 */   915,  978,  -70,  -70,  -70,  280,  280, 1244, 1269, 1244,
 /*   150 */  1244, 1244,  958,  719,  146,   46,   46,   46,   46,  159,
 /*   160 */   337,  651,  600,  575,  337,  569,  455,  429,  517,  514,
 /*   170 */   400,  305,  400,  425,  357,  319,  337,  761,  761,  761,
 /*   180 */  1094,  761,  761, 1145, 1094,  761,  761, 1085,  761,  978,
 /*   190 */   761,  761,  989, 1063,  761,  761, 1015,  992,  761,  761,
 /*   200 */   761,  761,  830,  830,  989,  971,  761,  761,  761,  761,
 /*   210 */   884,  761,  761,  761,  761,  884,  880,  761,  761,  761,
 /*   220 */   761,  761,  761,  761,  916,  852,  761,  761,  898,  761,
 /*   230 */   916,  916,  916,  916,  915,  853,  859,  761,  890,  830,
 /*   240 */   830,  818,  866,  818,  866,  866,  871,  866,  856,  761,
 /*   250 */   761,  -70,  -70,  -70,  -70,  -70,  -70, 1207, 1187, 1167,
 /*   260 */  1133, 1103, 1072, 1042, 1011,  982,  953,  933,  902,  882,
 /*   270 */   851, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410,
 /*   280 */  1410, 1410, 1410,  377,  480,   -8,  339,  339,  124,   78,
 /*   290 */    74,   34,   34,   34,   34,   34,   34,   34,   34,   34,
 /*   300 */    34,  482,  408,  407,  385,  354,  317,  336,  241,  109,
 /*   310 */   172,  155,  222,  155,  190,  -25,   94,    4,   79,  807,
 /*   320 */   732,  805,  754,  803,  748,  795,  744,  800,  790,  791,
 /*   330 */   730,  755,  821,  789,  782,  698,  715,  720,  781,  776,
 /*   340 */   769,  775,  768,  767,  766,  765,  763,  760,  737,  750,
 /*   350 */   758,  675,  751,  731,  753,  672,  659,  736,  685,  705,
 /*   360 */   721,  728,  727,  723,  729,  722,  726,  724,  717,  634,
 /*   370 */   718,  711,  682,  708,  703,  699,  690,  692,  680,  678,
 /*   380 */   648,  671,  625,  615,  608,  593,  584,  454,  447,  574,
 /*   390 */   445,  417,  382,  349,  335,  375,  184,  278,  278,  278,
 /*   400 */   278,  278,  278,  208,  203,  148,  148,   22,   11,   -2,
 /*   410 */   -69,
};
#define YY_REDUCE_USE_DFLT (-179)
#define YY_REDUCE_COUNT (256)
#define YY_REDUCE_MIN   (-178)
#define YY_REDUCE_MAX   (2657)
static const short yy_reduce_ofst[] = {
 /*     0 */   -17,  509,  463,  419,  373,  327,  270,  224,  177,  131,
 /*    10 */  -101,  525,  599,  572,  546,  525, 1201,  179, 1189, 1169,
 /*    20 */  1074, 1305,  462,  130, 2317, 2283, 2261, 2237, 2215, 2186,
 /*    30 */  2156, 2134, 2106, 2084, 2059, 2025, 2003, 1979, 1957, 1928,
 /*    40 */  1898, 1876, 1848, 1826, 1801, 1767, 1745, 1721, 1699, 1670,
 /*    50 */  1640, 1618, 1590, 1568, 1543, 1509, 1487, 1463, 1437, 1413,
 /*    60 */  1384, 1365, 1334,  957, 2618, 2599, 2580, 2561, 2539, 2518,
 /*    70 */  2499, 2480, 2460, 2441, 2419, 2400, 2379, 2357,  909, 2657,
 /*    80 */  2638,  311, 1704, 1444, 1185, 1076, 1019,  939,  848,  657,
 /*    90 */   420,   30, -178, 1477, 1371, 1318, 1214, 1137,  820,  706,
 /*   100 */   434,  321,   -6,  -30,  -56,  -79,  -54,  642,  444,  358,
 /*   110 */    87,  -89, -159,  273,  158,  213,  201,  282,  102,  -10,
 /*   120 */   121, -126,  -70, -120,  -48,  535,  457,  501,  477,  487,
 /*   130 */   534, -104,  395,  246,  409,  369,  275,  -76,  293, -106,
 /*   140 */   384,  343,  346,  380,  -40, 1095, 1080, 1043, 1140, 1030,
 /*   150 */  1028, 1027,  996, 1031, 1068, 1067, 1067, 1067, 1067, 1022,
 /*   160 */  1055, 1029, 1018, 1013, 1047, 1006,  994, 1091, 1001,  998,
 /*   170 */   987, 1026,  968,  980,  977,  975, 1016,  962,  961,  948,
 /*   180 */   928,  946,  945,  910,  919,  927,  926,  873,  925,  922,
 /*   190 */   918,  917,  924,  886,  914,  911,  862,  812,  908,  907,
 /*   200 */   892,  888,  846,  837,  838,  773,  881,  878,  877,  835,
 /*   210 */   860,  831,  827,  826,  823,  857,  847,  817,  816,  802,
 /*   220 */   801,  785,  774,  772,  793,  804,  756,  741,  779,  714,
 /*   230 */   788,  787,  786,  771,  743,  770,  764,  696,  792,  740,
 /*   240 */   733,  739,  783,  712,  762,  759,  738,  757,  654,  636,
 /*   250 */   632,  627,  710,  709,  573,  582,  604,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   989,  924,  924,  924,  924,  924,  924,  924,  924,  924,
 /*    10 */   710,  905,  659,  659,  659,  904,  989,  989,  989,  989,
 /*    20 */   659,  989,  984,  989,  989,  989,  989,  989,  989,  989,
 /*    30 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    40 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    50 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    60 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    70 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    80 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*    90 */   989,  989,  696,  989,  989,  989,  989,  989,  989,  989,
 /*   100 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*   110 */   989,  724,  989,  989,  989,  717,  717,  989,  927,  989,
 /*   120 */   977,  989,  850,  850,  770,  954,  989,  989,  987,  989,
 /*   130 */   989,  725,  989,  989,  985,  989,  989,  770,  773,  989,
 /*   140 */   989,  985,  705,  685,  824,  989,  989,  989,  701,  989,
 /*   150 */   989,  989,  989,  744,  989,  965,  964,  963,  759,  989,
 /*   160 */   860,  989,  989,  989,  860,  989,  989,  989,  989,  989,
 /*   170 */   989,  989,  989,  989,  989,  989,  860,  989,  989,  989,
 /*   180 */   989,  821,  989,  989,  989,  818,  989,  989,  989,  989,
 /*   190 */   989,  989,  989,  989,  989,  989,  773,  989,  989,  989,
 /*   200 */   989,  989,  966,  966,  989,  989,  989,  989,  989,  989,
 /*   210 */   978,  989,  989,  989,  989,  978,  987,  989,  989,  989,
 /*   220 */   989,  989,  989,  989,  989,  928,  989,  989,  738,  989,
 /*   230 */   989,  989,  989,  989,  836,  976,  835,  989,  989,  966,
 /*   240 */   966,  865,  867,  865,  867,  867,  989,  867,  989,  989,
 /*   250 */   989,  696,  903,  874,  854,  853,  677,  989,  989,  989,
 /*   260 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*   270 */   989,  847,  708,  709,  988,  979,  702,  810,  667,  669,
 /*   280 */   768,  769,  665,  989,  989,  758,  767,  766,  989,  989,
 /*   290 */   989,  757,  756,  755,  754,  753,  752,  751,  750,  749,
 /*   300 */   748,  989,  989,  989,  989,  989,  989,  989,  989,  804,
 /*   310 */   989,  939,  989,  938,  989,  989,  804,  989,  989,  989,
 /*   320 */   989,  989,  989,  989,  811,  989,  989,  989,  989,  989,
 /*   330 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*   340 */   989,  989,  989,  989,  989,  989,  989,  798,  989,  989,
 /*   350 */   989,  879,  989,  989,  989,  989,  989,  989,  989,  989,
 /*   360 */   989,  989,  989,  989,  989,  989,  989,  989,  932,  989,
 /*   370 */   989,  989,  989,  989,  989,  989,  989,  989,  989,  989,
 /*   380 */   989,  989,  968,  989,  989,  989,  989,  862,  861,  989,
 /*   390 */   989,  666,  668,  989,  989,  989,  804,  765,  764,  763,
 /*   400 */   762,  761,  760,  989,  989,  747,  746,  989,  989,  989,
 /*   410 */   989,  742,  908,  907,  906,  825,  823,  822,  820,  819,
 /*   420 */   817,  815,  814,  813,  812,  816,  902,  901,  900,  899,
 /*   430 */   898,  897,  782,  952,  950,  949,  948,  947,  946,  945,
 /*   440 */   944,  943,  909,  858,  732,  951,  873,  872,  871,  852,
 /*   450 */   851,  849,  848,  784,  785,  786,  783,  775,  776,  774,
 /*   460 */   800,  801,  772,  671,  791,  793,  795,  797,  799,  796,
 /*   470 */   794,  792,  790,  789,  780,  779,  778,  777,  670,  771,
 /*   480 */   719,  718,  716,  660,  658,  657,  656,  953,  712,  711,
 /*   490 */   707,  706,  893,  926,  925,  923,  922,  921,  920,  919,
 /*   500 */   918,  917,  916,  915,  914,  913,  895,  894,  892,  808,
 /*   510 */   876,  870,  869,  806,  805,  733,  713,  704,  680,  681,
 /*   520 */   679,  676,  655,  731,  933,  941,  942,  937,  935,  940,
 /*   530 */   936,  934,  859,  804,  929,  931,  857,  856,  930,  730,
 /*   540 */   740,  739,  737,  736,  834,  831,  830,  829,  828,  827,
 /*   550 */   833,  832,  975,  974,  972,  973,  971,  970,  969,  968,
 /*   560 */   986,  983,  982,  981,  980,  729,  727,  735,  734,  728,
 /*   570 */   726,  809,  864,  863,  688,  839,  967,  912,  911,  855,
 /*   580 */   838,  837,  695,  866,  694,  693,  692,  691,  868,  745,
 /*   590 */   881,  880,  781,  662,  891,  890,  889,  888,  887,  886,
 /*   600 */   885,  884,  956,  962,  961,  960,  959,  958,  957,  955,
 /*   610 */   883,  882,  846,  845,  844,  843,  842,  841,  840,  896,
 /*   620 */   826,  803,  802,  788,  661,  879,  878,  703,  715,  714,
 /*   630 */   664,  663,  690,  689,  687,  684,  683,  682,  678,  675,
 /*   640 */   674,  673,  672,  686,  723,  722,  721,  720,  700,  699,
 /*   650 */   698,  697,  910,  807,  743,
};

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx */
#endif
  int yyerrcnt;                 /* Shifts left before out of the error */
  ParseARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "TOK_EQUAL",     "TOK_GREATER_EQUAL",  "TOK_GREATER_THAN",
  "TOK_IN",        "TOK_INST_EQUAL",  "TOK_INST_NOT_EQUAL",  "TOK_LESS_EQUAL",
  "TOK_LESS_THAN",  "TOK_LIKE",      "TOK_NOT_EQUAL",  "TOK_MINUS",   
  "TOK_PLUS",      "TOK_OR",        "TOK_XOR",       "TOK_DIV",     
  "TOK_MOD",       "TOK_REAL_DIV",  "TOK_TIMES",     "TOK_AND",     
  "TOK_ANDOR",     "TOK_CONCAT_OP",  "TOK_EXP",       "TOK_NOT",     
  "TOK_DOT",       "TOK_BACKSLASH",  "TOK_LEFT_BRACKET",  "TOK_LEFT_PAREN",
  "TOK_RIGHT_PAREN",  "TOK_RIGHT_BRACKET",  "TOK_COLON",     "TOK_COMMA",   
  "TOK_AGGREGATE",  "TOK_OF",        "TOK_IDENTIFIER",  "TOK_ALIAS",   
  "TOK_FOR",       "TOK_END_ALIAS",  "TOK_ARRAY",     "TOK_ASSIGNMENT",
  "TOK_BAG",       "TOK_BOOLEAN",   "TOK_INTEGER",   "TOK_REAL",    
  "TOK_NUMBER",    "TOK_LOGICAL",   "TOK_BINARY",    "TOK_STRING",  
  "TOK_BY",        "TOK_LEFT_CURL",  "TOK_RIGHT_CURL",  "TOK_OTHERWISE",
  "TOK_CASE",      "TOK_END_CASE",  "TOK_BEGIN",     "TOK_END",     
  "TOK_PI",        "TOK_E",         "TOK_CONSTANT",  "TOK_END_CONSTANT",
  "TOK_DERIVE",    "TOK_END_ENTITY",  "TOK_ENTITY",    "TOK_ENUMERATION",
  "TOK_ESCAPE",    "TOK_SELF",      "TOK_OPTIONAL",  "TOK_VAR",     
  "TOK_END_FUNCTION",  "TOK_FUNCTION",  "TOK_BUILTIN_FUNCTION",  "TOK_LIST",    
  "TOK_SET",       "TOK_GENERIC",   "TOK_QUESTION_MARK",  "TOK_IF",      
  "TOK_THEN",      "TOK_END_IF",    "TOK_ELSE",      "TOK_INCLUDE", 
  "TOK_STRING_LITERAL",  "TOK_TO",        "TOK_AS",        "TOK_REFERENCE",
  "TOK_FROM",      "TOK_USE",       "TOK_INVERSE",   "TOK_INTEGER_LITERAL",
  "TOK_REAL_LITERAL",  "TOK_STRING_LITERAL_ENCODED",  "TOK_LOGICAL_LITERAL",  "TOK_BINARY_LITERAL",
  "TOK_LOCAL",     "TOK_END_LOCAL",  "TOK_ONEOF",     "TOK_UNIQUE",  
  "TOK_FIXED",     "TOK_END_PROCEDURE",  "TOK_PROCEDURE",  "TOK_BUILTIN_PROCEDURE",
  "TOK_QUERY",     "TOK_ALL_IN",    "TOK_SUCH_THAT",  "TOK_REPEAT",  
  "TOK_END_REPEAT",  "TOK_RETURN",    "TOK_END_RULE",  "TOK_RULE",    
  "TOK_END_SCHEMA",  "TOK_SCHEMA",    "TOK_SELECT",    "TOK_SEMICOLON",
  "TOK_SKIP",      "TOK_SUBTYPE",   "TOK_ABSTRACT",  "TOK_SUPERTYPE",
  "TOK_END_TYPE",  "TOK_TYPE",      "TOK_UNTIL",     "TOK_WHERE",   
  "TOK_WHILE",     "error",         "case_action",   "case_otherwise",
  "entity_body",   "aggregate_init_element",  "aggregate_initializer",  "assignable",  
  "attribute_decl",  "by_expression",  "constant",      "expression",  
  "function_call",  "general_ref",   "group_ref",     "identifier",  
  "initializer",   "interval",      "literal",       "local_initializer",
  "precision_spec",  "query_expression",  "query_start",   "simple_expression",
  "unary_expression",  "supertype_expression",  "until_control",  "while_control",
  "function_header",  "fh_lineno",     "rule_header",   "rh_start",    
  "rh_get_line",   "procedure_header",  "ph_get_line",   "action_body", 
  "actual_parameters",  "aggregate_init_body",  "explicit_attr_list",  "case_action_list",
  "case_block",    "case_labels",   "where_clause_list",  "derive_decl", 
  "explicit_attribute",  "expression_list",  "formal_parameter",  "formal_parameter_list",
  "formal_parameter_rep",  "id_list",       "defined_type_list",  "nested_id_list",
  "statement_rep",  "subtype_decl",  "where_rule",    "where_rule_OPT",
  "supertype_expression_list",  "labelled_attrib_list_list",  "labelled_attrib_list",  "inverse_attr_list",
  "inverse_clause",  "attribute_decl_list",  "derived_attribute_rep",  "unique_clause",
  "rule_formal_parameter_list",  "qualified_attr_list",  "rel_op",        "optional_or_unique",
  "optional_fixed",  "optional",      "var",           "unique",      
  "qualified_attr",  "qualifier",     "alias_statement",  "assignment_statement",
  "case_statement",  "compound_statement",  "escape_statement",  "if_statement",
  "proc_call_statement",  "repeat_statement",  "return_statement",  "skip_statement",
  "statement",     "subsuper_decl",  "supertype_decl",  "supertype_factor",
  "function_id",   "procedure_id",  "attribute_type",  "defined_type",
  "parameter_type",  "generic_type",  "basic_type",    "select_type", 
  "aggregate_type",  "aggregation_type",  "array_type",    "bag_type",    
  "conformant_aggregation",  "list_type",     "set_type",      "set_or_bag_of_entity",
  "type",          "cardinality_op",  "index_spec",    "limit_spec",  
  "inverse_attr",  "derived_attribute",  "rule_formal_parameter",  "where_clause",
  "action_body_item_rep",  "action_body_item",  "declaration",   "constant_decl",
  "local_decl",    "semicolon",     "alias_push_scope",  "block_list",  
  "block_member",  "include_directive",  "rule_decl",     "constant_body",
  "constant_body_list",  "entity_decl",   "function_decl",  "procedure_decl",
  "type_decl",     "entity_header",  "enumeration_type",  "express_file",
  "schema_decl_list",  "schema_decl",   "fh_push_scope",  "fh_plist",    
  "increment_control",  "rename",        "rename_list",   "parened_rename_list",
  "reference_clause",  "reference_head",  "use_clause",    "use_head",    
  "interface_specification",  "interface_specification_list",  "right_curl",    "local_variable",
  "local_body",    "allow_generic_types",  "disallow_generic_types",  "oneof_op",    
  "ph_push_scope",  "schema_body",   "schema_header",  "type_item_body",
  "type_item",     "ti_start",      "td_start",    
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "action_body ::= action_body_item_rep statement_rep",
 /*   1 */ "action_body_item ::= declaration",
 /*   2 */ "action_body_item ::= constant_decl",
 /*   3 */ "action_body_item ::= local_decl",
 /*   4 */ "action_body_item_rep ::=",
 /*   5 */ "action_body_item_rep ::= action_body_item action_body_item_rep",
 /*   6 */ "actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN",
 /*   7 */ "actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN",
 /*   8 */ "aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET",
 /*   9 */ "aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET",
 /*  10 */ "aggregate_init_element ::= expression",
 /*  11 */ "aggregate_init_body ::= aggregate_init_element",
 /*  12 */ "aggregate_init_body ::= aggregate_init_element TOK_COLON expression",
 /*  13 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element",
 /*  14 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression",
 /*  15 */ "aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type",
 /*  16 */ "aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type",
 /*  17 */ "aggregation_type ::= array_type",
 /*  18 */ "aggregation_type ::= bag_type",
 /*  19 */ "aggregation_type ::= list_type",
 /*  20 */ "aggregation_type ::= set_type",
 /*  21 */ "alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon",
 /*  22 */ "alias_push_scope ::=",
 /*  23 */ "array_type ::= TOK_ARRAY index_spec TOK_OF optional_or_unique attribute_type",
 /*  24 */ "assignable ::= assignable qualifier",
 /*  25 */ "assignable ::= identifier",
 /*  26 */ "assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon",
 /*  27 */ "attribute_type ::= aggregation_type",
 /*  28 */ "attribute_type ::= basic_type",
 /*  29 */ "attribute_type ::= defined_type",
 /*  30 */ "explicit_attr_list ::=",
 /*  31 */ "explicit_attr_list ::= explicit_attr_list explicit_attribute",
 /*  32 */ "bag_type ::= TOK_BAG limit_spec TOK_OF attribute_type",
 /*  33 */ "bag_type ::= TOK_BAG TOK_OF attribute_type",
 /*  34 */ "basic_type ::= TOK_BOOLEAN",
 /*  35 */ "basic_type ::= TOK_INTEGER precision_spec",
 /*  36 */ "basic_type ::= TOK_REAL precision_spec",
 /*  37 */ "basic_type ::= TOK_NUMBER",
 /*  38 */ "basic_type ::= TOK_LOGICAL",
 /*  39 */ "basic_type ::= TOK_BINARY precision_spec optional_fixed",
 /*  40 */ "basic_type ::= TOK_STRING precision_spec optional_fixed",
 /*  41 */ "block_list ::=",
 /*  42 */ "block_list ::= block_list block_member",
 /*  43 */ "block_member ::= declaration",
 /*  44 */ "block_member ::= include_directive",
 /*  45 */ "block_member ::= rule_decl",
 /*  46 */ "by_expression ::=",
 /*  47 */ "by_expression ::= TOK_BY expression",
 /*  48 */ "cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL",
 /*  49 */ "case_action ::= case_labels TOK_COLON statement",
 /*  50 */ "case_action_list ::=",
 /*  51 */ "case_action_list ::= case_action_list case_action",
 /*  52 */ "case_block ::= case_action_list case_otherwise",
 /*  53 */ "case_labels ::= expression",
 /*  54 */ "case_labels ::= case_labels TOK_COMMA expression",
 /*  55 */ "case_otherwise ::=",
 /*  56 */ "case_otherwise ::= TOK_OTHERWISE TOK_COLON statement",
 /*  57 */ "case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon",
 /*  58 */ "compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon",
 /*  59 */ "constant ::= TOK_PI",
 /*  60 */ "constant ::= TOK_E",
 /*  61 */ "constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon",
 /*  62 */ "constant_body_list ::=",
 /*  63 */ "constant_body_list ::= constant_body constant_body_list",
 /*  64 */ "constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon",
 /*  65 */ "declaration ::= entity_decl",
 /*  66 */ "declaration ::= function_decl",
 /*  67 */ "declaration ::= procedure_decl",
 /*  68 */ "declaration ::= type_decl",
 /*  69 */ "derive_decl ::=",
 /*  70 */ "derive_decl ::= TOK_DERIVE derived_attribute_rep",
 /*  71 */ "derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon",
 /*  72 */ "derived_attribute_rep ::= derived_attribute",
 /*  73 */ "derived_attribute_rep ::= derived_attribute_rep derived_attribute",
 /*  74 */ "entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT",
 /*  75 */ "entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon",
 /*  76 */ "entity_header ::= TOK_ENTITY TOK_IDENTIFIER",
 /*  77 */ "enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list",
 /*  78 */ "escape_statement ::= TOK_ESCAPE semicolon",
 /*  79 */ "attribute_decl ::= TOK_IDENTIFIER",
 /*  80 */ "attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER",
 /*  81 */ "attribute_decl_list ::= attribute_decl",
 /*  82 */ "attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl",
 /*  83 */ "optional ::=",
 /*  84 */ "optional ::= TOK_OPTIONAL",
 /*  85 */ "explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon",
 /*  86 */ "express_file ::= schema_decl_list",
 /*  87 */ "schema_decl_list ::= schema_decl",
 /*  88 */ "schema_decl_list ::= schema_decl_list schema_decl",
 /*  89 */ "expression ::= simple_expression",
 /*  90 */ "expression ::= expression TOK_AND expression",
 /*  91 */ "expression ::= expression TOK_OR expression",
 /*  92 */ "expression ::= expression TOK_XOR expression",
 /*  93 */ "expression ::= expression TOK_LESS_THAN expression",
 /*  94 */ "expression ::= expression TOK_GREATER_THAN expression",
 /*  95 */ "expression ::= expression TOK_EQUAL expression",
 /*  96 */ "expression ::= expression TOK_LESS_EQUAL expression",
 /*  97 */ "expression ::= expression TOK_GREATER_EQUAL expression",
 /*  98 */ "expression ::= expression TOK_NOT_EQUAL expression",
 /*  99 */ "expression ::= expression TOK_INST_EQUAL expression",
 /* 100 */ "expression ::= expression TOK_INST_NOT_EQUAL expression",
 /* 101 */ "expression ::= expression TOK_IN expression",
 /* 102 */ "expression ::= expression TOK_LIKE expression",
 /* 103 */ "expression ::= simple_expression cardinality_op simple_expression",
 /* 104 */ "simple_expression ::= unary_expression",
 /* 105 */ "simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression",
 /* 106 */ "simple_expression ::= simple_expression TOK_EXP simple_expression",
 /* 107 */ "simple_expression ::= simple_expression TOK_TIMES simple_expression",
 /* 108 */ "simple_expression ::= simple_expression TOK_DIV simple_expression",
 /* 109 */ "simple_expression ::= simple_expression TOK_REAL_DIV simple_expression",
 /* 110 */ "simple_expression ::= simple_expression TOK_MOD simple_expression",
 /* 111 */ "simple_expression ::= simple_expression TOK_PLUS simple_expression",
 /* 112 */ "simple_expression ::= simple_expression TOK_MINUS simple_expression",
 /* 113 */ "expression_list ::= expression",
 /* 114 */ "expression_list ::= expression_list TOK_COMMA expression",
 /* 115 */ "var ::=",
 /* 116 */ "var ::= TOK_VAR",
 /* 117 */ "formal_parameter ::= var id_list TOK_COLON parameter_type",
 /* 118 */ "formal_parameter_list ::=",
 /* 119 */ "formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN",
 /* 120 */ "formal_parameter_rep ::= formal_parameter",
 /* 121 */ "formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter",
 /* 122 */ "parameter_type ::= basic_type",
 /* 123 */ "parameter_type ::= conformant_aggregation",
 /* 124 */ "parameter_type ::= defined_type",
 /* 125 */ "parameter_type ::= generic_type",
 /* 126 */ "function_call ::= function_id actual_parameters",
 /* 127 */ "function_decl ::= function_header action_body TOK_END_FUNCTION semicolon",
 /* 128 */ "function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon",
 /* 129 */ "fh_lineno ::= TOK_FUNCTION",
 /* 130 */ "fh_push_scope ::= TOK_IDENTIFIER",
 /* 131 */ "fh_plist ::= formal_parameter_list",
 /* 132 */ "function_id ::= TOK_IDENTIFIER",
 /* 133 */ "function_id ::= TOK_BUILTIN_FUNCTION",
 /* 134 */ "conformant_aggregation ::= aggregate_type",
 /* 135 */ "conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type",
 /* 136 */ "conformant_aggregation ::= TOK_ARRAY index_spec TOK_OF optional_or_unique parameter_type",
 /* 137 */ "conformant_aggregation ::= TOK_BAG TOK_OF parameter_type",
 /* 138 */ "conformant_aggregation ::= TOK_BAG index_spec TOK_OF parameter_type",
 /* 139 */ "conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type",
 /* 140 */ "conformant_aggregation ::= TOK_LIST index_spec TOK_OF unique parameter_type",
 /* 141 */ "conformant_aggregation ::= TOK_SET TOK_OF parameter_type",
 /* 142 */ "conformant_aggregation ::= TOK_SET index_spec TOK_OF parameter_type",
 /* 143 */ "generic_type ::= TOK_GENERIC",
 /* 144 */ "generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER",
 /* 145 */ "id_list ::= TOK_IDENTIFIER",
 /* 146 */ "id_list ::= id_list TOK_COMMA TOK_IDENTIFIER",
 /* 147 */ "identifier ::= TOK_SELF",
 /* 148 */ "identifier ::= TOK_QUESTION_MARK",
 /* 149 */ "identifier ::= TOK_IDENTIFIER",
 /* 150 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon",
 /* 151 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon",
 /* 152 */ "include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon",
 /* 153 */ "increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression",
 /* 154 */ "index_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET",
 /* 155 */ "initializer ::= TOK_ASSIGNMENT expression",
 /* 156 */ "rename ::= TOK_IDENTIFIER",
 /* 157 */ "rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER",
 /* 158 */ "rename_list ::= rename",
 /* 159 */ "rename_list ::= rename_list TOK_COMMA rename",
 /* 160 */ "parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN",
 /* 161 */ "reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 162 */ "reference_clause ::= reference_head parened_rename_list semicolon",
 /* 163 */ "reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER",
 /* 164 */ "use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 165 */ "use_clause ::= use_head parened_rename_list semicolon",
 /* 166 */ "use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER",
 /* 167 */ "interface_specification ::= use_clause",
 /* 168 */ "interface_specification ::= reference_clause",
 /* 169 */ "interface_specification_list ::=",
 /* 170 */ "interface_specification_list ::= interface_specification_list interface_specification",
 /* 171 */ "interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl",
 /* 172 */ "set_or_bag_of_entity ::= defined_type",
 /* 173 */ "set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type",
 /* 174 */ "set_or_bag_of_entity ::= TOK_SET limit_spec TOK_OF defined_type",
 /* 175 */ "set_or_bag_of_entity ::= TOK_BAG limit_spec TOK_OF defined_type",
 /* 176 */ "set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type",
 /* 177 */ "inverse_attr_list ::= inverse_attr",
 /* 178 */ "inverse_attr_list ::= inverse_attr_list inverse_attr",
 /* 179 */ "inverse_attr ::= TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon",
 /* 180 */ "inverse_clause ::=",
 /* 181 */ "inverse_clause ::= TOK_INVERSE inverse_attr_list",
 /* 182 */ "limit_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET",
 /* 183 */ "list_type ::= TOK_LIST limit_spec TOK_OF unique attribute_type",
 /* 184 */ "list_type ::= TOK_LIST TOK_OF unique attribute_type",
 /* 185 */ "literal ::= TOK_INTEGER_LITERAL",
 /* 186 */ "literal ::= TOK_REAL_LITERAL",
 /* 187 */ "literal ::= TOK_STRING_LITERAL",
 /* 188 */ "literal ::= TOK_STRING_LITERAL_ENCODED",
 /* 189 */ "literal ::= TOK_LOGICAL_LITERAL",
 /* 190 */ "literal ::= TOK_BINARY_LITERAL",
 /* 191 */ "literal ::= constant",
 /* 192 */ "local_initializer ::= TOK_ASSIGNMENT expression",
 /* 193 */ "local_variable ::= id_list TOK_COLON parameter_type semicolon",
 /* 194 */ "local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon",
 /* 195 */ "local_body ::=",
 /* 196 */ "local_body ::= local_variable local_body",
 /* 197 */ "local_decl ::= TOK_LOCAL allow_generic_types local_body TOK_END_LOCAL semicolon disallow_generic_types",
 /* 198 */ "allow_generic_types ::=",
 /* 199 */ "disallow_generic_types ::=",
 /* 200 */ "defined_type ::= TOK_IDENTIFIER",
 /* 201 */ "defined_type_list ::= defined_type",
 /* 202 */ "defined_type_list ::= defined_type_list TOK_COMMA defined_type",
 /* 203 */ "nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN",
 /* 204 */ "oneof_op ::= TOK_ONEOF",
 /* 205 */ "optional_or_unique ::=",
 /* 206 */ "optional_or_unique ::= TOK_OPTIONAL",
 /* 207 */ "optional_or_unique ::= TOK_UNIQUE",
 /* 208 */ "optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE",
 /* 209 */ "optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL",
 /* 210 */ "optional_fixed ::=",
 /* 211 */ "optional_fixed ::= TOK_FIXED",
 /* 212 */ "precision_spec ::=",
 /* 213 */ "precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 214 */ "proc_call_statement ::= procedure_id actual_parameters semicolon",
 /* 215 */ "proc_call_statement ::= procedure_id semicolon",
 /* 216 */ "procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon",
 /* 217 */ "procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon",
 /* 218 */ "ph_push_scope ::= TOK_IDENTIFIER",
 /* 219 */ "ph_get_line ::=",
 /* 220 */ "procedure_id ::= TOK_IDENTIFIER",
 /* 221 */ "procedure_id ::= TOK_BUILTIN_PROCEDURE",
 /* 222 */ "group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 223 */ "qualifier ::= TOK_DOT TOK_IDENTIFIER",
 /* 224 */ "qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 225 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET",
 /* 226 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET",
 /* 227 */ "query_expression ::= query_start expression TOK_RIGHT_PAREN",
 /* 228 */ "query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT",
 /* 229 */ "rel_op ::= TOK_LESS_THAN",
 /* 230 */ "rel_op ::= TOK_GREATER_THAN",
 /* 231 */ "rel_op ::= TOK_EQUAL",
 /* 232 */ "rel_op ::= TOK_LESS_EQUAL",
 /* 233 */ "rel_op ::= TOK_GREATER_EQUAL",
 /* 234 */ "rel_op ::= TOK_NOT_EQUAL",
 /* 235 */ "rel_op ::= TOK_INST_EQUAL",
 /* 236 */ "rel_op ::= TOK_INST_NOT_EQUAL",
 /* 237 */ "repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 238 */ "repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 239 */ "return_statement ::= TOK_RETURN semicolon",
 /* 240 */ "return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon",
 /* 241 */ "right_curl ::= TOK_RIGHT_CURL",
 /* 242 */ "rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon",
 /* 243 */ "rule_formal_parameter ::= TOK_IDENTIFIER",
 /* 244 */ "rule_formal_parameter_list ::= rule_formal_parameter",
 /* 245 */ "rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter",
 /* 246 */ "rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon",
 /* 247 */ "rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN",
 /* 248 */ "rh_get_line ::=",
 /* 249 */ "schema_body ::= interface_specification_list block_list",
 /* 250 */ "schema_body ::= interface_specification_list constant_decl block_list",
 /* 251 */ "schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon",
 /* 252 */ "schema_decl ::= include_directive",
 /* 253 */ "schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon",
 /* 254 */ "select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 255 */ "semicolon ::= TOK_SEMICOLON",
 /* 256 */ "set_type ::= TOK_SET limit_spec TOK_OF attribute_type",
 /* 257 */ "set_type ::= TOK_SET TOK_OF attribute_type",
 /* 258 */ "skip_statement ::= TOK_SKIP semicolon",
 /* 259 */ "statement ::= alias_statement",
 /* 260 */ "statement ::= assignment_statement",
 /* 261 */ "statement ::= case_statement",
 /* 262 */ "statement ::= compound_statement",
 /* 263 */ "statement ::= escape_statement",
 /* 264 */ "statement ::= if_statement",
 /* 265 */ "statement ::= proc_call_statement",
 /* 266 */ "statement ::= repeat_statement",
 /* 267 */ "statement ::= return_statement",
 /* 268 */ "statement ::= skip_statement",
 /* 269 */ "statement_rep ::=",
 /* 270 */ "statement_rep ::= semicolon statement_rep",
 /* 271 */ "statement_rep ::= statement statement_rep",
 /* 272 */ "subsuper_decl ::=",
 /* 273 */ "subsuper_decl ::= supertype_decl",
 /* 274 */ "subsuper_decl ::= subtype_decl",
 /* 275 */ "subsuper_decl ::= supertype_decl subtype_decl",
 /* 276 */ "subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 277 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE",
 /* 278 */ "supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 279 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 280 */ "supertype_expression ::= supertype_factor",
 /* 281 */ "supertype_expression ::= supertype_expression TOK_AND supertype_factor",
 /* 282 */ "supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor",
 /* 283 */ "supertype_expression_list ::= supertype_expression",
 /* 284 */ "supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression",
 /* 285 */ "supertype_factor ::= identifier",
 /* 286 */ "supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN",
 /* 287 */ "supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 288 */ "type ::= aggregation_type",
 /* 289 */ "type ::= basic_type",
 /* 290 */ "type ::= defined_type",
 /* 291 */ "type ::= select_type",
 /* 292 */ "type_item_body ::= enumeration_type",
 /* 293 */ "type_item_body ::= type",
 /* 294 */ "type_item ::= ti_start type_item_body semicolon",
 /* 295 */ "ti_start ::= TOK_IDENTIFIER TOK_EQUAL",
 /* 296 */ "type_decl ::= td_start TOK_END_TYPE semicolon",
 /* 297 */ "td_start ::= TOK_TYPE type_item where_rule_OPT",
 /* 298 */ "general_ref ::= assignable group_ref",
 /* 299 */ "general_ref ::= assignable",
 /* 300 */ "unary_expression ::= aggregate_initializer",
 /* 301 */ "unary_expression ::= unary_expression qualifier",
 /* 302 */ "unary_expression ::= literal",
 /* 303 */ "unary_expression ::= function_call",
 /* 304 */ "unary_expression ::= identifier",
 /* 305 */ "unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 306 */ "unary_expression ::= interval",
 /* 307 */ "unary_expression ::= query_expression",
 /* 308 */ "unary_expression ::= TOK_NOT unary_expression",
 /* 309 */ "unary_expression ::= TOK_PLUS unary_expression",
 /* 310 */ "unary_expression ::= TOK_MINUS unary_expression",
 /* 311 */ "unique ::=",
 /* 312 */ "unique ::= TOK_UNIQUE",
 /* 313 */ "qualified_attr ::= TOK_IDENTIFIER",
 /* 314 */ "qualified_attr ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER",
 /* 315 */ "qualified_attr_list ::= qualified_attr",
 /* 316 */ "qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr",
 /* 317 */ "labelled_attrib_list ::= qualified_attr_list semicolon",
 /* 318 */ "labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon",
 /* 319 */ "labelled_attrib_list_list ::= labelled_attrib_list",
 /* 320 */ "labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list",
 /* 321 */ "unique_clause ::=",
 /* 322 */ "unique_clause ::= TOK_UNIQUE labelled_attrib_list_list",
 /* 323 */ "until_control ::=",
 /* 324 */ "until_control ::= TOK_UNTIL expression",
 /* 325 */ "where_clause ::= expression semicolon",
 /* 326 */ "where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon",
 /* 327 */ "where_clause_list ::= where_clause",
 /* 328 */ "where_clause_list ::= where_clause_list where_clause",
 /* 329 */ "where_rule ::= TOK_WHERE where_clause_list",
 /* 330 */ "where_rule_OPT ::=",
 /* 331 */ "where_rule_OPT ::= where_rule",
 /* 332 */ "while_control ::=",
 /* 333 */ "while_control ::= TOK_WHILE expression",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.
*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  ParseARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos;

  if( pParser->yyidx<0 ) return 0;

  yytos = &pParser->yystack[pParser->yyidx];

#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void ParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int ParseStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_COUNT
   || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
#line 2462 "expparse.y"

    fprintf(stderr, "Express parser experienced stack overflow.\n");
#line 1863 "expparse.c"
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 155, 2 },
  { 233, 1 },
  { 233, 1 },
  { 233, 1 },
  { 232, 0 },
  { 232, 2 },
  { 156, 3 },
  { 156, 2 },
  { 126, 2 },
  { 126, 3 },
  { 125, 1 },
  { 157, 1 },
  { 157, 3 },
  { 157, 3 },
  { 157, 5 },
  { 216, 3 },
  { 216, 5 },
  { 217, 1 },
  { 217, 1 },
  { 217, 1 },
  { 217, 1 },
  { 194, 9 },
  { 238, 0 },
  { 218, 5 },
  { 127, 2 },
  { 127, 1 },
  { 195, 4 },
  { 210, 1 },
  { 210, 1 },
  { 210, 1 },
  { 158, 0 },
  { 158, 2 },
  { 219, 4 },
  { 219, 3 },
  { 214, 1 },
  { 214, 2 },
  { 214, 2 },
  { 214, 1 },
  { 214, 1 },
  { 214, 3 },
  { 214, 3 },
  { 239, 0 },
  { 239, 2 },
  { 240, 1 },
  { 240, 1 },
  { 240, 1 },
  { 129, 0 },
  { 129, 2 },
  { 225, 5 },
  { 122, 3 },
  { 159, 0 },
  { 159, 2 },
  { 160, 2 },
  { 161, 1 },
  { 161, 3 },
  { 123, 0 },
  { 123, 3 },
  { 196, 6 },
  { 197, 4 },
  { 130, 1 },
  { 130, 1 },
  { 243, 6 },
  { 244, 0 },
  { 244, 2 },
  { 235, 4 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 163, 0 },
  { 163, 2 },
  { 229, 5 },
  { 182, 1 },
  { 182, 2 },
  { 124, 5 },
  { 245, 6 },
  { 249, 2 },
  { 250, 3 },
  { 198, 2 },
  { 128, 1 },
  { 128, 5 },
  { 181, 1 },
  { 181, 3 },
  { 189, 0 },
  { 189, 1 },
  { 164, 5 },
  { 251, 1 },
  { 252, 1 },
  { 252, 2 },
  { 131, 1 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 131, 3 },
  { 143, 1 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 143, 3 },
  { 165, 1 },
  { 165, 3 },
  { 190, 0 },
  { 190, 1 },
  { 166, 4 },
  { 167, 0 },
  { 167, 3 },
  { 168, 1 },
  { 168, 3 },
  { 212, 1 },
  { 212, 1 },
  { 212, 1 },
  { 212, 1 },
  { 132, 2 },
  { 246, 4 },
  { 148, 6 },
  { 149, 1 },
  { 254, 1 },
  { 255, 1 },
  { 208, 1 },
  { 208, 1 },
  { 220, 1 },
  { 220, 4 },
  { 220, 5 },
  { 220, 3 },
  { 220, 4 },
  { 220, 4 },
  { 220, 5 },
  { 220, 3 },
  { 220, 4 },
  { 213, 1 },
  { 213, 3 },
  { 169, 1 },
  { 169, 3 },
  { 135, 1 },
  { 135, 1 },
  { 135, 1 },
  { 199, 6 },
  { 199, 8 },
  { 241, 3 },
  { 256, 6 },
  { 226, 5 },
  { 136, 2 },
  { 257, 1 },
  { 257, 3 },
  { 258, 1 },
  { 258, 3 },
  { 259, 3 },
  { 260, 4 },
  { 260, 3 },
  { 261, 3 },
  { 262, 4 },
  { 262, 3 },
  { 263, 3 },
  { 264, 1 },
  { 264, 1 },
  { 265, 0 },
  { 265, 2 },
  { 137, 7 },
  { 223, 1 },
  { 223, 3 },
  { 223, 4 },
  { 223, 4 },
  { 223, 3 },
  { 179, 1 },
  { 179, 2 },
  { 228, 6 },
  { 180, 0 },
  { 180, 2 },
  { 227, 5 },
  { 221, 5 },
  { 221, 4 },
  { 138, 1 },
  { 138, 1 },
  { 138, 1 },
  { 138, 1 },
  { 138, 1 },
  { 138, 1 },
  { 138, 1 },
  { 139, 2 },
  { 267, 4 },
  { 267, 5 },
  { 268, 0 },
  { 268, 2 },
  { 236, 6 },
  { 269, 0 },
  { 270, 0 },
  { 211, 1 },
  { 170, 1 },
  { 170, 3 },
  { 171, 3 },
  { 271, 1 },
  { 187, 0 },
  { 187, 1 },
  { 187, 1 },
  { 187, 2 },
  { 187, 2 },
  { 188, 0 },
  { 188, 1 },
  { 140, 0 },
  { 140, 3 },
  { 200, 3 },
  { 200, 2 },
  { 247, 4 },
  { 153, 5 },
  { 272, 1 },
  { 154, 0 },
  { 209, 1 },
  { 209, 1 },
  { 134, 2 },
  { 193, 2 },
  { 193, 2 },
  { 193, 3 },
  { 193, 5 },
  { 141, 3 },
  { 142, 6 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 186, 1 },
  { 201, 8 },
  { 201, 7 },
  { 202, 2 },
  { 202, 5 },
  { 266, 1 },
  { 242, 5 },
  { 230, 1 },
  { 184, 1 },
  { 184, 3 },
  { 150, 4 },
  { 151, 5 },
  { 152, 0 },
  { 273, 2 },
  { 273, 3 },
  { 253, 4 },
  { 253, 1 },
  { 274, 3 },
  { 215, 4 },
  { 237, 1 },
  { 222, 4 },
  { 222, 3 },
  { 203, 2 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 204, 1 },
  { 172, 0 },
  { 172, 2 },
  { 172, 2 },
  { 205, 0 },
  { 205, 1 },
  { 205, 1 },
  { 205, 2 },
  { 173, 5 },
  { 206, 2 },
  { 206, 5 },
  { 206, 6 },
  { 145, 1 },
  { 145, 3 },
  { 145, 3 },
  { 176, 1 },
  { 176, 3 },
  { 207, 1 },
  { 207, 4 },
  { 207, 3 },
  { 224, 1 },
  { 224, 1 },
  { 224, 1 },
  { 224, 1 },
  { 275, 1 },
  { 275, 1 },
  { 276, 3 },
  { 277, 2 },
  { 248, 3 },
  { 278, 3 },
  { 133, 2 },
  { 133, 1 },
  { 144, 1 },
  { 144, 2 },
  { 144, 1 },
  { 144, 1 },
  { 144, 1 },
  { 144, 3 },
  { 144, 1 },
  { 144, 1 },
  { 144, 2 },
  { 144, 2 },
  { 144, 2 },
  { 191, 0 },
  { 191, 1 },
  { 192, 1 },
  { 192, 5 },
  { 185, 1 },
  { 185, 3 },
  { 178, 2 },
  { 178, 4 },
  { 177, 1 },
  { 177, 2 },
  { 183, 0 },
  { 183, 2 },
  { 146, 0 },
  { 146, 2 },
  { 231, 2 },
  { 231, 4 },
  { 162, 1 },
  { 162, 2 },
  { 174, 2 },
  { 175, 0 },
  { 175, 1 },
  { 147, 0 },
  { 147, 2 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  ParseARG_FETCH;

  yymsp = &yypParser->yystack[yypParser->yyidx];

  if( yyruleno>=0 ) {
#ifndef NDEBUG
      if ( yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0]))) {
         if (yyTraceFILE) {
      fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
              yyRuleName[yyruleno]);
    }
   }
#endif /* NDEBUG */
  } else {
    /* invalid rule number range */
    return;
  }


  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0: /* action_body ::= action_body_item_rep statement_rep */
      case 70: /* derive_decl ::= TOK_DERIVE derived_attribute_rep */ yytestcase(yyruleno==70);
      case 181: /* inverse_clause ::= TOK_INVERSE inverse_attr_list */ yytestcase(yyruleno==181);
      case 270: /* statement_rep ::= semicolon statement_rep */ yytestcase(yyruleno==270);
      case 322: /* unique_clause ::= TOK_UNIQUE labelled_attrib_list_list */ yytestcase(yyruleno==322);
      case 329: /* where_rule ::= TOK_WHERE where_clause_list */ yytestcase(yyruleno==329);
      case 331: /* where_rule_OPT ::= where_rule */ yytestcase(yyruleno==331);
#line 357 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
}
#line 2328 "expparse.c"
        break;
      case 1: /* action_body_item ::= declaration */
      case 2: /* action_body_item ::= constant_decl */ yytestcase(yyruleno==2);
      case 3: /* action_body_item ::= local_decl */ yytestcase(yyruleno==3);
      case 43: /* block_member ::= declaration */ yytestcase(yyruleno==43);
      case 44: /* block_member ::= include_directive */ yytestcase(yyruleno==44);
      case 45: /* block_member ::= rule_decl */ yytestcase(yyruleno==45);
      case 65: /* declaration ::= entity_decl */ yytestcase(yyruleno==65);
      case 66: /* declaration ::= function_decl */ yytestcase(yyruleno==66);
      case 67: /* declaration ::= procedure_decl */ yytestcase(yyruleno==67);
      case 68: /* declaration ::= type_decl */ yytestcase(yyruleno==68);
      case 87: /* schema_decl_list ::= schema_decl */ yytestcase(yyruleno==87);
      case 158: /* rename_list ::= rename */ yytestcase(yyruleno==158);
      case 167: /* interface_specification ::= use_clause */ yytestcase(yyruleno==167);
      case 168: /* interface_specification ::= reference_clause */ yytestcase(yyruleno==168);
      case 204: /* oneof_op ::= TOK_ONEOF */ yytestcase(yyruleno==204);
      case 252: /* schema_decl ::= include_directive */ yytestcase(yyruleno==252);
      case 292: /* type_item_body ::= enumeration_type */ yytestcase(yyruleno==292);
#line 363 "expparse.y"
{
    yygotominor.yy0 = yymsp[0].minor.yy0;
}
#line 2351 "expparse.c"
        break;
      case 5: /* action_body_item_rep ::= action_body_item action_body_item_rep */
      case 42: /* block_list ::= block_list block_member */ yytestcase(yyruleno==42);
      case 63: /* constant_body_list ::= constant_body constant_body_list */ yytestcase(yyruleno==63);
      case 88: /* schema_decl_list ::= schema_decl_list schema_decl */ yytestcase(yyruleno==88);
      case 170: /* interface_specification_list ::= interface_specification_list interface_specification */ yytestcase(yyruleno==170);
      case 196: /* local_body ::= local_variable local_body */ yytestcase(yyruleno==196);
      case 249: /* schema_body ::= interface_specification_list block_list */ yytestcase(yyruleno==249);
#line 380 "expparse.y"
{
    yygotominor.yy0 = yymsp[-1].minor.yy0;
}
#line 2364 "expparse.c"
        break;
      case 6: /* actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN */
      case 203: /* nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==203);
      case 276: /* subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==276);
#line 397 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 2373 "expparse.c"
        break;
      case 7: /* actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN */
      case 321: /* unique_clause ::= */ yytestcase(yyruleno==321);
#line 401 "expparse.y"
{
    yygotominor.yy371 = 0;
}
#line 2381 "expparse.c"
        break;
      case 8: /* aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET */
#line 407 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = LISTcreate();
}
#line 2389 "expparse.c"
        break;
      case 9: /* aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET */
#line 413 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = yymsp[-1].minor.yy371;
}
#line 2397 "expparse.c"
        break;
      case 10: /* aggregate_init_element ::= expression */
      case 25: /* assignable ::= identifier */ yytestcase(yyruleno==25);
      case 47: /* by_expression ::= TOK_BY expression */ yytestcase(yyruleno==47);
      case 89: /* expression ::= simple_expression */ yytestcase(yyruleno==89);
      case 104: /* simple_expression ::= unary_expression */ yytestcase(yyruleno==104);
      case 155: /* initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==155);
      case 191: /* literal ::= constant */ yytestcase(yyruleno==191);
      case 192: /* local_initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==192);
      case 299: /* general_ref ::= assignable */ yytestcase(yyruleno==299);
      case 300: /* unary_expression ::= aggregate_initializer */ yytestcase(yyruleno==300);
      case 302: /* unary_expression ::= literal */ yytestcase(yyruleno==302);
      case 303: /* unary_expression ::= function_call */ yytestcase(yyruleno==303);
      case 304: /* unary_expression ::= identifier */ yytestcase(yyruleno==304);
      case 306: /* unary_expression ::= interval */ yytestcase(yyruleno==306);
      case 307: /* unary_expression ::= query_expression */ yytestcase(yyruleno==307);
      case 309: /* unary_expression ::= TOK_PLUS unary_expression */ yytestcase(yyruleno==309);
      case 324: /* until_control ::= TOK_UNTIL expression */ yytestcase(yyruleno==324);
      case 333: /* while_control ::= TOK_WHILE expression */ yytestcase(yyruleno==333);
#line 419 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 2421 "expparse.c"
        break;
      case 11: /* aggregate_init_body ::= aggregate_init_element */
#line 424 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2429 "expparse.c"
        break;
      case 12: /* aggregate_init_body ::= aggregate_init_element TOK_COLON expression */
#line 429 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);

    LISTadd(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[-2].minor.yy401->type->u.type->body->flags.repeat = 1;
}
#line 2441 "expparse.c"
        break;
      case 13: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element */
#line 439 "expparse.y"
{ 
    yygotominor.yy371 = yymsp[-2].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2451 "expparse.c"
        break;
      case 14: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression */
#line 447 "expparse.y"
{
    yygotominor.yy371 = yymsp[-4].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[-2].minor.yy401->type->u.type->body->flags.repeat = 1;
}
#line 2463 "expparse.c"
        break;
      case 15: /* aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type */
#line 457 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(aggregate_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 2479 "expparse.c"
        break;
      case 16: /* aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type */
#line 471 "expparse.y"
{
    Type t = TYPEcreate_user_defined_tag(yymsp[0].minor.yy297, CURRENT_SCOPE, yymsp[-2].minor.yy0.symbol);

    if (t) {
        SCOPEadd_super(t);
        yygotominor.yy477 = TYPEBODYcreate(aggregate_);
        yygotominor.yy477->tag = t;
        yygotominor.yy477->base = yymsp[0].minor.yy297;
    }
}
#line 2493 "expparse.c"
        break;
      case 17: /* aggregation_type ::= array_type */
      case 18: /* aggregation_type ::= bag_type */ yytestcase(yyruleno==18);
      case 19: /* aggregation_type ::= list_type */ yytestcase(yyruleno==19);
      case 20: /* aggregation_type ::= set_type */ yytestcase(yyruleno==20);
#line 483 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;
}
#line 2503 "expparse.c"
        break;
      case 21: /* alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon */
#line 502 "expparse.y"
{
    Expression e = EXPcreate_from_symbol(Type_Attribute, yymsp[-7].minor.yy0.symbol);
    Variable v = VARcreate(e, Type_Unknown);

    v->initializer = yymsp[-5].minor.yy401; 

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-7].minor.yy0.symbol->name, (Generic)v,
	    yymsp[-7].minor.yy0.symbol, OBJ_VARIABLE);
    yygotominor.yy332 = ALIAScreate(CURRENT_SCOPE, v, yymsp[-2].minor.yy371);

    POP_SCOPE();
}
#line 2519 "expparse.c"
        break;
      case 22: /* alias_push_scope ::= */
#line 516 "expparse.y"
{
    struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
    PUSH_SCOPE(s, (Symbol *)0, OBJ_ALIAS);
}
#line 2527 "expparse.c"
        break;
      case 23: /* array_type ::= TOK_ARRAY index_spec TOK_OF optional_or_unique attribute_type */
#line 523 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);

    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2540 "expparse.c"
        break;
      case 24: /* assignable ::= assignable qualifier */
      case 301: /* unary_expression ::= unary_expression qualifier */ yytestcase(yyruleno==301);
#line 535 "expparse.y"
{
    yymsp[0].minor.yy46.first->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy46.expr;
}
#line 2549 "expparse.c"
        break;
      case 26: /* assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon */
#line 546 "expparse.y"
{ 
    yygotominor.yy332 = ASSIGNcreate(yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
}
#line 2556 "expparse.c"
        break;
      case 27: /* attribute_type ::= aggregation_type */
      case 28: /* attribute_type ::= basic_type */ yytestcase(yyruleno==28);
      case 122: /* parameter_type ::= basic_type */ yytestcase(yyruleno==122);
      case 123: /* parameter_type ::= conformant_aggregation */ yytestcase(yyruleno==123);
#line 551 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(yymsp[0].minor.yy477);
    SCOPEadd_super(yygotominor.yy297);
}
#line 2567 "expparse.c"
        break;
      case 29: /* attribute_type ::= defined_type */
      case 124: /* parameter_type ::= defined_type */ yytestcase(yyruleno==124);
      case 125: /* parameter_type ::= generic_type */ yytestcase(yyruleno==125);
#line 561 "expparse.y"
{
    yygotominor.yy297 = yymsp[0].minor.yy297;
}
#line 2576 "expparse.c"
        break;
      case 30: /* explicit_attr_list ::= */
      case 50: /* case_action_list ::= */ yytestcase(yyruleno==50);
      case 69: /* derive_decl ::= */ yytestcase(yyruleno==69);
      case 269: /* statement_rep ::= */ yytestcase(yyruleno==269);
#line 566 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
}
#line 2586 "expparse.c"
        break;
      case 31: /* explicit_attr_list ::= explicit_attr_list explicit_attribute */
#line 570 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371); 
}
#line 2594 "expparse.c"
        break;
      case 32: /* bag_type ::= TOK_BAG limit_spec TOK_OF attribute_type */
      case 138: /* conformant_aggregation ::= TOK_BAG index_spec TOK_OF parameter_type */ yytestcase(yyruleno==138);
#line 576 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 2605 "expparse.c"
        break;
      case 33: /* bag_type ::= TOK_BAG TOK_OF attribute_type */
#line 583 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2613 "expparse.c"
        break;
      case 34: /* basic_type ::= TOK_BOOLEAN */
#line 589 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(boolean_);
}
#line 2620 "expparse.c"
        break;
      case 35: /* basic_type ::= TOK_INTEGER precision_spec */
#line 593 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(integer_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2628 "expparse.c"
        break;
      case 36: /* basic_type ::= TOK_REAL precision_spec */
#line 598 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(real_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2636 "expparse.c"
        break;
      case 37: /* basic_type ::= TOK_NUMBER */
#line 603 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(number_);
}
#line 2643 "expparse.c"
        break;
      case 38: /* basic_type ::= TOK_LOGICAL */
#line 607 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(logical_);
}
#line 2650 "expparse.c"
        break;
      case 39: /* basic_type ::= TOK_BINARY precision_spec optional_fixed */
#line 611 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(binary_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2659 "expparse.c"
        break;
      case 40: /* basic_type ::= TOK_STRING precision_spec optional_fixed */
#line 617 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(string_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2668 "expparse.c"
        break;
      case 46: /* by_expression ::= */
#line 643 "expparse.y"
{
    yygotominor.yy401 = LITERAL_ONE;
}
#line 2675 "expparse.c"
        break;
      case 48: /* cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL */
      case 154: /* index_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */ yytestcase(yyruleno==154);
      case 182: /* limit_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */ yytestcase(yyruleno==182);
#line 653 "expparse.y"
{
    yygotominor.yy253.lower_limit = yymsp[-3].minor.yy401;
    yygotominor.yy253.upper_limit = yymsp[-1].minor.yy401;
}
#line 2685 "expparse.c"
        break;
      case 49: /* case_action ::= case_labels TOK_COLON statement */
#line 659 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(yymsp[-2].minor.yy371, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2693 "expparse.c"
        break;
      case 51: /* case_action_list ::= case_action_list case_action */
#line 669 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-1].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy321);
}
#line 2704 "expparse.c"
        break;
      case 52: /* case_block ::= case_action_list case_otherwise */
#line 678 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

    if (yymsp[0].minor.yy321) {
        LISTadd_last(yygotominor.yy371,
        (Generic)yymsp[0].minor.yy321);
    }
}
#line 2716 "expparse.c"
        break;
      case 53: /* case_labels ::= expression */
#line 688 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2725 "expparse.c"
        break;
      case 54: /* case_labels ::= case_labels TOK_COMMA expression */
#line 694 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2735 "expparse.c"
        break;
      case 55: /* case_otherwise ::= */
#line 702 "expparse.y"
{
    yygotominor.yy321 = (Case_Item)0;
}
#line 2742 "expparse.c"
        break;
      case 56: /* case_otherwise ::= TOK_OTHERWISE TOK_COLON statement */
#line 706 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(LIST_NULL, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2750 "expparse.c"
        break;
      case 57: /* case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon */
#line 713 "expparse.y"
{
    yygotominor.yy332 = CASEcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 2757 "expparse.c"
        break;
      case 58: /* compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon */
#line 718 "expparse.y"
{
    yygotominor.yy332 = COMP_STMTcreate(yymsp[-2].minor.yy371);
}
#line 2764 "expparse.c"
        break;
      case 59: /* constant ::= TOK_PI */
#line 723 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_PI;
}
#line 2771 "expparse.c"
        break;
      case 60: /* constant ::= TOK_E */
#line 728 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_E;
}
#line 2778 "expparse.c"
        break;
      case 61: /* constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon */
#line 735 "expparse.y"
{
    Variable v;

    yymsp[-5].minor.yy401->type = yymsp[-3].minor.yy297;
    v = VARcreate(yymsp[-5].minor.yy401, yymsp[-3].minor.yy297);
    v->initializer = yymsp[-1].minor.yy401;
    v->flags.constant = 1;
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-5].minor.yy401->symbol.name, (Generic)v,
	&yymsp[-5].minor.yy401->symbol, OBJ_VARIABLE);
}
#line 2792 "expparse.c"
        break;
      case 64: /* constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon */
#line 754 "expparse.y"
{
    yygotominor.yy0 = yymsp[-3].minor.yy0;
}
#line 2799 "expparse.c"
        break;
      case 71: /* derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon */
#line 786 "expparse.y"
{
    yygotominor.yy91 = VARcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy297);
    yygotominor.yy91->initializer = yymsp[-1].minor.yy401;
    yygotominor.yy91->flags.attribute = true;
}
#line 2808 "expparse.c"
        break;
      case 72: /* derived_attribute_rep ::= derived_attribute */
      case 177: /* inverse_attr_list ::= inverse_attr */ yytestcase(yyruleno==177);
#line 793 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2817 "expparse.c"
        break;
      case 73: /* derived_attribute_rep ::= derived_attribute_rep derived_attribute */
      case 178: /* inverse_attr_list ::= inverse_attr_list inverse_attr */ yytestcase(yyruleno==178);
#line 798 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2826 "expparse.c"
        break;
      case 74: /* entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT */
#line 805 "expparse.y"
{
    yygotominor.yy176.attributes = yymsp[-4].minor.yy371;
    /* this is flattened out in entity_decl - DEL */
    LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-3].minor.yy371);

    if (yymsp[-2].minor.yy371 != LIST_NULL) {
	LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-2].minor.yy371);
    }

    yygotominor.yy176.unique = yymsp[-1].minor.yy371;
    yygotominor.yy176.where = yymsp[0].minor.yy371;
}
#line 2842 "expparse.c"
        break;
      case 75: /* entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon */
#line 820 "expparse.y"
{
    CURRENT_SCOPE->u.entity->subtype_expression = yymsp[-4].minor.yy242.subtypes;
    CURRENT_SCOPE->u.entity->supertype_symbols = yymsp[-4].minor.yy242.supertypes;
    LISTdo (yymsp[-2].minor.yy176.attributes, l, Linked_List)
	LISTdo (l, a, Variable)
	    ENTITYadd_attribute(CURRENT_SCOPE, a);
	LISTod;
    LISTod;
    CURRENT_SCOPE->u.entity->abstract = yymsp[-4].minor.yy242.abstract;
    CURRENT_SCOPE->u.entity->unique = yymsp[-2].minor.yy176.unique;
    CURRENT_SCOPE->where = yymsp[-2].minor.yy176.where;
    POP_SCOPE();
}
#line 2859 "expparse.c"
        break;
      case 76: /* entity_header ::= TOK_ENTITY TOK_IDENTIFIER */
#line 835 "expparse.y"
{
    Entity e = ENTITYcreate(yymsp[0].minor.yy0.symbol);

    if (print_objects_while_running & OBJ_ENTITY_BITS) {
	fprintf(stdout, "parse: %s (entity)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(e, yymsp[0].minor.yy0.symbol, OBJ_ENTITY);
}
#line 2872 "expparse.c"
        break;
      case 77: /* enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list */
#line 846 "expparse.y"
{
    int value = 0;
    Expression x;
    Symbol *tmp;
    TypeBody tb;
    tb = TYPEBODYcreate(enumeration_);
    CURRENT_SCOPE->u.type->head = 0;
    CURRENT_SCOPE->u.type->body = tb;
    tb->list = yymsp[0].minor.yy371;

    if (!CURRENT_SCOPE->symbol_table) {
        CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }
    if (!PREVIOUS_SCOPE->enum_table) {
        PREVIOUS_SCOPE->enum_table = DICTcreate(25);
    }
    LISTdo_links(yymsp[0].minor.yy371, id) {
        tmp = (Symbol *)id->data;
        id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
        x->symbol = *(tmp);
        x->u.integer = ++value;

        /* define both in enum scope and scope of */
        /* 1st visibility */
        DICT_define(CURRENT_SCOPE->symbol_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        DICTdefine(PREVIOUS_SCOPE->symbol_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        SYMBOL_destroy(tmp);
    } LISTod;
}
#line 2907 "expparse.c"
        break;
      case 78: /* escape_statement ::= TOK_ESCAPE semicolon */
#line 879 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_ESCAPE;
}
#line 2914 "expparse.c"
        break;
      case 79: /* attribute_decl ::= TOK_IDENTIFIER */
#line 884 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Attribute);
    yygotominor.yy401->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2923 "expparse.c"
        break;
      case 80: /* attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 891 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1->e.op_code = OP_GROUP;
    yygotominor.yy401->e.op1->e.op1 = EXPcreate(Type_Self);
    yygotominor.yy401->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity, yymsp[-2].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[-2].minor.yy0.symbol);

    yygotominor.yy401->e.op_code = OP_DOT;
    yygotominor.yy401->e.op2 = EXPcreate_from_symbol(Type_Attribute, yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2939 "expparse.c"
        break;
      case 81: /* attribute_decl_list ::= attribute_decl */
#line 905 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2948 "expparse.c"
        break;
      case 82: /* attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl */
      case 114: /* expression_list ::= expression_list TOK_COMMA expression */ yytestcase(yyruleno==114);
#line 912 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2957 "expparse.c"
        break;
      case 83: /* optional ::= */
#line 918 "expparse.y"
{
    yygotominor.yy252.optional = 0;
}
#line 2964 "expparse.c"
        break;
      case 84: /* optional ::= TOK_OPTIONAL */
#line 922 "expparse.y"
{
    yygotominor.yy252.optional = 1;
}
#line 2971 "expparse.c"
        break;
      case 85: /* explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon */
#line 928 "expparse.y"
{
    Variable v;

    LISTdo_links (yymsp[-4].minor.yy371, attr)
	v = VARcreate((Expression)attr->data, yymsp[-1].minor.yy297);
	v->flags.optional = yymsp[-2].minor.yy252.optional;
	v->flags.attribute = true;
	attr->data = (Generic)v;
    LISTod;

    yygotominor.yy371 = yymsp[-4].minor.yy371;
}
#line 2987 "expparse.c"
        break;
      case 90: /* expression ::= expression TOK_AND expression */
#line 957 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2996 "expparse.c"
        break;
      case 91: /* expression ::= expression TOK_OR expression */
#line 963 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_OR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3005 "expparse.c"
        break;
      case 92: /* expression ::= expression TOK_XOR expression */
#line 969 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_XOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3014 "expparse.c"
        break;
      case 93: /* expression ::= expression TOK_LESS_THAN expression */
#line 975 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3023 "expparse.c"
        break;
      case 94: /* expression ::= expression TOK_GREATER_THAN expression */
#line 981 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3032 "expparse.c"
        break;
      case 95: /* expression ::= expression TOK_EQUAL expression */
#line 987 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3041 "expparse.c"
        break;
      case 96: /* expression ::= expression TOK_LESS_EQUAL expression */
#line 993 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3050 "expparse.c"
        break;
      case 97: /* expression ::= expression TOK_GREATER_EQUAL expression */
#line 999 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3059 "expparse.c"
        break;
      case 98: /* expression ::= expression TOK_NOT_EQUAL expression */
#line 1005 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3068 "expparse.c"
        break;
      case 99: /* expression ::= expression TOK_INST_EQUAL expression */
#line 1011 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3077 "expparse.c"
        break;
      case 100: /* expression ::= expression TOK_INST_NOT_EQUAL expression */
#line 1017 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3086 "expparse.c"
        break;
      case 101: /* expression ::= expression TOK_IN expression */
#line 1023 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_IN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3095 "expparse.c"
        break;
      case 102: /* expression ::= expression TOK_LIKE expression */
#line 1029 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LIKE, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3104 "expparse.c"
        break;
      case 103: /* expression ::= simple_expression cardinality_op simple_expression */
      case 241: /* right_curl ::= TOK_RIGHT_CURL */ yytestcase(yyruleno==241);
      case 255: /* semicolon ::= TOK_SEMICOLON */ yytestcase(yyruleno==255);
#line 1035 "expparse.y"
{
    yyerrok;
}
#line 3113 "expparse.c"
        break;
      case 105: /* simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression */
#line 1045 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_CONCAT, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3122 "expparse.c"
        break;
      case 106: /* simple_expression ::= simple_expression TOK_EXP simple_expression */
#line 1051 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EXP, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3131 "expparse.c"
        break;
      case 107: /* simple_expression ::= simple_expression TOK_TIMES simple_expression */
#line 1057 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_TIMES, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3140 "expparse.c"
        break;
      case 108: /* simple_expression ::= simple_expression TOK_DIV simple_expression */
#line 1063 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3149 "expparse.c"
        break;
      case 109: /* simple_expression ::= simple_expression TOK_REAL_DIV simple_expression */
#line 1069 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_REAL_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3158 "expparse.c"
        break;
      case 110: /* simple_expression ::= simple_expression TOK_MOD simple_expression */
#line 1075 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MOD, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3167 "expparse.c"
        break;
      case 111: /* simple_expression ::= simple_expression TOK_PLUS simple_expression */
#line 1081 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_PLUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3176 "expparse.c"
        break;
      case 112: /* simple_expression ::= simple_expression TOK_MINUS simple_expression */
#line 1087 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MINUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3185 "expparse.c"
        break;
      case 113: /* expression_list ::= expression */
      case 283: /* supertype_expression_list ::= supertype_expression */ yytestcase(yyruleno==283);
#line 1094 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 3194 "expparse.c"
        break;
      case 115: /* var ::= */
#line 1105 "expparse.y"
{
    yygotominor.yy252.var = 1;
}
#line 3201 "expparse.c"
        break;
      case 116: /* var ::= TOK_VAR */
#line 1109 "expparse.y"
{
    yygotominor.yy252.var = 0;
}
#line 3208 "expparse.c"
        break;
      case 117: /* formal_parameter ::= var id_list TOK_COLON parameter_type */
#line 1114 "expparse.y"
{
    Symbol *tmp;
    Expression e;
    Variable v;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTdo_links(yygotominor.yy371, param)
    tmp = (Symbol*)param->data;

    e = EXPcreate_from_symbol(Type_Attribute, tmp);
    v = VARcreate(e, yymsp[0].minor.yy297);
    v->flags.optional = yymsp[-3].minor.yy252.var;
    v->flags.parameter = true;
    param->data = (Generic)v;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table,
    tmp->name, (Generic)v, tmp, OBJ_VARIABLE);

    LISTod;
}
#line 3233 "expparse.c"
        break;
      case 118: /* formal_parameter_list ::= */
      case 180: /* inverse_clause ::= */ yytestcase(yyruleno==180);
      case 330: /* where_rule_OPT ::= */ yytestcase(yyruleno==330);
#line 1137 "expparse.y"
{
    yygotominor.yy371 = LIST_NULL;
}
#line 3242 "expparse.c"
        break;
      case 119: /* formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN */
#line 1142 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

}
#line 3250 "expparse.c"
        break;
      case 120: /* formal_parameter_rep ::= formal_parameter */
#line 1148 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;

}
#line 3258 "expparse.c"
        break;
      case 121: /* formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter */
#line 1154 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_all(yygotominor.yy371, yymsp[0].minor.yy371);
}
#line 3266 "expparse.c"
        break;
      case 126: /* function_call ::= function_id actual_parameters */
#line 1179 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Funcall);
    yygotominor.yy401->symbol = *yymsp[-1].minor.yy275;
    SYMBOL_destroy(yymsp[-1].minor.yy275);
    yygotominor.yy401->u.funcall.list = yymsp[0].minor.yy371;
}
#line 3276 "expparse.c"
        break;
      case 127: /* function_decl ::= function_header action_body TOK_END_FUNCTION semicolon */
#line 1188 "expparse.y"
{
    FUNCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3285 "expparse.c"
        break;
      case 128: /* function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon */
#line 1196 "expparse.y"
{ 
    Function f = CURRENT_SCOPE;

    f->u.func->return_type = yymsp[-1].minor.yy297;
    yygotominor.yy507 = yymsp[-5].minor.yy507;
}
#line 3295 "expparse.c"
        break;
      case 129: /* fh_lineno ::= TOK_FUNCTION */
      case 219: /* ph_get_line ::= */ yytestcase(yyruleno==219);
      case 248: /* rh_get_line ::= */ yytestcase(yyruleno==248);
#line 1204 "expparse.y"
{
    yygotominor.yy507 = SCANtell();
}
#line 3304 "expparse.c"
        break;
      case 130: /* fh_push_scope ::= TOK_IDENTIFIER */
#line 1209 "expparse.y"
{
    Function f = ALGcreate(OBJ_FUNCTION);
    tag_count = 0;
    if (print_objects_while_running & OBJ_FUNCTION_BITS) {
        fprintf(stdout, "parse: %s (function)\n", yymsp[0].minor.yy0.symbol->name);
    }
    PUSH_SCOPE(f, yymsp[0].minor.yy0.symbol, OBJ_FUNCTION);
}
#line 3316 "expparse.c"
        break;
      case 131: /* fh_plist ::= formal_parameter_list */
#line 1219 "expparse.y"
{
    Function f = CURRENT_SCOPE;
    f->u.func->parameters = yymsp[0].minor.yy371;
    f->u.func->pcount = LISTget_length(yymsp[0].minor.yy371);
    f->u.func->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
}
#line 3327 "expparse.c"
        break;
      case 132: /* function_id ::= TOK_IDENTIFIER */
      case 220: /* procedure_id ::= TOK_IDENTIFIER */ yytestcase(yyruleno==220);
      case 221: /* procedure_id ::= TOK_BUILTIN_PROCEDURE */ yytestcase(yyruleno==221);
#line 1228 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;
}
#line 3336 "expparse.c"
        break;
      case 133: /* function_id ::= TOK_BUILTIN_FUNCTION */
#line 1232 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;

}
#line 3344 "expparse.c"
        break;
      case 134: /* conformant_aggregation ::= aggregate_type */
#line 1238 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;

}
#line 3352 "expparse.c"
        break;
      case 135: /* conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type */
#line 1244 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3362 "expparse.c"
        break;
      case 136: /* conformant_aggregation ::= TOK_ARRAY index_spec TOK_OF optional_or_unique parameter_type */
#line 1252 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3374 "expparse.c"
        break;
      case 137: /* conformant_aggregation ::= TOK_BAG TOK_OF parameter_type */
#line 1261 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3383 "expparse.c"
        break;
      case 139: /* conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type */
#line 1274 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3393 "expparse.c"
        break;
      case 140: /* conformant_aggregation ::= TOK_LIST index_spec TOK_OF unique parameter_type */
#line 1282 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3404 "expparse.c"
        break;
      case 141: /* conformant_aggregation ::= TOK_SET TOK_OF parameter_type */
      case 257: /* set_type ::= TOK_SET TOK_OF attribute_type */ yytestcase(yyruleno==257);
#line 1290 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3413 "expparse.c"
        break;
      case 142: /* conformant_aggregation ::= TOK_SET index_spec TOK_OF parameter_type */
#line 1295 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3423 "expparse.c"
        break;
      case 143: /* generic_type ::= TOK_GENERIC */
#line 1303 "expparse.y"
{
    yygotominor.yy297 = Type_Generic;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 3438 "expparse.c"
        break;
      case 144: /* generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER */
#line 1315 "expparse.y"
{
    TypeBody g = TYPEBODYcreate(generic_);
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(g);

    SCOPEadd_super(yygotominor.yy297);

    g->tag = TYPEcreate_user_defined_tag(yygotominor.yy297, CURRENT_SCOPE, yymsp[0].minor.yy0.symbol);
    if (g->tag) {
        SCOPEadd_super(g->tag);
    }
}
#line 3453 "expparse.c"
        break;
      case 145: /* id_list ::= TOK_IDENTIFIER */
#line 1328 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);

}
#line 3462 "expparse.c"
        break;
      case 146: /* id_list ::= id_list TOK_COMMA TOK_IDENTIFIER */
#line 1334 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);
}
#line 3472 "expparse.c"
        break;
      case 147: /* identifier ::= TOK_SELF */
#line 1342 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Self);
}
#line 3479 "expparse.c"
        break;
      case 148: /* identifier ::= TOK_QUESTION_MARK */
#line 1346 "expparse.y"
{
    yygotominor.yy401 = LITERAL_INFINITY;
}
#line 3486 "expparse.c"
        break;
      case 149: /* identifier ::= TOK_IDENTIFIER */
#line 1350 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Identifier);
    yygotominor.yy401->symbol = *(yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3495 "expparse.c"
        break;
      case 150: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon */
#line 1358 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371, STATEMENT_LIST_NULL);
}
#line 3502 "expparse.c"
        break;
      case 151: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon */
#line 1363 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-6].minor.yy401, yymsp[-4].minor.yy371, yymsp[-2].minor.yy371);
}
#line 3509 "expparse.c"
        break;
      case 152: /* include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon */
#line 1368 "expparse.y"
{
    SCANinclude_file(yymsp[-1].minor.yy0.string);
}
#line 3516 "expparse.c"
        break;
      case 153: /* increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression */
#line 1374 "expparse.y"
{
    Increment i = INCR_CTLcreate(yymsp[-5].minor.yy0.symbol, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401, yymsp[0].minor.yy401);

    /* scope doesn't really have/need a name, I suppose */
    /* naming it by the iterator variable is fine */

    PUSH_SCOPE(i, (Symbol *)0, OBJ_INCREMENT);
}
#line 3528 "expparse.c"
        break;
      case 156: /* rename ::= TOK_IDENTIFIER */
#line 1396 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[0].minor.yy0, yymsp[0].minor.yy0);
}
#line 3535 "expparse.c"
        break;
      case 157: /* rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER */
#line 1400 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
#line 3542 "expparse.c"
        break;
      case 159: /* rename_list ::= rename_list TOK_COMMA rename */
      case 162: /* reference_clause ::= reference_head parened_rename_list semicolon */ yytestcase(yyruleno==162);
      case 165: /* use_clause ::= use_head parened_rename_list semicolon */ yytestcase(yyruleno==165);
      case 250: /* schema_body ::= interface_specification_list constant_decl block_list */ yytestcase(yyruleno==250);
      case 296: /* type_decl ::= td_start TOK_END_TYPE semicolon */ yytestcase(yyruleno==296);
#line 1409 "expparse.y"
{
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 3553 "expparse.c"
        break;
      case 161: /* reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1416 "expparse.y"
{
    if (!CURRENT_SCHEMA->ref_schemas) {
        CURRENT_SCHEMA->ref_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->ref_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3564 "expparse.c"
        break;
      case 163: /* reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER */
#line 1429 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_reference;
}
#line 3572 "expparse.c"
        break;
      case 164: /* use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1435 "expparse.y"
{
    if (!CURRENT_SCHEMA->use_schemas) {
        CURRENT_SCHEMA->use_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->use_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3583 "expparse.c"
        break;
      case 166: /* use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER */
#line 1448 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_use;
}
#line 3591 "expparse.c"
        break;
      case 171: /* interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl */
#line 1471 "expparse.y"
{
    Expression	tmp1, tmp2;

    yygotominor.yy401 = (Expression)0;
    tmp1 = BIN_EXPcreate(yymsp[-4].minor.yy126, yymsp[-5].minor.yy401, yymsp[-3].minor.yy401);
    tmp2 = BIN_EXPcreate(yymsp[-2].minor.yy126, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, tmp1, tmp2);
}
#line 3603 "expparse.c"
        break;
      case 172: /* set_or_bag_of_entity ::= defined_type */
      case 290: /* type ::= defined_type */ yytestcase(yyruleno==290);
#line 1483 "expparse.y"
{
    yygotominor.yy378.type = yymsp[0].minor.yy297;
    yygotominor.yy378.body = 0;
}
#line 3612 "expparse.c"
        break;
      case 173: /* set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type */
#line 1488 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;

}
#line 3622 "expparse.c"
        break;
      case 174: /* set_or_bag_of_entity ::= TOK_SET limit_spec TOK_OF defined_type */
#line 1495 "expparse.y"
{
    yygotominor.yy378.type = 0; 
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3633 "expparse.c"
        break;
      case 175: /* set_or_bag_of_entity ::= TOK_BAG limit_spec TOK_OF defined_type */
#line 1503 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3644 "expparse.c"
        break;
      case 176: /* set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type */
#line 1511 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
}
#line 3653 "expparse.c"
        break;
      case 179: /* inverse_attr ::= TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon */
#line 1530 "expparse.y"
{
    Expression e = EXPcreate(Type_Attribute);

    e->symbol = *yymsp[-5].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[-5].minor.yy0.symbol);

    if (yymsp[-3].minor.yy378.type) {
        yygotominor.yy91 = VARcreate(e, yymsp[-3].minor.yy378.type);
    } else {
        Type t = TYPEcreate_from_body_anonymously(yymsp[-3].minor.yy378.body);
        SCOPEadd_super(t);
        yygotominor.yy91 = VARcreate(e, t);
    }

    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->inverse_symbol = yymsp[-1].minor.yy0.symbol;
}
#line 3674 "expparse.c"
        break;
      case 183: /* list_type ::= TOK_LIST limit_spec TOK_OF unique attribute_type */
#line 1565 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
}
#line 3685 "expparse.c"
        break;
      case 184: /* list_type ::= TOK_LIST TOK_OF unique attribute_type */
#line 1573 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
}
#line 3694 "expparse.c"
        break;
      case 185: /* literal ::= TOK_INTEGER_LITERAL */
#line 1580 "expparse.y"
{
    if (yymsp[0].minor.yy0.iVal == 0) {
        yygotominor.yy401 = LITERAL_ZERO;
    } else if (yymsp[0].minor.yy0.iVal == 1) {
	yygotominor.yy401 = LITERAL_ONE;
    } else {
	yygotominor.yy401 = EXPcreate_simple(Type_Integer);
	yygotominor.yy401->u.integer = (int)yymsp[0].minor.yy0.iVal;
	resolved_all(yygotominor.yy401);
    }
}
#line 3709 "expparse.c"
        break;
      case 186: /* literal ::= TOK_REAL_LITERAL */
#line 1592 "expparse.y"
{
    if (yymsp[0].minor.yy0.rVal == 0.0) {
	yygotominor.yy401 = LITERAL_ZERO;
    } else {
	yygotominor.yy401 = EXPcreate_simple(Type_Real);
	yygotominor.yy401->u.real = yymsp[0].minor.yy0.rVal;
	resolved_all(yygotominor.yy401);
    }
}
#line 3722 "expparse.c"
        break;
      case 187: /* literal ::= TOK_STRING_LITERAL */
#line 1602 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3731 "expparse.c"
        break;
      case 188: /* literal ::= TOK_STRING_LITERAL_ENCODED */
#line 1608 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String_Encoded);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3740 "expparse.c"
        break;
      case 189: /* literal ::= TOK_LOGICAL_LITERAL */
#line 1614 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Logical);
    yygotominor.yy401->u.logical = yymsp[0].minor.yy0.logical;
    resolved_all(yygotominor.yy401);
}
#line 3749 "expparse.c"
        break;
      case 190: /* literal ::= TOK_BINARY_LITERAL */
#line 1620 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Binary);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.binary;
    resolved_all(yygotominor.yy401);
}
#line 3758 "expparse.c"
        break;
      case 193: /* local_variable ::= id_list TOK_COLON parameter_type semicolon */
#line 1636 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-3].minor.yy371, sym, Symbol *)

    /* convert symbol to name-expression */

    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-1].minor.yy297);
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-3].minor.yy371);
}
#line 3777 "expparse.c"
        break;
      case 194: /* local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon */
#line 1653 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-4].minor.yy371, sym, Symbol *)
    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-2].minor.yy297);
    v->initializer = yymsp[-1].minor.yy401;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-4].minor.yy371);
}
#line 3794 "expparse.c"
        break;
      case 198: /* allow_generic_types ::= */
#line 1676 "expparse.y"
{
    tag_count = 0; /* don't signal an error if we find a generic_type */
}
#line 3801 "expparse.c"
        break;
      case 199: /* disallow_generic_types ::= */
#line 1681 "expparse.y"
{
    tag_count = -1; /* signal an error if we find a generic_type */
}
#line 3808 "expparse.c"
        break;
      case 200: /* defined_type ::= TOK_IDENTIFIER */
#line 1686 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(yygotominor.yy297);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3817 "expparse.c"
        break;
      case 201: /* defined_type_list ::= defined_type */
#line 1693 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd(yygotominor.yy371, (Generic)yymsp[0].minor.yy297);

}
#line 3826 "expparse.c"
        break;
      case 202: /* defined_type_list ::= defined_type_list TOK_COMMA defined_type */
#line 1699 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371,
    (Generic)yymsp[0].minor.yy297);
}
#line 3835 "expparse.c"
        break;
      case 205: /* optional_or_unique ::= */
#line 1716 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 0;
}
#line 3843 "expparse.c"
        break;
      case 206: /* optional_or_unique ::= TOK_OPTIONAL */
#line 1721 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 1;
}
#line 3851 "expparse.c"
        break;
      case 207: /* optional_or_unique ::= TOK_UNIQUE */
#line 1726 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 0;
}
#line 3859 "expparse.c"
        break;
      case 208: /* optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE */
      case 209: /* optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL */ yytestcase(yyruleno==209);
#line 1731 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 1;
}
#line 3868 "expparse.c"
        break;
      case 210: /* optional_fixed ::= */
#line 1742 "expparse.y"
{
    yygotominor.yy252.fixed = 0;
}
#line 3875 "expparse.c"
        break;
      case 211: /* optional_fixed ::= TOK_FIXED */
#line 1746 "expparse.y"
{
    yygotominor.yy252.fixed = 1;
}
#line 3882 "expparse.c"
        break;
      case 212: /* precision_spec ::= */
#line 1751 "expparse.y"
{
    yygotominor.yy401 = (Expression)0;
}
#line 3889 "expparse.c"
        break;
      case 213: /* precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */
      case 305: /* unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */ yytestcase(yyruleno==305);
#line 1755 "expparse.y"
{
    yygotominor.yy401 = yymsp[-1].minor.yy401;
}
#line 3897 "expparse.c"
        break;
      case 214: /* proc_call_statement ::= procedure_id actual_parameters semicolon */
#line 1765 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate(yymsp[-1].minor.yy371);
    yygotominor.yy332->symbol = *(yymsp[-2].minor.yy275);
}
#line 3905 "expparse.c"
        break;
      case 215: /* proc_call_statement ::= procedure_id semicolon */
#line 1770 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate((Linked_List)0);
    yygotominor.yy332->symbol = *(yymsp[-1].minor.yy275);
}
#line 3913 "expparse.c"
        break;
      case 216: /* procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon */
#line 1777 "expparse.y"
{
    PROCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3922 "expparse.c"
        break;
      case 217: /* procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon */
#line 1785 "expparse.y"
{
    Procedure p = CURRENT_SCOPE;
    p->u.proc->parameters = yymsp[-1].minor.yy371;
    p->u.proc->pcount = LISTget_length(yymsp[-1].minor.yy371);
    p->u.proc->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 3934 "expparse.c"
        break;
      case 218: /* ph_push_scope ::= TOK_IDENTIFIER */
#line 1795 "expparse.y"
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    tag_count = 0;

    if (print_objects_while_running & OBJ_PROCEDURE_BITS) {
	fprintf(stdout, "parse: %s (procedure)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(p, yymsp[0].minor.yy0.symbol, OBJ_PROCEDURE);
}
#line 3948 "expparse.c"
        break;
      case 222: /* group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1821 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy401->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy401->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3958 "expparse.c"
        break;
      case 223: /* qualifier ::= TOK_DOT TOK_IDENTIFIER */
#line 1829 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_DOT, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3968 "expparse.c"
        break;
      case 224: /* qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1836 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3978 "expparse.c"
        break;
      case 225: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET */
#line 1843 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_ARRAY_ELEMENT, (Expression)0,
	(Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-1].minor.yy401;
}
#line 3987 "expparse.c"
        break;
      case 226: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET */
#line 1850 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = TERN_EXPcreate(OP_SUBCOMPONENT, (Expression)0,
	(Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-3].minor.yy401;
    yygotominor.yy46.expr->e.op3 = yymsp[-1].minor.yy401;
}
#line 3997 "expparse.c"
        break;
      case 227: /* query_expression ::= query_start expression TOK_RIGHT_PAREN */
#line 1858 "expparse.y"
{
    yygotominor.yy401 = yymsp[-2].minor.yy401;
    yygotominor.yy401->u.query->expression = yymsp[-1].minor.yy401;
    POP_SCOPE();
}
#line 4006 "expparse.c"
        break;
      case 228: /* query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT */
#line 1866 "expparse.y"
{
    yygotominor.yy401 = QUERYcreate(yymsp[-3].minor.yy0.symbol, yymsp[-1].minor.yy401);
    SYMBOL_destroy(yymsp[-3].minor.yy0.symbol);
    PUSH_SCOPE(yygotominor.yy401->u.query->scope, (Symbol *)0, OBJ_QUERY);
}
#line 4015 "expparse.c"
        break;
      case 229: /* rel_op ::= TOK_LESS_THAN */
#line 1873 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_THAN;
}
#line 4022 "expparse.c"
        break;
      case 230: /* rel_op ::= TOK_GREATER_THAN */
#line 1877 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_THAN;
}
#line 4029 "expparse.c"
        break;
      case 231: /* rel_op ::= TOK_EQUAL */
#line 1881 "expparse.y"
{
    yygotominor.yy126 = OP_EQUAL;
}
#line 4036 "expparse.c"
        break;
      case 232: /* rel_op ::= TOK_LESS_EQUAL */
#line 1885 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_EQUAL;
}
#line 4043 "expparse.c"
        break;
      case 233: /* rel_op ::= TOK_GREATER_EQUAL */
#line 1889 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_EQUAL;
}
#line 4050 "expparse.c"
        break;
      case 234: /* rel_op ::= TOK_NOT_EQUAL */
#line 1893 "expparse.y"
{
    yygotominor.yy126 = OP_NOT_EQUAL;
}
#line 4057 "expparse.c"
        break;
      case 235: /* rel_op ::= TOK_INST_EQUAL */
#line 1897 "expparse.y"
{
    yygotominor.yy126 = OP_INST_EQUAL;
}
#line 4064 "expparse.c"
        break;
      case 236: /* rel_op ::= TOK_INST_NOT_EQUAL */
#line 1901 "expparse.y"
{
    yygotominor.yy126 = OP_INST_NOT_EQUAL;
}
#line 4071 "expparse.c"
        break;
      case 237: /* repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1909 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate(CURRENT_SCOPE, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);

    /* matching PUSH_SCOPE is in increment_control */
    POP_SCOPE();
}
#line 4081 "expparse.c"
        break;
      case 238: /* repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1917 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate((struct Scope_ *)0, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 4088 "expparse.c"
        break;
      case 239: /* return_statement ::= TOK_RETURN semicolon */
#line 1922 "expparse.y"
{
    yygotominor.yy332 = RETcreate((Expression)0);
}
#line 4095 "expparse.c"
        break;
      case 240: /* return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon */
#line 1927 "expparse.y"
{
    yygotominor.yy332 = RETcreate(yymsp[-2].minor.yy401);
}
#line 4102 "expparse.c"
        break;
      case 242: /* rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon */
#line 1938 "expparse.y"
{
    RULEput_body(CURRENT_SCOPE, yymsp[-3].minor.yy371);
    RULEput_where(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-4].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 4112 "expparse.c"
        break;
      case 243: /* rule_formal_parameter ::= TOK_IDENTIFIER */
#line 1946 "expparse.y"
{
    Expression e;
    Type t;

    /* it's true that we know it will be an entity_ type later */
    TypeBody tb = TYPEBODYcreate(set_);
    tb->base = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(tb->base);
    t = TYPEcreate_from_body_anonymously(tb);
    SCOPEadd_super(t);
    e = EXPcreate_from_symbol(t, yymsp[0].minor.yy0.symbol);
    yygotominor.yy91 = VARcreate(e, t);
    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->flags.parameter = true;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[0].minor.yy0.symbol->name, (Generic)yygotominor.yy91,
	yymsp[0].minor.yy0.symbol, OBJ_VARIABLE);
}
#line 4135 "expparse.c"
        break;
      case 244: /* rule_formal_parameter_list ::= rule_formal_parameter */
#line 1967 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd(yygotominor.yy371, (Generic)yymsp[0].minor.yy91); 
}
#line 4143 "expparse.c"
        break;
      case 245: /* rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter */
#line 1973 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 4151 "expparse.c"
        break;
      case 246: /* rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon */
#line 1980 "expparse.y"
{
    CURRENT_SCOPE->u.rule->parameters = yymsp[-2].minor.yy371;

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4160 "expparse.c"
        break;
      case 247: /* rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN */
#line 1988 "expparse.y"
{
    Rule r = ALGcreate(OBJ_RULE);

    if (print_objects_while_running & OBJ_RULE_BITS) {
	fprintf(stdout, "parse: %s (rule)\n", yymsp[-2].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(r, yymsp[-2].minor.yy0.symbol, OBJ_RULE);

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4175 "expparse.c"
        break;
      case 251: /* schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon */
#line 2015 "expparse.y"
{
    POP_SCOPE();
}
#line 4182 "expparse.c"
        break;
      case 253: /* schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon */
#line 2024 "expparse.y"
{
    Schema schema = ( Schema ) DICTlookup(CURRENT_SCOPE->symbol_table, yymsp[-1].minor.yy0.symbol->name);

    if (print_objects_while_running & OBJ_SCHEMA_BITS) {
	fprintf(stdout, "parse: %s (schema)\n", yymsp[-1].minor.yy0.symbol->name);
    }

    if (EXPRESSignore_duplicate_schemas && schema) {
	SCANskip_to_end_schema(parseData.scanner);
	PUSH_SCOPE_DUMMY();
    } else {
	schema = SCHEMAcreate();
	LISTadd_last(PARSEnew_schemas, (Generic)schema);
	PUSH_SCOPE(schema, yymsp[-1].minor.yy0.symbol, OBJ_SCHEMA);
    }
}
#line 4202 "expparse.c"
        break;
      case 254: /* select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */
#line 2043 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(select_);
    yygotominor.yy477->list = yymsp[-1].minor.yy371;
}
#line 4210 "expparse.c"
        break;
      case 256: /* set_type ::= TOK_SET limit_spec TOK_OF attribute_type */
#line 2054 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
}
#line 4220 "expparse.c"
        break;
      case 258: /* skip_statement ::= TOK_SKIP semicolon */
#line 2067 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_SKIP;
}
#line 4227 "expparse.c"
        break;
      case 259: /* statement ::= alias_statement */
      case 260: /* statement ::= assignment_statement */ yytestcase(yyruleno==260);
      case 261: /* statement ::= case_statement */ yytestcase(yyruleno==261);
      case 262: /* statement ::= compound_statement */ yytestcase(yyruleno==262);
      case 263: /* statement ::= escape_statement */ yytestcase(yyruleno==263);
      case 264: /* statement ::= if_statement */ yytestcase(yyruleno==264);
      case 265: /* statement ::= proc_call_statement */ yytestcase(yyruleno==265);
      case 266: /* statement ::= repeat_statement */ yytestcase(yyruleno==266);
      case 267: /* statement ::= return_statement */ yytestcase(yyruleno==267);
      case 268: /* statement ::= skip_statement */ yytestcase(yyruleno==268);
#line 2072 "expparse.y"
{
    yygotominor.yy332 = yymsp[0].minor.yy332;
}
#line 4243 "expparse.c"
        break;
      case 271: /* statement_rep ::= statement statement_rep */
#line 2121 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
    LISTadd_first(yygotominor.yy371, (Generic)yymsp[-1].minor.yy332); 
}
#line 4251 "expparse.c"
        break;
      case 272: /* subsuper_decl ::= */
#line 2131 "expparse.y"
{
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4260 "expparse.c"
        break;
      case 273: /* subsuper_decl ::= supertype_decl */
#line 2137 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[0].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[0].minor.yy385.abstract;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4269 "expparse.c"
        break;
      case 274: /* subsuper_decl ::= subtype_decl */
#line 2143 "expparse.y"
{
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
}
#line 4278 "expparse.c"
        break;
      case 275: /* subsuper_decl ::= supertype_decl subtype_decl */
#line 2149 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[-1].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[-1].minor.yy385.abstract;
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
}
#line 4287 "expparse.c"
        break;
      case 277: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE */
#line 2162 "expparse.y"
{
    yygotominor.yy385.subtypes = (Expression)0;
    yygotominor.yy385.abstract = true;
}
#line 4295 "expparse.c"
        break;
      case 278: /* supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2168 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = false;
}
#line 4303 "expparse.c"
        break;
      case 279: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2174 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = true;
}
#line 4311 "expparse.c"
        break;
      case 280: /* supertype_expression ::= supertype_factor */
#line 2180 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy385.subtypes;
}
#line 4318 "expparse.c"
        break;
      case 281: /* supertype_expression ::= supertype_expression TOK_AND supertype_factor */
#line 2184 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4325 "expparse.c"
        break;
      case 282: /* supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor */
#line 2189 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_ANDOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4332 "expparse.c"
        break;
      case 284: /* supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression */
#line 2200 "expparse.y"
{
    LISTadd_last(yymsp[-2].minor.yy371, (Generic)yymsp[0].minor.yy401);
    yygotominor.yy371 = yymsp[-2].minor.yy371;
}
#line 4340 "expparse.c"
        break;
      case 285: /* supertype_factor ::= identifier */
#line 2206 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[0].minor.yy401;
}
#line 4347 "expparse.c"
        break;
      case 286: /* supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN */
#line 2211 "expparse.y"
{
    yygotominor.yy385.subtypes = EXPcreate(Type_Oneof);
    yygotominor.yy385.subtypes->u.list = yymsp[-1].minor.yy371;
}
#line 4355 "expparse.c"
        break;
      case 287: /* supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2216 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
}
#line 4362 "expparse.c"
        break;
      case 288: /* type ::= aggregation_type */
      case 289: /* type ::= basic_type */ yytestcase(yyruleno==289);
      case 291: /* type ::= select_type */ yytestcase(yyruleno==291);
#line 2221 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = yymsp[0].minor.yy477;
}
#line 4372 "expparse.c"
        break;
      case 293: /* type_item_body ::= type */
#line 2246 "expparse.y"
{
    CURRENT_SCOPE->u.type->head = yymsp[0].minor.yy378.type;
    CURRENT_SCOPE->u.type->body = yymsp[0].minor.yy378.body;
}
#line 4380 "expparse.c"
        break;
      case 295: /* ti_start ::= TOK_IDENTIFIER TOK_EQUAL */
#line 2254 "expparse.y"
{
    Type t = TYPEcreate_name(yymsp[-1].minor.yy0.symbol);
    PUSH_SCOPE(t, yymsp[-1].minor.yy0.symbol, OBJ_TYPE);
}
#line 4388 "expparse.c"
        break;
      case 297: /* td_start ::= TOK_TYPE type_item where_rule_OPT */
#line 2265 "expparse.y"
{
    CURRENT_SCOPE->where = yymsp[0].minor.yy371;
    POP_SCOPE();
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 4397 "expparse.c"
        break;
      case 298: /* general_ref ::= assignable group_ref */
#line 2272 "expparse.y"
{
    yymsp[0].minor.yy401->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 4405 "expparse.c"
        break;
      case 308: /* unary_expression ::= TOK_NOT unary_expression */
#line 2315 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NOT, yymsp[0].minor.yy401);
}
#line 4412 "expparse.c"
        break;
      case 310: /* unary_expression ::= TOK_MINUS unary_expression */
#line 2323 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NEGATE, yymsp[0].minor.yy401);
}
#line 4419 "expparse.c"
        break;
      case 311: /* unique ::= */
#line 2328 "expparse.y"
{
    yygotominor.yy252.unique = 0;
}
#line 4426 "expparse.c"
        break;
      case 312: /* unique ::= TOK_UNIQUE */
#line 2332 "expparse.y"
{
    yygotominor.yy252.unique = 1;
}
#line 4433 "expparse.c"
        break;
      case 313: /* qualified_attr ::= TOK_IDENTIFIER */
#line 2337 "expparse.y"
{
    yygotominor.yy457 = QUAL_ATTR_new();
    yygotominor.yy457->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4441 "expparse.c"
        break;
      case 314: /* qualified_attr ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 2343 "expparse.y"
{
    yygotominor.yy457 = QUAL_ATTR_new();
    yygotominor.yy457->entity = yymsp[-2].minor.yy0.symbol;
    yygotominor.yy457->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4450 "expparse.c"
        break;
      case 315: /* qualified_attr_list ::= qualified_attr */
#line 2350 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy457);
}
#line 4458 "expparse.c"
        break;
      case 316: /* qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr */
#line 2355 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy457);
}
#line 4466 "expparse.c"
        break;
      case 317: /* labelled_attrib_list ::= qualified_attr_list semicolon */
#line 2361 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)EXPRESSION_NULL);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4474 "expparse.c"
        break;
      case 318: /* labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon */
#line 2367 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)yymsp[-3].minor.yy0.symbol); 
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4482 "expparse.c"
        break;
      case 319: /* labelled_attrib_list_list ::= labelled_attrib_list */
#line 2374 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371);
}
#line 4490 "expparse.c"
        break;
      case 320: /* labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list */
#line 2380 "expparse.y"
{
    LISTadd_last(yymsp[-1].minor.yy371, (Generic)yymsp[0].minor.yy371);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4498 "expparse.c"
        break;
      case 323: /* until_control ::= */
      case 332: /* while_control ::= */ yytestcase(yyruleno==332);
#line 2395 "expparse.y"
{
    yygotominor.yy401 = 0;
}
#line 4506 "expparse.c"
        break;
      case 325: /* where_clause ::= expression semicolon */
#line 2404 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = SYMBOLcreate("<unnamed>", yylineno, current_filename);
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;
}
#line 4515 "expparse.c"
        break;
      case 326: /* where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon */
#line 2410 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = yymsp[-3].minor.yy0.symbol;
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;

    if (!CURRENT_SCOPE->symbol_table) {
	CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-3].minor.yy0.symbol->name, (Generic)yygotominor.yy234,
	yymsp[-3].minor.yy0.symbol, OBJ_WHERE);
}
#line 4531 "expparse.c"
        break;
      case 327: /* where_clause_list ::= where_clause */
#line 2424 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4539 "expparse.c"
        break;
      case 328: /* where_clause_list ::= where_clause_list where_clause */
#line 2429 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4547 "expparse.c"
        break;
      default:
      /* (4) action_body_item_rep ::= */ yytestcase(yyruleno==4);
      /* (41) block_list ::= */ yytestcase(yyruleno==41);
      /* (62) constant_body_list ::= */ yytestcase(yyruleno==62);
      /* (86) express_file ::= schema_decl_list */ yytestcase(yyruleno==86);
      /* (160) parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==160);
      /* (169) interface_specification_list ::= */ yytestcase(yyruleno==169);
      /* (195) local_body ::= */ yytestcase(yyruleno==195);
      /* (197) local_decl ::= TOK_LOCAL allow_generic_types local_body TOK_END_LOCAL semicolon disallow_generic_types */ yytestcase(yyruleno==197);
      /* (294) type_item ::= ti_start type_item_body semicolon */ yytestcase(yyruleno==294);
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 2457 "expparse.y"

    yyerrstatus++;
    fprintf(stderr, "Express parser experienced syntax error at line %d.\n", yylineno);
#line 4622 "expparse.c"
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Parse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  ParseTOKENTYPE yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  ParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact<YYNSTATE ){
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
