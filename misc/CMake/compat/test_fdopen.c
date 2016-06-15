#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int
main(void)
{
  char *nam = tmpnam(NULL);
  int fd = open(nam, O_WRONLY);
  FILE *fp __attribute__((unused)) = fdopen(fd, "w");

  return 0;
}
