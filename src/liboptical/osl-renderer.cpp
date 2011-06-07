#include "osl-renderer.h"
#include "render_svc.h"

int Renderer2(point_t *p){
  (*p)[0] = 0.0;
  (*p)[1] = 1.0;
  (*p)[2] = 0.0;
  return 1;
}

