#ifndef _ADRT_STRUCT_H
#define _ADRT_STRUCT_H

#ifdef TIE_PRECISION
# if TIE_PRECISION != TIE_SINGLE_PRECISION
#  error "Need single floating point precision out of tie"
# endif
#else
# error "Need TIE_PRECISION set to 0 in the CFLAGS"
#endif

#include "tie.h"
#include "texture_internal.h"
#include "render.h"

#define COMMON_BIT_DEPTH_24	0
#define	COMMON_BIT_DEPTH_128	1

/* Attributes */
typedef struct adrt_mesh_attributes_s {
  TIE_3 color; /* base color of the material */
  tfloat density; /* density of the material, x-ray/vulnerability stuff */
  tfloat gloss; /* smoothness of the surface, ability to reflect */
  tfloat emission; /* emission, power of light source */
  tfloat ior; /* index of refraction */
} adrt_mesh_attributes_t;


/* Mesh */
typedef struct adrt_mesh_s {
  int flags;
  char name[256];
  TIE_3 min, max;
  tfloat matrix[16];
  tfloat matinv[16];
  adrt_mesh_attributes_t *attributes;
  struct texture_s *texture;
} adrt_mesh_t;

#define ADRT_MESH(_m) ((adrt_mesh_t *)_m)

#endif
