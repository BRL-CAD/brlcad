/*
 * $Id$
 */

#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "tienet.h"
#include "time.h"
#ifdef HAVE_CONFIG_H
#include "brlcad_config.h"
#endif



void util_image_init() {
}


void util_image_free() {
}


void util_image_load_ppm(char *filename, void *image, int *w, int *h) {
}


void util_image_load_raw(char *filename, void *image, int *w, int *h) {
  FILE	*fh;
  long	dest_len;
  void	*src;


  fh = fopen(filename, "rb");
  if(fh) {
    fread(w, sizeof(int), 1, fh);
    fread(h, sizeof(int), 1, fh);

    src = malloc(*w * *h * 4);

    fread(src, *w * *h * 4, 1, fh);

    dest_len = *w * *h * 4 + 1024;
    uncompress(image, &dest_len, src, *w * *h * 4);

    free(src);
    fclose(fh);
  }
}


void util_image_save_ppm(char *filename, void *image, int w, int h) {
  FILE *fh;
  char text[16];

  fh = fopen(filename, "wb");

  strcpy(text, "P6\n");
  fwrite(text, strlen(text), 1, fh);
  sprintf(text, "%d %d\n", w, h);
  fwrite(text, strlen(text), 1, fh);
  strcpy(text, "255\n");
  fwrite(text, strlen(text), 1, fh);
  fwrite(image, 3 * w * h, 1, fh);

  fclose(fh);
}

/*
* Format, 128-bit image, 4 floats, RGBA
*/
void util_image_save_raw(char *filename, void *image, int w, int h) {
  FILE	*fh;
  char	*dest;
  long	dest_len;

  dest = malloc(w * h * 4 + 1024);
  dest_len = w * h * 4 + 1024;

  compress(dest, &dest_len, image, w * h * 4);

  fh = fopen(filename, "wb");

  fwrite(&w, sizeof(int), 1, fh);
  fwrite(&h, sizeof(int), 1, fh);
  fwrite(dest, dest_len, w * h * 4, fh);

  fclose(fh);
  free(dest);
}


void util_image_convert_128to24(void *image24, void *image128, int w, int h) {
  int	i;

  for(i = 0; i < w * h; i++) {
    ((unsigned char *)image24)[3*i+0] = (unsigned char)(255.0 * ((tfloat *)image128)[4*i+0]);
    ((unsigned char *)image24)[3*i+1] = (unsigned char)(255.0 * ((tfloat *)image128)[4*i+1]);
    ((unsigned char *)image24)[3*i+2] = (unsigned char)(255.0 * ((tfloat *)image128)[4*i+2]);
  }
}


void util_image_convert_32to24(void *image24, void *image32, int w, int h, int endian) {
  int	i;

  for(i = 0; i < w * h; i++) {
    if(endian) {
      ((unsigned char *)image24)[3*i+0] = ((unsigned char *)image32)[4*i+0];
      ((unsigned char *)image24)[3*i+1] = ((unsigned char *)image32)[4*i+1];
      ((unsigned char *)image24)[3*i+2] = ((unsigned char *)image32)[4*i+2];
    } else {
      ((unsigned char *)image24)[3*i+0] = ((unsigned char *)image32)[4*i+2];
      ((unsigned char *)image24)[3*i+1] = ((unsigned char *)image32)[4*i+1];
      ((unsigned char *)image24)[3*i+2] = ((unsigned char *)image32)[4*i+0];
    }
  }
}

