/*                       P C M A T H L F . H
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
 */
#ifndef __PCMATHLF_H__
#define __PCMATHLF_H__

#include "pcMathVM.h"

#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/phoenix1.hpp>

#include <boost/shared_ptr.hpp>

#include <string>

/**
 * Lazy function wrapper for boost::spirit::symbols::add()
 */
struct addsymbol_impl {
    typedef boost::shared_ptr<MathFunction> FunctionPtr;
    typedef boost::spirit::classic::symbols<FunctionPtr> FunctionTable;

    template<typename T, typename Arg1, typename Arg2 = boost::spirit::classic::nil_t>
    struct result
    {
	typedef void type;
    };
    template <typename SymbolT>
    void operator()(SymbolT & symbols, std::string const & name) const
    {
	symbols.add(name.c_str());
    }

    void operator()(FunctionTable & symbols, std::string const & name,
		    UserFunction const & func) const
    {
	FunctionPtr *fp = find(symbols, name.c_str());
	if (fp)
	    fp->reset(new UserFunction(func));
	else
	    symbols.add(name.c_str(), FunctionPtr(new UserFunction(func)));
    }

};
phoenix::function<addsymbol_impl> const addsymbol = addsymbol_impl();

/**
 * Lazy function wrapper for boost::spirit::classic::symbols::find()
 */
struct findsymbol_impl {
    template <typename T, typename Arg>
    struct result
    {
	typedef typename T::symbol_data_t *type;
    };

    template <typename SymbolT>
    typename result<SymbolT, std::string>::type
    operator()(SymbolT const & symbols, std::string const & symbol) const
    {
	return find(symbols, symbol.c_str());
    }
};
phoenix::function<findsymbol_impl> const findsymbol = findsymbol_impl();

/**
 * Lazy function wrapper for Stack::push_back & generic Container::push_back
 */
struct push_back_impl {
    template <typename T, typename Arg>
    struct result
    {
	typedef void type;
    };
    void operator()(Stack & stack, Node *n) const
    {
	BOOST_ASSERT(n);
	stack.push_back(n);
    }
    template<typename Container>
    void operator()(Container & c, typename Container::value_type const & data) const
    {
	c.push_back(data);
    }
};
phoenix::function<push_back_impl> const push_back = push_back_impl();

/**
 * Lazy function wrapper for T::size()
 */
struct size_impl {
    template <typename T>
    struct result {
	typedef std::size_t type;
    };
    template <typename T>
    std::size_t operator()(T const & t) const
    {
    	return t.size();
    }
};
phoenix::function<size_impl> const size = size_impl();

/**
 * Lazy function wrapper for boost::shared_ptr<T>::reset
 */
struct reset_impl {
    template <typename T1, typename T2>
    struct result {
	typedef void type;
    };
    template <typename T>
    void operator()(boost::shared_ptr<T> *shptr, T *ptr = 0) const
    {
	shptr->reset(ptr);
    }
};
phoenix::function<reset_impl> const reset = reset_impl();


/**
 * Lazy function wrapper for boost::reference_wrapper<double>::get_pointer.
 */
struct address_of_impl {
    template <typename T>
    struct result {
	typedef double *type;
    };
    double *const operator()(boost::reference_wrapper<double> const & ref) const
    {
	return ref.get_pointer();
    }
};
phoenix::function<address_of_impl> const address_of = address_of_impl();

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
