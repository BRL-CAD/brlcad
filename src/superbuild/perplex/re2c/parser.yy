/* lemon input based on re2c's parser.y yacc input */
%include {

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <set>

#include "globals.h"
#include "re2c_parser.h"
#include "basics.h"
#include "dfa.h"
#include "re.h"
#include "scanner.h"

#define YYMALLOC malloc
#define YYFREE free

using namespace re2c;

int yylex();
void yyerror(const char*);

YYSTYPE yylval;

static re2c::uint       accept;
static re2c::RegExpMap  specMap;
static RegExp           *spec = NULL, *specNone = NULL;
static RuleOpList       specStar;
static Scanner          *in = NULL;
static Scanner::ParseMode  parseMode;
static SetupMap            ruleSetupMap;
static bool                foundRules;

/* strdup() isn't standard C, so if we don't have it, we'll create our
 * own version
 */
#if !defined(HAVE_STRDUP)
static char* strdup(const char* s)
{
	char* rv = (char*)malloc(strlen(s) + 1);

	if (rv == NULL)
	{
		return NULL;
	}
	strcpy(rv, s);
	return rv;
}
#endif

void context_check(CondList *clist)
{
	if (!cFlag)
	{
		delete clist;
		in->fatal("conditions are only allowed when using -c switch");
	}
}

void context_none(CondList *clist)
{
	delete clist;
	context_check(NULL);
	in->fatal("no expression specified");
}

void context_rule(CondList *clist, RegExp *expr, RegExp *look, Str *newcond, Token *code)
{
	context_check(clist);
	for(CondList::const_iterator it = clist->begin(); it != clist->end(); ++it)
	{
		//Str *condcpy = newcond ? new Str(*newcond) : newcond;
		Token *token = new Token(code, sourceFileInfo, newcond);//condcpy);
		RuleOp *rule = new RuleOp(expr, look, token, accept++);

		RegExpMap::iterator itRE = specMap.find(*it);

		if (itRE != specMap.end())
		{
			itRE->second.second = mkAlt(itRE->second.second, rule);
		}
		else
		{
			size_t nIndex = specMap.size() + 1; // 0 is reserved for "0"-spec
			specMap[*it] = std::make_pair(nIndex, rule);
		}
		
	}
	delete clist;
	delete newcond;
	delete code;
}

void setup_rule(CondList *clist, Token *code)
{
	assert(clist);
	assert(code);
	context_check(clist);
	if (bFirstPass)
	{
		for(CondList::const_iterator it = clist->begin(); it != clist->end(); ++it)
		{
			if (ruleSetupMap.find(*it) != ruleSetupMap.end())
			{
				in->fatalf_at(code->line, "code to setup rule '%s' is already defined", it->c_str());
			}
			ruleSetupMap[*it] = std::make_pair(code->line, code->text.to_string());
		}
	}
	delete clist;
	delete code;
}

} /* include */

%token_type { YYSTYPE }

%left BACKSLASH.
%left ID RANGE STRING LEFT_PAREN.

%type CLOSE	{ char }
%type STAR	{ char }
%type SETUP	{ char }
%type close	{ char }
%type CLOSESIZE	{ re2c::ExtOp }
%type ID	{ re2c::Symbol* }
%type FID	{ re2c::Symbol* }
%type CODE	{ re2c::Token* }
%type RANGE	{ re2c::RegExp* }
%type STRING	{ re2c::RegExp* }
%type rule	{ re2c::RegExp* }
%type look	{ re2c::RegExp* }
%type expr	{ re2c::RegExp* }
%type diff	{ re2c::RegExp* }
%type term	{ re2c::RegExp* }
%type factor	{ re2c::RegExp* }
%type primary	{ re2c::RegExp* }
%type CONFIG	{ re2c::Str* }
%type VALUE	{ re2c::Str* }
%type newcond	{ re2c::Str* }
%type cond	{ re2c::CondList* }
%type clist	{ re2c::CondList* }
%type NUMBER	{ int }

start ::= spec.

spec ::= /* empty */.
spec ::= spec rule.
{
    foundRules = true;
}
spec ::= spec decl.

decl ::= ID(V1) EQUALS expr(V3) SEMICOLON.
{
    if (V1.symbol->re)
    {
	in->fatal("sym already defined");
    }
    V1.symbol->re = V3;
}
decl ::= FID(V1) expr(V2).
{
    if (V1.symbol->re)
    {
	in->fatal("sym already defined");
    }
    V1.symbol->re = V2;
}
decl ::= ID EQUALS expr SLASH.
{
    in->fatal("trailing contexts are not allowed in named definitions");
}
decl ::= FID expr SLASH.
{
    in->fatal("trailing contexts are not allowed in named definitions");
}
decl ::= CONFIG(V1) EQUALS VALUE(V3) SEMICOLON.
{
    in->config(*V1.str, *V3.str);
    delete V1.str;
    delete V3.str;
}
decl ::= CONFIG(V1) EQUALS NUMBER(V3) SEMICOLON.
{
    in->config(*V1.str, V3.number);
    delete V1.str;
}

rule(V0) ::= expr(V1) look(V2) CODE(V3).
{
    if (cFlag)
    {
	in->fatal("condition or '<*>' required when using -c switch");
    }
    V0 = new RuleOp(V1, V2, V3.token, accept++);
    spec = spec? mkAlt(spec, V0) : V0;
}
rule ::= LESS_THAN cond(V2) GREATER_THAN expr(V4) look(V5) newcond(V6) CODE(V7).
{
    context_rule(V2, V4, V5, V6, V7.token);
}
rule ::= LESS_THAN cond(V2) GREATER_THAN expr(V4) look(V5) COLON newcond(V7).
{
    assert(V7);
    context_rule(V2, V4, V5, V7, NULL);
}
rule ::= LESS_THAN cond(V2) GREATER_THAN look newcond(V5) CODE.
{
    context_none(V2);
    delete V5;
}
rule ::= LESS_THAN cond(V2) GREATER_THAN look COLON newcond(V6).
{
    assert(V6);
    context_none(V2);
    delete V6;
}
rule ::= LESS_THAN STAR GREATER_THAN expr(V4) look(V5) newcond(V6) CODE(V7).
{
    context_check(NULL);
    Token *token = new Token(V7.token, V7.token->source, V7.token->line, V6);
    delete V7.token;
    delete V6;
    specStar.push_back(new RuleOp(V4, V5, token, accept++));
}
rule ::= LESS_THAN STAR GREATER_THAN expr(V4) look(V5) COLON newcond(V7).
{
    assert(V7);
    context_check(NULL);
    Token *token = new Token(NULL, sourceFileInfo, V7);
    delete V7;
    specStar.push_back(new RuleOp(V4, V5, token, accept++));
}
rule ::= LESS_THAN STAR GREATER_THAN look newcond(V5) CODE.
{
    context_none(NULL);
    delete V5;
}
rule ::= LESS_THAN STAR GREATER_THAN look COLON newcond(V6).
{
    assert(V6);
    context_none(NULL);
    delete V6;
}
rule(V0) ::= NOCOND newcond(V2) CODE(V3).
{
    context_check(NULL);
    if (specNone)
    {
	in->fatal("code to handle illegal condition already defined");
    }
    Token *token = new Token(V3.token, V3.token->source, V3.token->line, V2);
    delete V2;
    delete V3.token;
    V0 = specNone = new RuleOp(new NullOp(), new NullOp(), token, accept++);
}
rule(V0) ::= NOCOND COLON newcond(V3).
{
    assert(V3);
    context_check(NULL);
    if (specNone)
    {
	in->fatal("code to handle illegal condition already defined");
    }
    Token *token = new Token(NULL, sourceFileInfo, V3);
    delete V3;
    V0 = specNone = new RuleOp(new NullOp(), new NullOp(), token, accept++);
}
rule ::= SETUP STAR GREATER_THAN CODE(V4).
{
    CondList *clist = new CondList();
    clist->insert("*");
    setup_rule(clist, V4.token);
}
rule ::= SETUP cond(V2) GREATER_THAN CODE(V4).
{
    setup_rule(V2, V4.token);
}

cond ::= /* empty */.
{
	in->fatal("unnamed condition not supported");
}
cond(V0) ::= clist(V1).
{
	V0 = V1;
}

clist(V0) ::= ID(V1).
{
    V0 = new CondList();
    V0->insert(V1.symbol->GetName().to_string());
}
clist(V0) ::= clist(V1) COMMA ID(V3).
{
    V1->insert(V3.symbol->GetName().to_string());
    V0 = V1;
}

newcond(V0) ::= /* empty */.
{
    V0 = NULL;
}
newcond(V0) ::= EQUALS GREATER_THAN ID(V3).
{
    V0 = new Str(V3.symbol->GetName().to_string().c_str());
}

look(V0) ::= /* empty */.
{
    V0 = new NullOp;
}
look(V0) ::= SLASH expr(V2).
{
    V0 = V2;
}

expr(V0) ::= diff(V1).
{
    V0 = V1;
}
expr(V0) ::= expr(V1) VERTICAL_BAR diff(V3).
{
    V0 = mkAlt(V1, V3);
}

diff(V0) ::= term(V1). [BACKSLASH]
{
    V0 = V1;
}
diff(V0) ::= diff(V1) BACKSLASH term(V3).
{
    V0 = mkDiff(V1, V3);
    if(!V0)
    {
	in->fatal("can only difference char sets");
    }
}

term(V0) ::= factor(V1).
{
    V0 = V1;
}
term(V0) ::= term(V1) factor(V2).
{
    V0 = new CatOp(V1, V2);
}

factor(V0) ::= primary(V1).
{
    V0 = V1;
}
factor(V0) ::= primary(V1) close(V2).
{
    switch(V2)
    {
    case '*':
	V0 = mkAlt(new CloseOp(V1), new NullOp());
	break;
    case '+':
	V0 = new CloseOp(V1);
	break;
    case '?':
	V0 = mkAlt(V1, new NullOp());
	break;
    }
}
factor(V0) ::= primary(V1) CLOSESIZE(V2).
{
    V0 = new CloseVOp(V1, V2.extop.minsize, V2.extop.maxsize);
}

close(V0) ::= CLOSE(V1).
{
    V0 = V1.op;
}
close(V0) ::= STAR(V1).
{
    V0 = V1.op;
}
close(V0) ::= close(V1) CLOSE(V2).
{
    V0 = (V1 == V2.op) ? V1 : '*';
}
close(V0) ::= close(V1) STAR(V2).
{
    V0 = (V1 == V2.op) ? V1 : '*';
}

primary(V0) ::= ID(V1).
{
    if(!V1.symbol->re)
    {
	in->fatal("can't find symbol");
    }
    V0 = V1.symbol->re;
}
primary(V0) ::= RANGE(V1).
{
    V0 = V1.regexp;
}
primary(V0) ::= STRING(V1).
{
    V0 = V1.regexp;
}
primary(V0) ::= LEFT_PAREN expr(V2) RIGHT_PAREN.
{
    V0 = V2;
}

%include {

void yyerror(const char* s)
{
	in->fatal(s);
}

int yylex() {
	return in ? in->scan() : 0;
}

void yyparse()
{
    void *parser;
    int tokenID;
    extern YYSTYPE yylval;

    parser = ParseAlloc(malloc);
    while ((tokenID = yylex()) != 0) {
	Parse(parser, tokenID, yylval);
    }
    Parse(parser, 0, yylval);
    ParseFree(parser, free);
}

namespace re2c
{

void parse(Scanner& i, std::ostream& o, std::ostream* h)
{
	std::map<std::string, DFA*>  dfa_map;
	ScannerState rules_state;

	in = &i;

	o << "/* Generated by re2c " PACKAGE_VERSION;
	if (!bNoGenerationDate)
	{
		o << " on ";
		time_t now = time(&now);
		o.write(ctime(&now), 24);
	}
	o << " */\n";
	o << sourceFileInfo;
	
	bool uFlagOld = uFlag;
	bool wFlagOld = wFlag;
	uint nRealCharsOld = nRealChars;
	
	while ((parseMode = i.echo()) != Scanner::Stop)
	{
		bool bPrologBrace = false;
		ScannerState curr_state;

		i.save_state(curr_state);
		foundRules = false;

		if (rFlag && parseMode == Scanner::Rules && dfa_map.size())
		{
			in->fatal("cannot have a second 'rules:re2c' block");
		}
		if (parseMode == Scanner::Reuse)
		{
			if (dfa_map.empty())
			{
				in->fatal("got 'use:re2c' without 'rules:re2c'");
			}
		}
		else if (parseMode == Scanner::Rules)
		{
			i.save_state(rules_state);
		}
		else
		{
			dfa_map.clear();
		}
		accept = 0;
		spec = NULL;
		in->set_in_parse(true);
		yyparse();
		in->set_in_parse(false);
		if (rFlag && parseMode == Scanner::Reuse)
		{
			uint nRealCharsLast = nRealChars;
			if (uFlag)
			{
				nRealChars = 0x110000; // 17 times w-Flag
			}
			else if (wFlag)
			{
				nRealChars = (1<<16); // 0x10000
			}
			else
			{
				nRealChars = (1<<8); // 0x100
			}
			if (foundRules || nRealCharsLast != nRealChars)
			{
				// Re-parse rules
				parseMode = Scanner::Parse;
				i.restore_state(rules_state);
				i.reuse();
				dfa_map.clear();
				parse_cleanup();
				spec = NULL;
				accept = 0;
				in->set_in_parse(true);
				yyparse();
				in->set_in_parse(false);

				// Now append potential new rules
				i.restore_state(curr_state);
				parseMode = Scanner::Parse;
				in->set_in_parse(true);
				yyparse();
				in->set_in_parse(false);
			}
			uFlagOld = uFlag;
			wFlagOld = wFlag;
			nRealCharsOld = nRealChars;
		}
		if (cFlag)
		{
			RegExpMap::iterator it;
			SetupMap::const_iterator itRuleSetup;

			if (parseMode != Scanner::Reuse)
			{
				if (!specStar.empty())
				{
					for (it = specMap.begin(); it != specMap.end(); ++it)
					{
						assert(it->second.second);
						for (RuleOpList::const_iterator itOp = specStar.begin(); itOp != specStar.end(); ++itOp)
						{
							it->second.second = mkAlt((*itOp)->copy(accept++), it->second.second);
						}
					}
				}
	
				if (specNone)
				{
					// After merging star rules merge none code to specmap
					// this simplifies some stuff.
					// Note that "0" inserts first, which is important.
					specMap["0"] = std::make_pair(0, specNone);
				}
				else
				{
					// We reserved 0 for specNone but it is not present,
					// so we can decrease all specs.
					for (it = specMap.begin(); it != specMap.end(); ++it)
					{
						it->second.first--;
					}
				}
			}

			size_t nCount = specMap.size();

			for (it = specMap.begin(); it != specMap.end(); ++it)
			{
				assert(it->second.second);

				if (parseMode != Scanner::Reuse)
				{
					itRuleSetup = ruleSetupMap.find(it->first);				
					if (itRuleSetup != ruleSetupMap.end())
					{
						yySetupRule = itRuleSetup->second.second;
					}
					else
					{
						itRuleSetup = ruleSetupMap.find("*");
						if (itRuleSetup != ruleSetupMap.end())
						{
							yySetupRule = itRuleSetup->second.second;
						}
						else
						{
							yySetupRule = "";
						}
					}
					dfa_map[it->first] = genCode(it->second.second);
					dfa_map[it->first]->prepare();
				}
				if (parseMode != Scanner::Rules && dfa_map.find(it->first) != dfa_map.end())
				{
					dfa_map[it->first]->emit(o, topIndent, &specMap, it->first, !--nCount, bPrologBrace);
				}
			}
			if (!h && !bTypesDone)
			{
				genTypes(typesInline, 0, specMap);
			}
		}
		else if (spec || !dfa_map.empty())
		{
			if (parseMode != Scanner::Reuse)
			{
				dfa_map[""] = genCode(spec);
				dfa_map[""]->prepare();
			}
			if (parseMode != Scanner::Rules && dfa_map.find("") != dfa_map.end())
			{
				dfa_map[""]->emit(o, topIndent, NULL, "", 0, bPrologBrace);
			}
		}
		o << sourceFileInfo;
		/* restore original char handling mode*/
		uFlag = uFlagOld;
		wFlag = wFlagOld;
		nRealChars = nRealCharsOld;
	}

	if (cFlag)
	{
		SetupMap::const_iterator itRuleSetup;
		for (itRuleSetup = ruleSetupMap.begin(); itRuleSetup != ruleSetupMap.end(); ++itRuleSetup)
		{
			if (itRuleSetup->first != "*" && specMap.find(itRuleSetup->first) == specMap.end())
			{
				in->fatalf_at(itRuleSetup->second.first, "setup for non existing rule '%s' found", itRuleSetup->first.c_str());
			}
		}
		if (specMap.size() < ruleSetupMap.size())
		{
			uint line = in->get_cline();
			itRuleSetup = ruleSetupMap.find("*");
			if (itRuleSetup != ruleSetupMap.end())
			{
				line = itRuleSetup->second.first;
			}
			in->fatalf_at(line, "setup for all rules with '*' not possible when all rules are setup explicitly");
		}
	}

	if (h)
	{
		genHeader(*h, 0, specMap);
	}
	
	parse_cleanup();
	in = NULL;
}

void parse_cleanup()
{
	RegExp::vFreeList.clear();
	Range::vFreeList.clear();
	Symbol::ClearTable();
	specMap.clear();
	specStar.clear();
	specNone = NULL;
}

} // end namespace re2c
} /* include */
