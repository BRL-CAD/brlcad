/*                       P C M A T H L F . H
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
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
/** @addtogroup libpc */
/** @{ */
/** @file pcMathLF.h
 *
 * Lazy Function wrappers for Math VM
 *
 * @author Dawn Thomas
 */
#ifndef __PCMATHLF_H__
#define __PCMATHLF_H__

#include "pcMathVM.h"

#include <boost/spirit/symbols/symbols.hpp>

#include <boost/spirit/phoenix/functions.hpp>
#include <boost/shared_ptr.hpp>

/**
 * Lazy function wrapper for boost::spirit::symbols::add()
 */
struct addsymbol_impl {
	void operator()()
	{}
};
boost::phoenix::function<addsymbol_impl> const addsymbol = addsymbol_impl();

/**
 * Lazy function wrapper for boost::spirit::symbols::find()
 */
struct findsymbol_impl {
};
boost::phoenix::function<findsymbol_impl> const findsymbol = findsymbol_impl();

/**
 * Lazy function wrapper for T::size()
 */
struct size_impl {
    template <typename T>
    /* struct result {
	typedef std::size_t type;
    };*/
    template <typename T>
    std::size_t operator()(T const & t) const
    {
    	return t.size();
    }
};
boost::phoenix::function<size_impl> const size = size_impl();

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
