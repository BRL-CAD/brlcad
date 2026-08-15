/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <cstring>

#include "bu/app.h"
#include "linenoise.hpp"

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    linenoise::linenoiseState terminal("test> ", 0, 1);
    if (argc > 1) {
	terminal.SetColorMode(linenoise::COLOR_ALWAYS);
	terminal.SetHighlightCallback([](const char *input,
		std::vector<linenoise::HighlightSpan> &spans) {
	    spans.push_back({0, std::strlen(input), linenoise::HIGHLIGHT_VALID});
	});
    }
    std::string line;
    if (terminal.Readline(line)) return EXIT_FAILURE;
    std::cout << "RESULT:" << line << std::endl;
    return EXIT_SUCCESS;
}
