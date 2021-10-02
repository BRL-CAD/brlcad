/* Based on OpenBSD's: fnm_test.c ,v1.3 2019/01/25 00:19:26
 *
 * Public domain, 2008, Todd C. Miller <millert@openbsd.org>
 */

#include "common.h"

#include <fstream>
#include <string>

#include "bu.h"

int
main(int argc, char **argv)
{
    int error_cnt = 0;

    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Error - need input file\n");
    }

    /*
     * Read in test file, which is formatted thusly:
     *
     * pattern string flags expected_result
     *
     * lines starting with '#' are comments
     */
    std::ifstream fs;
    fs.open(std::string(argv[1]));
    if (!fs.is_open()) {
	bu_log("Unable to open %s for reading, skipping\n", argv[1]);
	return -1;
    }
    std::string sline;
    struct bu_vls lcpy = BU_VLS_INIT_ZERO;
    while (std::getline(fs, sline)) {
	int lflags, want, got;
	char pattern[1024] = {'\0'};
	char sbuf[1024] = {'\0'};
	bu_vls_sprintf(&lcpy, "%s", sline.c_str());
	got = bu_sscanf(bu_vls_cstr(&lcpy), "%1023s %1023s 0x%x %d", pattern, sbuf, (unsigned int *)&lflags, &want);
	if (got == EOF) {
	    break;
	}
	if (pattern[0] == '#') {
	    continue;
	}
	if (got == 4) {
	    got = bu_path_match(pattern, sbuf, lflags);
	    if (got != want) {
		bu_log("%s %s %d: want %d, got %d", pattern, sbuf, lflags, want, got);
		error_cnt++;
	    }
	} else {
	    bu_log("unrecognized line '%s'\n", sline.c_str());
	    error_cnt++;
	}

    }
    bu_vls_free(&lcpy);

    return error_cnt;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
