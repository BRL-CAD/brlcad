#include "bio.h"

#ifdef _WIN32
#  define sleep(_SECONDS) (Sleep(1000 * (_SECONDS)))
#endif

int main(void) {
  sleep(1);
  return 0;
}
