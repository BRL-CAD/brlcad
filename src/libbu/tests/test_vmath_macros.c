#include "common.h"
#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"

#define CHECK_V3(label, got, exp)                                    \
  do                                                                 \
  {                                                                  \
    if (!NEAR_EQUAL((got)[X], (exp)[X], SMALL_FASTF) ||              \
        !NEAR_EQUAL((got)[Y], (exp)[Y], SMALL_FASTF) ||              \
        !NEAR_EQUAL((got)[Z], (exp)[Z], SMALL_FASTF))                \
    {                                                                \
      bu_log("FAIL [%s]: got (%g, %g, %g), expected (%g, %g, %g)\n", \
             (label), V3ARGS(got), V3ARGS(exp));                     \
      return 1;                                                      \
    }                                                                \
  } while (0)

#define CHECK_V4(label, got, exp)                                            \
  do                                                                         \
  {                                                                          \
    if (!NEAR_EQUAL((got)[X], (exp)[X], SMALL_FASTF) ||                      \
        !NEAR_EQUAL((got)[Y], (exp)[Y], SMALL_FASTF) ||                      \
        !NEAR_EQUAL((got)[Z], (exp)[Z], SMALL_FASTF) ||                      \
        !NEAR_EQUAL((got)[W], (exp)[W], SMALL_FASTF))                        \
    {                                                                        \
      bu_log("FAIL [%s]: got (%g, %g, %g, %g), expected (%g, %g, %g, %g)\n", \
             (label), V4ARGS(got), V4ARGS(exp));                             \
      return 1;                                                              \
    }                                                                        \
  } while (0)

static int
test_vadd3scale(void)
{
  vect_t a = {1.0, 2.0, 3.0};
  vect_t b = {4.0, -2.0, 0.5};
  vect_t c = {-2.0, 3.0, 1.5};
  vect_t out;
  vect_t exp = {(1.0 + 4.0 - 2.0) * 0.5,
                (2.0 - 2.0 + 3.0) * 0.5,
                (3.0 + 0.5 + 1.5) * 0.5};

  VADD3SCALE(out, a, b, c, 0.25 + 0.25);
  CHECK_V3("VADD3SCALE basic + expr scalar", out, exp);

  VADD3SCALE(a, a, b, c, 0.5);
  CHECK_V3("VADD3SCALE aliased output", a, exp);

  return 0;
}

static int
test_v4add2scale(void)
{
  hvect_t a = {2.0, -4.0, 6.0, 1.0};
  hvect_t b = {4.0, 10.0, -2.0, 3.0};
  hvect_t out;
  hvect_t exp = {(2.0 + 4.0) * 0.5,
                 (-4.0 + 10.0) * 0.5,
                 (6.0 - 2.0) * 0.5,
                 (1.0 + 3.0) * 0.5};

  V4ADD2SCALE(out, a, b, 0.5);
  CHECK_V4("V4ADD2SCALE basic", out, exp);

  V4ADD2SCALE(a, a, b, 0.5);
  CHECK_V4("V4ADD2SCALE aliased output", a, exp);

  return 0;
}

static int
test_v4comb2(void)
{
  hvect_t va = {1.0, 2.0, 3.0, 4.0};
  hvect_t vb = {5.0, -1.0, 0.5, 2.0};
  hvect_t out;
  hvect_t exp = {2.0 * 1.0 + (-3.0) * 5.0,
                 2.0 * 2.0 + (-3.0) * (-1.0),
                 2.0 * 3.0 + (-3.0) * 0.5,
                 2.0 * 4.0 + (-3.0) * 2.0};

  V4COMB2(out, 1.0 + 1.0, va, -3.0, vb);
  CHECK_V4("V4COMB2 basic + expr scalar", out, exp);

  V4COMB2(va, 2.0, va, -3.0, vb);
  CHECK_V4("V4COMB2 aliased output", va, exp);

  return 0;
}

static int
test_vcomb4n(void)
{
  fastf_t va[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
  fastf_t vb[5] = {2.0, 0.0, -1.0, 1.0, 2.0};
  fastf_t vc[5] = {-1.0, 1.0, 2.0, 0.0, -2.0};
  fastf_t vd[5] = {0.5, 0.5, 0.5, 0.5, 0.5};
  fastf_t out[5] = {0};

  VCOMB4N(out, 1.0, va, 2.0, vb, -1.0, vc, 4.0, vd, 5);

  for (int i = 0; i < 5; i++)
  {
    fastf_t exp = 1.0 * va[i] + 2.0 * vb[i] - 1.0 * vc[i] + 4.0 * vd[i];
    if (!NEAR_EQUAL(out[i], exp, SMALL_FASTF))
    {
      bu_log("FAIL [VCOMB4N index %d]: got %g, expected %g\n", i, out[i], exp);
      return 1;
    }
  }

  out[0] = 42.0;
  VCOMB4N(out, 1.0, va, 2.0, vb, -1.0, vc, 4.0, vd, 0);
  if (!NEAR_EQUAL(out[0], 42.0, SMALL_FASTF))
  {
    bu_log("FAIL [VCOMB4N n=0]: modified out[0] to %g\n", out[0]);
    return 1;
  }

  return 0;
}

int main(int ac, char *av[])
{
  bu_setprogname(av[0]);
  if (ac > 1)
  {
    bu_log("Usage: %s\n", av[0]);
  }

  if (test_vadd3scale() != 0)
    return 1;
  if (test_v4add2scale() != 0)
    return 1;
  if (test_v4comb2() != 0)
    return 1;
  if (test_vcomb4n() != 0)
    return 1;

  bu_log("vmath_macros: all tests passed.\n");
  return 0;
}