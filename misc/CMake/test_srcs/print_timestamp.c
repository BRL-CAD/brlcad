#include <time.h>
#include <stdio.h>
int main(void) {
  time_t t;
  struct tm *currenttime;
  t = time(NULL);
  currenttime = localtime(&t);
  printf("\nBuild Time: %s\n", asctime(currenttime));
  return 0;
}
