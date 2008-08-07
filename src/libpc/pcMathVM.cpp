/*                       P C M A T H V M . C P P
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
/** @file pcMathVM.cpp
 *
 * Math VM Method implementation
 *
 * @author Dawn Thomas
 */

#include "pcMathVM.h"

Stack::Stack(Stack const & s)
{
    copy(s.data);
}

void Stack::copy(Stack::container_t const & a)
{
    data.clear();
    Stack::container_t::const_iterator i = a.begin();
    Stack::container_t::const_iterator const end = a.end();
    for (; i != end; ++i)
	data.push_back(*i);
}

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
