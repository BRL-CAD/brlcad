#include "post.h"
#include <stdlib.h>
#include <string.h>


void rise_post_process(tfloat *image, int w, int h) {
  int	i;

  for(i = 0; i < 4 * w * h; i++)
    if(image[i] > 1) image[i] = 1;
}
