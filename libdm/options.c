#include "conf.h"

#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

int
dm_process_options(dmp, width, height, argc, argv)
struct dm *dmp;
int *width;
int *height;
int argc;
char *argv[];
{
  register int c;

  bu_optind = 0;	 /* re-init bu_getopt */
  while((c = bu_getopt(argc, argv, "N:S:W:d:i:n:t:")) != EOF){
    switch(c){
    case 'N':
      *height = atoi(bu_optarg);
      break;
    case 'S':
      *width = *height = atoi(bu_optarg);
      break;
    case 'W':
      *width = atoi(bu_optarg);
      break;
    case 'd':
      bu_vls_strcpy(&dmp->dm_dName, bu_optarg);
      break;
    case 'i':
      bu_vls_strcpy(&dmp->dm_initWinProc, bu_optarg);
      break;
    case 'n':
      if(*bu_optarg != '.')
	bu_vls_printf(&dmp->dm_pathName, ".%s", bu_optarg);
      else
	bu_vls_strcpy(&dmp->dm_pathName, bu_optarg);
      break;
    case 't':
      dmp->dm_top = atoi(bu_optarg);
      break;
    default:
      break;
    }
  }

  return bu_optind;
}
