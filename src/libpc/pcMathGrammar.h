/*                 P C M A T H G R A M M A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file pcMathGrammar.cpp
 *
 * Math Grammar
 *
 * Plan:
 *	1. Expression grammar definiton
 *	2. Variable grammar completion
 *	3. lazy function evaluation
 * @author Dawn Thomas
 */
#ifndef PC_MATH_GRAMMAR
#define PC_MATH_GRAMMAR

#include "pcMathVM.h"

#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/core.hpp>
#include <boost/spirit/attribute/closure.hpp>
#include <boost/spirit/phoenix.hpp>
#include <boost/spirit/symbols/symbols.hpp>

/**
 * 				boost::spirit::closure
 * a nifty implementation of the closure concept, needed in our case
 * since the grammar would be reentrant. More details can be accessed
 * at ( or the latest boost documentation )
 * http://www.boost.org/doc/libs/1_36_0/libs/spirit/classic/doc/closures.html 
 */
struct StackClosure : boost::spirit::closure<StackClosure, Stack>
{
    member1 stack;
};

class NameGrammar : public boost::spirit::classic::grammar<NameGrammar
{
    boost::spirit::symbols<char> dummy_reserved_keywords;
public:
    static boost::spirit::symbols<char> reserved_keywords;
    boost::spirit::symbols<char> & keywords;

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
	typedef typename boost::spirit::rule<ScannerT> rule_t;
	rule_t const & start() const { return name;}
	private:
	    rule_t name;
    };
};

boost::spirit::symbols<char> NameGrammar::reserved_keywords;
/** Different types of closures */

struct FuncExprClosure : boost spirit::closure<FuncExprClosure, Stack, std::string, int, boost::shared_ptr<MathFunction>
{
    member1 stack;
    member2 name;
    member3 arity;
    member4 function_ptr;
};

struct LogicalClosure : boost::spirit::closure<LogicalClosure, Stack, bool>
{
    member1 stack;
    member2 or_op;
};


struct ConditionalClosure : boost::spirit::closure<ConditionalClosure, Stack, Stack, Stack>
{
    member1 stack;
    member2 stack1;
    member3 stack2;
};

/**
 * ExpressionGrammar implementation
 * Stack closure is attached to the grammar itself
 */
struct ExpressionGrammar : public boost::spirit::classic::grammar<ExpressionGrammar,StackClosure::context_t>
{
};

/**
 * VariableGrammar implementation
 * Stack closure is attached to the grammar itself
 */
struct VariableGrammar : public boost::spirit::classic::grammar<VariableGrammar, StackClosure::context_t>
{
    typedef boost::spirit::symbols<boost::shared_ptr<MathFunction> > FunctionTable;
    typedef boost::spirit::symbols<double> VarTable;

    FunctionTable functions;
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
	}
	boost::spirit::classic::rule<ScannerT> const & start const {}
    };
};


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
