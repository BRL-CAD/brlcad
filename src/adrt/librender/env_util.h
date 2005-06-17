#ifndef _ENV_UTIL_H
#define _ENV_UTIL_H

#include "struct.h"

#define	RISE_MAX_DEPTH	128

extern	rise_mesh_t*	rise_env_util_refract(tie_t *tie, rise_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id);

#endif
