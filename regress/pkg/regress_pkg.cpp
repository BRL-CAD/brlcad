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

#include "common.h"

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
    std::cout << "SERVER thread starting" << std::endl;
    std::string rcmd = std::string("\"") + r.cmd + std::string("\"");
    r.cmd_ret = std::system(rcmd.c_str());
    std::cout << "SERVER thread ending" << std::endl;
}

void
run_client(cmd_result &r)
{
    std::cout << "CLIENT thread starting" << std::endl;
    std::string rcmd = std::string("\"") + r.cmd + std::string("\"");
    r.cmd_ret = std::system(rcmd.c_str());
    std::cout << "CLIENT thread ending" << std::endl;
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
    std::cout << "Launching server" << std::endl;
    std::thread server(run_server, std::ref(s));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "Launching client" << std::endl;
    std::thread client(run_client, std::ref(c));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Wait for both to finish up, client first so we know it tried
    std::cout << "Waiting for client to exit" << std::endl;
    client.join();
    std::cout << "Waiting for server to exit" << std::endl;
    server.join();

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
