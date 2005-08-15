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
#include "bench.h"


#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
	{ "help",	no_argument,	NULL, 'h' },
	{ "dump image",	no_argument,	NULL, 'd' },
};
#endif

static char shortopts[] = "dh";


static void finish(int sig) {
  printf("Collected signal %d, aborting!\n", sig);
  exit(EXIT_FAILURE);
}


static void help() {
  printf("%s\n", "usage: adrt_bench [options] [proj_env_file]\n\
  -d\t\tdump output.\n\
  -h\t\tdisplay help.\n");
}


int main(int argc, char **argv) {
  int c = 0, dump;
  char proj[64], temp[64];


  signal(SIGINT, finish);

  if(argc == 1) {
    help();
    return EXIT_FAILURE;
  }

  /* Initialize strings */
  dump = 0;
  proj[0] = 0;

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
		  case 'd':
			  dump = 1;
			  break;
		  case 'h':
			  help();
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
    bench(proj, dump);
  } else {
    help();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
