%{
/*
**	Grammar for Cakefiles
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<ctype.h>

extern	List	*set_flag();
extern	int	yylineno;
extern	char	*yyfile;
%}

%start		file

%union
{
	int	Uint;
	char	*Ustr;
	List	*Ulist;
	Entry	*Uent;
	Test	*Utest;
	Act	*Uact;
	Pat	*Upat;
}

%token		NL EMPTYLINE
%token		NONVOL PRECIOUS REDUNDANT PSEUDO WHEN
%token		IF IN MATCH EXIST CANDO OK
%token		LPAREN RPAREN
%token	<Ustr>	COLON PAT PATCMD TESTCMD
%token	<Uact>	BEGIN_ACT ACT END_ACT
%token		GARBAGE
%left		OR
%left		AND
%left		NOT

%type	<Uent>	rule
%type	<Utest>	cond test
%type	<Upat>	basepat
%type	<Uact>	act
%type	<Uint>	flag flag_el
%type	<Ulist>	pat
%type	<Ulist>	rule_l ACT_el act_el pat_el pat_l

%%

/**********************************************************************/
/*		Entries						      */

file	:	empty_el ent_esl empty_el
	;

entry	:	rule_l act_el
		{	new_entry($1, $2);		}
	;

rule	:	pat_l COLON pat_el cond NL
		{	$$ = make_dep($1, $3, $4, new_name(yyfile), $2);	}
	;

cond	:	IF test
		{	$$ = $2;			}
	|	/* empty */
		{	$$ = (Test *) NULL;		}
	;

test	:	basepat IN pat_l
		{	$$ = make_test_l($1, $3);	}
	|	basepat MATCH basepat
		{	$$ = make_test_m($1, (Pat *) NULL, $3);}
	|	basepat MATCH basepat basepat
		{	$$ = make_test_m($1, $3, $4);	}
	|	EXIST basepat
		{	$$ = make_test_s(t_EXIST, $2);	}
	|	CANDO basepat
		{	$$ = make_test_s(t_CANDO, $2);	}
	|	OK basepat
		{	$$ = make_test_s(t_OK, $2);	}
	|	TESTCMD
		{	$$ = make_test_c($1);		}
	|	test AND test
		{	$$ = make_test_b(t_AND, $1, $3);}
	|	test OR test
		{	$$ = make_test_b(t_OR, $1, $3);	}
	|	NOT test
		{	$$ = make_test_u(t_NOT, $2);	}
	|	LPAREN test RPAREN
		{	$$ = $2;			}
	;

act	:	ACT
		{	$$ = $1;			}
	|	BEGIN_ACT ACT_el END_ACT
		{	$$ = prep_script($1, $2, $3);	}
	;

basepat	:	PAT
		{	$$ = make_pat($1, FALSE, 0);	}
	|	PATCMD
		{	$$ = make_pat($1, TRUE, 0);	}
	;

pat	:	basepat flag_el
		{	$$ = set_flag(makelist($1), $2);}
	|	LPAREN pat_l RPAREN flag_el
		{	$$ = set_flag($2, $4);		}
	;

flag	:	NONVOL
		{	$$ = nf_NONVOL;			}
	|	PRECIOUS
		{	$$ = nf_PRECIOUS;		}
	|	REDUNDANT
		{	$$ = nf_REDUNDANT;		}
	|	PSEUDO
		{	$$ = nf_PSEUDO;			}
	|	WHEN
		{	$$ = nf_WHEN;			}
	;

/* From here on, the grammar is JUNK				      */
/**********************************************************************/
/*		Lists						      */

ent_esl	:	entry
	|	ent_esl empty_l entry
	;

rule_l	:	rule
		{	$$ = makelist($1);		}
	|	rule_l rule
		{	$$ = addtail($1, $2);		}
	;

ACT_el	:	/* empty */
		{	$$ = makelist0();		}
	|	ACT_el ACT
		{	$$ = addtail($1, $2);		}
	;

act_el	:	/* empty */
		{	$$ = makelist0();		}
	|	act_el act
		{	$$ = addtail($1, $2);		}
	;

pat_el	:	/* empty */
		{	$$ = makelist0();		}
	|	pat_l
		{	$$ = $1;			}
	;

pat_l	:	pat
		{	$$ = $1;			}
	|	pat_l pat
		{	$$ = addlist($1, $2);		}
	;

flag_el	:	/* empty */
		{	$$ = 0;				}
	|	flag_el flag
		{	$$ = $1 | $2;			}
	;

empty_el:	/* empty */
	|	empty_l
	;

empty_l	:	EMPTYLINE
	|	empty_l EMPTYLINE
	;

%%

void
yyerror(s)
char	*s;
{
	extern	int	yyleng;
	extern	char	yytext[];
	char		buf[80];

	if (yychar <= 0)
	{
		sprintf(buf, "premature EOF");
		yylineno--;
	}
	or (yytext[0] == '\n' || yytext[0] == '\f')
		sprintf(buf, "%s at end of line", s);
	or (isprint(yytext[0]))
		sprintf(buf, "%s at symbol %s", s, yytext);
	else
		sprintf(buf, "%s at \\%o", s, yytext[0]);
	
	if (cakedebug)
		printf("%s, %d: %s, token %d\n", yyfile, yylineno, buf, yychar);
	else
		printf("%s, %d: %s\n", yyfile, yylineno, buf);
}
