/*                 P C M A T H G R A M M A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
 * 	1. Stack closure inclusion
 *	2. Expression grammar definiton
 *	3. Variable grammar completion
 *	*. lazy function evaluation
 * @author Dawn Thomas
 */
#ifndef PC_MATH_GRAMMAR
#define PC_MATH_GRAMMAR

#include "yac_stack_closure.hpp"

#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/core.hpp>
#include <boost/spirit/phoenix.hpp>
#include <boost/spirit/symbols/symbols.hpp>

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
	typedef typename boost::spirit::rule<T> rule_t;
	rule_t const & start() const { return name;}
	private:
	    rule_t name;
    };
};

struct ExpressionGrammar : public boost::spirit::classic::grammar<ExpressionGrammar>
{
};

struct VariableGrammar : public boost::spirit::classic::grammar<VariableGrammar>
{
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
