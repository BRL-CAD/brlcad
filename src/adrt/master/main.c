/*                          M A I N . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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
/** @file main.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "bio.h"
#include "adrt.h"
#include "master.h"

#include "bu.h"

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
    { "exec",	required_argument,	NULL, 'e' },
    { "help",	no_argument,		NULL, 'h' },
    { "comp_host",	required_argument,	NULL, 'c' },
    { "daemon",	no_argument,		NULL, 'd' },
    { "obs_port",	required_argument,	NULL, 'o' },
    { "port",	required_argument,	NULL, 'p' },
    { "build",	no_argument,		NULL, 'b' },
    { "verbose",	no_argument,		NULL, 'v' },
    { "list",	required_argument,	NULL, 'l' },
};
#endif

static char shortopts[] = "bc:de:i:ho:p:vl:";

unsigned char go_daemon_mode = 0;
unsigned int master_listener_result = 1;
unsigned int observer_listener_result = 1;

static void finish(int sig) {
    printf("Collected signal %d, aborting!\n", sig);
    exit(EXIT_FAILURE);
}


static void help() {
    printf("%s\n", "usage: adrt_master [options]\n\
  -h\t\tdisplay help.\n\
  -c\t\tconnect to component server.\n\
  -d\t\tdaemon mode.\n\
  -e\t\tscript to execute that starts slaves.\n\
  -l\t\tfile containing list of slaves to use as compute nodes.\n\
  -o\t\tset observer port number.\n\
  -p\t\tset master port number.\n\
  -v\t\tverbose.\n\
  -b\t\tdisplay build.\n");
}


int main(int argc, char **argv) {
    int port = 0, obs_port = 0, c = 0;
    char exec[64], list[64], comp_host[64];


    signal(SIGINT, finish);


    /* Initialize strings */
    list[0] = 0;
    exec[0] = 0;
    comp_host[0] = 0;
    port = TN_MASTER_PORT;
    obs_port = ADRT_PORT;

    /* Parse command line options */

    while ((c =
#ifdef HAVE_GETOPT_LONG
	    getopt_long(argc, argv, shortopts, longopts, NULL)
#else
	    getopt(argc, argv, shortopts)
#endif
	       )!= -1)
    {
	switch (c) {
            case 'c':
		strncpy(comp_host, optarg, 64-1);
		comp_host[64-1] = '\0'; /* sanity */
		break;

	    case 'd':
		go_daemon_mode = 1;
		break;

            case 'h':
		help();
		return EXIT_SUCCESS;

            case 'o':
		obs_port = atoi(optarg);
		break;

            case 'p':
		port = atoi(optarg);
		break;

            case 'l':
		strncpy(list, optarg, 64-1);
		list[64-1] = '\0'; /* sanity */
		break;

            case 'e':
		strncpy(exec, optarg, 64-1);
		exec[64-1] = '\0'; /* sanity */
		break;

            case 'b':
		printf("adrt_master build: %s %s\n", __DATE__, __TIME__);
		return EXIT_SUCCESS;
		break;

            case 'v':
		if(!(bu_debug & BU_DEBUG_UNUSED_1))
		    bu_debug |= BU_DEBUG_UNUSED_1;
		else if(!(bu_debug & BU_DEBUG_UNUSED_2))
		    bu_debug |= BU_DEBUG_UNUSED_2;
		else if(!(bu_debug & BU_DEBUG_UNUSED_3))
		    bu_debug |= BU_DEBUG_UNUSED_3;
		else
		    bu_log("Too verbose!\n");
		break;

            default:
		help();
		return EXIT_FAILURE;
	}
    }
    argc -= optind;
    argv += optind;

    master_init (port, obs_port, list, exec, comp_host);

    return EXIT_SUCCESS;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
