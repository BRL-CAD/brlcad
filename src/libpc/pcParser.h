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

#include <boost/spirit.hpp>
using namespace boost::spirit;
/**
 * Grammar for the expression used for representing the constraint
 *
 */
struct pcexpression_grammar : public grammar<pcexpression_grammar>
{
    template<typename ScannerT>
    struct definition
    {
	rule<ScannerT> expr;
    	rule<ScannerT> group;
	definition(pcexpression_grammar const &self) {
	    group = '(' >> expr >> ')';
	}
	rule<ScannerT> const& start() { return group;}
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
