#include "ged.h"

int main(int ac, char *av[]) {
  struct ged g;

  GED_INIT(&g, NULL);

  return ged_help(&g, ac, (const char **)av);
}
