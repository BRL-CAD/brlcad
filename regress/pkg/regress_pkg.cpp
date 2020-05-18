/*                 R E G R E S S _ P K G . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file regress_pkg.cpp
 *
 * Contol program that launches the regression test client and server for
 * libpkg, and returns the overall results.
 *
 */

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include "bu/app.h"

class cmd_result {
    public:
	int cmd_ret = 0;
	std::string cmd;
};

void
run_server(cmd_result &r)
{
    r.cmd_ret = std::system(r.cmd.c_str());
}

void
run_client(cmd_result &r)
{
    r.cmd_ret = std::system(r.cmd.c_str());
}

int
main(int argc, const char *argv[])
{
    if (argc != 3) {
	std::cout << "Usage: regress_pkg server client\n";
	return -1;
    }

    bu_setprogname(argv[0]);

    // Set up the information the threads will need
    cmd_result s, c;
    s.cmd = std::string(argv[1]);
    c.cmd = std::string(argv[2]);

    // Launch the client and server
    std::vector<std::thread> t;
    t.push_back(std::thread(run_server, std::ref(s)));
    t.push_back(std::thread(run_client, std::ref(c)));

    // Wait for both commands to finish up
    for (int i = 0; i < 2; i++) {
	t[i].join();
    }

    // If either client or server had a problem, overall test fails
    return (s.cmd_ret || c.cmd_ret) ? 1 : 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
