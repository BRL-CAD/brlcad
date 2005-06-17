#include "cdb.h"
#include <string.h>
#include "canim.h"
#include "env.h"


int common_db_load(common_db_t *db, char *path);


int common_db_load(common_db_t *db, char *path) {
  char fpath[255];

  /* Parse path out of proj and chdir to it */
  strcpy(fpath, path);
  if(path[strlen(path)-1] != '/')
    strcat(fpath, "/");

  chdir(fpath);

  /* Load Environment Data */
  common_env_init(&db->env);
  common_env_read(&db->env);
  common_env_prep(&db->env);
  
  /* Load Animation Data */
  common_anim_read(&db->anim);

  return(0);
}
