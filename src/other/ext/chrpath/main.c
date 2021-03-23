/*
 * Author: Petter Reinholdtsen <pere@hungry.com>
 * date:   2001-01-20
 *
 * Alter ELF rpath information (insert, modify, remove).
 *
 * Based on source from Peeter Joot <peeterj@ca.ibm.com> and Geoffrey
 * Keating <geoffk@ozemail.com.au>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "protos.h"

#ifdef HAVE_GETOPT_LONG
#  define GETOPT_LONG getopt_long

static struct option long_options[] =
{
  {"convert",   0, 0, 'c'},
  {"delete",    0, 0, 'd'},
  {"help",      0, 0, 'h'},
  {"keepgoing", 0, 0, 'k'},
  {"list",      0, 0, 'l'},
  {"replace",   1, 0, 'r'},
  {"version",   0, 0, 'v'},
  {0, 0, 0, 0}
};

#else /* not HAVE_GETOPT_LONG */
#  define GETOPT_LONG(argc,argv,optstr,lopts,lidx) getopt(argc,argv,optstr)
#endif /* not HAVE_GETOPT_LONG */

static void
usage(char *progname)
{
  printf("Usage: %s [-v|-d|-c|-r <path>] <program> [<program> ...]\n\n",
         progname);
  printf("   -v|--version                Display program version number\n");
  printf("   -d|--delete                 Delete current rpath/runpath setting\n");
#if defined(DT_RUNPATH)
  printf("   -c|--convert                Convert rpath to runpath\n");
#endif /* DT_RUNPATH */
  printf("   -r <path>|--replace <path>  Replace current rpath/runpath setting\n");
  printf("                               with the path given\n");
  printf("   -l|--list                   List the current rpath/runpath (default)\n");
  printf("   -k|--keepgoing              Do not fail on first error\n");
  printf("   -h|--help                   Show this usage information.\n");
#ifndef HAVE_GETOPT_LONG
  printf("\n *** The long options are not available on this platform");
#endif /* not HAVE_GETOPT_LONG */
#if !defined(DT_RUNPATH)
  printf("\n *** There is no support for runpath on this platform");
#endif /* DT_RUNPATH */
  printf("\n");
}

int
main(int argc, char * const argv[])
{
  int retval = 0;
  int convert = 0;      /* convert to given type */
  int remove = 0;       /* remove or not */
  int keep_going = 0;   /* Break on first error, or keep going? */
  char *newpath = NULL; /* insert this path */
  int opt;
#ifdef HAVE_GETOPT_LONG
  int option_index = 0;
#endif /* HAVE_GETOPT_LONG */

  if (argc < 2)
    {
      usage(argv[0]);
      return 1;
    }

  do {
    opt = GETOPT_LONG(argc, argv, "cdhklr:v", long_options, &option_index);
    switch (opt)
      {
#if defined(DT_RUNPATH)
      case 'c':
        convert = 1;
        break;
#endif /* DT_RUNPATH */
      case 'd':
        remove = 1;
        break;
      case 'k':
        keep_going = 1;
        break;
      case 'r':
        newpath = optarg;
        break;
      case 'v':
        printf("%s version %s\n", PACKAGE, VERSION);
        exit(0);
        break;
      case 'l': /* This is the default action */
        newpath = NULL;
        break;
      case -1:
        break;
      default:
        printf("Invalid argument '%c'\n", opt);
	/* Fall through */
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      }
  } while (-1 != opt);

  while (optind < argc && (!retval || keep_going))
    {
      if (remove)
        retval |= killrpath(argv[optind++]);
      else
        /* list by default, replace if path is set */
        retval |= chrpath(argv[optind++], newpath, convert);
    }

  return retval;
}
