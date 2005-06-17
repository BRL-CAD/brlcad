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
#include "slave.h"
#include "tienet.h"


#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
	{ "help",	no_argument,		NULL, 'h' },
	{ "port",	required_argument,	NULL, 'P' },
	{ "threads",	required_argument,	NULL, 't' },
	{ "version",	no_argument,		NULL, 'v' },
  	{ "host",	required_argument,	NULL, 'H' }
};
#endif
static char shortopts[] = "XdhP:t:vH:";


static void finish(int sig) {
  printf("Collected signal %d, aborting!\n", sig);
  exit(EXIT_FAILURE);
}


static void help() {
  printf("%s\n", IGVT_VER_DETAIL);
  printf("%s", "usage: igvt_slave [options]\n\
  -v\t\tdisplay version\n\
  -h\t\tdisplay help\n\
  -P\t\tport number\n\
  -H ...\tconnect to master and shutdown when complete\n\
  -t ...\tnumber of threads to launch for processing\n");
}


int main(int argc, char **argv) {
  int		port = 0, c = 0, threads = 0;
  char		host[64], temp[64];


  /* Default Port */
  signal(SIGINT, finish);

  /* Initialize strings */
  host[0] = 0;
  port = TN_SLAVE_PORT;


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
		  case 't':
			  strncpy(temp, optarg, 4);
			  threads = atoi(temp);
			  if(threads < 0) threads = 0;
			  if(threads > 32) threads = 32;
			  break;
		  case 'H':
			  port = TN_MASTER_PORT;
			  strncpy(host, optarg, 64);
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

  igvt_slave(port, host, threads);

  return EXIT_SUCCESS;
}
