#ifndef MATH_UTILS_H
#define MATH_UTILS_H

/* needed for double abs(double) below */
#include <cstdlib>

/* needed for fabs, sqrt() below */
#include <cmath>



namespace TNT
{
/**
	@returns hypotenuse of real (non-complex) scalars a and b by
	avoiding underflow/overflow
	using (a * sqrt( 1 + (b/a) * (b/a))), rather than
	sqrt(a*a + b*b).
*/
template <class Real>
Real hypot(const Real &a, const Real &b)
{

	if (a== 0)
		return abs(b);
	else
	{
		Real c = b/a;
		return abs(a) * sqrt(1 + c*c);
	}
}
} /* TNT namespace */



#endif
/* MATH_UTILS_H */
