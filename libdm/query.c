#include "conf.h"
#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

int dm_best_type();
char *dm_best_name();
int dm_name2type();
char **dm_names();
char *dm_type2name();
int *dm_types();

int
dm_best_type()
{
	int t;

	t = DM_TYPE_NULL;

#ifdef DM_X
	t = DM_TYPE_X;
#endif

#ifdef DM_GLX
	t = DM_TYPE_GLX;
#endif

#ifdef DM_OGL
	t = DM_TYPE_OGL;
#endif  

	return t;
}

char *
dm_best_name()
{
#ifdef DM_OGL
  return dm_type2name(DM_TYPE_OGL);
#endif  

#ifdef DM_GLX
  return dm_type2name(DM_TYPE_GLX);
#endif

#ifdef DM_X
  return dm_type2name(DM_TYPE_X);
#endif

  return dm_type2name(DM_TYPE_BAD);
}

int
dm_name2type(name)
char *name;
{
  if(!strcmp("nu", name))
    return DM_TYPE_NULL;

  if(!strcmp("plot", name))
    return DM_TYPE_PLOT;

  if(!strcmp("ps", name))
    return DM_TYPE_PS;

#ifdef DM_OGL
  if(!strcmp("ogl", name))
    return DM_TYPE_OGL;
#endif

#ifdef DM_GLX
  if(!strcmp("glx", name))
    return DM_TYPE_GLX;
#endif

#ifdef DM_X
  if(!strcmp("X", name))
    return DM_TYPE_X;
#endif

  return DM_TYPE_BAD;
}

char **
dm_names()
{
  int *types;
  int i;
  char **names = (char **)NULL;

  types = dm_types();
  for(i = 0; types[i] != DM_TYPE_BAD; ++i){
    names = (char **)bu_realloc((genptr_t)names, sizeof(char **)*(i+1), "dm_names: names");
    names[i] = dm_type2name(types[i]);
  }
  names = (char **)bu_realloc((genptr_t)names, sizeof(char **)*(i+1), "dm_names: names");
  names[i] = (char *)NULL;

  bu_free((genptr_t)types, "dm_names: types");
  return names;
}

char *
dm_type2name(type)
int type;
{
  switch(type){
  case DM_TYPE_NULL:
    return "nu";
  case DM_TYPE_PLOT:
    return "plot";
  case DM_TYPE_PS:
    return "ps";
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return "ogl";
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
    return "glx";
#endif
#ifdef DM_X
  case DM_TYPE_X:
    return "X";
#endif
  default:
    return (char *)NULL;
  }
}

int *
dm_types()
{
  int *types = (int *)NULL;
  int i = 4;

  types = (int *)bu_realloc((genptr_t)types, sizeof(int)*i, "dm_types: types");
  types[0] = DM_TYPE_NULL;
  types[1] = DM_TYPE_PLOT;
  types[2] = DM_TYPE_PS;
  types[3] = DM_TYPE_BAD;

#ifdef DM_OGL
  ++i;
  types = (int *)bu_realloc((genptr_t)types, sizeof(int)*i, "dm_types: types");
  types[i - 2] = DM_TYPE_OGL;
  types[i - 1] = DM_TYPE_BAD;
#endif

#ifdef DM_GLX
  ++i;
  types = (int *)bu_realloc((genptr_t)types, sizeof(int)*i, "dm_types: types");
  types[i - 2] = DM_TYPE_GLX;
  types[i - 1] = DM_TYPE_BAD;
#endif

#ifdef DM_X
  ++i;
  types = (int *)bu_realloc((genptr_t)types, sizeof(int)*i, "dm_types: types");
  types[i - 2] = DM_TYPE_X;
  types[i - 1] = DM_TYPE_BAD;
#endif

  return types;
}
