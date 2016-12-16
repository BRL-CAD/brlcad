/*                      N A M E G E N . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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

#ifndef BU_NAMEGEN_H
#define BU_NAMEGEN_H

#include "common.h"
#include "bu/defines.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_namegen
 * @brief
 * Sequential name generation.
 *
 * A problem frequently encountered when generating names is generating names
 * that are in some sense structured but avoid colliding.  For example, given a
 * geometry object named:
 *
 * engine_part.s
 *
 * an application wanting to make multiple copies of engine_part.s
 * automatically might want to produce a list of names such as:
 *
 * engine_part.s-1 engine_part.s-2 engine_part.s-3 ...
 *
 * However, it is equally plausible that the desired pattern might be:
 *
 * engine_part_001.s engine_part_002.s engine_part_003.s ...
 *
 * This module implements an engine for generating the "next" name in a
 * sequence, given a pattern and a state definition.  The state definition is
 * opaque to the user and is used internally to define a notion of current and
 * next names in the sequence.
 *
 * TODO - describe how to specify regex expressions for matching - will use ()
 * subexpressions.
 *
 * The caller may optionally specify incrementation behavior with an incrementer
 * specification string, which has the following form:
 *
 * minwidth:init:max:step[:left_sepchar:right_sepchar]
 *
 * In the case of spacing, init, min, max, and step a '0' always indicates default
 * unspecified behavior.  So, an incrementer specified as 0:0:0:0 would behave
 * as follows:
 *
 * spacing: standard printf handling of number value
 * init: 0
 * max: LONG_MAX
 * step: 1
 *
 * The last two separator chars are optional - they may be specified
 * if the user wants to guarantee a separation between the active incremented
 * substring and its surroundings.  If the user wants to use a colon *as* the
 * separator character, they need to quote it with the '\' character.  For
 * example, the following would prefix the incrementer output with a colon
 * and add a suffix with a dash:
 *
 * 2:1:50:5:\::-
 *
 * If no specifier is passed in, bu_namegen will analyze the string to
 * determine the minimum width of the incrementer string.  So, for example,
 * engine_part-001.s would by default be incremented to engine_part-002.s, and
 * so on to maintain an incrementer string width of at least 3.
 */
/** @{ */
/** @file bu/namegen.h */

BU_EXPORT extern int bu_namegen(struct bu_vls *name, const char *regex_str, const char *incr_spec);


__END_DECLS

#endif  /* BU_NAMEGEN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
