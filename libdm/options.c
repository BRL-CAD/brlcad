#include "conf.h"

#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

int dm_processOptions(dmp, init_proc_vls, argc, argv)
struct dm *dmp;
struct bu_vls *init_proc_vls;
int argc;
char *argv[];
{
  register int c;

  bu_optind = 0;	 /* re-init bu_getopt */
  bu_opterr = 0;
  while((c = bu_getopt(argc, argv, "N:S:W:s:d:i:n:t:")) != EOF){
    switch(c){
    case 'N':
      dmp->dm_height = atoi(bu_optarg);
      break;
    case 'S':
    case 's':
      dmp->dm_width = dmp->dm_height = atoi(bu_optarg);
      break;
    case 'W':
      dmp->dm_width = atoi(bu_optarg);
      break;
    case 'd':
      bu_vls_strcpy(&dmp->dm_dName, bu_optarg);
      break;
    case 'i':
      bu_vls_strcpy(init_proc_vls, bu_optarg);
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
      bu_log("dm_processOptions: option '%c' unknown\n", bu_optopt);
      break;
    }
  }

  return bu_optind;
}
