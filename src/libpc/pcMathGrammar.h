/*                   P C M A T H G R A M M A R . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @addtogroup pcsolver */
/** @{ */
/** @file pcMathGrammar.h
 *
 * Math Grammar
 *
 * Plan:
 *	1. Expression grammar definiton
 *	2. Variable grammar completion
 *	3. lazy function evaluation
 *
 */
#ifndef PC_MATH_GRAMMAR
#define PC_MATH_GRAMMAR
#define PHOENIX_LIMIT 6

#include "pcMathLF.h"
#include "pcMathVM.h"

#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/phoenix1.hpp>
#include <boost/spirit/include/classic_lazy.hpp>
#include <boost/spirit/include/classic_if.hpp>

#include <string>
#include <vector>

/*#include <boost/spirit/core.hpp>
  #include <boost/spirit/phoenix.hpp>
  #include <boost/spirit/utility/functor_parser.hpp>
  #include <boost/spirit/symbols/symbols.hpp>
  #include <boost/spirit/attribute/closure.hpp>
  #include <boost/spirit/utility/lists.hpp>
  #include <boost/spirit/dynamic/if.hpp>
  #include <boost/spirit/phoenix/functions.hpp>
*/

/**
 * boost::spirit::classic::closure
 * a nifty implementation of the closure concept, needed in our case
 * since the grammar would be reentrant. More details can be accessed
 * at (or the latest boost documentation)
 * http://www.boost.org/doc/libs/1_36_0/libs/spirit/classic/doc/closures.html 
 */
struct StackClosure : boost::spirit::classic::closure<StackClosure, Stack>
{
    member1 stack;
};


class NameGrammar : public boost::spirit::classic::grammar<NameGrammar>
{
    boost::spirit::classic::symbols<char> dummy_reserved_keywords;
public:
    static boost::spirit::classic::symbols<char> reserved_keywords;
    boost::spirit::classic::symbols<char> & keywords;

    NameGrammar(bool checkreserved = true)
    	: keywords(checkreserved ? reserved_keywords : dummy_reserved_keywords)
    {}
    template <typename ScannerT>
    struct definition {
	definition(NameGrammar const & self) {
	    name = boost::spirit::classic::lexeme_d
		[
		    ((boost::spirit::classic::alpha_p|'_')
		     >> *(boost::spirit::classic::alnum_p | '_'))
		    ]
		- self.keywords
		;
	}
	typedef typename boost::spirit::classic::rule<ScannerT> rule_t;
	rule_t const & start() const { return name;}
    private:
	rule_t name;
    };
};


boost::spirit::classic::symbols<char> NameGrammar::reserved_keywords;

/** Finding the math function using the name */

boost::shared_ptr<MathFunction>
checked_find(boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > const & symbols, std::string const & name)
{
    boost::shared_ptr<MathFunction> *ptr = find(symbols, name.c_str());
    BOOST_ASSERT(ptr && ptr->get());
    return *ptr;
}


struct checked_find_impl {
    template <typename T1, typename T2>
    struct result {
	typedef boost::shared_ptr<MathFunction> type;
    };
    boost::shared_ptr<MathFunction> operator()(boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > const & symbols, \
					       std::string const & name) const
    {
	boost::shared_ptr<MathFunction> *ptr = find(symbols, name.c_str());
	return ptr ? *ptr: boost::shared_ptr<MathFunction>();
    }
};


phoenix::function<checked_find_impl> const checked_find_ = checked_find_impl();

/** Different types of closures */
struct FuncExprClosure 
    : boost::spirit::classic::closure<FuncExprClosure,
			     Stack, std::string, int, boost::shared_ptr<MathFunction> >
{
    member1 stack;
    member2 name;
    member3 arity;
    member4 function_ptr;
};


struct LogicalClosure : boost::spirit::classic::closure<LogicalClosure, Stack, bool>
{
    member1 stack;
    member2 or_op;
};


struct ConditionalClosure : boost::spirit::classic::closure<ConditionalClosure, Stack, Stack, Stack>
{
    member1 stack;
    member2 stack1;
    member3 stack2;
};


/**
 * ExpressionGrammar implementation
 * Stack closure is attached to the grammar itself
 */

struct ExpressionGrammar : public boost::spirit::classic::grammar<ExpressionGrammar, StackClosure::context_t>
{
    typedef boost::shared_ptr<MathFunction> FunctionPointer;
    typedef boost::spirit::classic::symbols<FunctionPointer> FunctionTable;
    typedef boost::spirit::classic::symbols<double> VarTable;

    VarTable const dummy_local_vars;
    FunctionTable const & functions;
    VarTable const & global_vars;
    VarTable const & local_vars;

    ExpressionGrammar(FunctionTable const & funcs, VarTable const & gvars)
    	: functions(funcs), global_vars(gvars), local_vars(dummy_local_vars)
    {}

    ExpressionGrammar(FunctionTable const & funcs, VarTable const & gvars, VarTable const & lvars)
    	: functions(funcs), global_vars(gvars), local_vars(lvars)
    {}

    template <typename ScannerT>
    struct definition
    {
    	definition(ExpressionGrammar const & self)
	    : name(false)
	{
	    using phoenix::arg1;
	    using phoenix::arg2;
	    using phoenix::construct_;
	    using phoenix::if_;
	    using phoenix::new_;
	    using phoenix::var;

	    boolean_op.add
	    	("&&", false)
		("||", true);
	    add_op.add
	    	("+", checked_find(self.functions, "add"))
		("-", checked_find(self.functions, "subtract"));
	    bitwise_op.add
		("&", checked_find(self.functions, "bitwise_and"))
		("|", checked_find(self.functions, "bitwise_or"))
		("^", checked_find(self.functions, "bitwise_xor"));
	    compare_op.add
		("<", checked_find(self.functions, "less"))
		(">", checked_find(self.functions, "greater"))
		("<=", checked_find(self.functions, "leq"))
		(">=", checked_find(self.functions, "geq"));
	    equality_op.add
		("==", checked_find(self.functions, "equal"))
		("+", checked_find(self.functions, "notequal"));
	    mult_op.add
		("*", checked_find(self.functions, "multiply"))
		("/", checked_find(self.functions, "divide"))
		("%", checked_find(self.functions, "mod"));
	    shift_op.add
		("<<", checked_find(self.functions, "lshift"))
		(">>", checked_find(self.functions, "rshift"));
	    unary_op.add
		("+", FunctionPointer())
		("-", checked_find(self.functions, "negate"))
		("!", checked_find(self.functions, "logical_not"));
	    top
	    	= expr[self.stack = arg1];
	    expr
	    	= logical_expr[expr.stack = arg1]
		    >> !conditional_expr_helper[expr.stack +=arg1];
	    conditional_expr_helper
	    	= (boost::spirit::classic::ch_p('?')
		   >> expr
		   [
		       conditional_expr_helper.stack1 = arg1
		       ]
		   >> ':'
		   >> expr
		   [
		       conditional_expr_helper.stack2 = arg2
		       ]
		   >> !conditional_expr_helper
		   [
		       conditional_expr_helper.stack2 +=arg1
		       ]
		    )
		[
		    push_back(conditional_expr_helper.stack,
			      new_<BranchNode>(conditional_expr_helper.stack1,
					       conditional_expr_helper.stack2))
		    ]
		;
	    logical_expr
	    	= bitwise_expr
		[
		    logical_expr.stack = arg1
		    ]
			>> *(logical_expr_helper
			     [
				 logical_expr.stack +=arg1
				 ]
			    )
		;
	    logical_expr_helper
	    	= boolean_op
		[
		    logical_expr_helper.or_op = arg1
		    ]
			>> bitwise_expr
		[
		    if_(logical_expr_helper.or_op)
		    [
			push_back(logical_expr_helper.stack, new_<OrNode>(arg1))
			]
		    .else_
		    [
			logical_expr_helper.stack = arg1,
			push_back(logical_expr_helper.stack,
				  new_<sysFunctionNode>(checked_find(self.functions, "logical_and")))
			]
		    ]
		;
	    bitwise_expr
	    	= binary_op_p(equality_expr, bitwise_op)[bitwise_expr.stack = arg1];
	    equality_expr
	    	= binary_op_p(compare_expr, equality_op)[equality_expr.stack = arg1];
	    compare_expr
	    	= binary_op_p(shift_expr, compare_op)[compare_expr.stack = arg1];
	    shift_expr
	    	= binary_op_p(add_expr, shift_op)[shift_expr.stack = arg1];
	    add_expr
	    	= binary_op_p(mult_expr, add_op)[add_expr.stack = arg1];
	    mult_expr
	    	= binary_op_p(expr_atom, mult_op)[mult_expr.stack = arg1];
	    expr_atom
	    	= number
		[
		    expr_atom.stack = arg1
		    ]
		| func
		[
		    expr_atom.stack = arg1
		    ]
		| ('(' >>
		   expr
		   [
		       expr_atom.stack = arg1
		       ]
		   >> ')')
		| unary_expr
		[
		    expr_atom.stack = arg1
		    ]
		;
	    unary_expr
		= unary_op
		[
		    unary_expr.function_ptr = arg1
		    ]
			>> expr_atom
		[
		    unary_expr.stack = arg1,
		    if_(unary_expr.function_ptr != FunctionPointer())
		    [
			push_back(unary_expr.stack,
				  new_<sysFunctionNode>(unary_expr.function_ptr))
			]
		    ]
		;

	    number
		= boost::spirit::classic::real_p
		[
		    push_back(number.stack, new_<ConstantNode>(arg1))
		    ]
		| self.local_vars
		[
		    push_back(number.stack,
			      new_<VariableNode>(address_of(arg1)))
		    ]
		| self.global_vars
		[
		    push_back(number.stack,
			      new_<VariableNode>(address_of(arg1)))
		    ]
		;
	    func
		= name
		[
		    func.arity = 0,
		    func.name = construct_<std::string>(arg1, arg2)
		    ]
		>> ('(' >> !boost::spirit::classic::list_p(arg, ', ') >> ')')
		[
		    func.function_ptr
		    = checked_find_(var(self.functions),
				    mangled_name(func.name, func.arity))
		    ]
		>> boost::spirit::classic::if_p(func.function_ptr != FunctionPointer())
		[
		    boost::spirit::classic::epsilon_p
		    [
			push_back(func.stack,
				  new_<sysFunctionNode>(func.function_ptr))
			]
		    ]
		.else_p[boost::spirit::classic::nothing_p]
		;
	    arg = expr
		[
		    func.arity +=1,
		    func.stack += arg1
		    ]
		;
	}

	typedef boost::spirit::classic::rule<ScannerT> RuleT;
	RuleT const & start() const { return top; }
    private:
    	typedef boost::spirit::classic::rule<ScannerT, StackClosure::context_t> SRuleT;
    	typedef boost::spirit::classic::rule<ScannerT, FuncExprClosure::context_t> FRuleT;
    	typedef boost::spirit::classic::rule<ScannerT, LogicalClosure::context_t> LRuleT;
    	typedef boost::spirit::classic::rule<ScannerT, ConditionalClosure::context_t> CRuleT;

	RuleT arg, top;
	SRuleT add_expr, and_expr, bitwise_expr, compare_expr, equality_expr, expr, expr_atom,
		logical_expr, number, or_expr, or_op, mult_expr, shift_expr;
	FRuleT unary_expr, func;
	CRuleT conditional_expr_helper;
	LRuleT logical_expr_helper;
	NameGrammar name;
	boost::spirit::classic::symbols<bool> boolean_op;
	FunctionTable and_op, add_op, bitwise_op, compare_op, equality_op, \
		      shift_op, mult_op, unary_op;
    };
};


/**
 *
 * FunctionGrammar implementation
 * Function closure is attached to the grammar itself
 */
struct FunctionClosure : boost::spirit::classic::closure<FunctionClosure, std::string, std::vector<std::string>, boost::shared_ptr<boost::spirit::classic::symbols<double> >, std::string, boost::shared_ptr<ExpressionGrammar> >
{
   member1 name;
   member2 args;
   member3 local_vars;
   member4 expr;
};


struct FunctionGrammar : public boost::spirit::classic::grammar<FunctionGrammar, StackClosure::context_t>
{
    boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > & functions;
    boost::spirit::classic::symbols<double> const & globalvars;

    FunctionGrammar(boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > & f, boost::spirit::classic::symbols<double> const & v) : functions(f), globalvars(v)
    {}

    template <typename ScannerT>
    struct definition {
	definition(FunctionGrammar const & self)
	{
	    using phoenix::arg1;
	    using phoenix::arg2;
	    using phoenix::construct_;
	    using phoenix::new_;
	    using phoenix::var;

	    top = funcdef;

	    funcdef
	    	=  funcdecl
		   [
		   	add_symbol(var(self.functions), funcdef.name, \
					construct_<UserFunction>(\
						funcdef.name, \
						size(funcdef.args))),
			/* create expression grammar for the function */
			reset(funcdef.expr, new_<ExpressionGrammar>(
						var(self.functions), \
						var(self.globalvars), \
						*funcdef.localvars))
		   ]
		>> '='
		>> boost::spirit::classic::lazy_p(*funcdef.expr)
		   [
		   	/* add node to the stack */
			push_bakc(self.stack, \
				new_<FuncDefNode>(
					*find_symbol(var(self.functions), funcdef.name), \
					construct_<UserFuncExpression>(funcdef.args, funcdef.localvars, arg1)))
		   ]
		;
	    funcdecl
	    	=  name
		   [
		   	funcdef.name = construct_<std::string>(arg1, arg2)
		   ]
		>> boost::spirit::classic::ch_p('(')
		   [
		   	reset(funcdef.localvars, new_<boost::spirit::classic::symbols<double> >())
		   ]
		>> !boost::spirit::classic::list_p((name - lazy_p(*funcdef.localvars))
			   [
			   	push_back(funcdef.args,
					construct_<std::string>(arg1, arg2)),
				add_symbol(*funcdef.localvars,
					construct_<std::string>(arg1, arg2))
			   ]
			, ', ')
		>> ')'
		;
	}
	boost::spirit::classic::rule<ScannerT> const & start() const { return top; }
    private:
        boost::spirit::classic::rule<ScannerT> top, funcdecl;
    	boost::spirit::classic::rule<ScannerT, FunctionClosure::context_t> funcdef;
	NameGrammar name;
    };
};
/**
 * VariableGrammar implementation
 * Stack closure is attached to the grammar itself
 */
struct VariableClosure : boost::spirit::classic::closure<VariableClosure, std::string, Stack>
{
    member1 name;
    member2 stack;
};


struct VariableGrammar : public boost::spirit::classic::grammar<VariableGrammar, StackClosure::context_t>
{
    typedef boost::spirit::classic::symbols<boost::shared_ptr<MathFunction> > FunctionTable;
    typedef boost::spirit::classic::symbols<double> VarTable;

    FunctionTable const & functions;
    VarTable & variables;

    VariableGrammar(FunctionTable const & f, VarTable & v)
        : functions(f), variables(v)
    {}

    template <typename ScannerT>
    struct definition
    {
	definition(VariableGrammar const & self)
	    : expression(self.functions, self.variables)
	{
	using phoenix::arg1;
	using phoenix::arg2;
	using phoenix::construct_;
	using phoenix::if_;
	using phoenix::new_;
	using phoenix::var;
	top = step2;

	/** Parse and perform the assignment of type "a=4". Add the symbols
	    the self variable table and generate the stack representation */
	step2 =
	    step1
	    [
		if_(findsymbol(var(self.variables), step2.name) == (double*) 0)
		[
		    addsymbol(var(self.variables), step2.name)
		    ]
		]
	    [
		push_back(self.stack,
			  new_<VariableNode>(findsymbol(var(self.variables), step2.name))),
		self.stack += step2.stack,
		push_back(self.stack, new_<AssignNode>())
		]
	    ;
	step1 =
	    name
	    [
		step2.name = construct_<std::string>(arg1, arg2)
		]
		    >> '='
		    >> expression
	    [
		step2.stack = arg1
		]
	    ;
	}
	typename boost::spirit::classic::rule<ScannerT> const & start() const { return top; }
    private:
	typedef typename boost::spirit::classic::rule<ScannerT, VariableClosure::context_t> VarRuleT;
	boost::spirit::classic::rule<ScannerT> top, step1;
	VarRuleT step2;

	NameGrammar name;
	ExpressionGrammar expression;
    };
};

#endif

/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
