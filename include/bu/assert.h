/*                       A S S E R T . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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

/** @addtogroup bu_defines */
/** @{ */

#ifndef BU_ASSERT_H
#define BU_ASSERT_H

#include "common.h"

#include "bu/defines.h"
#include "bu/log.h"
#include "bu/exit.h"


/**
 * @def BU_ASSERT(expression)
 * @brief Alternative for assert(3) that calls LIBBU logging+bombing.
 *
 * This is a simple macro wrapper that logs an assertion-failure error
 * message and aborts application execution if the specified
 * expression is not hold true.  While it is similar in use, this
 * wrapper does not utilize assert(3) or NDEBUG but is disabled if
 * NO_BOMBING_MACROS is defined by the configuration.
 */
#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT(expression_) (void)(expression_)
#else
#  define BU_ASSERT(expression_)	\
    if (UNLIKELY(!(expression_))) { \
	const char *expression_buf_ = #expression_; \
	bu_log("BU_ASSERT(%s) failure in file %s, line %d\n", \
	       expression_buf_, __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT FAILED\n"); \
    }
#endif

/** @} */

#endif  /* BU_ASSERT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
