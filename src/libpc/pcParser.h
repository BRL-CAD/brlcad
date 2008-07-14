/*                    P C P A R S E R . H
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
/** @addtogroup pcparser */
/** @{ */
/** @file pcParser.h
 *
 * EBNF Parser generation using boost:Spirit
 *
 * @author Dawn Thomas
 */
#ifndef __PCPARSER_H__
#define __PCPARSER_H__

#include "common.h"

#include <boost/spirit.hpp>

#include "bu.h"
#include "raytrace.h"
#include "tcl.h"
#include "pc.h"


/**
 * Grammar for the expression used for representing the constraint
 *
 */
class pcVariable_grammar : public boost::spirit::grammar<pcVariable_grammar>
{
    template<typename ScannerT>
    struct definition
    {
	boost::spirit::rule<ScannerT> Variable;
	boost::spirit::rule<ScannerT> Expression;
	definition(pcVariable_grammar const &self) {
	    Variable = '(' >> Expression >> ')';
	}
	boost::spirit::rule<ScannerT> const& start() { return Variable;}
    };
};

class pcConstraint_grammar : public boost::spirit::grammar<pcConstraint_grammar>
{
    template<typename ScannerT>
    struct definition
    {
	boost::spirit::rule<ScannerT> expr;
	boost::spirit::rule<ScannerT> group;
	definition(pcConstraint_grammar const &self) {
	    group = '(' >> expr >> ')';
	}
	boost::spirit::rule<ScannerT> const& start() { return group;}
    };
};

/**
 *  Various precompiled functors which are called during parsing depending
 *  on the constraint represented in the expression
 */

struct is_equal
{
    template<typename IteratorT>
    void operator() (IteratorT first,IteratorT last) const;
};

/**
 *
 * The Parser Object which takes the pc_pc_set as the input and performs
 * the following functions
 * 1. Generate Variables corresponding to pc_params
 * 2. Generate Constraint objects corresponding to pc_constrnt by parsing
 *    according to the pcexpression_grammar
 * 3. Return the set of Variables and Constraints/ Call the Generator
 */
class Parser {
private:
    int a;
public:
    void parse(struct pc_pc_set * pcs);
};

/* Parser method implementation */

void Parser::parse(struct pc_pc_set * pcs)
{
    /*Iterate through the parameter set first*/
    struct pc_param * par;
    struct pc_constrnt * con;
    while (BU_LIST_WHILE(par,pc_param,&(pcs->ps->l))) {
	std::cout<<"Parameter: "<<par->pname<<std::endl;
	BU_LIST_DEQUEUE(&(par->l));
	bu_free(par,"free parameter");
    }
    while (BU_LIST_WHILE(con,pc_constrnt,&(pcs->cs->l))) {
	std::cout<<"Constraint: "<<con->cname<<std::endl;
	BU_LIST_DEQUEUE(&(con->l));
	bu_free(con,"free constraint");
    }
}
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
