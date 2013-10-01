#include <stdio.h>

int
main(void)
{
  FILE *fp = tmpfile();
  int fd __attribute((unused))__ = fileno(fp);

  return 0;
}
