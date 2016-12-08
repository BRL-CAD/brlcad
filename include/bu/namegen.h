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
 * An incrementer specification has the following form:
 *
 * d[width:init:max:step[:left_sepchar:right_sepchar]]
 * u[left_sepchar:right_sepchar]
 *
 * d = sequential digit sequence (0-9, 10-99, 100-999, etc.)
 * u = UUID
 *
 * In the case of spacing, init, min, max, and step a '0' always indicates default
 * unspecified behavior.  So, an incrementer specified as d:0:0:0:0 would behave
 * as follows:
 *
 * spacing: standard printf handling of number value
 * init: 0
 * max: LONG_MAX
 * step: 1
 *
 * These modifiers are optional, but their presence is an all or nothing
 * proposition - their absence will result in the default behavior of the
 * sequence type.  A partial sequence of modifiers will result in a fatal
 * error.  The last two separator chars *are* optional - they may be specified
 * if the user wants to guarantee a separation between the active incremented
 * substring and its surroundings.  If the user wants to use a colon *as* the
 * separator character, they need to quote it with the '\' character.  For
 * example, the following would prefix the incrementer output with a colon
 * and add a suffix with a dash:
 *
 * #d:2:1:50:5:\::-
 *
 * If multiple incrementers are present in an template, they will be
 * incremented in least significant(right) to most significant(left) order.  So
 * the incrementer closest to the end of the template will be exhausted before
 * the one to the left of it is incremented, and once the one to the left is
 * incremented the least significant incrementer resets and begins again.
 *
 * For example, a "digital clock" incrementer would be specified using three
 * incrementers as follows:
 *
 * const char *incrs[4];
 * const char *hrs = "d:0:1:12:0";
 * const char *min = "d:2:0:59:0";
 * const char *sec = "d:2:0:59:0";
 * incrs[0] = hrs;
 * incrs[1] = min;
 * incrs[2] = sec;
 * incrs[3] = NULL;
 *
 * and the state of each increment would be held in an array:
 * unsigned long incr_states[3] = {0};
 *
 * The "input string" to set up the formatting would be:
 *
 * const char *clock_str = "00:00:00"
 *
 * and the regular expression to identify the incrementers would be:
 *
 * const char *regexp = "([0-9]+):([0-9]+):([0-9]+).*($)";
 *
 * If we wanted a clock with 24 hour counting, we could change the hrs specifier
 * to:
 *
 * const char *hrs = "d:2:0:23:0";
 *
 */
/** @{ */
/** @file bu/namegen.h */

BU_EXPORT extern int bu_namegen(struct bu_vls *name, const char *in_str, const char *regex_str, const char **incrs, long *incr_states);


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
