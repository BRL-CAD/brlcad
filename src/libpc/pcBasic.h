/*                       P C B A S I C . H
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
/** @addtogroup libpc */
/** @{ */
/** @file pcBasic.h
 *
 * Basic classes for Parametrics and Constraints Library
 *
 */
#ifndef __PCBASIC_H__
#define __PCBASIC_H__

#include "common.h"

#include <string>

/* Basic Exception Handling classes */

class pcException {
public:
    pcException() {};
    pcException(const char *temp) {str=temp;};
    ~pcException() {};
    std::string Error() const {
	return str;
    }
private:
    std::string str;
};


/* Structures for defining varios derived objects from
 * constrained_value template in Boost
 */

struct is_even {
    bool operator () (int i) const
    { return (i % 2) == 0; }
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
