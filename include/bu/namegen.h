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
   @brief
   Sequential name generation.
  
   A problem frequently encountered when generating names is generating names
   that are in some sense structured but avoid colliding.  For example, given a
   geometry object named:

   @verbatim
                             engine_part.s
   @endverbatim

   an application wanting to make multiple copies of engine_part.s
   automatically might want to produce a list of names such as:

   @verbatim
             engine_part.s-1, engine_part.s-2, engine_part.s-3, ...
   @endverbatim

   However, it is equally plausible that the desired pattern might be:

   @verbatim
          engine_part_0010.s, engine_part_0020.s, engine_part_0030.s, ...
   @endverbatim

   The logic in this module attempts to provide a practical and general solution
   to the problem of automatic name generation.
*/
/** @{ */
/** @file bu/namegen.h */

/**
   This function implements an engine for generating the "next" name in a
   sequence, given an initial name supplied by the caller and (optionally)
   information to identify the incrementor in the string and the incrementing
   behavior desired.  bu_namegen does not track any "state" for individual
   strings - all information is contained either in the current state of the
   input string itself or the incrementation specifier (more details on the
   latter can be found below.)

   @param[in,out] name     Contains the "seed" string for the name generation.  Upon return the old string is cleared and the new one resides in name
   @param[in]     regex_str  Optional - user supplied regular expression for locating the incrementer substring.
   @param[in]     incr_spec  Optional - string of colon separated parameters defining function behavior.

   \section bu_namegen_regexp Incrementer Substring Identification

   bu_namegen uses regular expressions to identify the numerical part of a
   supplied string.  By default, if no regular expression is supplied by the
   caller, bu_namegen will use a numerical sequence at the end of the string
   (more precisely, it will use the last sequence of numbers if and only if
   there are no non-numerical characters between that sequence and the end of
   the string.) If no appropriate match is found, the incrementer will be
   appended to the end of the string.

   When no pre-existing number is found to start the sequence, the default behavior
   is to treat the existing string as the "0"-th item in the sequence.  In such cases
   bu_namegen will thus return one, not zero, as the first incremented name in the
   sequence.

   If the caller wishes to be more sophisticated about where incrementers are
   found in strings, they may supply their own regular expression that uses
   parenthesis bounded matchers to identify numerical identifiers.  For example,
   the regular expression:

   @verbatim
               ([-_:]*[0-9]+[-_:]*)[^0-9]*$
   @endverbatim

   will instruct bu_namegen to define not just the last number but the last
   number and any immediately surrounding separator characters as the subset of
   the string to process.  Combined with a custom incrementation specifier,
   this option allows calling programs to exercise broad flexibility in how
   they process strings.

   \section bu_namegen_inc Specifying Incrementor Behavior

   The caller may optionally specify incrementation behavior with an incrementer
   specification string, which has the following form: "minwidth:init:max:step[:left_sepchar:right_sepchar]"
   The table below explains the individual elements.

   <table>
   <tr><th colspan="2">"minwidth:init:max:step[:left_sepchar:right_sepchar]"</th></tr>
   <tr><td>minwidth</td>     <td>specifies the minimum number of digits used when printing the incrementer substring</td>
   <tr><td>init</td>         <td>specifies the initial minimum value to use when returning an incremented string. Overrides the string-based value if that value is less than the init value.</td>
   <tr><td>max</td>          <td>specifies the maximum value of the incrementer - if the step takes the value past this number, the counter "rolls over" to the init value.</td>
   <tr><td>step</td>         <td>value by which the incrementor value is increased</td>
   <tr><td>left_sepchar</td> <td>optional - specify a character to insert to the left of the numerical substring</td>
   <tr><td>right_sepchar</td><td>optional - specify a character to insert to the right of the numerical substring</td>
   </table>

   In the case of minwidth, init, max, and step a '0' always indicates
   unspecified (i.e. "default") behavior.  So, an incrementer specified as
   0:0:0:0 would behave as follows:

   minwidth: width found in string, or standard printf handling if nothing useful is found.  For example, engine_part-001.s would by default be incremented to engine_part-002.s, preserving the prefix zeros.
   init: from initial name string, or 0 if not found
   max: LONG_MAX
   step: 1

   The last two separator chars are optional - they are useful if the caller
   wants to guarantee a separation between the active incremented substring and
   its surroundings. For example, the following would prefix the incrementer
   output with an underscore and add a suffix with a dash:

   @verbatim
   0:0:0:0:_:-
   @endverbatim

   \section bu_namegen_ex Examples

   So, we return to our engine_part.s example cases.  To generate the initial pattern:

   @verbatim
             engine_part.s-1, engine_part.s-2, engine_part.s-3, ...
   @endverbatim


   the code is actually quite simple:

   @code
   int i = 0;
   const char *estr = "engine_part.s"
   struct bu_vls name = BU_VLS_INIT_ZERO;
   bu_vls_sprintf(&name, "%s", estr);
   while (i < 10) {
       (void)bu_namegen(&name, NULL, "0:0:0:0:-");
       bu_log("%s\n", bu_vls_addr(&name));
   }
   bu_vls_free(&name);
   @endcode

   The second case gets a bit more interesting, since there is no number
   present in the original string, we want the number *before* the .s suffix,
   we're incrementing by more than 1, we want four numerical digits in the
   string, and we want an underscore prefix spacer before the number:

   @verbatim
          engine_part_0010.s, engine_part_0020.s, engine_part_0030.s, ...
   @endverbatim

   To set this up correctly we take advantage of the bu_path_component function
   and construct an initial "seed" string with a zero incrementer where we
   need it:

   @code
   int i = 0;
   const char *estr = "engine_part.s"
   struct bu_vls name = BU_VLS_INIT_ZERO;
   bu_vls_sprintf(&name, "%s0.%s", bu_path_component(estr,BU_PATH_EXTLESS),bu_path_component(estr,BU_PATH_EXT));
   while (i < 10) {
       (void)bu_namegen(&name, NULL, "4:10:0:10:_");
       bu_log("%s\n", bu_vls_addr(&name));
   }
   bu_vls_free(&name);
   @endcode

*/
BU_EXPORT extern int bu_namegen(struct bu_vls *name, const char *regex_str, const char *incr_spec);

/** @} */

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
