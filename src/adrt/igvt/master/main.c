/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "brlcad_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "igvt.h"
#include "master.h"
#include "tienet.h"


#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
	{ "exec",	required_argument,	NULL, 'e' },
	{ "geom",	required_argument,	NULL, 'g' },
	{ "args",	required_argument,	NULL, 'a' },
	{ "help",	no_argument,		NULL, 'h' },
	{ "interval",	required_argument,	NULL, 'i' },
	{ "obs_port",	required_argument,	NULL, 'O' },
	{ "port",	required_argument,	NULL, 'P' },
	{ "version",	no_argument,		NULL, 'v' },
  	{ "list",	required_argument,	NULL, 'l' },
  	{ "proj",	required_argument,	NULL, 'p' },
};
#endif

static char shortopts[] = "e:a:g:i:hO:P:vl:p:";


static void finish(int sig) {
  printf("Collected signal %d, aborting!\n", sig);
  exit(EXIT_FAILURE);
}


static void help() {
  printf("%s\n", IGVT_VER_DETAIL);
  printf("%s\n", "usage: igvt_master [options]\n\
  -v\t\tdisplay version\n\
  -h\t\tdisplay help\n\
  -P\t\tport number\n\
  -O\t\tobserver port number\n\
  -e ...\tfile to execute that starts slaves\n\
  -i ...\tinterval in minutes between each autosave dump\n\
  -l ...\tfile containing list of slave hostnames to distribute work to\n\
  -p ...\tproject directory\n\
  -g ...\tlocation of geometry file\n\
  -a ...\targuments to geometry file\n");
}


int main(int argc, char **argv) {
  int port = 0, obs_port = 0, c = 0, interval = 1;
  char proj[64], exec[64], list[64], temp[64], geom[64], args[64];


  signal(SIGINT, finish);

  if(argc == 1) {
    help();
    return EXIT_FAILURE;
  }

  /* Initialize strings */
  list[0] = 0;
  exec[0] = 0;
  proj[0] = 0;
  geom[0] = 0;
  args[0] = 0;
  port = TN_MASTER_PORT;
  obs_port = IGVT_OBSERVER_PORT;

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
		  case 'O':
			  obs_port = atoi(optarg);
			  break;
		  case 'P':
			  port = atoi(optarg);
			  break;
		  case 'a':
			  strncpy(args, optarg, 64);
			  break;
		  case 'e':
			  strncpy(exec, optarg, 64);
			  break;
		  case 'g':
			  strncpy(geom, optarg, 64);
			  break;
		  case 'i':
			  strncpy(temp, optarg, 4);
			  interval = atoi(temp);
			  if(interval < 0) interval = 0;
			  if(interval > 60) interval = 60;
			  break;
		  case 'l':
			  strncpy(list, optarg, 64);
			  break;
		  case 'p':
			  strncpy(proj, optarg, 64);
			  break;
		  case 'h':
			  help();
			  return EXIT_SUCCESS;
		  case 'v':
			  printf("%s\n", IGVT_VER_DETAIL);
			  return EXIT_SUCCESS;
		default:
			  help();
			  return EXIT_FAILURE;
	  }
  }
  argc -= optind;
  argv += optind;

  if(proj[0]) {
    if(!geom[0]) {
      printf("must supply additional argument: -g /path/to/geometry/file\n");
      return EXIT_FAILURE;
    }
    igvt_master(port, obs_port, proj, geom, args, list, exec, interval);
  } else {
    help();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
