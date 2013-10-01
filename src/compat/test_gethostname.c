#include <unistd.h>

int
main(void)
{
  const size_t bufsiz = 512;
  char *name = (char *)malloc(bufsiz);
  int res __attribute((unused))__ = gethostname(name, bufsiz);

  if (name)
    free(name);

  return 0;
}
