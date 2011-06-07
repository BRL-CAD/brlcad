#include "osl-renderer.h"

int Renderer(point_t *p){
  (*p)[0] = 1.0;
  (*p)[1] = 0.0;
  (*p)[2] = 1.0;
  return 1;
}
