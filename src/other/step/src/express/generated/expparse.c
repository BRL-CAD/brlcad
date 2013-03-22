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
#line 2474 "expparse.y"

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
#define YYNOCODE 281
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE  YYSTYPE 
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  struct entity_body yy24;
  struct subsuper_decl yy34;
  Qualified_Attr* yy101;
  Expression yy145;
  Type yy155;
  Op_Code yy206;
  struct upper_lower yy210;
  Integer yy215;
  struct type_flags yy224;
  Case_Item yy259;
  struct subtypes yy261;
  struct type_either yy352;
  struct qualifier yy384;
  Where yy428;
  Variable yy443;
  TypeBody yy457;
  Symbol* yy461;
  Linked_List yy471;
  Statement yy522;
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
#define YY_ACTTAB_COUNT (2731)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    79,   80,  624,   69,   70,   47,  388,   73,   71,   72,
 /*    10 */    74,  741,   81,   76,   75,   16,   44,  593,  404,  403,
 /*    20 */    77,  491,  490,  396,  372,  609,   59,   58,  250,  612,
 /*    30 */   272,  607,   62,   35,  606,  387,  604,  608,   68,   91,
 /*    40 */   603,   46,  153,  158,  350,  629,  628,  114,  113,  579,
 /*    50 */    79,   80,  228,  167,  622,  168,  531,  251,  111,  623,
 /*    60 */   310,   15,   81,  621,  109,   16,   44,  409,  631,  616,
 /*    70 */   457,  533,  159,  396,  114,  113,  618,  617,  615,  614,
 /*    80 */   613,  413,  416,  184,  417,  180,  415,  169,   68,  395,
 /*    90 */   410,  990,  119,  411,  204,  629,  628,  114,  113,  176,
 /*   100 */    79,   80,  622,  552,  622,  559,  526,  246,  543,  623,
 /*   110 */   170,  621,   81,  621,  144,   16,   44,  557,   89,  616,
 /*   120 */   319,  131,  305,  396,   19,   26,  618,  617,  615,  614,
 /*   130 */   613,  532,  475,  462,  474,  477,  381,  473,   68,  395,
 /*   140 */   354,  476,  305,  236,  643,  629,  628,  371,  369,  365,
 /*   150 */    79,   80,  115,  348,  622,  475,  478,  474,  477,  623,
 /*   160 */   473,  315,   81,  621,  476,   16,   44,  455,  566,  616,
 /*   170 */   404,  403,   77,  396,  219,    5,  618,  617,  615,  614,
 /*   180 */   613,  475,  472,  474,  477,  630,  473,   23,   68,  395,
 /*   190 */   476,  533,  314,  114,  113,  629,  628,  404,  352,   77,
 /*   200 */    79,   80,  524,  350,  622,  475,  471,  474,  477,  623,
 /*   210 */   473,   41,   81,  621,  476,   16,   44,  363,   84,  616,
 /*   220 */   122,  337,  622,  316,  453,  534,  618,  617,  615,  614,
 /*   230 */   613,  621,  529,  537,  410,   62,  120,  553,   68,  395,
 /*   240 */   122,  450,  313,  568,  170,  629,  628,  787,  154,  535,
 /*   250 */    36,  557,  224,  253,  622,  338,  518,  249,  225,  623,
 /*   260 */    69,   70,  252,  621,   73,   71,   72,   74,   41,  616,
 /*   270 */    76,   75,  136,  312,  386,  239,  618,  617,  615,  614,
 /*   280 */   613,  599,  597,  600,   75,  595,  594,  598,  601,  395,
 /*   290 */   596,   69,   70,  522,  413,   73,   71,   72,   74,  619,
 /*   300 */   525,   76,   75,  108,  528,  154,  654,  105,  339,  875,
 /*   310 */   626,  115,  481,  518,  787,  504,  503,  502,  501,  500,
 /*   320 */   499,  498,  497,  496,  495,    2,  306,  144,  579,  458,
 /*   330 */   129,  625,  165,   29,  163,  633,  247,  245,  586,  585,
 /*   340 */   244,  242,   73,   71,   72,   74,  529,  234,   76,   75,
 /*   350 */   360,  171,  154,  204,   14,  205,   12,  133,    3,   13,
 /*   360 */   518,  341,  459,  125,  104,  162,  161,  347,  366,  223,
 /*   370 */   210,  518,  504,  503,  502,  501,  500,  499,  498,  497,
 /*   380 */   496,  495,    2,  443,   69,   70,  339,  129,   73,   71,
 /*   390 */    72,   74,  394,  875,   76,   75,  551,  359,  154,   93,
 /*   400 */   339,  579,  591,   65,  529,  248,  518,  175,  633,  247,
 /*   410 */   245,  586,  585,  244,  242,    3,  393,  652,  530,  504,
 /*   420 */   503,  502,  501,  500,  499,  498,  497,  496,  495,    2,
 /*   430 */   334,  652,  568,  442,  129,  302,  224,   21,  174,  173,
 /*   440 */   650,  186,  154,  308,  187,  652,  651,  649,  648,  217,
 /*   450 */   518,  647,  646,  645,  644,  118,  421,   41,  121,  183,
 /*   460 */    24,  116,    3,  386,   86,  504,  503,  502,  501,  500,
 /*   470 */   499,  498,  497,  496,  495,    2,  527,  333,   41,  216,
 /*   480 */   129,  429,  366,   41,  335,  233,  326,  358,  322,  154,
 /*   490 */   230,   39,  238,  635,   45,   39,  636,  518,  164,  637,
 /*   500 */   642,  641,  102,  640,  639,   10,  353,   43,    3,  504,
 /*   510 */   503,  502,  501,  500,  499,  498,  497,  496,  495,    2,
 /*   520 */    39,  652,   39,   92,  129,  560,  565,  203,  409,  100,
 /*   530 */   511,  320,  652,  609,  356,  154,   42,  612,  151,  607,
 /*   540 */   366,  303,  606,  518,  604,  608,  134,   78,  603,   46,
 /*   550 */   153,  158,    3,  560,  433,  357,  504,  503,  502,  501,
 /*   560 */   500,  499,  498,  497,  496,  495,    2,  423,  301,   22,
 /*   570 */   200,  129,  475,  470,  474,  477,  172,  473,  240,  154,
 /*   580 */   494,  476,  537,  557,  579,  487,  549,  518,  248,  382,
 /*   590 */   175,  633,  247,  245,  586,  585,  244,  242,  374,    3,
 /*   600 */    87,  572,  504,  503,  502,  501,  500,  499,  498,  497,
 /*   610 */   496,  495,    2,  573,  385,  246,  384,  129,  567,  383,
 /*   620 */   381,  174,  173,  130,  493,  380,  154,  410,  378,  569,
 /*   630 */    14,  205,  379,  558,  518,   13,  126,  331,  562,  373,
 /*   640 */   232,   27,  309,  362,  520,    3,  504,  503,  502,  501,
 /*   650 */   500,  499,  498,  497,  496,  495,    2,  319,   14,  205,
 /*   660 */    34,  129,    7,   13,  231,  336,  229,  370,  452,  139,
 /*   670 */   226,  355,  220,  622,  368,  475,  469,  474,  477,  367,
 /*   680 */   473,  110,  621,   33,  476,   14,  205,  106,  364,    3,
 /*   690 */    13,  107,  189,  504,  503,  502,  501,  500,  499,  498,
 /*   700 */   497,  496,  495,    2,  523,  361,  222,  510,  129,  117,
 /*   710 */   218,  128,  221,  166,    9,   20,  486,  485,  484,  652,
 /*   720 */   211,   32,  213,  208,   18,  207,  351,  647,  646,  645,
 /*   730 */   644,  118,  475,  467,  474,  477,    3,  473,  877,   82,
 /*   740 */    25,  476,    9,   20,  486,  485,  484,  206,  346,  468,
 /*   750 */    99,  199,  202,   97,  160,  647,  646,  645,  644,  118,
 /*   760 */   335,  340,   95,   94,  461,  198,  194,  193,  135,    9,
 /*   770 */    20,  486,  485,  484,  434,  190,  328,  426,  185,  327,
 /*   780 */   325,  188,  647,  646,  645,  644,  118,  323,  335,  424,
 /*   790 */    55,   53,   56,   49,   51,   50,   54,   57,   48,   52,
 /*   800 */   321,  181,   59,   58,  177,  638,  635,  178,   62,  636,
 /*   810 */   529,    8,  637,  642,  641,  335,  640,  639,  449,   60,
 /*   820 */   290,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*   830 */    52,  123,  197,   59,   58,  652,  329,   11,  408,   62,
 /*   840 */   653,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*   850 */    52,  143,   39,   59,   58,  475,  466,  474,  477,   62,
 /*   860 */   473,   63,   21,  632,  476,  592,  587,  243,  611,  584,
 /*   870 */   627,  583,   55,   53,   56,   49,   51,   50,   54,   57,
 /*   880 */    48,   52,  528,   88,   59,   58,  241,  582,  576,   85,
 /*   890 */    62,  237,   55,   53,   56,   49,   51,   50,   54,   57,
 /*   900 */    48,   52,   37,  570,   59,   58,  112,   69,   70,  235,
 /*   910 */    62,   73,   71,   72,   74,  140,  141,   76,   75,  605,
 /*   920 */   377,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*   930 */    52,  550,  579,   59,   58,  548,   83,  538,  544,   62,
 /*   940 */   541,  540,  539,  547,  371,   61,  366,  546,  588,  545,
 /*   950 */    55,   53,   56,   49,   51,   50,   54,   57,   48,   52,
 /*   960 */   536,  215,   59,   58,   27,  214,   28,  138,   62,  475,
 /*   970 */   465,  474,  477,  610,  473,  209,  256,  521,  476,   40,
 /*   980 */   454,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*   990 */    52,  516,  307,   59,   58,  460,  634,  635,  515,   62,
 /*  1000 */   636,  101,  514,  637,  642,  641,  513,  640,  639,  581,
 /*  1010 */   512,   55,   53,   56,   49,   51,   50,   54,   57,   48,
 /*  1020 */    52,  609,    4,   59,   58,  612,  265,  607,  508,   62,
 /*  1030 */   606,   98,  604,  608,  506,  505,  603,   46,  153,  158,
 /*  1040 */    38,    1,   55,   53,   56,   49,   51,   50,   54,   57,
 /*  1050 */    48,   52,  492,  488,   59,   58,  196,  480,  439,  456,
 /*  1060 */    62,  440,  438,  451,  441,  642,  641,  446,  640,  639,
 /*  1070 */   571,  436,   55,   53,   56,   49,   51,   50,   54,   57,
 /*  1080 */    48,   52,  392,  609,   59,   58,  195,  612,  282,  607,
 /*  1090 */    62,  254,  606,  448,  604,  608,  124,  437,  603,   46,
 /*  1100 */   153,  158,  447,  246,  142,  445,   55,   53,   56,   49,
 /*  1110 */    51,   50,   54,   57,   48,   52,  444,  192,   59,   58,
 /*  1120 */   304,  435,  191,  432,   62,   17,   55,   53,   56,   49,
 /*  1130 */    51,   50,   54,   57,   48,   52,  428,  330,   59,   58,
 /*  1140 */   431,  427,  430,  132,   62,  425,   55,   53,   56,   49,
 /*  1150 */    51,   50,   54,   57,   48,   52,  324,  182,   59,   58,
 /*  1160 */   529,   14,  205,  420,   62,  246,   13,  422,  580,  635,
 /*  1170 */   289,  419,  636,  212,  179,  637,  642,  641,  418,  640,
 /*  1180 */   639,    6,   90,   55,   53,   56,   49,   51,   50,   54,
 /*  1190 */    57,   48,   52,  414,  407,   59,   58,  412,  390,  389,
 /*  1200 */   556,   62,  555,  554,  376,  375,   31,  542,   55,   53,
 /*  1210 */    56,   49,   51,   50,   54,   57,   48,   52,  609,  507,
 /*  1220 */    59,   58,  612,  342,  607,  103,   62,  606,  343,  604,
 /*  1230 */   608,  344,  528,  603,   46,  154,  157,   96,  345,  602,
 /*  1240 */   137,  520,   64,  518,  509,  483,   20,  486,  485,  484,
 /*  1250 */   620,  564,  563,  519,   67,   30,  154,  482,  647,  646,
 /*  1260 */   645,  644,  118,   66,  518,  391,  609,  479,  332,  991,
 /*  1270 */   612,  282,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  1280 */   991,  603,   46,  153,  158,  475,  464,  474,  477,  991,
 /*  1290 */   473,  335,  991,  652,  476,  991,  366,  318,  991,  991,
 /*  1300 */   246,  991,  504,  503,  502,  501,  500,  499,  498,  497,
 /*  1310 */   496,  495,  517,  475,  463,  474,  477,  129,  473,  991,
 /*  1320 */   991,  991,  476,  504,  503,  502,  501,  500,  499,  498,
 /*  1330 */   497,  496,  495,  489,  991,  991,  991,  991,  129,  991,
 /*  1340 */   991,  991,  991,  991,  991,  991,  991,  991,  246,  991,
 /*  1350 */    55,   53,   56,   49,   51,   50,   54,   57,   48,   52,
 /*  1360 */   991,  991,   59,   58,  475,  201,  474,  477,   62,  473,
 /*  1370 */   991,  609,  991,  476,  991,  612,  151,  607,  991,  991,
 /*  1380 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1390 */   609,  991,  991,  991,  612,  280,  607,  991,  991,  606,
 /*  1400 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  609,
 /*  1410 */   991,  991,  991,  612,  589,  607,  991,  991,  606,  991,
 /*  1420 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  317,
 /*  1430 */   609,  991,  991,  991,  612,  270,  607,  991,  991,  606,
 /*  1440 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  991,
 /*  1450 */   991,  609,  991,  246,  529,  612,  269,  607,  991,  991,
 /*  1460 */   606,  991,  604,  608,  311,  529,  603,   46,  153,  158,
 /*  1470 */   991,  991,  246,  991,  991,  288,  561,  991,  609,  991,
 /*  1480 */   991,  991,  612,  406,  607,  991,  991,  606,  991,  604,
 /*  1490 */   608,  246,  991,  603,   46,  153,  158,  991,  991,  609,
 /*  1500 */   991,  991,  991,  612,  405,  607,  991,  991,  606,  991,
 /*  1510 */   604,  608,  246,  991,  603,   46,  153,  158,  991,  991,
 /*  1520 */   991,  991,  991,  609,  991,  991,  528,  612,  300,  607,
 /*  1530 */   991,  991,  606,  246,  604,  608,  991,  528,  603,   46,
 /*  1540 */   153,  158,  991,  991,  991,  991,  609,  991,  991,  991,
 /*  1550 */   612,  299,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  1560 */   246,  603,   46,  153,  158,  991,  991,  609,  991,  991,
 /*  1570 */   991,  612,  298,  607,  991,  991,  606,  991,  604,  608,
 /*  1580 */   991,  246,  603,   46,  153,  158,  991,  991,  991,  991,
 /*  1590 */   366,  609,  991,  991,  991,  612,  297,  607,  991,  991,
 /*  1600 */   606,  366,  604,  608,  991,  246,  603,   46,  153,  158,
 /*  1610 */   991,  991,  991,  991,  991,  609,  991,  991,  991,  612,
 /*  1620 */   296,  607,  991,  991,  606,  991,  604,  608,  246,  991,
 /*  1630 */   603,   46,  153,  158,  991,  991,  609,  991,  991,  991,
 /*  1640 */   612,  295,  607,  991,  991,  606,  991,  604,  608,  246,
 /*  1650 */   991,  603,   46,  153,  158,  991,  991,  991,  991,  991,
 /*  1660 */   609,  991,  991,  991,  612,  294,  607,  991,  991,  606,
 /*  1670 */   991,  604,  608,  246,  991,  603,   46,  153,  158,  991,
 /*  1680 */   991,  991,  991,  609,  991,  991,  991,  612,  293,  607,
 /*  1690 */   991,  991,  606,  991,  604,  608,  991,  246,  603,   46,
 /*  1700 */   153,  158,  991,  991,  609,  991,  991,  991,  612,  292,
 /*  1710 */   607,  991,  991,  606,  991,  604,  608,  991,  246,  603,
 /*  1720 */    46,  153,  158,  609,  991,  991,  991,  612,  291,  607,
 /*  1730 */   991,  991,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  1740 */   153,  158,  246,  609,  991,  991,  991,  612,  281,  607,
 /*  1750 */   991,  991,  606,  991,  604,  608,  991,  991,  603,   46,
 /*  1760 */   153,  158,  991,  991,  609,  246,  991,  991,  612,  268,
 /*  1770 */   607,  991,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  1780 */    46,  153,  158,  991,  991,  991,  246,  991,  991,  991,
 /*  1790 */   991,  991,  609,  991,  991,  991,  612,  267,  607,  991,
 /*  1800 */   991,  606,  991,  604,  608,  246,  991,  603,   46,  153,
 /*  1810 */   158,  609,  991,  991,  991,  612,  266,  607,  991,  991,
 /*  1820 */   606,  991,  604,  608,  991,  246,  603,   46,  153,  158,
 /*  1830 */   991,  991,  609,  991,  991,  991,  612,  279,  607,  991,
 /*  1840 */   991,  606,  991,  604,  608,  991,  246,  603,   46,  153,
 /*  1850 */   158,  609,  991,  991,  991,  612,  278,  607,  991,  991,
 /*  1860 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1870 */   991,  609,  991,  991,  246,  612,  264,  607,  991,  991,
 /*  1880 */   606,  991,  604,  608,  991,  991,  603,   46,  153,  158,
 /*  1890 */   991,  991,  609,  246,  991,  991,  612,  263,  607,  991,
 /*  1900 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  153,
 /*  1910 */   158,  991,  991,  991,  246,  991,  991,  991,  991,  991,
 /*  1920 */   609,  991,  991,  991,  612,  262,  607,  991,  991,  606,
 /*  1930 */   991,  604,  608,  246,  991,  603,   46,  153,  158,  609,
 /*  1940 */   991,  991,  991,  612,  261,  607,  991,  991,  606,  991,
 /*  1950 */   604,  608,  991,  246,  603,   46,  153,  158,  991,  991,
 /*  1960 */   609,  991,  991,  991,  612,  277,  607,  991,  991,  606,
 /*  1970 */   991,  604,  608,  991,  246,  603,   46,  153,  158,  609,
 /*  1980 */   991,  991,  991,  612,  150,  607,  991,  991,  606,  991,
 /*  1990 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  609,
 /*  2000 */   991,  991,  246,  612,  149,  607,  991,  991,  606,  991,
 /*  2010 */   604,  608,  991,  991,  603,   46,  153,  158,  991,  991,
 /*  2020 */   609,  246,  991,  991,  612,  260,  607,  991,  991,  606,
 /*  2030 */   991,  604,  608,  991,  991,  603,   46,  153,  158,  991,
 /*  2040 */   991,  991,  246,  991,  991,  991,  991,  991,  609,  991,
 /*  2050 */   991,  991,  612,  259,  607,  991,  991,  606,  991,  604,
 /*  2060 */   608,  246,  991,  603,   46,  153,  158,  609,  991,  991,
 /*  2070 */   991,  612,  258,  607,  991,  991,  606,  991,  604,  608,
 /*  2080 */   991,  246,  603,   46,  153,  158,  991,  991,  609,  991,
 /*  2090 */   991,  991,  612,  148,  607,  991,  991,  606,  991,  604,
 /*  2100 */   608,  991,  246,  603,   46,  153,  158,  609,  991,  991,
 /*  2110 */   991,  612,  276,  607,  991,  991,  606,  991,  604,  608,
 /*  2120 */   991,  991,  603,   46,  153,  158,  991,  609,  991,  991,
 /*  2130 */   246,  612,  257,  607,  991,  991,  606,  991,  604,  608,
 /*  2140 */   991,  991,  603,   46,  153,  158,  991,  991,  609,  246,
 /*  2150 */   991,  991,  612,  275,  607,  991,  991,  606,  991,  604,
 /*  2160 */   608,  991,  991,  603,   46,  153,  158,  991,  991,  991,
 /*  2170 */   246,  991,  991,  991,  991,  991,  609,  991,  991,  991,
 /*  2180 */   612,  274,  607,  991,  991,  606,  991,  604,  608,  246,
 /*  2190 */   991,  603,   46,  153,  158,  609,  991,  991,  991,  612,
 /*  2200 */   273,  607,  991,  991,  606,  991,  604,  608,  991,  246,
 /*  2210 */   603,   46,  153,  158,  991,  991,  609,  991,  991,  991,
 /*  2220 */   612,  147,  607,  991,  991,  606,  991,  604,  608,  991,
 /*  2230 */   246,  603,   46,  153,  158,  609,  991,  991,  991,  612,
 /*  2240 */   271,  607,  309,  362,  606,  991,  604,  608,  991,  991,
 /*  2250 */   603,   46,  153,  158,  991,  991,  991,  991,  246,  991,
 /*  2260 */    34,  991,    7,  991,  991,  475,  127,  474,  477,  991,
 /*  2270 */   473,  991,  220,  622,  476,  116,  991,  246,  991,  363,
 /*  2280 */   991,  991,  621,   33,  991,  609,  453,  991,  991,  612,
 /*  2290 */   991,  607,  991,  991,  606,  991,  604,  608,  246,  991,
 /*  2300 */   603,   46,  283,  158,  991,  991,  991,  510,  991,  255,
 /*  2310 */   609,  128,  991,  166,  612,  253,  607,  246,  991,  606,
 /*  2320 */   211,  604,  608,  991,  991,  603,   46,  402,  158,  609,
 /*  2330 */   991,  991,  991,  612,  136,  607,  991,  991,  606,  991,
 /*  2340 */   604,  608,  991,  991,  603,   46,  401,  158,  609,  991,
 /*  2350 */   991,  991,  612,  991,  607,  991,  991,  606,  991,  604,
 /*  2360 */   608,  991,  991,  603,   46,  400,  158,  246,  609,  991,
 /*  2370 */   991,  991,  612,  991,  607,  991,  991,  606,  991,  604,
 /*  2380 */   608,  991,  991,  603,   46,  399,  158,  991,  991,  991,
 /*  2390 */   609,  991,  246,  991,  612,  991,  607,  991,  991,  606,
 /*  2400 */   991,  604,  608,  991,  991,  603,   46,  398,  158,  991,
 /*  2410 */   609,  246,  991,  991,  612,  991,  607,  991,  991,  606,
 /*  2420 */   991,  604,  608,  991,  991,  603,   46,  397,  158,  609,
 /*  2430 */   246,  991,  991,  612,  991,  607,  991,  991,  606,  991,
 /*  2440 */   604,  608,  991,  991,  603,   46,  287,  158,  609,  991,
 /*  2450 */   246,  991,  612,  991,  607,  991,  991,  606,  991,  604,
 /*  2460 */   608,  991,  991,  603,   46,  286,  158,  991,  991,  609,
 /*  2470 */   991,  991,  246,  612,  991,  607,  991,  991,  606,  991,
 /*  2480 */   604,  608,  991,  991,  603,   46,  146,  158,  991,  991,
 /*  2490 */   991,  609,  246,  991,  991,  612,  991,  607,  991,  991,
 /*  2500 */   606,  991,  604,  608,  991,  991,  603,   46,  145,  158,
 /*  2510 */   991,  246,  991,  991,  991,  991,  991,  991,  991,  991,
 /*  2520 */   609,  991,  991,  991,  612,  991,  607,  991,  991,  606,
 /*  2530 */   246,  604,  608,  991,  991,  603,   46,  152,  158,  991,
 /*  2540 */   991,  991,  991,  991,  609,  991,  991,  991,  612,  991,
 /*  2550 */   607,  246,  991,  606,  991,  604,  608,  991,  991,  603,
 /*  2560 */    46,  284,  158,  609,  991,  991,  991,  612,  991,  607,
 /*  2570 */   991,  991,  606,  246,  604,  608,  991,  991,  603,   46,
 /*  2580 */   285,  158,  609,  991,  991,  991,  612,  991,  607,  991,
 /*  2590 */   991,  606,  991,  604,  608,  991,  991,  603,   46,  991,
 /*  2600 */   156,  609,  246,  991,  991,  612,  991,  607,  991,  991,
 /*  2610 */   606,  991,  604,  608,  991,  991,  603,   46,  991,  155,
 /*  2620 */   991,  991,  991,  991,   69,   70,  246,  991,   73,   71,
 /*  2630 */    72,   74,  991,  991,   76,   75,  991,  991,  991,  578,
 /*  2640 */   635,  991,  590,  636,  991,  246,  637,  642,  641,  991,
 /*  2650 */   640,  639,  991,  991,  991,  991,  991,  991,  577,  635,
 /*  2660 */   991,  991,  636,  991,  246,  637,  642,  641,  991,  640,
 /*  2670 */   639,  991,  991,  991,  991,  991,  991,  575,  635,  991,
 /*  2680 */   991,  636,  991,  246,  637,  642,  641,  991,  640,  639,
 /*  2690 */   574,  635,  991,  991,  636,  991,  991,  637,  642,  641,
 /*  2700 */   991,  640,  639,  227,  635,  991,  991,  636,  991,  991,
 /*  2710 */   637,  642,  641,  991,  640,  639,  991,  991,  349,  635,
 /*  2720 */   991,  991,  636,  991,  991,  637,  642,  641,  991,  640,
 /*  2730 */   639,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    11,   12,   28,   11,   12,   31,   66,   15,   16,   17,
 /*    10 */    18,    0,   23,   21,   22,   26,   27,   28,   24,   25,
 /*    20 */    26,  123,  124,   34,  125,  127,   13,   14,   80,  131,
 /*    30 */   132,  133,   19,   39,  136,   95,  138,  139,   49,   30,
 /*    40 */   142,  143,  144,  145,  136,   56,   57,   19,   20,   34,
 /*    50 */    11,   12,   30,   31,   65,   40,   28,  236,  159,   70,
 /*    60 */   162,  240,   23,   74,   27,   26,   27,  129,   29,   80,
 /*    70 */   167,   34,  169,   34,   19,   20,   87,   88,   89,   90,
 /*    80 */    91,  242,  261,  262,  263,  264,  265,   72,   49,  100,
 /*    90 */    79,  252,  253,  254,  191,   56,   57,   19,   20,   33,
 /*   100 */    11,   12,   65,  179,   65,   34,   28,  209,  129,   70,
 /*   110 */   186,   74,   23,   74,  275,   26,   27,  193,   33,   80,
 /*   120 */   109,  183,  170,   34,   30,   31,   87,   88,   89,   90,
 /*   130 */    91,   94,  212,  213,  214,  215,   65,  217,   49,  100,
 /*   140 */    51,  221,  170,  164,  165,   56,   57,  113,  114,  115,
 /*   150 */    11,   12,  244,  245,   65,  212,  213,  214,  215,   70,
 /*   160 */   217,  182,   23,   74,  221,   26,   27,  168,  230,   80,
 /*   170 */    24,   25,   26,   34,   77,   78,   87,   88,   89,   90,
 /*   180 */    91,  212,  213,  214,  215,   29,  217,   31,   49,  100,
 /*   190 */   221,   34,  171,   19,   20,   56,   57,   24,   25,   26,
 /*   200 */    11,   12,   28,  136,   65,  212,  213,  214,  215,   70,
 /*   210 */   217,   26,   23,   74,  221,   26,   27,   62,   33,   80,
 /*   220 */   268,  269,   65,   34,   69,  174,   87,   88,   89,   90,
 /*   230 */    91,   74,  136,  212,   79,   19,  178,  179,   49,  100,
 /*   240 */   268,  269,  146,   34,  186,   56,   57,   27,  128,   28,
 /*   250 */    30,  193,   31,   98,   65,  256,  136,  206,  207,   70,
 /*   260 */    11,   12,  107,   74,   15,   16,   17,   18,   26,   80,
 /*   270 */    21,   22,  117,  177,   65,   33,   87,   88,   89,   90,
 /*   280 */    91,    1,    2,    3,   22,    5,    6,    7,    8,  100,
 /*   290 */    10,   11,   12,  173,  242,   15,   16,   17,   18,   50,
 /*   300 */    28,   21,   22,   31,  208,  128,  254,   30,   31,   27,
 /*   310 */    34,  244,  245,  136,   27,  195,  196,  197,  198,  199,
 /*   320 */   200,  201,  202,  203,  204,  205,   32,  275,   34,  167,
 /*   330 */   210,   34,   38,   27,   40,   41,   42,   43,   44,   45,
 /*   340 */    46,   47,   15,   16,   17,   18,  136,  180,   21,   22,
 /*   350 */   173,   31,  128,  191,  149,  150,  151,  152,  238,  154,
 /*   360 */   136,   30,   28,  128,   33,   71,   72,   73,  272,  134,
 /*   370 */   148,  136,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   380 */   203,  204,  205,   28,   11,   12,   31,  210,   15,   16,
 /*   390 */    17,   18,   27,  111,   21,   22,  229,  173,  128,   30,
 /*   400 */    31,   34,   29,   30,  136,   38,  136,   40,   41,   42,
 /*   410 */    43,   44,   45,   46,   47,  238,   34,  111,  208,  195,
 /*   420 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  205,
 /*   430 */    63,  111,   34,   28,  210,  185,   31,   27,   71,   72,
 /*   440 */   235,   28,  128,  173,   31,  111,  241,  242,  243,  157,
 /*   450 */   136,  246,  247,  248,  249,  250,   28,   26,   60,   31,
 /*   460 */    39,   58,  238,   65,   33,  195,  196,  197,  198,  199,
 /*   470 */   200,  201,  202,  203,  204,  205,  208,  110,   26,  257,
 /*   480 */   210,  231,  272,   26,  279,   33,   83,  173,   85,  128,
 /*   490 */    33,   26,  211,  212,  101,   26,  215,  136,   33,  218,
 /*   500 */   219,  220,   33,  222,  223,  160,  161,   30,  238,  195,
 /*   510 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  205,
 /*   520 */    26,  111,   26,  266,  210,  175,  176,   33,  129,   33,
 /*   530 */   238,  274,  111,  127,  173,  128,   30,  131,  132,  133,
 /*   540 */   272,  171,  136,  136,  138,  139,  277,  278,  142,  143,
 /*   550 */   144,  145,  238,  175,  176,   34,  195,  196,  197,  198,
 /*   560 */   199,  200,  201,  202,  203,  204,  205,  258,  259,  163,
 /*   570 */   140,  210,  212,  213,  214,  215,  186,  217,   33,  128,
 /*   580 */   173,  221,  212,  193,   34,  135,  212,  136,   38,   34,
 /*   590 */    40,   41,   42,   43,   44,   45,   46,   47,  224,  238,
 /*   600 */    33,   66,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   610 */   203,  204,  205,   95,   25,  209,   34,  210,   34,   24,
 /*   620 */    65,   71,   72,   30,  173,   25,  128,   79,   24,  230,
 /*   630 */   149,  150,   34,   34,  136,  154,   30,  156,  232,   36,
 /*   640 */    33,  120,   34,   35,  194,  238,  195,  196,  197,  198,
 /*   650 */   199,  200,  201,  202,  203,  204,  205,  109,  149,  150,
 /*   660 */    52,  210,   54,  154,   33,  156,   34,   33,  238,   27,
 /*   670 */    61,  173,   64,   65,  115,  212,  213,  214,  215,   33,
 /*   680 */   217,   27,   74,   75,  221,  149,  150,   27,   33,  238,
 /*   690 */   154,   27,  156,  195,  196,  197,  198,  199,  200,  201,
 /*   700 */   202,  203,  204,  205,   34,   34,   37,   99,  210,   36,
 /*   710 */    77,  103,   55,  105,  233,  234,  235,  236,  237,  111,
 /*   720 */   112,   39,  104,  104,   30,   53,   34,  246,  247,  248,
 /*   730 */   249,  250,  212,  213,  214,  215,  238,  217,  111,   30,
 /*   740 */    39,  221,  233,  234,  235,  236,  237,   59,   30,   34,
 /*   750 */    33,   93,   33,   33,   33,  246,  247,  248,  249,  250,
 /*   760 */   279,   34,   33,   30,   34,   97,  116,   33,   27,  233,
 /*   770 */   234,  235,  236,  237,    1,   68,   34,   27,   34,   36,
 /*   780 */    84,  106,  246,  247,  248,  249,  250,   82,  279,   34,
 /*   790 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   800 */    84,   34,   13,   14,   34,  211,  212,  108,   19,  215,
 /*   810 */   136,  239,  218,  219,  220,  279,  222,  223,  271,   30,
 /*   820 */   146,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   830 */    10,  270,  155,   13,   14,  111,  153,  240,  227,   19,
 /*   840 */   238,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   850 */    10,  238,   26,   13,   14,  212,  213,  214,  215,   19,
 /*   860 */   217,   27,   27,  141,  221,  157,  141,  141,   28,  189,
 /*   870 */    50,   96,    1,    2,    3,    4,    5,    6,    7,    8,
 /*   880 */     9,   10,  208,  192,   13,   14,  141,  189,   95,  192,
 /*   890 */    19,  137,    1,    2,    3,    4,    5,    6,    7,    8,
 /*   900 */     9,   10,   39,  238,   13,   14,   95,   11,   12,  181,
 /*   910 */    19,   15,   16,   17,   18,   86,  184,   21,   22,   28,
 /*   920 */    34,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   930 */    10,  229,   34,   13,   14,  212,  190,  174,  238,   19,
 /*   940 */    66,  238,  238,  212,  113,   49,  272,  212,   28,  212,
 /*   950 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   960 */   212,  148,   13,   14,  120,  147,  118,  255,   19,  212,
 /*   970 */   213,  214,  215,  102,  217,  147,  238,  238,  221,   30,
 /*   980 */    34,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   990 */    10,  238,  170,   13,   14,   34,  211,  212,  238,   19,
 /*  1000 */   215,  192,  238,  218,  219,  220,  238,  222,  223,   29,
 /*  1010 */   238,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*  1020 */    10,  127,  238,   13,   14,  131,  132,  133,  238,   19,
 /*  1030 */   136,  192,  138,  139,  238,  238,  142,  143,  144,  145,
 /*  1040 */    30,  238,    1,    2,    3,    4,    5,    6,    7,    8,
 /*  1050 */     9,   10,  238,  238,   13,   14,  273,  238,  212,  238,
 /*  1060 */    19,  215,  216,  238,  218,  219,  220,   34,  222,  223,
 /*  1070 */    29,  225,    1,    2,    3,    4,    5,    6,    7,    8,
 /*  1080 */     9,   10,  126,  127,   13,   14,  168,  131,  132,  133,
 /*  1090 */    19,  238,  136,  238,  138,  139,   27,  251,  142,  143,
 /*  1100 */   144,  145,  238,  209,   33,  238,    1,    2,    3,    4,
 /*  1110 */     5,    6,    7,    8,    9,   10,  172,   27,   13,   14,
 /*  1120 */   170,  238,  276,  238,   19,  119,    1,    2,    3,    4,
 /*  1130 */     5,    6,    7,    8,    9,   10,  231,  175,   13,   14,
 /*  1140 */   238,  238,   34,   27,   19,  238,    1,    2,    3,    4,
 /*  1150 */     5,    6,    7,    8,    9,   10,   34,  260,   13,   14,
 /*  1160 */   136,  149,  150,  238,   19,  209,  154,  258,  211,  212,
 /*  1170 */   146,  238,  215,   28,  260,  218,  219,  220,  238,  222,
 /*  1180 */   223,   76,  188,    1,    2,    3,    4,    5,    6,    7,
 /*  1190 */     8,    9,   10,  238,  228,   13,   14,  238,  228,  228,
 /*  1200 */   193,   19,  238,  238,  228,  228,   81,  129,    1,    2,
 /*  1210 */     3,    4,    5,    6,    7,    8,    9,   10,  127,  238,
 /*  1220 */    13,   14,  131,  227,  133,  188,   19,  136,  227,  138,
 /*  1230 */   139,  227,  208,  142,  143,  128,  145,  188,  227,  194,
 /*  1240 */   238,  194,  226,  136,  130,  233,  234,  235,  236,  237,
 /*  1250 */   267,  238,  238,  238,  187,   48,  128,  238,  246,  247,
 /*  1260 */   248,  249,  250,  187,  136,  126,  127,   67,   34,  280,
 /*  1270 */   131,  132,  133,  280,  280,  136,  280,  138,  139,  280,
 /*  1280 */   280,  142,  143,  144,  145,  212,  213,  214,  215,  280,
 /*  1290 */   217,  279,  280,  111,  221,  280,  272,  158,  280,  280,
 /*  1300 */   209,  280,  195,  196,  197,  198,  199,  200,  201,  202,
 /*  1310 */   203,  204,  205,  212,  213,  214,  215,  210,  217,  280,
 /*  1320 */   280,  280,  221,  195,  196,  197,  198,  199,  200,  201,
 /*  1330 */   202,  203,  204,  205,  280,  280,  280,  280,  210,  280,
 /*  1340 */   280,  280,  280,  280,  280,  280,  280,  280,  209,  280,
 /*  1350 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*  1360 */   280,  280,   13,   14,  212,  213,  214,  215,   19,  217,
 /*  1370 */   280,  127,  280,  221,  280,  131,  132,  133,  280,  280,
 /*  1380 */   136,  280,  138,  139,  280,  280,  142,  143,  144,  145,
 /*  1390 */   127,  280,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  1400 */   280,  138,  139,  280,  280,  142,  143,  144,  145,  127,
 /*  1410 */   280,  280,  280,  131,  132,  133,  280,  280,  136,  280,
 /*  1420 */   138,  139,  280,  280,  142,  143,  144,  145,  280,  166,
 /*  1430 */   127,  280,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  1440 */   280,  138,  139,  280,  280,  142,  143,  144,  145,  280,
 /*  1450 */   280,  127,  280,  209,  136,  131,  132,  133,  280,  280,
 /*  1460 */   136,  280,  138,  139,  146,  136,  142,  143,  144,  145,
 /*  1470 */   280,  280,  209,  280,  280,  146,  232,  280,  127,  280,
 /*  1480 */   280,  280,  131,  132,  133,  280,  280,  136,  280,  138,
 /*  1490 */   139,  209,  280,  142,  143,  144,  145,  280,  280,  127,
 /*  1500 */   280,  280,  280,  131,  132,  133,  280,  280,  136,  280,
 /*  1510 */   138,  139,  209,  280,  142,  143,  144,  145,  280,  280,
 /*  1520 */   280,  280,  280,  127,  280,  280,  208,  131,  132,  133,
 /*  1530 */   280,  280,  136,  209,  138,  139,  280,  208,  142,  143,
 /*  1540 */   144,  145,  280,  280,  280,  280,  127,  280,  280,  280,
 /*  1550 */   131,  132,  133,  280,  280,  136,  280,  138,  139,  280,
 /*  1560 */   209,  142,  143,  144,  145,  280,  280,  127,  280,  280,
 /*  1570 */   280,  131,  132,  133,  280,  280,  136,  280,  138,  139,
 /*  1580 */   280,  209,  142,  143,  144,  145,  280,  280,  280,  280,
 /*  1590 */   272,  127,  280,  280,  280,  131,  132,  133,  280,  280,
 /*  1600 */   136,  272,  138,  139,  280,  209,  142,  143,  144,  145,
 /*  1610 */   280,  280,  280,  280,  280,  127,  280,  280,  280,  131,
 /*  1620 */   132,  133,  280,  280,  136,  280,  138,  139,  209,  280,
 /*  1630 */   142,  143,  144,  145,  280,  280,  127,  280,  280,  280,
 /*  1640 */   131,  132,  133,  280,  280,  136,  280,  138,  139,  209,
 /*  1650 */   280,  142,  143,  144,  145,  280,  280,  280,  280,  280,
 /*  1660 */   127,  280,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  1670 */   280,  138,  139,  209,  280,  142,  143,  144,  145,  280,
 /*  1680 */   280,  280,  280,  127,  280,  280,  280,  131,  132,  133,
 /*  1690 */   280,  280,  136,  280,  138,  139,  280,  209,  142,  143,
 /*  1700 */   144,  145,  280,  280,  127,  280,  280,  280,  131,  132,
 /*  1710 */   133,  280,  280,  136,  280,  138,  139,  280,  209,  142,
 /*  1720 */   143,  144,  145,  127,  280,  280,  280,  131,  132,  133,
 /*  1730 */   280,  280,  136,  280,  138,  139,  280,  280,  142,  143,
 /*  1740 */   144,  145,  209,  127,  280,  280,  280,  131,  132,  133,
 /*  1750 */   280,  280,  136,  280,  138,  139,  280,  280,  142,  143,
 /*  1760 */   144,  145,  280,  280,  127,  209,  280,  280,  131,  132,
 /*  1770 */   133,  280,  280,  136,  280,  138,  139,  280,  280,  142,
 /*  1780 */   143,  144,  145,  280,  280,  280,  209,  280,  280,  280,
 /*  1790 */   280,  280,  127,  280,  280,  280,  131,  132,  133,  280,
 /*  1800 */   280,  136,  280,  138,  139,  209,  280,  142,  143,  144,
 /*  1810 */   145,  127,  280,  280,  280,  131,  132,  133,  280,  280,
 /*  1820 */   136,  280,  138,  139,  280,  209,  142,  143,  144,  145,
 /*  1830 */   280,  280,  127,  280,  280,  280,  131,  132,  133,  280,
 /*  1840 */   280,  136,  280,  138,  139,  280,  209,  142,  143,  144,
 /*  1850 */   145,  127,  280,  280,  280,  131,  132,  133,  280,  280,
 /*  1860 */   136,  280,  138,  139,  280,  280,  142,  143,  144,  145,
 /*  1870 */   280,  127,  280,  280,  209,  131,  132,  133,  280,  280,
 /*  1880 */   136,  280,  138,  139,  280,  280,  142,  143,  144,  145,
 /*  1890 */   280,  280,  127,  209,  280,  280,  131,  132,  133,  280,
 /*  1900 */   280,  136,  280,  138,  139,  280,  280,  142,  143,  144,
 /*  1910 */   145,  280,  280,  280,  209,  280,  280,  280,  280,  280,
 /*  1920 */   127,  280,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  1930 */   280,  138,  139,  209,  280,  142,  143,  144,  145,  127,
 /*  1940 */   280,  280,  280,  131,  132,  133,  280,  280,  136,  280,
 /*  1950 */   138,  139,  280,  209,  142,  143,  144,  145,  280,  280,
 /*  1960 */   127,  280,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  1970 */   280,  138,  139,  280,  209,  142,  143,  144,  145,  127,
 /*  1980 */   280,  280,  280,  131,  132,  133,  280,  280,  136,  280,
 /*  1990 */   138,  139,  280,  280,  142,  143,  144,  145,  280,  127,
 /*  2000 */   280,  280,  209,  131,  132,  133,  280,  280,  136,  280,
 /*  2010 */   138,  139,  280,  280,  142,  143,  144,  145,  280,  280,
 /*  2020 */   127,  209,  280,  280,  131,  132,  133,  280,  280,  136,
 /*  2030 */   280,  138,  139,  280,  280,  142,  143,  144,  145,  280,
 /*  2040 */   280,  280,  209,  280,  280,  280,  280,  280,  127,  280,
 /*  2050 */   280,  280,  131,  132,  133,  280,  280,  136,  280,  138,
 /*  2060 */   139,  209,  280,  142,  143,  144,  145,  127,  280,  280,
 /*  2070 */   280,  131,  132,  133,  280,  280,  136,  280,  138,  139,
 /*  2080 */   280,  209,  142,  143,  144,  145,  280,  280,  127,  280,
 /*  2090 */   280,  280,  131,  132,  133,  280,  280,  136,  280,  138,
 /*  2100 */   139,  280,  209,  142,  143,  144,  145,  127,  280,  280,
 /*  2110 */   280,  131,  132,  133,  280,  280,  136,  280,  138,  139,
 /*  2120 */   280,  280,  142,  143,  144,  145,  280,  127,  280,  280,
 /*  2130 */   209,  131,  132,  133,  280,  280,  136,  280,  138,  139,
 /*  2140 */   280,  280,  142,  143,  144,  145,  280,  280,  127,  209,
 /*  2150 */   280,  280,  131,  132,  133,  280,  280,  136,  280,  138,
 /*  2160 */   139,  280,  280,  142,  143,  144,  145,  280,  280,  280,
 /*  2170 */   209,  280,  280,  280,  280,  280,  127,  280,  280,  280,
 /*  2180 */   131,  132,  133,  280,  280,  136,  280,  138,  139,  209,
 /*  2190 */   280,  142,  143,  144,  145,  127,  280,  280,  280,  131,
 /*  2200 */   132,  133,  280,  280,  136,  280,  138,  139,  280,  209,
 /*  2210 */   142,  143,  144,  145,  280,  280,  127,  280,  280,  280,
 /*  2220 */   131,  132,  133,  280,  280,  136,  280,  138,  139,  280,
 /*  2230 */   209,  142,  143,  144,  145,  127,  280,  280,  280,  131,
 /*  2240 */   132,  133,   34,   35,  136,  280,  138,  139,  280,  280,
 /*  2250 */   142,  143,  144,  145,  280,  280,  280,  280,  209,  280,
 /*  2260 */    52,  280,   54,  280,  280,  212,  213,  214,  215,  280,
 /*  2270 */   217,  280,   64,   65,  221,   58,  280,  209,  280,   62,
 /*  2280 */   280,  280,   74,   75,  280,  127,   69,  280,  280,  131,
 /*  2290 */   280,  133,  280,  280,  136,  280,  138,  139,  209,  280,
 /*  2300 */   142,  143,  144,  145,  280,  280,  280,   99,  280,   92,
 /*  2310 */   127,  103,  280,  105,  131,   98,  133,  209,  280,  136,
 /*  2320 */   112,  138,  139,  280,  280,  142,  143,  144,  145,  127,
 /*  2330 */   280,  280,  280,  131,  117,  133,  280,  280,  136,  280,
 /*  2340 */   138,  139,  280,  280,  142,  143,  144,  145,  127,  280,
 /*  2350 */   280,  280,  131,  280,  133,  280,  280,  136,  280,  138,
 /*  2360 */   139,  280,  280,  142,  143,  144,  145,  209,  127,  280,
 /*  2370 */   280,  280,  131,  280,  133,  280,  280,  136,  280,  138,
 /*  2380 */   139,  280,  280,  142,  143,  144,  145,  280,  280,  280,
 /*  2390 */   127,  280,  209,  280,  131,  280,  133,  280,  280,  136,
 /*  2400 */   280,  138,  139,  280,  280,  142,  143,  144,  145,  280,
 /*  2410 */   127,  209,  280,  280,  131,  280,  133,  280,  280,  136,
 /*  2420 */   280,  138,  139,  280,  280,  142,  143,  144,  145,  127,
 /*  2430 */   209,  280,  280,  131,  280,  133,  280,  280,  136,  280,
 /*  2440 */   138,  139,  280,  280,  142,  143,  144,  145,  127,  280,
 /*  2450 */   209,  280,  131,  280,  133,  280,  280,  136,  280,  138,
 /*  2460 */   139,  280,  280,  142,  143,  144,  145,  280,  280,  127,
 /*  2470 */   280,  280,  209,  131,  280,  133,  280,  280,  136,  280,
 /*  2480 */   138,  139,  280,  280,  142,  143,  144,  145,  280,  280,
 /*  2490 */   280,  127,  209,  280,  280,  131,  280,  133,  280,  280,
 /*  2500 */   136,  280,  138,  139,  280,  280,  142,  143,  144,  145,
 /*  2510 */   280,  209,  280,  280,  280,  280,  280,  280,  280,  280,
 /*  2520 */   127,  280,  280,  280,  131,  280,  133,  280,  280,  136,
 /*  2530 */   209,  138,  139,  280,  280,  142,  143,  144,  145,  280,
 /*  2540 */   280,  280,  280,  280,  127,  280,  280,  280,  131,  280,
 /*  2550 */   133,  209,  280,  136,  280,  138,  139,  280,  280,  142,
 /*  2560 */   143,  144,  145,  127,  280,  280,  280,  131,  280,  133,
 /*  2570 */   280,  280,  136,  209,  138,  139,  280,  280,  142,  143,
 /*  2580 */   144,  145,  127,  280,  280,  280,  131,  280,  133,  280,
 /*  2590 */   280,  136,  280,  138,  139,  280,  280,  142,  143,  280,
 /*  2600 */   145,  127,  209,  280,  280,  131,  280,  133,  280,  280,
 /*  2610 */   136,  280,  138,  139,  280,  280,  142,  143,  280,  145,
 /*  2620 */   280,  280,  280,  280,   11,   12,  209,  280,   15,   16,
 /*  2630 */    17,   18,  280,  280,   21,   22,  280,  280,  280,  211,
 /*  2640 */   212,  280,   29,  215,  280,  209,  218,  219,  220,  280,
 /*  2650 */   222,  223,  280,  280,  280,  280,  280,  280,  211,  212,
 /*  2660 */   280,  280,  215,  280,  209,  218,  219,  220,  280,  222,
 /*  2670 */   223,  280,  280,  280,  280,  280,  280,  211,  212,  280,
 /*  2680 */   280,  215,  280,  209,  218,  219,  220,  280,  222,  223,
 /*  2690 */   211,  212,  280,  280,  215,  280,  280,  218,  219,  220,
 /*  2700 */   280,  222,  223,  211,  212,  280,  280,  215,  280,  280,
 /*  2710 */   218,  219,  220,  280,  222,  223,  280,  280,  211,  212,
 /*  2720 */   280,  280,  215,  280,  280,  218,  219,  220,  280,  222,
 /*  2730 */   223,
};
#define YY_SHIFT_USE_DFLT (-61)
#define YY_SHIFT_COUNT (410)
#define YY_SHIFT_MIN   (-60)
#define YY_SHIFT_MAX   (2613)
static const short yy_shift_ofst[] = {
 /*     0 */   548,  608,  608,  608,  608,  608,  608,  608,  608,  608,
 /*    10 */    89,  155, 2217, 2217, 2217,  155,   39,  189, 2208, 2208,
 /*    20 */  2217,  -11,  189,  139,  139,  139,  139,  139,  139,  139,
 /*    30 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    40 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    50 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    60 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    70 */   139,  139,  139,  139,  139,  139,  139,  139,  367,  139,
 /*    80 */   139,  139,  550,  550,  550,  550,  550,  550,  550,  550,
 /*    90 */   550,  550,  403,  294,  294,  294,  294,  294,  294,  294,
 /*   100 */   294,  294,  294,  294,  294,  294,   37,   37,   37,   37,
 /*   110 */    37,  398,  555,   37,   37,  157,  157,  157,   34,   11,
 /*   120 */   555,  209,  961,  961, 1200,  173,   15,  421,  521,  410,
 /*   130 */    71,  209, 1122, 1108, 1006,  898, 1234, 1200, 1069,  898,
 /*   140 */   886, 1006,  -61,  -61,  -61,  280,  280, 1182, 1207, 1182,
 /*   150 */  1182, 1182,  249,  896,   -6,  146,  146,  146,  146,  334,
 /*   160 */   -60,  496,  494,  469,  -60,  465,  306,  209,  457,  452,
 /*   170 */   320,   71,  320,  431,  242,  185,  -60,  724,  724,  724,
 /*   180 */  1116,  724,  724, 1122, 1116,  724,  724, 1108,  724, 1006,
 /*   190 */   724,  724,  961, 1090,  724,  724, 1069, 1033,  724,  724,
 /*   200 */   724,  724,  793,  793,  961,  946,  724,  724,  724,  724,
 /*   210 */   848,  724,  724,  724,  724,  848,  844,  724,  724,  724,
 /*   220 */   724,  724,  724,  724,  898,  831,  724,  724,  874,  724,
 /*   230 */   898,  898,  898,  898,  886,  811,  829,  724,  863,  793,
 /*   240 */   793,  775,  834,  775,  834,  834,  835,  834,  826,  724,
 /*   250 */   724,  -61,  -61,  -61,  -61,  -61,  -61, 1145, 1125, 1105,
 /*   260 */  1071, 1041, 1010,  980,  949,  920,  891,  871,  840,  820,
 /*   270 */   789, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349, 1349,
 /*   280 */  1349, 1349, 1349,  373, 2613,   -8,  327,  327,  174,   78,
 /*   290 */    28,   13,   13,   13,   13,   13,   13,   13,   13,   13,
 /*   300 */    13,  428,  413,  405,  355,  369,  331,  277,   97,  282,
 /*   310 */    94,   55,  272,   55,  221,   22,  220,  -26,  156,  770,
 /*   320 */   699,  767,  716,  755,  705,  744,  696,  750,  743,  742,
 /*   330 */   675,  707,  773,  741,  734,  650,  668,  658,  733,  730,
 /*   340 */   729,  727,  721,  720,  719,  717,  715,  718,  688,  701,
 /*   350 */   709,  627,  692,  672,  694,  619,  618,  682,  633,  657,
 /*   360 */   669,  673,  671,  670,  664,  655,  660,  654,  646,  559,
 /*   370 */   642,  634,  609,  632,  603,  631,  607,  606,  599,  604,
 /*   380 */   598,  600,  593,  584,  595,  582,  589,  535,  518,  567,
 /*   390 */   545,  506,  477,  393,  382,  365,  287,  262,  262,  262,
 /*   400 */   262,  262,  262,  297,  276,  216,  216,   85,   66,    9,
 /*   410 */   -52,
};
#define YY_REDUCE_USE_DFLT (-180)
#define YY_REDUCE_COUNT (256)
#define YY_REDUCE_MIN   (-179)
#define YY_REDUCE_MAX   (2507)
static const short yy_reduce_ofst[] = {
 /*     0 */  -161,  498,  451,  407,  361,  314,  270,  224,  177,  120,
 /*    10 */  -102,  205,  536,  509,  481,  205, 1139,  406, 1128, 1107,
 /*    20 */  1012, 1263, 1244,  956, 2108, 2089, 2068, 2049, 2021, 2000,
 /*    30 */  1980, 1961, 1940, 1921, 1893, 1872, 1852, 1833, 1812, 1793,
 /*    40 */  1765, 1744, 1724, 1705, 1684, 1665, 1637, 1616, 1596, 1577,
 /*    50 */  1556, 1533, 1509, 1488, 1464, 1440, 1419, 1396, 1372, 1351,
 /*    60 */  1324, 1303, 1282,  894, 2436, 2417, 2393, 2364, 2342, 2321,
 /*    70 */  2302, 2283, 2263, 2241, 2221, 2202, 2183, 2158,  846, 2474,
 /*    80 */  2455, 1091, 2507, 2492, 2479, 2466, 2447, 2428,  957,  785,
 /*    90 */   594,  281, -179, 2053, 1152, 1101, 1073,  757,  643,  520,
 /*   100 */   463,  360,   -7,  -31,  -57,  -80,   96, 1329, 1318, 1024,
 /*   110 */   674,  -21,   58,  268,  210,   67,  -92,  235,   51,   52,
 /*   120 */   -76,  -62,  -28,  -48,  -97,  450,  374,  430,  222,  292,
 /*   130 */   390,  399,  309,  250,  378,  370,  269,  162,   -1,   21,
 /*   140 */   167,  350,  345, -101,  257, 1076, 1067, 1019, 1114, 1015,
 /*   150 */  1014, 1013,  983, 1016, 1047, 1045, 1045, 1045, 1045, 1002,
 /*   160 */  1049, 1011, 1004, 1001, 1037,  996,  981, 1078,  977,  976,
 /*   170 */   965, 1007,  964,  971,  970,  966,  994,  959,  955,  940,
 /*   180 */   914,  933,  925,  909,  897,  907,  903,  905,  902,  962,
 /*   190 */   885,  883,  950,  944,  867,  864,  918,  783,  855,  853,
 /*   200 */   825,  821,  839,  809,  822,  712,  819,  815,  814,  803,
 /*   210 */   828,  797,  796,  790,  784,  818,  813,  772,  768,  764,
 /*   220 */   760,  753,  739,  738,  748,  763,  704,  703,  746,  700,
 /*   230 */   737,  735,  731,  723,  702,  732,  728,  665,  754,  697,
 /*   240 */   691,  698,  745,  680,  726,  725,  708,  722,  611,  613,
 /*   250 */   602,  597,  683,  677,  547,  561,  572,
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
  "TOK_WHILE",     "error",         "statement_list",  "case_action", 
  "case_otherwise",  "entity_body",   "aggregate_init_element",  "aggregate_initializer",
  "assignable",    "attribute_decl",  "by_expression",  "constant",    
  "expression",    "function_call",  "general_ref",   "group_ref",   
  "identifier",    "initializer",   "interval",      "literal",     
  "local_initializer",  "precision_spec",  "query_expression",  "query_start", 
  "simple_expression",  "unary_expression",  "supertype_expression",  "until_control",
  "while_control",  "function_header",  "fh_lineno",     "rule_header", 
  "rh_start",      "rh_get_line",   "procedure_header",  "ph_get_line", 
  "action_body",   "actual_parameters",  "aggregate_init_body",  "explicit_attr_list",
  "case_action_list",  "case_block",    "case_labels",   "where_clause_list",
  "derive_decl",   "explicit_attribute",  "expression_list",  "formal_parameter",
  "formal_parameter_list",  "formal_parameter_rep",  "id_list",       "defined_type_list",
  "nested_id_list",  "statement_rep",  "subtype_decl",  "where_rule",  
  "where_rule_OPT",  "supertype_expression_list",  "labelled_attrib_list_list",  "labelled_attrib_list",
  "inverse_attr_list",  "inverse_clause",  "attribute_decl_list",  "derived_attribute_rep",
  "unique_clause",  "rule_formal_parameter_list",  "qualified_attr_list",  "rel_op",      
  "optional_or_unique",  "optional_fixed",  "optional",      "var",         
  "unique",        "qualified_attr",  "qualifier",     "alias_statement",
  "assignment_statement",  "case_statement",  "compound_statement",  "escape_statement",
  "if_statement",  "proc_call_statement",  "repeat_statement",  "return_statement",
  "skip_statement",  "statement",     "subsuper_decl",  "supertype_decl",
  "supertype_factor",  "function_id",   "procedure_id",  "attribute_type",
  "defined_type",  "parameter_type",  "generic_type",  "basic_type",  
  "select_type",   "aggregate_type",  "aggregation_type",  "array_type",  
  "bag_type",      "conformant_aggregation",  "list_type",     "set_type",    
  "set_or_bag_of_entity",  "type",          "cardinality_op",  "index_spec",  
  "limit_spec",    "inverse_attr",  "derived_attribute",  "rule_formal_parameter",
  "where_clause",  "action_body_item_rep",  "action_body_item",  "declaration", 
  "constant_decl",  "local_decl",    "semicolon",     "alias_push_scope",
  "block_list",    "block_member",  "include_directive",  "rule_decl",   
  "constant_body",  "constant_body_list",  "entity_decl",   "function_decl",
  "procedure_decl",  "type_decl",     "entity_header",  "enumeration_type",
  "express_file",  "schema_decl_list",  "schema_decl",   "fh_push_scope",
  "fh_plist",      "increment_control",  "rename",        "rename_list", 
  "parened_rename_list",  "reference_clause",  "reference_head",  "use_clause",  
  "use_head",      "interface_specification",  "interface_specification_list",  "right_curl",  
  "local_variable",  "local_body",    "allow_generic_types",  "disallow_generic_types",
  "oneof_op",      "ph_push_scope",  "schema_body",   "schema_header",
  "type_item_body",  "type_item",     "ti_start",      "td_start",    
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
    case 122: /* statement_list */
{
#line 189 "expparse.y"

    if (parseData.scanner == NULL) {
	(yypminor->yy0).string = (char*)NULL;
    }

#line 1660 "expparse.c"
}
      break;
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
#line 2469 "expparse.y"

    fprintf(stderr, "Express parser experienced stack overflow.\n");
    fprintf(stderr, "Last token had value %x\n", yypMinor->yy0.val);
#line 1849 "expparse.c"
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
  { 156, 2 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 233, 0 },
  { 233, 2 },
  { 157, 3 },
  { 157, 2 },
  { 127, 2 },
  { 127, 3 },
  { 126, 1 },
  { 158, 1 },
  { 158, 3 },
  { 158, 3 },
  { 158, 5 },
  { 217, 3 },
  { 217, 5 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 195, 9 },
  { 239, 0 },
  { 219, 5 },
  { 128, 2 },
  { 128, 1 },
  { 196, 4 },
  { 211, 1 },
  { 211, 1 },
  { 211, 1 },
  { 159, 0 },
  { 159, 2 },
  { 220, 4 },
  { 220, 3 },
  { 215, 1 },
  { 215, 2 },
  { 215, 2 },
  { 215, 1 },
  { 215, 1 },
  { 215, 3 },
  { 215, 3 },
  { 240, 0 },
  { 240, 2 },
  { 241, 1 },
  { 241, 1 },
  { 241, 1 },
  { 130, 0 },
  { 130, 2 },
  { 226, 5 },
  { 123, 3 },
  { 160, 0 },
  { 160, 2 },
  { 161, 2 },
  { 162, 1 },
  { 162, 3 },
  { 124, 0 },
  { 124, 3 },
  { 197, 6 },
  { 198, 4 },
  { 131, 1 },
  { 131, 1 },
  { 244, 6 },
  { 245, 0 },
  { 245, 2 },
  { 236, 4 },
  { 235, 1 },
  { 235, 1 },
  { 235, 1 },
  { 235, 1 },
  { 164, 0 },
  { 164, 2 },
  { 230, 5 },
  { 183, 1 },
  { 183, 2 },
  { 125, 5 },
  { 246, 6 },
  { 250, 2 },
  { 251, 3 },
  { 199, 2 },
  { 129, 1 },
  { 129, 5 },
  { 182, 1 },
  { 182, 3 },
  { 190, 0 },
  { 190, 1 },
  { 165, 5 },
  { 252, 1 },
  { 253, 1 },
  { 253, 2 },
  { 132, 1 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 144, 1 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 166, 1 },
  { 166, 3 },
  { 191, 0 },
  { 191, 1 },
  { 167, 4 },
  { 168, 0 },
  { 168, 3 },
  { 169, 1 },
  { 169, 3 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 133, 2 },
  { 247, 4 },
  { 149, 6 },
  { 150, 1 },
  { 255, 1 },
  { 256, 1 },
  { 209, 1 },
  { 209, 1 },
  { 221, 1 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 214, 1 },
  { 214, 3 },
  { 170, 1 },
  { 170, 3 },
  { 136, 1 },
  { 136, 1 },
  { 136, 1 },
  { 200, 6 },
  { 200, 8 },
  { 242, 3 },
  { 257, 6 },
  { 227, 5 },
  { 137, 2 },
  { 258, 1 },
  { 258, 3 },
  { 259, 1 },
  { 259, 3 },
  { 260, 3 },
  { 261, 4 },
  { 261, 3 },
  { 262, 3 },
  { 263, 4 },
  { 263, 3 },
  { 264, 3 },
  { 265, 1 },
  { 265, 1 },
  { 266, 0 },
  { 266, 2 },
  { 138, 7 },
  { 224, 1 },
  { 224, 3 },
  { 224, 4 },
  { 224, 4 },
  { 224, 3 },
  { 180, 1 },
  { 180, 2 },
  { 229, 6 },
  { 181, 0 },
  { 181, 2 },
  { 228, 5 },
  { 222, 5 },
  { 222, 4 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 140, 2 },
  { 268, 4 },
  { 268, 5 },
  { 269, 0 },
  { 269, 2 },
  { 237, 6 },
  { 270, 0 },
  { 271, 0 },
  { 212, 1 },
  { 171, 1 },
  { 171, 3 },
  { 172, 3 },
  { 272, 1 },
  { 188, 0 },
  { 188, 1 },
  { 188, 1 },
  { 188, 2 },
  { 188, 2 },
  { 189, 0 },
  { 189, 1 },
  { 141, 0 },
  { 141, 3 },
  { 201, 3 },
  { 201, 2 },
  { 248, 4 },
  { 154, 5 },
  { 273, 1 },
  { 155, 0 },
  { 210, 1 },
  { 210, 1 },
  { 135, 2 },
  { 194, 2 },
  { 194, 2 },
  { 194, 3 },
  { 194, 5 },
  { 142, 3 },
  { 143, 6 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 202, 8 },
  { 202, 7 },
  { 203, 2 },
  { 203, 5 },
  { 267, 1 },
  { 243, 5 },
  { 231, 1 },
  { 185, 1 },
  { 185, 3 },
  { 151, 4 },
  { 152, 5 },
  { 153, 0 },
  { 274, 2 },
  { 274, 3 },
  { 254, 4 },
  { 254, 1 },
  { 275, 3 },
  { 216, 4 },
  { 238, 1 },
  { 223, 4 },
  { 223, 3 },
  { 204, 2 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 173, 0 },
  { 173, 2 },
  { 173, 2 },
  { 206, 0 },
  { 206, 1 },
  { 206, 1 },
  { 206, 2 },
  { 174, 5 },
  { 207, 2 },
  { 207, 5 },
  { 207, 6 },
  { 146, 1 },
  { 146, 3 },
  { 146, 3 },
  { 177, 1 },
  { 177, 3 },
  { 208, 1 },
  { 208, 4 },
  { 208, 3 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 276, 1 },
  { 276, 1 },
  { 277, 3 },
  { 278, 2 },
  { 249, 3 },
  { 279, 3 },
  { 134, 2 },
  { 134, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 1 },
  { 145, 1 },
  { 145, 1 },
  { 145, 3 },
  { 145, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 2 },
  { 145, 2 },
  { 192, 0 },
  { 192, 1 },
  { 193, 1 },
  { 193, 5 },
  { 186, 1 },
  { 186, 3 },
  { 179, 2 },
  { 179, 4 },
  { 178, 1 },
  { 178, 2 },
  { 184, 0 },
  { 184, 2 },
  { 147, 0 },
  { 147, 2 },
  { 232, 2 },
  { 232, 4 },
  { 163, 1 },
  { 163, 2 },
  { 175, 2 },
  { 176, 0 },
  { 176, 1 },
  { 148, 0 },
  { 148, 2 },
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
#line 363 "expparse.y"
{
    yygotominor.yy471 = yymsp[0].minor.yy471;
}
#line 2314 "expparse.c"
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
#line 369 "expparse.y"
{
    yygotominor.yy0 = yymsp[0].minor.yy0;
}
#line 2337 "expparse.c"
        break;
      case 5: /* action_body_item_rep ::= action_body_item action_body_item_rep */
      case 42: /* block_list ::= block_list block_member */ yytestcase(yyruleno==42);
      case 63: /* constant_body_list ::= constant_body constant_body_list */ yytestcase(yyruleno==63);
      case 88: /* schema_decl_list ::= schema_decl_list schema_decl */ yytestcase(yyruleno==88);
      case 170: /* interface_specification_list ::= interface_specification_list interface_specification */ yytestcase(yyruleno==170);
      case 196: /* local_body ::= local_variable local_body */ yytestcase(yyruleno==196);
      case 249: /* schema_body ::= interface_specification_list block_list */ yytestcase(yyruleno==249);
#line 386 "expparse.y"
{
    yygotominor.yy0 = yymsp[-1].minor.yy0;
}
#line 2350 "expparse.c"
        break;
      case 6: /* actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN */
      case 203: /* nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==203);
      case 276: /* subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==276);
#line 403 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;
}
#line 2359 "expparse.c"
        break;
      case 7: /* actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN */
      case 321: /* unique_clause ::= */ yytestcase(yyruleno==321);
#line 407 "expparse.y"
{
    yygotominor.yy471 = 0;
}
#line 2367 "expparse.c"
        break;
      case 8: /* aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET */
#line 413 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Aggregate);
    yygotominor.yy145->u.list = LISTcreate();
}
#line 2375 "expparse.c"
        break;
      case 9: /* aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET */
#line 419 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Aggregate);
    yygotominor.yy145->u.list = yymsp[-1].minor.yy471;
}
#line 2383 "expparse.c"
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
#line 425 "expparse.y"
{
    yygotominor.yy145 = yymsp[0].minor.yy145;
}
#line 2407 "expparse.c"
        break;
      case 11: /* aggregate_init_body ::= aggregate_init_element */
#line 430 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);
}
#line 2415 "expparse.c"
        break;
      case 12: /* aggregate_init_body ::= aggregate_init_element TOK_COLON expression */
#line 435 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd(yygotominor.yy471, (Generic)yymsp[-2].minor.yy145);

    LISTadd(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);

    yymsp[-2].minor.yy145->type->u.type->body->flags.repeat = 1;
}
#line 2427 "expparse.c"
        break;
      case 13: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element */
#line 445 "expparse.y"
{ 
    yygotominor.yy471 = yymsp[-2].minor.yy471;

    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);

}
#line 2437 "expparse.c"
        break;
      case 14: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression */
#line 453 "expparse.y"
{
    yygotominor.yy471 = yymsp[-4].minor.yy471;

    LISTadd_last(yygotominor.yy471, (Generic)yymsp[-2].minor.yy145);
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);

    yymsp[-2].minor.yy145->type->u.type->body->flags.repeat = 1;
}
#line 2449 "expparse.c"
        break;
      case 15: /* aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type */
#line 463 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(aggregate_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 2465 "expparse.c"
        break;
      case 16: /* aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type */
#line 477 "expparse.y"
{
    Type t = TYPEcreate_user_defined_tag(yymsp[0].minor.yy155, CURRENT_SCOPE, yymsp[-2].minor.yy0.symbol);

    if (t) {
        SCOPEadd_super(t);
        yygotominor.yy457 = TYPEBODYcreate(aggregate_);
        yygotominor.yy457->tag = t;
        yygotominor.yy457->base = yymsp[0].minor.yy155;
    }
}
#line 2479 "expparse.c"
        break;
      case 17: /* aggregation_type ::= array_type */
      case 18: /* aggregation_type ::= bag_type */ yytestcase(yyruleno==18);
      case 19: /* aggregation_type ::= list_type */ yytestcase(yyruleno==19);
      case 20: /* aggregation_type ::= set_type */ yytestcase(yyruleno==20);
#line 489 "expparse.y"
{
    yygotominor.yy457 = yymsp[0].minor.yy457;
}
#line 2489 "expparse.c"
        break;
      case 21: /* alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon */
#line 508 "expparse.y"
{
    Expression e = EXPcreate_from_symbol(Type_Attribute, yymsp[-7].minor.yy0.symbol);
    Variable v = VARcreate(e, Type_Unknown);

    v->initializer = yymsp[-5].minor.yy145; 

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-7].minor.yy0.symbol->name, (Generic)v,
	    yymsp[-7].minor.yy0.symbol, OBJ_VARIABLE);
    yygotominor.yy522 = ALIAScreate(CURRENT_SCOPE, v, yymsp[-2].minor.yy471);

    POP_SCOPE();
}
#line 2505 "expparse.c"
        break;
      case 22: /* alias_push_scope ::= */
#line 522 "expparse.y"
{
    struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
    PUSH_SCOPE(s, (Symbol *)0, OBJ_ALIAS);
}
#line 2513 "expparse.c"
        break;
      case 23: /* array_type ::= TOK_ARRAY index_spec TOK_OF optional_or_unique attribute_type */
#line 529 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(array_);

    yygotominor.yy457->flags.optional = yymsp[-1].minor.yy224.optional;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->upper = yymsp[-3].minor.yy210.upper_limit;
    yygotominor.yy457->lower = yymsp[-3].minor.yy210.lower_limit;
    yygotominor.yy457->base = yymsp[0].minor.yy155;
}
#line 2526 "expparse.c"
        break;
      case 24: /* assignable ::= assignable qualifier */
      case 301: /* unary_expression ::= unary_expression qualifier */ yytestcase(yyruleno==301);
#line 541 "expparse.y"
{
    yymsp[0].minor.yy384.first->e.op1 = yymsp[-1].minor.yy145;
    yygotominor.yy145 = yymsp[0].minor.yy384.expr;
}
#line 2535 "expparse.c"
        break;
      case 26: /* assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon */
#line 552 "expparse.y"
{ 
    yygotominor.yy522 = ASSIGNcreate(yymsp[-3].minor.yy145, yymsp[-1].minor.yy145);
}
#line 2542 "expparse.c"
        break;
      case 27: /* attribute_type ::= aggregation_type */
      case 28: /* attribute_type ::= basic_type */ yytestcase(yyruleno==28);
      case 122: /* parameter_type ::= basic_type */ yytestcase(yyruleno==122);
      case 123: /* parameter_type ::= conformant_aggregation */ yytestcase(yyruleno==123);
#line 557 "expparse.y"
{
    yygotominor.yy155 = TYPEcreate_from_body_anonymously(yymsp[0].minor.yy457);
    SCOPEadd_super(yygotominor.yy155);
}
#line 2553 "expparse.c"
        break;
      case 29: /* attribute_type ::= defined_type */
      case 124: /* parameter_type ::= defined_type */ yytestcase(yyruleno==124);
      case 125: /* parameter_type ::= generic_type */ yytestcase(yyruleno==125);
#line 567 "expparse.y"
{
    yygotominor.yy155 = yymsp[0].minor.yy155;
}
#line 2562 "expparse.c"
        break;
      case 30: /* explicit_attr_list ::= */
      case 50: /* case_action_list ::= */ yytestcase(yyruleno==50);
      case 69: /* derive_decl ::= */ yytestcase(yyruleno==69);
      case 269: /* statement_rep ::= */ yytestcase(yyruleno==269);
#line 572 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
}
#line 2572 "expparse.c"
        break;
      case 31: /* explicit_attr_list ::= explicit_attr_list explicit_attribute */
#line 576 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy471); 
}
#line 2580 "expparse.c"
        break;
      case 32: /* bag_type ::= TOK_BAG limit_spec TOK_OF attribute_type */
      case 138: /* conformant_aggregation ::= TOK_BAG index_spec TOK_OF parameter_type */ yytestcase(yyruleno==138);
#line 582 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(bag_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->upper = yymsp[-2].minor.yy210.upper_limit;
    yygotominor.yy457->lower = yymsp[-2].minor.yy210.lower_limit;
}
#line 2591 "expparse.c"
        break;
      case 33: /* bag_type ::= TOK_BAG TOK_OF attribute_type */
#line 589 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(bag_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
}
#line 2599 "expparse.c"
        break;
      case 34: /* basic_type ::= TOK_BOOLEAN */
#line 595 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(boolean_);
}
#line 2606 "expparse.c"
        break;
      case 35: /* basic_type ::= TOK_INTEGER precision_spec */
#line 599 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(integer_);
    yygotominor.yy457->precision = yymsp[0].minor.yy145;
}
#line 2614 "expparse.c"
        break;
      case 36: /* basic_type ::= TOK_REAL precision_spec */
#line 604 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(real_);
    yygotominor.yy457->precision = yymsp[0].minor.yy145;
}
#line 2622 "expparse.c"
        break;
      case 37: /* basic_type ::= TOK_NUMBER */
#line 609 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(number_);
}
#line 2629 "expparse.c"
        break;
      case 38: /* basic_type ::= TOK_LOGICAL */
#line 613 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(logical_);
}
#line 2636 "expparse.c"
        break;
      case 39: /* basic_type ::= TOK_BINARY precision_spec optional_fixed */
#line 617 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(binary_);
    yygotominor.yy457->precision = yymsp[-1].minor.yy145;
    yygotominor.yy457->flags.fixed = yymsp[0].minor.yy224.fixed;
}
#line 2645 "expparse.c"
        break;
      case 40: /* basic_type ::= TOK_STRING precision_spec optional_fixed */
#line 623 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(string_);
    yygotominor.yy457->precision = yymsp[-1].minor.yy145;
    yygotominor.yy457->flags.fixed = yymsp[0].minor.yy224.fixed;
}
#line 2654 "expparse.c"
        break;
      case 46: /* by_expression ::= */
#line 649 "expparse.y"
{
    yygotominor.yy145 = LITERAL_ONE;
}
#line 2661 "expparse.c"
        break;
      case 48: /* cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL */
      case 154: /* index_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */ yytestcase(yyruleno==154);
      case 182: /* limit_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */ yytestcase(yyruleno==182);
#line 659 "expparse.y"
{
    yygotominor.yy210.lower_limit = yymsp[-3].minor.yy145;
    yygotominor.yy210.upper_limit = yymsp[-1].minor.yy145;
}
#line 2671 "expparse.c"
        break;
      case 49: /* case_action ::= case_labels TOK_COLON statement */
#line 665 "expparse.y"
{
    yygotominor.yy259 = CASE_ITcreate(yymsp[-2].minor.yy471, yymsp[0].minor.yy522);
    SYMBOLset(yygotominor.yy259);
}
#line 2679 "expparse.c"
        break;
      case 51: /* case_action_list ::= case_action_list case_action */
#line 675 "expparse.y"
{
    yyerrok;

    yygotominor.yy471 = yymsp[-1].minor.yy471;

    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy259);
}
#line 2690 "expparse.c"
        break;
      case 52: /* case_block ::= case_action_list case_otherwise */
#line 684 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;

    if (yymsp[0].minor.yy259) {
        LISTadd_last(yygotominor.yy471,
        (Generic)yymsp[0].minor.yy259);
    }
}
#line 2702 "expparse.c"
        break;
      case 53: /* case_labels ::= expression */
#line 694 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();

    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);
}
#line 2711 "expparse.c"
        break;
      case 54: /* case_labels ::= case_labels TOK_COMMA expression */
#line 700 "expparse.y"
{
    yyerrok;

    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);
}
#line 2721 "expparse.c"
        break;
      case 55: /* case_otherwise ::= */
#line 708 "expparse.y"
{
    yygotominor.yy259 = (Case_Item)0;
}
#line 2728 "expparse.c"
        break;
      case 56: /* case_otherwise ::= TOK_OTHERWISE TOK_COLON statement */
#line 712 "expparse.y"
{
    yygotominor.yy259 = CASE_ITcreate(LIST_NULL, yymsp[0].minor.yy522);
    SYMBOLset(yygotominor.yy259);
}
#line 2736 "expparse.c"
        break;
      case 57: /* case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon */
#line 719 "expparse.y"
{
    yygotominor.yy522 = CASEcreate(yymsp[-4].minor.yy145, yymsp[-2].minor.yy471);
}
#line 2743 "expparse.c"
        break;
      case 58: /* compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon */
#line 724 "expparse.y"
{
    yygotominor.yy522 = COMP_STMTcreate(yymsp[-2].minor.yy471);
}
#line 2750 "expparse.c"
        break;
      case 59: /* constant ::= TOK_PI */
#line 729 "expparse.y"
{ 
    yygotominor.yy145 = LITERAL_PI;
}
#line 2757 "expparse.c"
        break;
      case 60: /* constant ::= TOK_E */
#line 734 "expparse.y"
{ 
    yygotominor.yy145 = LITERAL_E;
}
#line 2764 "expparse.c"
        break;
      case 61: /* constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon */
#line 741 "expparse.y"
{
    Variable v;

    yymsp[-5].minor.yy145->type = yymsp[-3].minor.yy155;
    v = VARcreate(yymsp[-5].minor.yy145, yymsp[-3].minor.yy155);
    v->initializer = yymsp[-1].minor.yy145;
    v->flags.constant = 1;
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-5].minor.yy145->symbol.name, (Generic)v,
	&yymsp[-5].minor.yy145->symbol, OBJ_VARIABLE);
}
#line 2778 "expparse.c"
        break;
      case 64: /* constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon */
#line 760 "expparse.y"
{
    yygotominor.yy0 = yymsp[-3].minor.yy0;
}
#line 2785 "expparse.c"
        break;
      case 71: /* derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon */
#line 792 "expparse.y"
{
    yygotominor.yy443 = VARcreate(yymsp[-4].minor.yy145, yymsp[-2].minor.yy155);
    yygotominor.yy443->initializer = yymsp[-1].minor.yy145;
    yygotominor.yy443->flags.attribute = true;
}
#line 2794 "expparse.c"
        break;
      case 72: /* derived_attribute_rep ::= derived_attribute */
      case 177: /* inverse_attr_list ::= inverse_attr */ yytestcase(yyruleno==177);
#line 799 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy443);
}
#line 2803 "expparse.c"
        break;
      case 73: /* derived_attribute_rep ::= derived_attribute_rep derived_attribute */
      case 178: /* inverse_attr_list ::= inverse_attr_list inverse_attr */ yytestcase(yyruleno==178);
#line 804 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy443);
}
#line 2812 "expparse.c"
        break;
      case 74: /* entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT */
#line 811 "expparse.y"
{
    yygotominor.yy24.attributes = yymsp[-4].minor.yy471;
    /* this is flattened out in entity_decl - DEL */
    LISTadd_last(yygotominor.yy24.attributes, (Generic)yymsp[-3].minor.yy471);

    if (yymsp[-2].minor.yy471 != LIST_NULL) {
	LISTadd_last(yygotominor.yy24.attributes, (Generic)yymsp[-2].minor.yy471);
    }

    yygotominor.yy24.unique = yymsp[-1].minor.yy471;
    yygotominor.yy24.where = yymsp[0].minor.yy471;
}
#line 2828 "expparse.c"
        break;
      case 75: /* entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon */
#line 826 "expparse.y"
{
    CURRENT_SCOPE->u.entity->subtype_expression = yymsp[-4].minor.yy34.subtypes;
    CURRENT_SCOPE->u.entity->supertype_symbols = yymsp[-4].minor.yy34.supertypes;
    LISTdo (yymsp[-2].minor.yy24.attributes, l, Linked_List)
	LISTdo (l, a, Variable)
	    ENTITYadd_attribute(CURRENT_SCOPE, a);
	LISTod;
    LISTod;
    CURRENT_SCOPE->u.entity->abstract = yymsp[-4].minor.yy34.abstract;
    CURRENT_SCOPE->u.entity->unique = yymsp[-2].minor.yy24.unique;
    CURRENT_SCOPE->where = yymsp[-2].minor.yy24.where;
    POP_SCOPE();
}
#line 2845 "expparse.c"
        break;
      case 76: /* entity_header ::= TOK_ENTITY TOK_IDENTIFIER */
#line 841 "expparse.y"
{
    Entity e = ENTITYcreate(yymsp[0].minor.yy0.symbol);

    if (print_objects_while_running & OBJ_ENTITY_BITS) {
	fprintf(stdout, "parse: %s (entity)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(e, yymsp[0].minor.yy0.symbol, OBJ_ENTITY);
}
#line 2858 "expparse.c"
        break;
      case 77: /* enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list */
#line 852 "expparse.y"
{
    int value = 0;
    Expression x;
    Symbol *tmp;
    TypeBody tb;
    tb = TYPEBODYcreate(enumeration_);
    CURRENT_SCOPE->u.type->head = 0;
    CURRENT_SCOPE->u.type->body = tb;
    tb->list = yymsp[0].minor.yy471;

    if (!CURRENT_SCOPE->symbol_table) {
        CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }
    if (!PREVIOUS_SCOPE->enum_table) {
        PREVIOUS_SCOPE->enum_table = DICTcreate(25);
    }
    LISTdo_links(yymsp[0].minor.yy471, id) {
        tmp = (Symbol *)id->data;
        id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
        x->symbol = *(tmp);
        x->u.integer = ++value;

        /* define both in enum scope and scope of */
        /* 1st visibility */
        DICT_define(CURRENT_SCOPE->symbol_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        DICTdefine(PREVIOUS_SCOPE->enum_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        SYMBOL_destroy(tmp);
    } LISTod;
}
#line 2893 "expparse.c"
        break;
      case 78: /* escape_statement ::= TOK_ESCAPE semicolon */
#line 885 "expparse.y"
{
    yygotominor.yy522 = STATEMENT_ESCAPE;
}
#line 2900 "expparse.c"
        break;
      case 79: /* attribute_decl ::= TOK_IDENTIFIER */
#line 890 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Attribute);
    yygotominor.yy145->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2909 "expparse.c"
        break;
      case 80: /* attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 897 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Expression);
    yygotominor.yy145->e.op1 = EXPcreate(Type_Expression);
    yygotominor.yy145->e.op1->e.op_code = OP_GROUP;
    yygotominor.yy145->e.op1->e.op1 = EXPcreate(Type_Self);
    yygotominor.yy145->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity, yymsp[-2].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[-2].minor.yy0.symbol);

    yygotominor.yy145->e.op_code = OP_DOT;
    yygotominor.yy145->e.op2 = EXPcreate_from_symbol(Type_Attribute, yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2925 "expparse.c"
        break;
      case 81: /* attribute_decl_list ::= attribute_decl */
#line 911 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);

}
#line 2934 "expparse.c"
        break;
      case 82: /* attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl */
      case 114: /* expression_list ::= expression_list TOK_COMMA expression */ yytestcase(yyruleno==114);
#line 918 "expparse.y"
{
    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);
}
#line 2943 "expparse.c"
        break;
      case 83: /* optional ::= */
#line 924 "expparse.y"
{
    yygotominor.yy224.optional = 0;
}
#line 2950 "expparse.c"
        break;
      case 84: /* optional ::= TOK_OPTIONAL */
#line 928 "expparse.y"
{
    yygotominor.yy224.optional = 1;
}
#line 2957 "expparse.c"
        break;
      case 85: /* explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon */
#line 934 "expparse.y"
{
    Variable v;

    LISTdo_links (yymsp[-4].minor.yy471, attr)
	v = VARcreate((Expression)attr->data, yymsp[-1].minor.yy155);
	v->flags.optional = yymsp[-2].minor.yy224.optional;
	v->flags.attribute = true;
	attr->data = (Generic)v;
    LISTod;

    yygotominor.yy471 = yymsp[-4].minor.yy471;
}
#line 2973 "expparse.c"
        break;
      case 90: /* expression ::= expression TOK_AND expression */
#line 963 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 2982 "expparse.c"
        break;
      case 91: /* expression ::= expression TOK_OR expression */
#line 969 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_OR, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 2991 "expparse.c"
        break;
      case 92: /* expression ::= expression TOK_XOR expression */
#line 975 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_XOR, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3000 "expparse.c"
        break;
      case 93: /* expression ::= expression TOK_LESS_THAN expression */
#line 981 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_LESS_THAN, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3009 "expparse.c"
        break;
      case 94: /* expression ::= expression TOK_GREATER_THAN expression */
#line 987 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_GREATER_THAN, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3018 "expparse.c"
        break;
      case 95: /* expression ::= expression TOK_EQUAL expression */
#line 993 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3027 "expparse.c"
        break;
      case 96: /* expression ::= expression TOK_LESS_EQUAL expression */
#line 999 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_LESS_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3036 "expparse.c"
        break;
      case 97: /* expression ::= expression TOK_GREATER_EQUAL expression */
#line 1005 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_GREATER_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3045 "expparse.c"
        break;
      case 98: /* expression ::= expression TOK_NOT_EQUAL expression */
#line 1011 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_NOT_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3054 "expparse.c"
        break;
      case 99: /* expression ::= expression TOK_INST_EQUAL expression */
#line 1017 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_INST_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3063 "expparse.c"
        break;
      case 100: /* expression ::= expression TOK_INST_NOT_EQUAL expression */
#line 1023 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_INST_NOT_EQUAL, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3072 "expparse.c"
        break;
      case 101: /* expression ::= expression TOK_IN expression */
#line 1029 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_IN, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3081 "expparse.c"
        break;
      case 102: /* expression ::= expression TOK_LIKE expression */
#line 1035 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_LIKE, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3090 "expparse.c"
        break;
      case 103: /* expression ::= simple_expression cardinality_op simple_expression */
      case 241: /* right_curl ::= TOK_RIGHT_CURL */ yytestcase(yyruleno==241);
      case 255: /* semicolon ::= TOK_SEMICOLON */ yytestcase(yyruleno==255);
#line 1041 "expparse.y"
{
    yyerrok;
}
#line 3099 "expparse.c"
        break;
      case 105: /* simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression */
#line 1051 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_CONCAT, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3108 "expparse.c"
        break;
      case 106: /* simple_expression ::= simple_expression TOK_EXP simple_expression */
#line 1057 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_EXP, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3117 "expparse.c"
        break;
      case 107: /* simple_expression ::= simple_expression TOK_TIMES simple_expression */
#line 1063 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_TIMES, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3126 "expparse.c"
        break;
      case 108: /* simple_expression ::= simple_expression TOK_DIV simple_expression */
#line 1069 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_DIV, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3135 "expparse.c"
        break;
      case 109: /* simple_expression ::= simple_expression TOK_REAL_DIV simple_expression */
#line 1075 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_REAL_DIV, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3144 "expparse.c"
        break;
      case 110: /* simple_expression ::= simple_expression TOK_MOD simple_expression */
#line 1081 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_MOD, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3153 "expparse.c"
        break;
      case 111: /* simple_expression ::= simple_expression TOK_PLUS simple_expression */
#line 1087 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_PLUS, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3162 "expparse.c"
        break;
      case 112: /* simple_expression ::= simple_expression TOK_MINUS simple_expression */
#line 1093 "expparse.y"
{
    yyerrok;

    yygotominor.yy145 = BIN_EXPcreate(OP_MINUS, yymsp[-2].minor.yy145, yymsp[0].minor.yy145);
}
#line 3171 "expparse.c"
        break;
      case 113: /* expression_list ::= expression */
      case 283: /* supertype_expression_list ::= supertype_expression */ yytestcase(yyruleno==283);
#line 1100 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy145);
}
#line 3180 "expparse.c"
        break;
      case 115: /* var ::= */
#line 1111 "expparse.y"
{
    yygotominor.yy224.var = 1;
}
#line 3187 "expparse.c"
        break;
      case 116: /* var ::= TOK_VAR */
#line 1115 "expparse.y"
{
    yygotominor.yy224.var = 0;
}
#line 3194 "expparse.c"
        break;
      case 117: /* formal_parameter ::= var id_list TOK_COLON parameter_type */
#line 1120 "expparse.y"
{
    Symbol *tmp;
    Expression e;
    Variable v;

    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTdo_links(yygotominor.yy471, param)
    tmp = (Symbol*)param->data;

    e = EXPcreate_from_symbol(Type_Attribute, tmp);
    v = VARcreate(e, yymsp[0].minor.yy155);
    v->flags.optional = yymsp[-3].minor.yy224.var;
    v->flags.parameter = true;
    param->data = (Generic)v;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table,
    tmp->name, (Generic)v, tmp, OBJ_VARIABLE);

    LISTod;
}
#line 3219 "expparse.c"
        break;
      case 118: /* formal_parameter_list ::= */
      case 180: /* inverse_clause ::= */ yytestcase(yyruleno==180);
      case 330: /* where_rule_OPT ::= */ yytestcase(yyruleno==330);
#line 1143 "expparse.y"
{
    yygotominor.yy471 = LIST_NULL;
}
#line 3228 "expparse.c"
        break;
      case 119: /* formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN */
#line 1148 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;

}
#line 3236 "expparse.c"
        break;
      case 120: /* formal_parameter_rep ::= formal_parameter */
#line 1154 "expparse.y"
{
    yygotominor.yy471 = yymsp[0].minor.yy471;

}
#line 3244 "expparse.c"
        break;
      case 121: /* formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter */
#line 1160 "expparse.y"
{
    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_all(yygotominor.yy471, yymsp[0].minor.yy471);
}
#line 3252 "expparse.c"
        break;
      case 126: /* function_call ::= function_id actual_parameters */
#line 1185 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Funcall);
    yygotominor.yy145->symbol = *yymsp[-1].minor.yy461;
    SYMBOL_destroy(yymsp[-1].minor.yy461);
    yygotominor.yy145->u.funcall.list = yymsp[0].minor.yy471;
}
#line 3262 "expparse.c"
        break;
      case 127: /* function_decl ::= function_header action_body TOK_END_FUNCTION semicolon */
#line 1194 "expparse.y"
{
    FUNCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy471);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy215, SCANtell());
    POP_SCOPE();
}
#line 3271 "expparse.c"
        break;
      case 128: /* function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon */
#line 1202 "expparse.y"
{ 
    Function f = CURRENT_SCOPE;

    f->u.func->return_type = yymsp[-1].minor.yy155;
    yygotominor.yy215 = yymsp[-5].minor.yy215;
}
#line 3281 "expparse.c"
        break;
      case 129: /* fh_lineno ::= TOK_FUNCTION */
      case 219: /* ph_get_line ::= */ yytestcase(yyruleno==219);
      case 248: /* rh_get_line ::= */ yytestcase(yyruleno==248);
#line 1210 "expparse.y"
{
    yygotominor.yy215 = SCANtell();
}
#line 3290 "expparse.c"
        break;
      case 130: /* fh_push_scope ::= TOK_IDENTIFIER */
#line 1215 "expparse.y"
{
    Function f = ALGcreate(OBJ_FUNCTION);
    tag_count = 0;
    if (print_objects_while_running & OBJ_FUNCTION_BITS) {
        fprintf(stdout, "parse: %s (function)\n", yymsp[0].minor.yy0.symbol->name);
    }
    PUSH_SCOPE(f, yymsp[0].minor.yy0.symbol, OBJ_FUNCTION);
}
#line 3302 "expparse.c"
        break;
      case 131: /* fh_plist ::= formal_parameter_list */
#line 1225 "expparse.y"
{
    Function f = CURRENT_SCOPE;
    f->u.func->parameters = yymsp[0].minor.yy471;
    f->u.func->pcount = LISTget_length(yymsp[0].minor.yy471);
    f->u.func->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
}
#line 3313 "expparse.c"
        break;
      case 132: /* function_id ::= TOK_IDENTIFIER */
      case 220: /* procedure_id ::= TOK_IDENTIFIER */ yytestcase(yyruleno==220);
      case 221: /* procedure_id ::= TOK_BUILTIN_PROCEDURE */ yytestcase(yyruleno==221);
#line 1234 "expparse.y"
{
    yygotominor.yy461 = yymsp[0].minor.yy0.symbol;
}
#line 3322 "expparse.c"
        break;
      case 133: /* function_id ::= TOK_BUILTIN_FUNCTION */
#line 1238 "expparse.y"
{
    yygotominor.yy461 = yymsp[0].minor.yy0.symbol;

}
#line 3330 "expparse.c"
        break;
      case 134: /* conformant_aggregation ::= aggregate_type */
#line 1244 "expparse.y"
{
    yygotominor.yy457 = yymsp[0].minor.yy457;

}
#line 3338 "expparse.c"
        break;
      case 135: /* conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type */
#line 1250 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(array_);
    yygotominor.yy457->flags.optional = yymsp[-1].minor.yy224.optional;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->base = yymsp[0].minor.yy155;
}
#line 3348 "expparse.c"
        break;
      case 136: /* conformant_aggregation ::= TOK_ARRAY index_spec TOK_OF optional_or_unique parameter_type */
#line 1258 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(array_);
    yygotominor.yy457->flags.optional = yymsp[-1].minor.yy224.optional;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->upper = yymsp[-3].minor.yy210.upper_limit;
    yygotominor.yy457->lower = yymsp[-3].minor.yy210.lower_limit;
}
#line 3360 "expparse.c"
        break;
      case 137: /* conformant_aggregation ::= TOK_BAG TOK_OF parameter_type */
#line 1267 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(bag_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;

}
#line 3369 "expparse.c"
        break;
      case 139: /* conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type */
#line 1280 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(list_);
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->base = yymsp[0].minor.yy155;

}
#line 3379 "expparse.c"
        break;
      case 140: /* conformant_aggregation ::= TOK_LIST index_spec TOK_OF unique parameter_type */
#line 1288 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(list_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->upper = yymsp[-3].minor.yy210.upper_limit;
    yygotominor.yy457->lower = yymsp[-3].minor.yy210.lower_limit;
}
#line 3390 "expparse.c"
        break;
      case 141: /* conformant_aggregation ::= TOK_SET TOK_OF parameter_type */
      case 257: /* set_type ::= TOK_SET TOK_OF attribute_type */ yytestcase(yyruleno==257);
#line 1296 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(set_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
}
#line 3399 "expparse.c"
        break;
      case 142: /* conformant_aggregation ::= TOK_SET index_spec TOK_OF parameter_type */
#line 1301 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(set_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->upper = yymsp[-2].minor.yy210.upper_limit;
    yygotominor.yy457->lower = yymsp[-2].minor.yy210.lower_limit;
}
#line 3409 "expparse.c"
        break;
      case 143: /* generic_type ::= TOK_GENERIC */
#line 1309 "expparse.y"
{
    yygotominor.yy155 = Type_Generic;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
#line 3424 "expparse.c"
        break;
      case 144: /* generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER */
#line 1321 "expparse.y"
{
    TypeBody g = TYPEBODYcreate(generic_);
    yygotominor.yy155 = TYPEcreate_from_body_anonymously(g);

    SCOPEadd_super(yygotominor.yy155);

    g->tag = TYPEcreate_user_defined_tag(yygotominor.yy155, CURRENT_SCOPE, yymsp[0].minor.yy0.symbol);
    if (g->tag) {
        SCOPEadd_super(g->tag);
    }
}
#line 3439 "expparse.c"
        break;
      case 145: /* id_list ::= TOK_IDENTIFIER */
#line 1334 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy0.symbol);

}
#line 3448 "expparse.c"
        break;
      case 146: /* id_list ::= id_list TOK_COMMA TOK_IDENTIFIER */
#line 1340 "expparse.y"
{
    yyerrok;

    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy0.symbol);
}
#line 3458 "expparse.c"
        break;
      case 147: /* identifier ::= TOK_SELF */
#line 1348 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Self);
}
#line 3465 "expparse.c"
        break;
      case 148: /* identifier ::= TOK_QUESTION_MARK */
#line 1352 "expparse.y"
{
    yygotominor.yy145 = LITERAL_INFINITY;
}
#line 3472 "expparse.c"
        break;
      case 149: /* identifier ::= TOK_IDENTIFIER */
#line 1356 "expparse.y"
{
    yygotominor.yy145 = EXPcreate(Type_Identifier);
    yygotominor.yy145->symbol = *(yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3481 "expparse.c"
        break;
      case 150: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon */
#line 1364 "expparse.y"
{
    yygotominor.yy522 = CONDcreate(yymsp[-4].minor.yy145, yymsp[-2].minor.yy471, STATEMENT_LIST_NULL);
}
#line 3488 "expparse.c"
        break;
      case 151: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon */
#line 1369 "expparse.y"
{
    yygotominor.yy522 = CONDcreate(yymsp[-6].minor.yy145, yymsp[-4].minor.yy471, yymsp[-2].minor.yy471);
}
#line 3495 "expparse.c"
        break;
      case 152: /* include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon */
#line 1374 "expparse.y"
{
    SCANinclude_file(yymsp[-1].minor.yy0.string);
}
#line 3502 "expparse.c"
        break;
      case 153: /* increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression */
#line 1380 "expparse.y"
{
    Increment i = INCR_CTLcreate(yymsp[-5].minor.yy0.symbol, yymsp[-3].minor.yy145, yymsp[-1].minor.yy145, yymsp[0].minor.yy145);

    /* scope doesn't really have/need a name, I suppose */
    /* naming it by the iterator variable is fine */

    PUSH_SCOPE(i, (Symbol *)0, OBJ_INCREMENT);
}
#line 3514 "expparse.c"
        break;
      case 156: /* rename ::= TOK_IDENTIFIER */
#line 1402 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[0].minor.yy0, yymsp[0].minor.yy0);
}
#line 3521 "expparse.c"
        break;
      case 157: /* rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER */
#line 1406 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
#line 3528 "expparse.c"
        break;
      case 159: /* rename_list ::= rename_list TOK_COMMA rename */
      case 162: /* reference_clause ::= reference_head parened_rename_list semicolon */ yytestcase(yyruleno==162);
      case 165: /* use_clause ::= use_head parened_rename_list semicolon */ yytestcase(yyruleno==165);
      case 250: /* schema_body ::= interface_specification_list constant_decl block_list */ yytestcase(yyruleno==250);
      case 296: /* type_decl ::= td_start TOK_END_TYPE semicolon */ yytestcase(yyruleno==296);
#line 1415 "expparse.y"
{
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 3539 "expparse.c"
        break;
      case 161: /* reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1422 "expparse.y"
{
    if (!CURRENT_SCHEMA->ref_schemas) {
        CURRENT_SCHEMA->ref_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->ref_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3550 "expparse.c"
        break;
      case 163: /* reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER */
#line 1435 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_reference;
}
#line 3558 "expparse.c"
        break;
      case 164: /* use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1441 "expparse.y"
{
    if (!CURRENT_SCHEMA->use_schemas) {
        CURRENT_SCHEMA->use_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->use_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3569 "expparse.c"
        break;
      case 166: /* use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER */
#line 1454 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_use;
}
#line 3577 "expparse.c"
        break;
      case 171: /* interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl */
#line 1477 "expparse.y"
{
    Expression	tmp1, tmp2;

    yygotominor.yy145 = (Expression)0;
    tmp1 = BIN_EXPcreate(yymsp[-4].minor.yy206, yymsp[-5].minor.yy145, yymsp[-3].minor.yy145);
    tmp2 = BIN_EXPcreate(yymsp[-2].minor.yy206, yymsp[-3].minor.yy145, yymsp[-1].minor.yy145);
    yygotominor.yy145 = BIN_EXPcreate(OP_AND, tmp1, tmp2);
}
#line 3589 "expparse.c"
        break;
      case 172: /* set_or_bag_of_entity ::= defined_type */
      case 290: /* type ::= defined_type */ yytestcase(yyruleno==290);
#line 1489 "expparse.y"
{
    yygotominor.yy352.type = yymsp[0].minor.yy155;
    yygotominor.yy352.body = 0;
}
#line 3598 "expparse.c"
        break;
      case 173: /* set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type */
#line 1494 "expparse.y"
{
    yygotominor.yy352.type = 0;
    yygotominor.yy352.body = TYPEBODYcreate(set_);
    yygotominor.yy352.body->base = yymsp[0].minor.yy155;

}
#line 3608 "expparse.c"
        break;
      case 174: /* set_or_bag_of_entity ::= TOK_SET limit_spec TOK_OF defined_type */
#line 1501 "expparse.y"
{
    yygotominor.yy352.type = 0; 
    yygotominor.yy352.body = TYPEBODYcreate(set_);
    yygotominor.yy352.body->base = yymsp[0].minor.yy155;
    yygotominor.yy352.body->upper = yymsp[-2].minor.yy210.upper_limit;
    yygotominor.yy352.body->lower = yymsp[-2].minor.yy210.lower_limit;
}
#line 3619 "expparse.c"
        break;
      case 175: /* set_or_bag_of_entity ::= TOK_BAG limit_spec TOK_OF defined_type */
#line 1509 "expparse.y"
{
    yygotominor.yy352.type = 0;
    yygotominor.yy352.body = TYPEBODYcreate(bag_);
    yygotominor.yy352.body->base = yymsp[0].minor.yy155;
    yygotominor.yy352.body->upper = yymsp[-2].minor.yy210.upper_limit;
    yygotominor.yy352.body->lower = yymsp[-2].minor.yy210.lower_limit;
}
#line 3630 "expparse.c"
        break;
      case 176: /* set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type */
#line 1517 "expparse.y"
{
    yygotominor.yy352.type = 0;
    yygotominor.yy352.body = TYPEBODYcreate(bag_);
    yygotominor.yy352.body->base = yymsp[0].minor.yy155;
}
#line 3639 "expparse.c"
        break;
      case 179: /* inverse_attr ::= TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon */
#line 1536 "expparse.y"
{
    Expression e = EXPcreate(Type_Attribute);

    e->symbol = *yymsp[-5].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[-5].minor.yy0.symbol);

    if (yymsp[-3].minor.yy352.type) {
        yygotominor.yy443 = VARcreate(e, yymsp[-3].minor.yy352.type);
    } else {
        Type t = TYPEcreate_from_body_anonymously(yymsp[-3].minor.yy352.body);
        SCOPEadd_super(t);
        yygotominor.yy443 = VARcreate(e, t);
    }

    yygotominor.yy443->flags.attribute = true;
    yygotominor.yy443->inverse_symbol = yymsp[-1].minor.yy0.symbol;
}
#line 3660 "expparse.c"
        break;
      case 183: /* list_type ::= TOK_LIST limit_spec TOK_OF unique attribute_type */
#line 1571 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(list_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
    yygotominor.yy457->lower = yymsp[-3].minor.yy210.lower_limit;
    yygotominor.yy457->upper = yymsp[-3].minor.yy210.upper_limit;
}
#line 3671 "expparse.c"
        break;
      case 184: /* list_type ::= TOK_LIST TOK_OF unique attribute_type */
#line 1579 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(list_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->flags.unique = yymsp[-1].minor.yy224.unique;
}
#line 3680 "expparse.c"
        break;
      case 185: /* literal ::= TOK_INTEGER_LITERAL */
#line 1586 "expparse.y"
{
    if (yymsp[0].minor.yy0.iVal == 0) {
        yygotominor.yy145 = LITERAL_ZERO;
    } else if (yymsp[0].minor.yy0.iVal == 1) {
	yygotominor.yy145 = LITERAL_ONE;
    } else {
	yygotominor.yy145 = EXPcreate_simple(Type_Integer);
	yygotominor.yy145->u.integer = (int)yymsp[0].minor.yy0.iVal;
	resolved_all(yygotominor.yy145);
    }
}
#line 3695 "expparse.c"
        break;
      case 186: /* literal ::= TOK_REAL_LITERAL */
#line 1598 "expparse.y"
{
    if (yymsp[0].minor.yy0.rVal == 0.0) {
	yygotominor.yy145 = LITERAL_ZERO;
    } else {
	yygotominor.yy145 = EXPcreate_simple(Type_Real);
	yygotominor.yy145->u.real = yymsp[0].minor.yy0.rVal;
	resolved_all(yygotominor.yy145);
    }
}
#line 3708 "expparse.c"
        break;
      case 187: /* literal ::= TOK_STRING_LITERAL */
#line 1608 "expparse.y"
{
    yygotominor.yy145 = EXPcreate_simple(Type_String);
    yygotominor.yy145->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy145);
}
#line 3717 "expparse.c"
        break;
      case 188: /* literal ::= TOK_STRING_LITERAL_ENCODED */
#line 1614 "expparse.y"
{
    yygotominor.yy145 = EXPcreate_simple(Type_String_Encoded);
    yygotominor.yy145->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy145);
}
#line 3726 "expparse.c"
        break;
      case 189: /* literal ::= TOK_LOGICAL_LITERAL */
#line 1620 "expparse.y"
{
    yygotominor.yy145 = EXPcreate_simple(Type_Logical);
    yygotominor.yy145->u.logical = yymsp[0].minor.yy0.logical;
    resolved_all(yygotominor.yy145);
}
#line 3735 "expparse.c"
        break;
      case 190: /* literal ::= TOK_BINARY_LITERAL */
#line 1626 "expparse.y"
{
    yygotominor.yy145 = EXPcreate_simple(Type_Binary);
    yygotominor.yy145->symbol.name = yymsp[0].minor.yy0.binary;
    resolved_all(yygotominor.yy145);
}
#line 3744 "expparse.c"
        break;
      case 193: /* local_variable ::= id_list TOK_COLON parameter_type semicolon */
#line 1642 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-3].minor.yy471, sym, Symbol *)

    /* convert symbol to name-expression */

    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-1].minor.yy155);
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-3].minor.yy471);
}
#line 3763 "expparse.c"
        break;
      case 194: /* local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon */
#line 1659 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-4].minor.yy471, sym, Symbol *)
    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-2].minor.yy155);
    v->initializer = yymsp[-1].minor.yy145;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-4].minor.yy471);
}
#line 3780 "expparse.c"
        break;
      case 198: /* allow_generic_types ::= */
#line 1682 "expparse.y"
{
    tag_count = 0; /* don't signal an error if we find a generic_type */
}
#line 3787 "expparse.c"
        break;
      case 199: /* disallow_generic_types ::= */
#line 1687 "expparse.y"
{
    tag_count = -1; /* signal an error if we find a generic_type */
}
#line 3794 "expparse.c"
        break;
      case 200: /* defined_type ::= TOK_IDENTIFIER */
#line 1692 "expparse.y"
{
    yygotominor.yy155 = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(yygotominor.yy155);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3803 "expparse.c"
        break;
      case 201: /* defined_type_list ::= defined_type */
#line 1699 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd(yygotominor.yy471, (Generic)yymsp[0].minor.yy155);

}
#line 3812 "expparse.c"
        break;
      case 202: /* defined_type_list ::= defined_type_list TOK_COMMA defined_type */
#line 1705 "expparse.y"
{
    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471,
    (Generic)yymsp[0].minor.yy155);
}
#line 3821 "expparse.c"
        break;
      case 205: /* optional_or_unique ::= */
#line 1722 "expparse.y"
{
    yygotominor.yy224.unique = 0;
    yygotominor.yy224.optional = 0;
}
#line 3829 "expparse.c"
        break;
      case 206: /* optional_or_unique ::= TOK_OPTIONAL */
#line 1727 "expparse.y"
{
    yygotominor.yy224.unique = 0;
    yygotominor.yy224.optional = 1;
}
#line 3837 "expparse.c"
        break;
      case 207: /* optional_or_unique ::= TOK_UNIQUE */
#line 1732 "expparse.y"
{
    yygotominor.yy224.unique = 1;
    yygotominor.yy224.optional = 0;
}
#line 3845 "expparse.c"
        break;
      case 208: /* optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE */
      case 209: /* optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL */ yytestcase(yyruleno==209);
#line 1737 "expparse.y"
{
    yygotominor.yy224.unique = 1;
    yygotominor.yy224.optional = 1;
}
#line 3854 "expparse.c"
        break;
      case 210: /* optional_fixed ::= */
#line 1748 "expparse.y"
{
    yygotominor.yy224.fixed = 0;
}
#line 3861 "expparse.c"
        break;
      case 211: /* optional_fixed ::= TOK_FIXED */
#line 1752 "expparse.y"
{
    yygotominor.yy224.fixed = 1;
}
#line 3868 "expparse.c"
        break;
      case 212: /* precision_spec ::= */
#line 1757 "expparse.y"
{
    yygotominor.yy145 = (Expression)0;
}
#line 3875 "expparse.c"
        break;
      case 213: /* precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */
      case 305: /* unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */ yytestcase(yyruleno==305);
#line 1761 "expparse.y"
{
    yygotominor.yy145 = yymsp[-1].minor.yy145;
}
#line 3883 "expparse.c"
        break;
      case 214: /* proc_call_statement ::= procedure_id actual_parameters semicolon */
#line 1771 "expparse.y"
{
    yygotominor.yy522 = PCALLcreate(yymsp[-1].minor.yy471);
    yygotominor.yy522->symbol = *(yymsp[-2].minor.yy461);
}
#line 3891 "expparse.c"
        break;
      case 215: /* proc_call_statement ::= procedure_id semicolon */
#line 1776 "expparse.y"
{
    yygotominor.yy522 = PCALLcreate((Linked_List)0);
    yygotominor.yy522->symbol = *(yymsp[-1].minor.yy461);
}
#line 3899 "expparse.c"
        break;
      case 216: /* procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon */
#line 1783 "expparse.y"
{
    PROCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy471);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy215, SCANtell());
    POP_SCOPE();
}
#line 3908 "expparse.c"
        break;
      case 217: /* procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon */
#line 1791 "expparse.y"
{
    Procedure p = CURRENT_SCOPE;
    p->u.proc->parameters = yymsp[-1].minor.yy471;
    p->u.proc->pcount = LISTget_length(yymsp[-1].minor.yy471);
    p->u.proc->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
    yygotominor.yy215 = yymsp[-3].minor.yy215;
}
#line 3920 "expparse.c"
        break;
      case 218: /* ph_push_scope ::= TOK_IDENTIFIER */
#line 1801 "expparse.y"
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    tag_count = 0;

    if (print_objects_while_running & OBJ_PROCEDURE_BITS) {
	fprintf(stdout, "parse: %s (procedure)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(p, yymsp[0].minor.yy0.symbol, OBJ_PROCEDURE);
}
#line 3934 "expparse.c"
        break;
      case 222: /* group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1827 "expparse.y"
{
    yygotominor.yy145 = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy145->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy145->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3944 "expparse.c"
        break;
      case 223: /* qualifier ::= TOK_DOT TOK_IDENTIFIER */
#line 1835 "expparse.y"
{
    yygotominor.yy384.expr = yygotominor.yy384.first = BIN_EXPcreate(OP_DOT, (Expression)0, (Expression)0);
    yygotominor.yy384.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy384.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3954 "expparse.c"
        break;
      case 224: /* qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1842 "expparse.y"
{
    yygotominor.yy384.expr = yygotominor.yy384.first = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy384.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy384.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3964 "expparse.c"
        break;
      case 225: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET */
#line 1849 "expparse.y"
{
    yygotominor.yy384.expr = yygotominor.yy384.first = BIN_EXPcreate(OP_ARRAY_ELEMENT, (Expression)0,
	(Expression)0);
    yygotominor.yy384.expr->e.op2 = yymsp[-1].minor.yy145;
}
#line 3973 "expparse.c"
        break;
      case 226: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET */
#line 1856 "expparse.y"
{
    yygotominor.yy384.expr = yygotominor.yy384.first = TERN_EXPcreate(OP_SUBCOMPONENT, (Expression)0,
	(Expression)0, (Expression)0);
    yygotominor.yy384.expr->e.op2 = yymsp[-3].minor.yy145;
    yygotominor.yy384.expr->e.op3 = yymsp[-1].minor.yy145;
}
#line 3983 "expparse.c"
        break;
      case 227: /* query_expression ::= query_start expression TOK_RIGHT_PAREN */
#line 1864 "expparse.y"
{
    yygotominor.yy145 = yymsp[-2].minor.yy145;
    yygotominor.yy145->u.query->expression = yymsp[-1].minor.yy145;
    POP_SCOPE();
}
#line 3992 "expparse.c"
        break;
      case 228: /* query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT */
#line 1872 "expparse.y"
{
    yygotominor.yy145 = QUERYcreate(yymsp[-3].minor.yy0.symbol, yymsp[-1].minor.yy145);
    SYMBOL_destroy(yymsp[-3].minor.yy0.symbol);
    PUSH_SCOPE(yygotominor.yy145->u.query->scope, (Symbol *)0, OBJ_QUERY);
}
#line 4001 "expparse.c"
        break;
      case 229: /* rel_op ::= TOK_LESS_THAN */
#line 1879 "expparse.y"
{
    yygotominor.yy206 = OP_LESS_THAN;
}
#line 4008 "expparse.c"
        break;
      case 230: /* rel_op ::= TOK_GREATER_THAN */
#line 1883 "expparse.y"
{
    yygotominor.yy206 = OP_GREATER_THAN;
}
#line 4015 "expparse.c"
        break;
      case 231: /* rel_op ::= TOK_EQUAL */
#line 1887 "expparse.y"
{
    yygotominor.yy206 = OP_EQUAL;
}
#line 4022 "expparse.c"
        break;
      case 232: /* rel_op ::= TOK_LESS_EQUAL */
#line 1891 "expparse.y"
{
    yygotominor.yy206 = OP_LESS_EQUAL;
}
#line 4029 "expparse.c"
        break;
      case 233: /* rel_op ::= TOK_GREATER_EQUAL */
#line 1895 "expparse.y"
{
    yygotominor.yy206 = OP_GREATER_EQUAL;
}
#line 4036 "expparse.c"
        break;
      case 234: /* rel_op ::= TOK_NOT_EQUAL */
#line 1899 "expparse.y"
{
    yygotominor.yy206 = OP_NOT_EQUAL;
}
#line 4043 "expparse.c"
        break;
      case 235: /* rel_op ::= TOK_INST_EQUAL */
#line 1903 "expparse.y"
{
    yygotominor.yy206 = OP_INST_EQUAL;
}
#line 4050 "expparse.c"
        break;
      case 236: /* rel_op ::= TOK_INST_NOT_EQUAL */
#line 1907 "expparse.y"
{
    yygotominor.yy206 = OP_INST_NOT_EQUAL;
}
#line 4057 "expparse.c"
        break;
      case 237: /* repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1915 "expparse.y"
{
    yygotominor.yy522 = LOOPcreate(CURRENT_SCOPE, yymsp[-5].minor.yy145, yymsp[-4].minor.yy145, yymsp[-2].minor.yy471);

    /* matching PUSH_SCOPE is in increment_control */
    POP_SCOPE();
}
#line 4067 "expparse.c"
        break;
      case 238: /* repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1923 "expparse.y"
{
    yygotominor.yy522 = LOOPcreate((struct Scope_ *)0, yymsp[-5].minor.yy145, yymsp[-4].minor.yy145, yymsp[-2].minor.yy471);
}
#line 4074 "expparse.c"
        break;
      case 239: /* return_statement ::= TOK_RETURN semicolon */
#line 1928 "expparse.y"
{
    yygotominor.yy522 = RETcreate((Expression)0);
}
#line 4081 "expparse.c"
        break;
      case 240: /* return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon */
#line 1933 "expparse.y"
{
    yygotominor.yy522 = RETcreate(yymsp[-2].minor.yy145);
}
#line 4088 "expparse.c"
        break;
      case 242: /* rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon */
#line 1944 "expparse.y"
{
    RULEput_body(CURRENT_SCOPE, yymsp[-3].minor.yy471);
    RULEput_where(CURRENT_SCOPE, yymsp[-2].minor.yy471);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-4].minor.yy215, SCANtell());
    POP_SCOPE();
}
#line 4098 "expparse.c"
        break;
      case 243: /* rule_formal_parameter ::= TOK_IDENTIFIER */
#line 1952 "expparse.y"
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
    yygotominor.yy443 = VARcreate(e, t);
    yygotominor.yy443->flags.attribute = true;
    yygotominor.yy443->flags.parameter = true;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[0].minor.yy0.symbol->name, (Generic)yygotominor.yy443,
	yymsp[0].minor.yy0.symbol, OBJ_VARIABLE);
}
#line 4121 "expparse.c"
        break;
      case 244: /* rule_formal_parameter_list ::= rule_formal_parameter */
#line 1973 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd(yygotominor.yy471, (Generic)yymsp[0].minor.yy443); 
}
#line 4129 "expparse.c"
        break;
      case 245: /* rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter */
#line 1979 "expparse.y"
{
    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy443);
}
#line 4137 "expparse.c"
        break;
      case 246: /* rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon */
#line 1986 "expparse.y"
{
    CURRENT_SCOPE->u.rule->parameters = yymsp[-2].minor.yy471;

    yygotominor.yy215 = yymsp[-3].minor.yy215;
}
#line 4146 "expparse.c"
        break;
      case 247: /* rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN */
#line 1994 "expparse.y"
{
    Rule r = ALGcreate(OBJ_RULE);

    if (print_objects_while_running & OBJ_RULE_BITS) {
	fprintf(stdout, "parse: %s (rule)\n", yymsp[-2].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(r, yymsp[-2].minor.yy0.symbol, OBJ_RULE);

    yygotominor.yy215 = yymsp[-3].minor.yy215;
}
#line 4161 "expparse.c"
        break;
      case 251: /* schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon */
#line 2021 "expparse.y"
{
    POP_SCOPE();
}
#line 4168 "expparse.c"
        break;
      case 253: /* schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon */
#line 2030 "expparse.y"
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
#line 4188 "expparse.c"
        break;
      case 254: /* select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */
#line 2049 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(select_);
    yygotominor.yy457->list = yymsp[-1].minor.yy471;
}
#line 4196 "expparse.c"
        break;
      case 256: /* set_type ::= TOK_SET limit_spec TOK_OF attribute_type */
#line 2060 "expparse.y"
{
    yygotominor.yy457 = TYPEBODYcreate(set_);
    yygotominor.yy457->base = yymsp[0].minor.yy155;
    yygotominor.yy457->lower = yymsp[-2].minor.yy210.lower_limit;
    yygotominor.yy457->upper = yymsp[-2].minor.yy210.upper_limit;
}
#line 4206 "expparse.c"
        break;
      case 258: /* skip_statement ::= TOK_SKIP semicolon */
#line 2073 "expparse.y"
{
    yygotominor.yy522 = STATEMENT_SKIP;
}
#line 4213 "expparse.c"
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
#line 2078 "expparse.y"
{
    yygotominor.yy522 = yymsp[0].minor.yy522;
}
#line 4229 "expparse.c"
        break;
      case 271: /* statement_rep ::= statement statement_rep */
#line 2127 "expparse.y"
{
    yygotominor.yy471 = yymsp[0].minor.yy471;
    LISTadd_first(yygotominor.yy471, (Generic)yymsp[-1].minor.yy522); 
}
#line 4237 "expparse.c"
        break;
      case 272: /* subsuper_decl ::= */
#line 2137 "expparse.y"
{
    yygotominor.yy34.subtypes = EXPRESSION_NULL;
    yygotominor.yy34.abstract = false;
    yygotominor.yy34.supertypes = LIST_NULL;
}
#line 4246 "expparse.c"
        break;
      case 273: /* subsuper_decl ::= supertype_decl */
#line 2143 "expparse.y"
{
    yygotominor.yy34.subtypes = yymsp[0].minor.yy261.subtypes;
    yygotominor.yy34.abstract = yymsp[0].minor.yy261.abstract;
    yygotominor.yy34.supertypes = LIST_NULL;
}
#line 4255 "expparse.c"
        break;
      case 274: /* subsuper_decl ::= subtype_decl */
#line 2149 "expparse.y"
{
    yygotominor.yy34.supertypes = yymsp[0].minor.yy471;
    yygotominor.yy34.abstract = false;
    yygotominor.yy34.subtypes = EXPRESSION_NULL;
}
#line 4264 "expparse.c"
        break;
      case 275: /* subsuper_decl ::= supertype_decl subtype_decl */
#line 2155 "expparse.y"
{
    yygotominor.yy34.subtypes = yymsp[-1].minor.yy261.subtypes;
    yygotominor.yy34.abstract = yymsp[-1].minor.yy261.abstract;
    yygotominor.yy34.supertypes = yymsp[0].minor.yy471;
}
#line 4273 "expparse.c"
        break;
      case 277: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE */
#line 2168 "expparse.y"
{
    yygotominor.yy261.subtypes = (Expression)0;
    yygotominor.yy261.abstract = true;
}
#line 4281 "expparse.c"
        break;
      case 278: /* supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2174 "expparse.y"
{
    yygotominor.yy261.subtypes = yymsp[-1].minor.yy145;
    yygotominor.yy261.abstract = false;
}
#line 4289 "expparse.c"
        break;
      case 279: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2180 "expparse.y"
{
    yygotominor.yy261.subtypes = yymsp[-1].minor.yy145;
    yygotominor.yy261.abstract = true;
}
#line 4297 "expparse.c"
        break;
      case 280: /* supertype_expression ::= supertype_factor */
#line 2186 "expparse.y"
{
    yygotominor.yy145 = yymsp[0].minor.yy261.subtypes;
}
#line 4304 "expparse.c"
        break;
      case 281: /* supertype_expression ::= supertype_expression TOK_AND supertype_factor */
#line 2190 "expparse.y"
{
    yygotominor.yy145 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy145, yymsp[0].minor.yy261.subtypes);
}
#line 4311 "expparse.c"
        break;
      case 282: /* supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor */
#line 2195 "expparse.y"
{
    yygotominor.yy145 = BIN_EXPcreate(OP_ANDOR, yymsp[-2].minor.yy145, yymsp[0].minor.yy261.subtypes);
}
#line 4318 "expparse.c"
        break;
      case 284: /* supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression */
#line 2206 "expparse.y"
{
    LISTadd_last(yymsp[-2].minor.yy471, (Generic)yymsp[0].minor.yy145);
    yygotominor.yy471 = yymsp[-2].minor.yy471;
}
#line 4326 "expparse.c"
        break;
      case 285: /* supertype_factor ::= identifier */
#line 2212 "expparse.y"
{
    yygotominor.yy261.subtypes = yymsp[0].minor.yy145;
}
#line 4333 "expparse.c"
        break;
      case 286: /* supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN */
#line 2217 "expparse.y"
{
    yygotominor.yy261.subtypes = EXPcreate(Type_Oneof);
    yygotominor.yy261.subtypes->u.list = yymsp[-1].minor.yy471;
}
#line 4341 "expparse.c"
        break;
      case 287: /* supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2222 "expparse.y"
{
    yygotominor.yy261.subtypes = yymsp[-1].minor.yy145;
}
#line 4348 "expparse.c"
        break;
      case 288: /* type ::= aggregation_type */
      case 289: /* type ::= basic_type */ yytestcase(yyruleno==289);
      case 291: /* type ::= select_type */ yytestcase(yyruleno==291);
#line 2227 "expparse.y"
{
    yygotominor.yy352.type = 0;
    yygotominor.yy352.body = yymsp[0].minor.yy457;
}
#line 4358 "expparse.c"
        break;
      case 293: /* type_item_body ::= type */
#line 2252 "expparse.y"
{
    CURRENT_SCOPE->u.type->head = yymsp[0].minor.yy352.type;
    CURRENT_SCOPE->u.type->body = yymsp[0].minor.yy352.body;
}
#line 4366 "expparse.c"
        break;
      case 295: /* ti_start ::= TOK_IDENTIFIER TOK_EQUAL */
#line 2260 "expparse.y"
{
    Type t = TYPEcreate_name(yymsp[-1].minor.yy0.symbol);
    PUSH_SCOPE(t, yymsp[-1].minor.yy0.symbol, OBJ_TYPE);
}
#line 4374 "expparse.c"
        break;
      case 297: /* td_start ::= TOK_TYPE type_item where_rule_OPT */
#line 2271 "expparse.y"
{
    CURRENT_SCOPE->where = yymsp[0].minor.yy471;
    POP_SCOPE();
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 4383 "expparse.c"
        break;
      case 298: /* general_ref ::= assignable group_ref */
#line 2278 "expparse.y"
{
    yymsp[0].minor.yy145->e.op1 = yymsp[-1].minor.yy145;
    yygotominor.yy145 = yymsp[0].minor.yy145;
}
#line 4391 "expparse.c"
        break;
      case 308: /* unary_expression ::= TOK_NOT unary_expression */
#line 2321 "expparse.y"
{
    yygotominor.yy145 = UN_EXPcreate(OP_NOT, yymsp[0].minor.yy145);
}
#line 4398 "expparse.c"
        break;
      case 310: /* unary_expression ::= TOK_MINUS unary_expression */
#line 2329 "expparse.y"
{
    yygotominor.yy145 = UN_EXPcreate(OP_NEGATE, yymsp[0].minor.yy145);
}
#line 4405 "expparse.c"
        break;
      case 311: /* unique ::= */
#line 2334 "expparse.y"
{
    yygotominor.yy224.unique = 0;
}
#line 4412 "expparse.c"
        break;
      case 312: /* unique ::= TOK_UNIQUE */
#line 2338 "expparse.y"
{
    yygotominor.yy224.unique = 1;
}
#line 4419 "expparse.c"
        break;
      case 313: /* qualified_attr ::= TOK_IDENTIFIER */
#line 2343 "expparse.y"
{
    yygotominor.yy101 = QUAL_ATTR_new();
    yygotominor.yy101->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4427 "expparse.c"
        break;
      case 314: /* qualified_attr ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 2349 "expparse.y"
{
    yygotominor.yy101 = QUAL_ATTR_new();
    yygotominor.yy101->entity = yymsp[-2].minor.yy0.symbol;
    yygotominor.yy101->attribute = yymsp[0].minor.yy0.symbol;
}
#line 4436 "expparse.c"
        break;
      case 315: /* qualified_attr_list ::= qualified_attr */
#line 2356 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy101);
}
#line 4444 "expparse.c"
        break;
      case 316: /* qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr */
#line 2361 "expparse.y"
{
    yygotominor.yy471 = yymsp[-2].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy101);
}
#line 4452 "expparse.c"
        break;
      case 317: /* labelled_attrib_list ::= qualified_attr_list semicolon */
#line 2367 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy471, (Generic)EXPRESSION_NULL);
    yygotominor.yy471 = yymsp[-1].minor.yy471;
}
#line 4460 "expparse.c"
        break;
      case 318: /* labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon */
#line 2373 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy471, (Generic)yymsp[-3].minor.yy0.symbol); 
    yygotominor.yy471 = yymsp[-1].minor.yy471;
}
#line 4468 "expparse.c"
        break;
      case 319: /* labelled_attrib_list_list ::= labelled_attrib_list */
#line 2380 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy471);
}
#line 4476 "expparse.c"
        break;
      case 320: /* labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list */
#line 2386 "expparse.y"
{
    LISTadd_last(yymsp[-1].minor.yy471, (Generic)yymsp[0].minor.yy471);
    yygotominor.yy471 = yymsp[-1].minor.yy471;
}
#line 4484 "expparse.c"
        break;
      case 323: /* until_control ::= */
      case 332: /* while_control ::= */ yytestcase(yyruleno==332);
#line 2401 "expparse.y"
{
    yygotominor.yy145 = 0;
}
#line 4492 "expparse.c"
        break;
      case 325: /* where_clause ::= expression semicolon */
#line 2410 "expparse.y"
{
    yygotominor.yy428 = WHERE_new();
    yygotominor.yy428->label = SYMBOLcreate("<unnamed>", yylineno, current_filename);
    yygotominor.yy428->expr = yymsp[-1].minor.yy145;
}
#line 4501 "expparse.c"
        break;
      case 326: /* where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon */
#line 2416 "expparse.y"
{
    yygotominor.yy428 = WHERE_new();
    yygotominor.yy428->label = yymsp[-3].minor.yy0.symbol;
    yygotominor.yy428->expr = yymsp[-1].minor.yy145;

    if (!CURRENT_SCOPE->symbol_table) {
	CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-3].minor.yy0.symbol->name, (Generic)yygotominor.yy428,
	yymsp[-3].minor.yy0.symbol, OBJ_WHERE);
}
#line 4517 "expparse.c"
        break;
      case 327: /* where_clause_list ::= where_clause */
#line 2430 "expparse.y"
{
    yygotominor.yy471 = LISTcreate();
    LISTadd(yygotominor.yy471, (Generic)yymsp[0].minor.yy428);
}
#line 4525 "expparse.c"
        break;
      case 328: /* where_clause_list ::= where_clause_list where_clause */
#line 2435 "expparse.y"
{
    yygotominor.yy471 = yymsp[-1].minor.yy471;
    LISTadd_last(yygotominor.yy471, (Generic)yymsp[0].minor.yy428);
}
#line 4533 "expparse.c"
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
#line 2463 "expparse.y"

    yyerrstatus++;
    fprintf(stderr, "Express parser experienced syntax error at line %d.\n", yylineno);
    fprintf(stderr, "Last token (type %d) had value %x\n", yymajor, yyminor.yy0.val);
#line 4609 "expparse.c"
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
