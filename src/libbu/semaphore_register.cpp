#include "common.h"

#include <vector>

#include "bu/str.h"
#include "bu/parallel.h"


static std::vector<const char *> &get_semaphores()
{
  static std::vector<const char *> semaphores = {};

  return semaphores;
}


extern "C" int
bu_semaphore_register(const char *name)
{
  std::vector<const char *> &semaphores = get_semaphores();

  // printf("registering %s, have %zu\n", name, semaphores.size());

  for (size_t i = 0; i < semaphores.size(); ++i) {
    if (BU_STR_EQUAL(semaphores[i], name)) {
      // printf("re-registered %s = %zu\n", semaphores[i], i+1);
      return i;
    }
  }

  semaphores.push_back(name);
  size_t ret = semaphores.size();

  // printf("registered %s = %zu\n", semaphores[ret-1], ret);

  return ret;
}

