#include <stdlib.h>

int
main(void)
{
  char *buf;
  const size_t bufsiz = 512;
  int res = posix_memalign(&buf, sizeof(void *), bufsiz);

  if (!res && buf)
    free(buf);

  return 0;
}
