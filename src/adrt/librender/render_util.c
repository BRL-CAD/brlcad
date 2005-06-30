#include "render_util.h"
#include "umath.h"


/* Generate vector list for a spall cone given a reference angle */
void render_util_spall(TIE_3 dir, tfloat angle, int vec_num, TIE_3 *vec_list) {
  TIE_3 vec;
  tfloat radius, t;
  int i;

  /* Figure out the rotations of the ray direction */
  vec = dir;
  vec.v[2] = 0;

  radius = sqrt(vec.v[0]*vec.v[0] + vec.v[1]*vec.v[1]);
  vec.v[0] /= radius;
  vec.v[1] /= radius;
  
  vec.v[0] = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*math_rad2deg : acos(vec.v[0])*math_rad2deg;

  /* triangles to approximate */
  for(i = 0; i < vec_num; i++) {
    t = angle * sin((i * 360 / vec_num) * math_deg2rad);
    vec_list[i].v[0] = cos((vec.v[0] + t) * math_deg2rad);
    vec_list[i].v[1] = sin((vec.v[0] + t) * math_deg2rad);

    t = angle * cos((i * 360 / vec_num) * math_deg2rad);
    vec_list[i].v[2] = cos(acos(dir.v[2]) + t * math_deg2rad);
  }
}
