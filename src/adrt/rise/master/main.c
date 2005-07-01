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
#include "rise.h"
#include "master.h"
#include "tienet.h"


#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
	{ "exec",	required_argument,	NULL, 'e' },
	{ "help",	no_argument,		NULL, 'h' },
	{ "interval",	required_argument,	NULL, 'i' },
	{ "obs_port",	required_argument,	NULL, 'o' },
	{ "port",	required_argument,	NULL, 'p' },
	{ "version",	no_argument,		NULL, 'v' },
  	{ "list",	required_argument,	NULL, 'l' },
};
#endif

static char shortopts[] = "e:i:hvl:p:";


static void finish(int sig) {
  printf("Collected signal %d, aborting!\n", sig);
  exit(EXIT_FAILURE);
}


static void help() {
  printf("%s\n", RISE_VER_DETAIL);
  printf("%s\n", "usage: rise_master [options] [proj_env_file]\n\
  -h\t\tdisplay help.\n\
  -p\t\tset master port number.\n\
  -o\t\tset observer port number.\n\
  -i\t\tinterval in minutes between each autosave.\n\
  -l\t\tfile containing list of slaves to use as compute nodes.\n\
  -e\t\tscript to execute that starts slaves.\n\  
  -v\t\tdisplay version info.\n");
}


int main(int argc, char **argv) {
  int port = 0, obs_port, c = 0, interval = 1;
  char proj[64], exec[64], list[64], temp[64];


  signal(SIGINT, finish);

  if(argc == 1) {
    help();
    return EXIT_FAILURE;
  }

  /* Initialize strings */
  list[0] = 0;
  exec[0] = 0;
  proj[0] = 0;
  port = TN_MASTER_PORT;
  obs_port = RISE_OBSERVER_PORT;

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
		  case 'o':
			  obs_port = atoi(optarg);
			  break;
		  case 'p':
			  port = atoi(optarg);
			  break;
		  case 'e':
			  strncpy(exec, optarg, 64);
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
		  case 'h':
			  help();
			  return EXIT_SUCCESS;
		  case 'v':
			  printf("%s\n", RISE_VER_DETAIL);
			  return EXIT_SUCCESS;
		default:
			  help();
			  return EXIT_FAILURE;
	  }
  }
  argc -= optind;
  argv += optind;

  strcpy(proj, argv[0]);

  if(proj[0]) {
    rise_master(port, obs_port, proj, list, exec, interval);
  } else {
    help();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
