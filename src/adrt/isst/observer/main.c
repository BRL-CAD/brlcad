/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "brlcad_config.h"
#endif

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "isst.h"
#include "observer.h"
#include "tienet.h"


#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
	{ "help",	no_argument,		NULL, 'h' },
	{ "port",	required_argument,	NULL, 'p' },
	{ "version",	no_argument,		NULL, 'v' },
};
#endif
static char shortopts[] = "XhP:v";


static void finish(int sig) {
  printf("Collected signal %d, aborting!\n", sig);
  exit(EXIT_FAILURE);
}


static void help() {
  printf("%s\n", ISST_VER_DETAIL);
  printf("%s\n", "usage: isst_observer [options]\n\
  -v\t\tdisplay version\n\
  -h\t\tdisplay help\n\
  -P\t\tport number\n\
  -H ...\tconnect to master as observer\n");
}


int main(int argc, char **argv) {
  int		port = 0, c = 0;
  char		host[64];


  signal(SIGINT, finish);

  if(argc == 1) {
    help();
    return EXIT_FAILURE;
  }

  /* Initialize strings */
  host[0] = 0;
  port = ISST_OBSERVER_PORT;

  /* Parse command line options */

  while((c = 
#ifdef HAVE_GETOPT_LONG
	getopt_long(argc, argv, shortopts, longopts, NULL)
#else
	getopt(argc, argv, shortopts)
#endif
	)!= -1)
  {
	  switch(c) {
		  case 'P':
			  port = atoi(optarg);
			  break;
		  case 'h':
			  help();
			  return EXIT_SUCCESS;
		  case 'v':
			  printf("ISST %s - Copyright (C) U.S Army Research Laboratory (2003 - 2004)\n", VERSION);
			  return EXIT_SUCCESS;
		default:
			  help();
			  return EXIT_FAILURE;
	  }
  }

  argc -= optind;
  argv += optind;

  if(argc)
    strncpy(host, argv[0], 64);

  if(host[0]) {
    printf("Observer mode: connecting to %s on port %d\n", host, port);
    isst_observer(host, port);
  } else {
    printf("must supply arguments: -H hostname\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
