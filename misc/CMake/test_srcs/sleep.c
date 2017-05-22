#include "bio.h"

#ifdef _WIN32
# define sleep Sleep
# define WAITLEN 1000
#else
# define WAITLEN 1
#endif

int main(void) {
  sleep(WAITLEN);
  return 0;
}
