/*                    T E S T _ D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file test_draw.cpp
 *
 * Experiment with approaches for managing drawing and selecting
 *
 */

#include "common.h"

#include <stdio.h>
#include <bu.h>
#include <ged.h>

static void
fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    while (s.length() && s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slashes

    std::string nstr;
    bool escaped = false;
    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if (escaped) {
		nstr.push_back(s[i]);
		escaped = false;
		continue;
	    }
	    escaped = true;
	    continue;
	}
	if (s[i] == '/' && !escaped) {
	    if (nstr.length())
		objs.push_back(nstr);
	    nstr.clear();
	    continue;
	}
	nstr.push_back(s[i]);
	escaped = false;
    }
    if (nstr.length())
	objs.push_back(nstr);
}

static std::string
name_deescape(std::string &name)
{
    std::string s(name);
    std::string nstr;

    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if ((i+1) < s.length())
		nstr.push_back(s[i+1]);
	    i++;
	} else {
	    nstr.push_back(s[i]);
	}
    }

    return nstr;
}

static void
draw(struct ged *gedp, const char *path)
{
    if (!gedp)
	return;

    std::vector<std::string> substrs;
    fp_path_split(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	bu_log("%s\n", cleared.c_str());
    }
    bu_log("\n");
}


int
main(int ac, char *av[]) {
    struct ged *gedp;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    gedp = ged_open("db", av[1], 1);


    draw(gedp, "all.g/test1.r/test.s");
    draw(gedp, "all.g/test1.r/test.s/");
    draw(gedp, "all.g/test1.r/test.s//");
    draw(gedp, "all.g/test1.r/test.s\\//");
    draw(gedp, "all.g\\/test1.r\\//test.s");
    draw(gedp, "all.g\\\\/test1.r\\//test.s");
    draw(gedp, "all.g\\\\\\/test1.r\\//test.s");
    draw(gedp, "all.g\\\\\\\\/test1.r\\//test.s");
    draw(gedp, "all.g\\test1.r\\//test.s");
    draw(gedp, "all.g\\\\test1.r\\//test.s");
    draw(gedp, "all.g\\\\\\test1.r\\//test.s");
    draw(gedp, "all.g\\\\\\\\test1.r\\//test.s");



    ged_close(gedp);

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
