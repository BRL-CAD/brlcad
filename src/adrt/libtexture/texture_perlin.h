#ifndef _TEXTURE_PERLIN_H
#define _TEXTURE_PERLIN_H

#include "texture.h"

typedef struct texture_perlin_s {
  int *PV;
  TIE_3 *RV;
} texture_perlin_t;

extern	void	texture_perlin_init(texture_perlin_t *P);
extern	void	texture_perlin_free(texture_perlin_t *P);
extern	tfloat	texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth);

#endif
