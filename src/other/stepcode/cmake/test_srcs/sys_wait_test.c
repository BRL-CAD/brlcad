#include <sys/types.h>
#include <sys/wait.h>
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned int) (stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

int
main ()
{
  int s;
  wait (&s);
  s = WIFEXITED (s) ? WEXITSTATUS (s) : 1;
  ;
  return 0;
}

