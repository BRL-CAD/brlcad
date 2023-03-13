#ifndef _parser_symbol_h
#define _parser_symbol_h

#include "scanner.h"
#include "re.h"
#include <iosfwd>

namespace re2c
{

class Symbol
{
public:

	RegExp*   re;

	static Symbol *find(const SubStr&);
	static void ClearTable();

	typedef std::map<std::string, Symbol*> SymbolTable;
	
	const Str& GetName() const
	{
		return name;
	}

protected:

	Symbol(const SubStr& str)
		: re(NULL)
		, name(str)
	{
	}

private:

	static SymbolTable symbol_table;

	Str	name;

#if defined(PEDANTIC) && (PEDANTIC>0)
	Symbol(const Symbol& oth)
		: re(oth.re)
		, name(oth.name)
	{
	}
	Symbol& operator = (const Symbol& oth)
	{
		new(this) Symbol(oth);
		return *this;
	}
#endif
};

extern void parse(Scanner&, std::ostream&, std::ostream*);
extern void parse_cleanup();

} // end namespace re2c
 
union YYSTYPE {
    re2c::Symbol   *symbol;
    re2c::RegExp   *regexp;
    re2c::Token    *token;
    char            op;
    int             number;
    re2c::ExtOp     extop;
    re2c::Str      *str;
    re2c::CondList *clist;
};

void *ParseAlloc(void *(*mallocProc)(size_t));
void Parse(void *parser, int tokenID, YYSTYPE value);
void ParseFree(void *parser, void (*freeProc)(void*));

#endif
