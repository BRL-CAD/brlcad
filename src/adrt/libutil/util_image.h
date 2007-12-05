#ifndef _UTIL_IMAGE_H
#define _UTIL_IMAGE_H


#include "tie.h"


void	util_image_init();
void	util_image_free();

void	util_image_load_ppm(char *filename, void *image, int *w, int *h);
void	util_image_save_ppm(char *filename, void *image, int w, int h);

void	util_image_load_raw(char *filename, void *image, int *w, int *h);
void	util_image_save_raw(char *filename, void *image, int w, int h);

void	util_image_convert_128to24(void *image24, void *image128, int w, int h);
void	util_image_convert_32to24(void *image24, void *image32, int w, int h, int endian);

extern tfloat	*rise_image_raw;

#endif
