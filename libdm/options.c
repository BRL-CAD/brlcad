#include "conf.h"

#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

int
dm_process_options(dmp, argc, argv)
struct dm *dmp;
int argc;
char *argv[];
{
  register int c;

  bu_optind = 1;	 /* re-init bu_getopt */
  while((c = bu_getopt(argc, argv, "n:i:")) != EOF){
    switch(c){
    case 'n':
      bu_vls_strcpy(&dmp->dm_pathName, bu_optarg);
      break;
    case 'i':
      bu_vls_strcpy(&dmp->dm_initWinProc, bu_optarg);
      break;
    default:
      break;
    }
  }

  return bu_optind;
}
