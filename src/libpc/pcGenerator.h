/*                    P C G E N E R A T O R . H
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
/** @addtogroup pcparser */
/** @{ */
/** @file pcGenerator.h
 *
 * Constraint Network Generator Class as well as various functors used
 * in the generation of constraints and parameters/variables
 *
 */
#ifndef __PCGENERATOR_H__
#define __PCGENERATOR_H__

#include "common.h"

#include "pcVCSet.h"
#include <string>

#include "pc.h"

/* Functors associated with the generation of Variables */
namespace Generators {

struct varname
{
public:
    varname(VCSet &vcs) : vcset(vcs) {}
    void operator () (char c) const;
private:
    VCSet &vcset;
};


struct varvalue
{
public:
    varvalue(VCSet &vcs) : vcset(vcs) {}
    void operator () (double v) const;
private:
    VCSet &vcset;
};


/**
 * Various precompiled functors which are called during parsing depending
 * on the constraint represented in the expression
 */

struct is_equal
{
    template<typename IteratorT>
    void operator() (IteratorT first, IteratorT last) const;
};
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
