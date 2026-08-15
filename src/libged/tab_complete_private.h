/*          T A B _ C O M P L E T E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#ifndef LIBGED_TAB_COMPLETE_PRIVATE_H
#define LIBGED_TAB_COMPLETE_PRIVATE_H

#include <stddef.h>
#include <string>
#include <vector>

struct ged_input_parse {
    char **argv;
    size_t argc;
    size_t cursor_arg;
    size_t input_len;
    /* Decoded token text from its start through the cursor. */
    std::string cursor_seed;
    std::string cursor_path_seed;
    /* Raw byte spans suitable for prefix-only replacement. */
    size_t cursor_content_start;
    size_t cursor_component_start;
    size_t cursor_value_start;
    size_t cursor_replace_end;
    std::vector<size_t> char_starts;
    std::vector<size_t> char_ends;
    char *copy;
};

int ged_input_parse_line(struct ged_input_parse *parsed, const char *input, size_t cursor_pos);
void ged_input_parse_free(struct ged_input_parse *parsed);

#endif /* LIBGED_TAB_COMPLETE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
