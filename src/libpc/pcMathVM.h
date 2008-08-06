/*                       P C M A T H V M . H
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
/** @file pcMathVM.h
 *
 * Math Virtual Machine for evaluating arbitrary math expressions
 *
 * @author Dawn Thomas
 */
#ifndef __PCMATHVM_H__
#define __PCMATHVM_H__

#include "common.h"

#include <boost/shared_ptr.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <list>

struct node {
    virtual ~node() {}
};

class Stack
{
    typedef std::list<boost::shared_ptr<node> > container_t;
    typedef boost::indirect_iterator<container_t::iterator> iterator;
public:
    iterator begin();
    iterator end();
private:
    container_t data;    
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
