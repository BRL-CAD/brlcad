#include <limits.h>
#include <stdlib.h>

int
main(int argc __attribute((unused))__, char **argv)
{
  char *name = realpath(argv[0], NULL);

  if (name)
    free(name);

  return 0;
}
