#include "env_util.h"
#include <stdlib.h>
#include "hit.h"
#include "rmath.h"
#include "struct.h"

rise_mesh_t* rise_env_util_refract(tie_t *tie, rise_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id);

rise_mesh_t* rise_env_util_refract(tie_t *tie, rise_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id) {
  /*
  * Snells law:
  * T= I*n + (n*N.I' - sqrt(1 - n^2 * (1 - N.I^2)))*N
  * derivation for the refracted ray which is:
  * T= (n*N.I - sqrt(1 - n^2 * (1 - N.I^2)))*N -I*n
  * where:
  * T: refracted vector
  * N: Normal
  * I: Ray Direction
  * n: Refraction Index
  * where n = sin(0t)/cos(0i)
  */
  TIE_3			T1, T2;
  tfloat		ni, NI, Radicand;


  if(ray->depth > RISE_MAX_DEPTH || !mesh)
    return(NULL);

  ni = 1.0/mesh->prop->ior;
  tie_vec_dot(NI, id->norm, ray->dir);
  NI = -NI;
  Radicand = 1.0 - ni*ni * (1.0 - NI*NI);

  /* Check for Total internal Reflection */
  if(Radicand >= 0) {
    /* Prep the refracted ray */
    ray->depth++;
    rise_vec_mul_scalar(ray->pos, id->norm, -TIE_PREC);
    tie_vec_add(ray->pos, ray->pos, id->pos);

    rise_vec_mul_scalar(T1, id->norm, (ni*NI-sqrt(Radicand)));
    rise_vec_mul_scalar(T2, ray->dir, ni);
    tie_vec_add(ray->dir, T1, T2);
    tie_vec_unitize(ray->dir);

    /* Propogate Ray through Refractive Material */
    if(tie_work(tie, ray, id, rise_hit, NULL)) {
      ni = mesh->prop->ior;
      tie_vec_dot(NI, id->norm, ray->dir);
      NI = -NI;
      Radicand = 1.0 - ni*ni * (1.0 - NI*NI);

      /* Check for Total internal Reflection */
      if(Radicand >= 0) {
        /* Prep the refracted ray */
        ray->depth++;
        /* Flip the normal so the ray doesn't shoot back inside */
        rise_vec_mul_scalar(ray->pos, id->norm, -TIE_PREC);
        tie_vec_add(ray->pos, ray->pos, id->pos);

        rise_vec_mul_scalar(T1, id->norm, (ni*NI-sqrt(Radicand)));
        rise_vec_mul_scalar(T2, ray->dir, ni);
        tie_vec_add(ray->dir, T1, T2);
        tie_vec_unitize(ray->dir);

        /* Exit ray through refractive material */
        return( (rise_mesh_t*)tie_work(tie, ray, id, rise_hit, NULL) );
      } else {
        /* Total internal reflection, reflect ray. */
        while(Radicand < 0) {
          if(ray->depth >= RISE_MAX_DEPTH)	/* Make sure internal reflection doesn't go on forever. */
            return(NULL);

          ray->depth++;

          rise_vec_mul_scalar(ray->pos, id->norm, TIE_PREC);
          tie_vec_add(ray->pos, ray->pos, id->pos);

          rise_vec_reflect(T1, ray->dir, id->norm);
          ray->dir = T1;

          tie_work(tie, ray, id, rise_hit, NULL);

          /*
          * Ray has now reflected inside the mesh.
          * Determine if the ray can now escape, or if it
          * will be reflected once again.
          */

          ni = mesh->prop->ior;
          tie_vec_dot(NI, id->norm, ray->dir);
          NI = -NI;
          Radicand = 1.0 - ni*ni * (1.0 - NI*NI);
        }

        /* Ray has now finished reflecting around inside the mesh and is ready to exit */
        ray->depth++;
        /* Flip the normal so the ray doesn't shoot back inside */
        rise_vec_mul_scalar(ray->pos, id->norm, -TIE_PREC);
        tie_vec_add(ray->pos, ray->pos, id->pos);

        rise_vec_mul_scalar(T1, id->norm, (ni*NI-sqrt(Radicand)));
        rise_vec_mul_scalar(T2, ray->dir, ni);
        tie_vec_add(ray->dir, T1, T2);
        tie_vec_unitize(ray->dir);

        /* Exit ray through refractive material */
        return( (rise_mesh_t*)tie_work(tie, ray, id, rise_hit, NULL) );
      }
    } else {
      /* Nothing intersected */
      return(NULL);
    }
  } else {
    /* should only be encountered if firing from inside tranmissive object */
    return(NULL);
  }
}
