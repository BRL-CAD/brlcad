#include <stdio.h>

int
main(void)
{
  FILE *fp = tmpfile();
  int fd __attribute__((unused)) = fileno(fp);

  return 0;
}
