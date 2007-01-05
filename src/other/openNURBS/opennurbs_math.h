/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#if !defined(ON_MATH_INC_)
#define ON_MATH_INC_

class ON_3dVector;
class ON_Interval;
class ON_Line;
class ON_Arc;
class ON_Plane;

/*
Description:
  Class for carefully adding long list of numbers.
*/
class ON_CLASS ON_Sum
{
public:

  /*
  Description:
    Calls ON_Sum::Begin(x)
  */
  void operator=(double x);

  /*
  Description:
    Calls ON_Sum::Plus(x);
  */
  void operator+=(double x);

  /*
  Description:
    Calls ON_Sum::Plus(-x);
  */
  void operator-=(double x);

  /*
  Description:
    Creates a sum that is ready to be used.
  */
  ON_Sum();

  /*
  Description:
    If a sum is being used more than once, call Begin()
    before starting each sum.
  Parameters:
    starting_value - [in] Initial value of sum.
  */
  void Begin( double starting_value = 0.0 );

  /*
  Description:
    Add x to the current sum.
  Parameters:
    x - [in] value to add to the current sum.
  */
  void Plus( double x );

  /*
  Description:
    Calculates the total sum.   
  Parameters:
    error_estimate - [out] if not NULL, the returned value of
       *error_estimate is an estimate of the error in the sum.
  Returns:
    Total of the sum.
  Remarks:
    You can get subtotals by mixing calls to Plus() and Total().
    In delicate sums, some precision may be lost in the final
    total if you call Total() to calculate subtotals.
  */
  double Total( double* error_estimate = NULL );

  /*
  Returns:
    Number of summands.
  */
  int SummandCount() const;

private:
  enum {
    sum1_max_count=256,
    sum2_max_count=512,
    sum3_max_count=1024
  };
  double m_sum_err;
  double m_pos_sum;     
  double m_neg_sum;  
  
  int m_zero_count; // number of zeros added
  int m_pos_count; // number of positive numbers added
  int m_neg_count; // number of negative numbers added
  
  int m_pos_sum1_count;
  int m_pos_sum2_count;
  int m_pos_sum3_count;
  double m_pos_sum1[sum1_max_count];
  double m_pos_sum2[sum2_max_count];
  double m_pos_sum3[sum3_max_count];
  
  int m_neg_sum1_count;
  int m_neg_sum2_count;
  int m_neg_sum3_count;
  double m_neg_sum1[sum1_max_count];
  double m_neg_sum2[sum2_max_count];
  double m_neg_sum3[sum3_max_count];

  double SortAndSum( int, double* );
};

/*
Description:
  Abstract function with an arbitrary number of parameters
  and values.  ON_Evaluator is used to pass functions to
  local solvers.
*/
class ON_CLASS ON_Evaluator
{
public:

  /*
  Description:
    Construction of the class for a function that takes
    parameter_count input functions and returns
    value_count values.  If the domain is infinite, pass
    a NULL for the domain[] and periodic[] arrays.  If
    the domain is finite, pass a domain[] array with
    parameter_count increasing intervals.  If one or more of
    the parameters is periodic, pass the fundamental domain
    in the domain[] array and a true in the periodic[] array.
  Parameters:
    parameter_count - [in] >= 1.  Number of input parameters
    value_count - [in] >= 1.  Number of output values.
    domain - [in] If not NULL, then this is an array
                  of parameter_count increasing intervals
                  that defines the domain of the function.
    periodic - [in] if not NULL, then this is an array of 
                parameter_count bools where b[i] is true if
                the i-th parameter is periodic.  Valid 
                increasing finite domains must be specificed
                when this parameter is not NULL.
  */
  ON_Evaluator( 
    int parameter_count,
    int value_count,
    const ON_Interval* domain,
    const bool* periodic
    );

  virtual ~ON_Evaluator();
  
  /*
  Description:
    Evaluate the function that takes m_parameter_count parameters
    and returns a m_value_count dimensional point.
  Parameters:
    parameters - [in] array of m_parameter_count evaluation parameters
    values - [out] array of m_value_count function values
    jacobian - [out] If NULL, simply evaluate the value of the function.
                     If not NULL, this is the jacobian of the function.
                     jacobian[i][j] = j-th partial of the i-th value
                     0 <= i < m_value_count,
                     0 <= j < m_parameter_count
                     If not NULL, then all the memory for the
                     jacobian is allocated, you just need to fill
                     in the answers.
  Example:
    If f(u,v) = square of the distance from a fixed point P to a 
    surface evaluated at (u,v), then

          values[0] = (S-P)o(S-P)
          jacobian[0] = ( 2*(Du o (S-P)), 2*(Dv o (S-P)) )

    where S, Du, Dv = surface point and first partials evaluated
    at u=parameters[0], v = parameters[1].

    If the function takes 3 parameters, say (x,y,z), and returns
    two values, say f(x,y,z) and g(z,y,z), then

          values[0] = f(x,y,z)
          values[1] = g(x,y,z)

          jacobian[0] = (DfDx, DfDy, DfDz)
          jacobian[1] = (DgDx, DgDy, DgDz)

    where dfx denotes the first partial of f with respect to x.

  Returns:
    0 = unable to evaluate
    1 = successful evaluation
    2 = found answer, terminate search
  */
  virtual int Evaluate(
       const double* parameters,
       double* values,
       double** jacobian
       ) = 0;

  /*
  Description:
    OPTIONAL ability to evaluate the hessian in the case when 
    m_value_count is one.  If your function has more that
    one value or it is not feasable to evaluate the hessian,
    then do not override this function.  The default implementation
    returns -1.
  Parameters:
    parameters - [in] array of m_parameter_count evaluation parameters
    value - [out] value of the function (one double)
    gradient - [out] The gradient of the function.  This is a vector
                     of length m_parameter_count; gradient[i] is
                     the first partial of the function with respect to
                     the i-th parameter.
    hessian - [out] The hessian of the function. This is an
                    m_parameter_count x m_parameter_count 
                    symmetric matrix: hessian[i][j] is the
                    second partial of the function with respect
                    to the i-th and j-th parameters.  The evaluator
                    is responsible for filling in both the upper
                    and lower triangles.  Since the matrix is
                    symmetrix, you should do something like evaluate
                    the upper triangle and copy the values to the
                    lower tiangle.
  Returns:
   -1 = Hessian evaluation not available.
    0 = unable to evaluate
    1 = successful evaluation
    2 = found answer, terminate search
  */
  virtual int EvaluateHessian(
       const double* parameters,
       double* value,
       double* gradient,
       double** hessian
       );
  
  // Number of the function's input parameters. This number
  // is >= 1 and is specified in the constructor.
  const int m_parameter_count;

  // Number of the function's output values. This number
  // is >= 1 and is specified in the constructor.
  const int m_value_count;

  /*
  Description:
    Functions can have finite or infinite domains. Finite domains
    are specified by passing the domain[] array to the constructor
    or filling in the m_domain[] member variable.  If
    m_domain.Count() == m_parameter_count > 0, then the function
    has finite domains.
  Returns:
    True if the domain of the function is finite.
  */
  bool FiniteDomain() const;

  /*
  Description:
    If a function has a periodic parameter, then the m_domain
    interval for that parameter is the fundamental domain and
    the m_bPeriodicParameter bool for that parameter is true.
    A parameter is periodic if, and only if, 
    m_domain.Count() == m_parameter_count, and 
    m_bPeriodicParameter.Count() == m_parameter_count, and
    m_bPeriodicParameter[parameter_index] is true.
  Returns:
    True if the function parameter is periodic.
  */
  bool Periodic(
    int parameter_index
    ) const;

  /*
  Description:
    If a function has a periodic parameter, then the m_domain
    interval for that parameter is the fundamental domain and
    the m_bPeriodicParameter bool for that parameter is true.
    A parameter is periodic if, and only if, 
    m_domain.Count() == m_parameter_count, and 
    m_bPeriodicParameter.Count() == m_parameter_count, and
    m_bPeriodicParameter[parameter_index] is true.
  Returns:
    The domain of the parameter.  If the domain is infinite,
    the (-1.0e300, +1.0e300) is returned.
  */
  ON_Interval Domain(
    int parameter_index
    ) const;


  // If the function has a finite domain or periodic
  // parameters, then m_domain[] is an array of 
  // m_parameter_count finite increasing intervals.
  ON_SimpleArray<ON_Interval> m_domain;

  // If the function has periodic parameters, then 
  // m_bPeriodicParameter[] is an array of m_parameter_count
  // bools.  If m_bPeriodicParameter[i] is true, then
  // the i-th parameter is periodic and m_domain[i] is 
  // the fundamental domain for that parameter.
  ON_SimpleArray<bool> m_bPeriodicParameter;

private:
  ON_Evaluator(); // prohibit default constructor
  ON_Evaluator& operator=(const ON_Evaluator&); // prohibit operator= (can't copy const members)
};

/*
Description:
  Test a double to make sure it is a valid number.
Returns:
  True if x != ON_UNSET_VALUE and _finite(x) is true.
*/
ON_DECL
bool ON_IsValid( double x );

ON_DECL
float ON_ArrayDotProduct( // returns AoB
          int,           // size of arrays (can be zero)
          const float*, // A[]
          const float*  // B[]
          );

ON_DECL
void   ON_ArrayScale( 
          int,           // size of arrays (can be zero)
          float,        // a
          const float*, // A[]
          float*        // returns a*A[]
          );

ON_DECL
void   ON_Array_aA_plus_B( 
          int,           // size of arrays (can be zero)
          float,        // a
          const float*, // A[]
          const float*, // B[]
          float*        // returns a*A[] + B[]
          );

ON_DECL
double ON_ArrayDotProduct( // returns AoB
          int,           // size of arrays (can be zero)
          const double*, // A[]
          const double*  // B[]
          );

ON_DECL
double ON_ArrayDotDifference( // returns A o ( B - C )
          int,           // size of arrays (can be zero)
          const double*, // A[]
          const double*, // B[]
          const double*  // C[]
          );

ON_DECL
double ON_ArrayMagnitude( // returns sqrt(AoA)
          int,           // size of arrays (can be zero)
          const double*  // A[]
          );

ON_DECL
double ON_ArrayMagnitudeSquared( // returns AoA
          int,           // size of arrays (can be zero)
          const double*  // A[]
          );

ON_DECL
double ON_ArrayDistance( // returns sqrt((A-B)o(A-B))
          int,           // size of arrays (can be zero)
          const double*, // A[]
          const double*  // B[]
          );

ON_DECL
double ON_ArrayDistanceSquared( // returns (A-B)o(A-B)
          int,           // size of arrays (can be zero)
          const double*, // A[]
          const double*  // B[]
          );

ON_DECL
void   ON_ArrayScale( 
          int,           // size of arrays (can be zero)
          double,        // a
          const double*, // A[]
          double*        // returns a*A[]
          );

ON_DECL
void   ON_Array_aA_plus_B( 
          int,           // size of arrays (can be zero)
          double,        // a
          const double*, // A[]
          const double*, // B[]
          double*        // returns a*A[] + B[]
          );

ON_DECL
int    ON_SearchMonotoneArray( // find a value in an increasing array
          // returns  -1: t < array[0]
          //           i: array[i] <= t < array[i+1] ( 0 <= i < length-1 )
          //    length-1: t == array[length-1]
          //      length: t >= array[length-1]
          const double*, // array[]
          int,           // length of array
          double         // t = value to search for
          );


/* 
Description:
  Compute a binomial coefficient.
Parameters:
  i - [in]
  j - [in]
Returns:
  (i+j)!/(i!j!), if 0 <= i and 0 <= j, and 0 otherwise.
See Also:
  ON_TrinomialCoefficient()
Remarks:
  If (i+j) <= 52, this function is fast and returns the exact
  value of the binomial coefficient.  

  For (i+j) > 52, the coefficient is computed recursively using
  the formula  bc(i,j) = bc(i-1,j) + bc(i,j-1).
  For (i+j) much larger than 60, this is inefficient.
  If you need binomial coefficients for large i and j, then you
  should probably be using something like Stirling's Formula.  
  (Look up "Stirling" or "Gamma function" in a calculus book.)
*/
ON_DECL
double ON_BinomialCoefficient( 
          int i,
          int j
          );


/* 
Description:
  Compute a trinomial coefficient.
Parameters:
  i - [in]
  j - [in]
  k - [in]
Returns:
  (i+j+k)!/(i!j!k!), if 0 <= i, 0 <= j and 0<= k, and 0 otherwise.
See Also:
  ON_BinomialCoefficient()
Remarks:
  The trinomial coefficient is computed using the formula

          (i+j+k)!      (i+j+k)!       (j+k)!
          --------   =  --------   *  -------
          i! j! k!      i! (j+k)!      j! k!

                      = ON_BinomialCoefficient(i,j+k)*ON_BinomialCoefficient(j,k)
  
*/
ON_DECL
double ON_TrinomialCoefficient( 
          int i,
          int j,
          int k
          );


ON_DECL
BOOL ON_GetParameterTolerance(
        double, double, // domain
        double,          // parameter in domain
        double*, double* // parameter tolerance (tminus, tplus) returned here
        );


ON_DECL
BOOL ON_IsValidPointList(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int,  // count
        int,  // stride
        const float*
        );

ON_DECL
BOOL ON_IsValidPointList(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int,  // count
        int,  // stride
        const double*
        );

ON_DECL
BOOL ON_IsValidPointGrid(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int, int, // point_count0, point_count1,
        int, int, // point_stride0, point_stride1,
        const double*
        );

ON_DECL
bool ON_ReversePointList(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int,  // count
        int,  // stride
        double*
        );

ON_DECL
BOOL ON_ReversePointGrid(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int, int, // point_count0, point_count1,
        int, int, // point_stride0, point_stride1,
        double*,
        int       // dir = 0 or 1
        );

ON_DECL
bool ON_SwapPointListCoordinates( 
        int, // count
        int, // stride
        float*,
        int, int // coordinates to swap
        );

ON_DECL
bool ON_SwapPointListCoordinates( 
        int, // count
        int, // stride
        double*,
        int, int // coordinates to swap
        );

ON_DECL
BOOL ON_SwapPointGridCoordinates(
        int, int, // point_count0, point_count1,
        int, int, // point_stride0, point_stride1,
        double*,
        int, int // coordinates to swap
        );

ON_DECL
bool ON_TransformPointList(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int,  // count
        int,  // stride
        float*,
        const ON_Xform&
        );

ON_DECL
bool ON_TransformPointList(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int,  // count
        int,  // stride
        double*,
        const ON_Xform&
        );

ON_DECL
BOOL ON_TransformPointGrid(
        int,      // dim
        BOOL,     // TRUE for homogeneous rational points
        int, int, // point_count0, point_count1,
        int, int, // point_stride0, point_stride1,
        double*,
        const ON_Xform&
        );

ON_DECL
BOOL ON_TransformVectorList(
       int,  // dim
       int,  // count
       int,  // stride
       float*,
       const ON_Xform&
       );

ON_DECL
BOOL ON_TransformVectorList(
       int,  // dim
       int,  // count
       int,  // stride
       double*,
       const ON_Xform&
       );

ON_DECL
int ON_ComparePoint( // returns 
                              // -1: first < second
                              //  0: first == second
                              // +1: first > second
          int,           // dim (>=0)
          BOOL,          // TRUE for rational CVs
          const double*, // first CV
          const double*  // secont CV
          );

ON_DECL
int ON_ComparePointList( // returns 
                              // -1: first < second
                              //  0: first == second
                              // +1: first > second
          int,           // dim (>=0)
          BOOL,          // TRUE for rational CVs
          int,           // count
          // first point list
          int,           // stride
          const double*, // point
          // second point list
          int,           // stride
          const double*  // point
          );

ON_DECL
BOOL ON_IsPointListClosed(
       int,  // dim
       int,  // TRUE for homogeneos rational points
       int,  // count
       int,  // stride
       const double*
       );

ON_DECL
BOOL ON_IsPointGridClosed(
        int,  // dim
        BOOL, // TRUE for homogeneous rational points
        int, int, // point_count0, point_count1,
        int, int, // point_stride0, point_stride1,
        const double*,
        int       // dir = 0 or 1
       );

ON_DECL
int ON_SolveQuadraticEquation( // solve a*X^2 + b*X + c = 0
        // returns 0: two distinct real roots (r0 < r1)
        //         1: one real root (r0 = r1)
        //         2: two complex conjugate roots (r0 +/- (r1)*sqrt(-1))
        //        -1: failure - a = 0, b != 0        (r0 = r1 = -c/b)
        //        -2: failure - a = 0, b  = 0 c != 0 (r0 = r1 = 0.0)
        //        -3: failure - a = 0, b  = 0 c  = 0 (r0 = r1 = 0.0)
       double, double, double, // a, b, c
       double*, double*        // roots r0 and r1 returned here
       );

ON_DECL
BOOL ON_SolveTriDiagonal( // solve TriDiagMatrix( a,b,c )*X = d
        int,               // dimension of d and X (>=1)
        int,               // number of equations (>=2)
        double*,           // a[n-1] = sub-diagonal (a is modified)
        const double*,     // b[n] = diagonal
        double*,           // c[n-1] = supra-diagonal
        const double*,     // d[n*dim]
        double*            // X[n*dim] = unknowns
        );

// returns rank - if rank != 2, system is under determined
// If rank = 2, then solution to 
//
//          a00*x0 + a01*x1 = b0, 
//          a10*x0 + a11*x1 = b1 
//
// is returned
ON_DECL
int ON_Solve2x2( 
        double, double,   // a00 a01 = first row of 2x2 matrix
        double, double,   // a10 a11 = second row of 2x2 matrix
        double, double,   // b0 b1
        double*, double*, // x0, x1 if not NULL, then solution is returned here
        double*           // if not NULL, then pivot_ratio returned here
        );

// Description:
//   Solves a system of 3 linear equations and 2 unknowns.
//
//          x*col0[0] + y*col1[0] = d0
//          x*col0[1] + y*col1[1] = d0
//          x*col0[2] + y*col1[2] = d0
//
// Parameters:
//   col0 - [in] coefficents for "x" unknown
//   col1 - [in] coefficents for "y" unknown
//   d0 - [in] constants
//   d1 - [in]
//   d2 - [in]
//   x - [out]
//   y - [out]
//   error - [out]
//   pivot_ratio - [out]
//
// Returns:
//   rank of the system.  
//   If rank != 2, system is under determined
//   If rank = 2, then the solution is
//
//         (*x)*[col0] + (*y)*[col1]
//         + (*error)*((col0 X col1)/|col0 X col1|)
//         = (d0,d1,d2).
ON_DECL
int ON_Solve3x2( 
        const double[3], // col0
        const double[3], // col1
        double,  // d0
        double,  // d1
        double,  // d2
        double*, // x
        double*, // y
        double*, // error
        double*  // pivot_ratio
        );

/* 
Description:
  Use Gauss-Jordan elimination with full pivoting to solve 
  a system of 3 linear equations and 3 unknowns(x,y,z)

        x*row0[0] + y*row0[1] + z*row0[2] = d0
        x*row1[0] + y*row1[1] + z*row1[2] = d1
        x*row2[0] + y*row2[1] + z*row2[2] = d2

Parameters:
    row0 - [in] first row of 3x3 matrix
    row1 - [in] second row of 3x3 matrix
    row2 - [in] third row of 3x3 matrix
    d0 - [in] 
    d1 - [in] 
    d2 - [in] (d0,d1,d2) right hand column of system
    x_addr - [in] first unknown
    y_addr - [in] second unknown
    z_addr - [in] third unknown
    pivot_ratio - [out] if not NULL, the pivot ration is 
         returned here.  If the pivot ratio is "small",
         then the matrix may be singular or ill 
         conditioned. You should test the results 
         before you use them.  "Small" depends on the
         precision of the input coefficients and the
         use of the solution.  If you can't figure out
         what "small" means in your case, then you
         must check the solution before you use it.

Returns:
    The rank of the 3x3 matrix (0,1,2, or 3)
    If ON_Solve3x3() is successful (returns 3), then
    the solution is returned in 
    (*x_addr, *y_addr, *z_addr)
    and *pivot_ratio = min(|pivots|)/max(|pivots|).
    If the return code is < 3, then (0,0,0) is returned
    as the "solution".

See Also:
  ON_Solve2x2
  ON_Solve3x2
  ON_Solve4x4
*/
ON_DECL
int ON_Solve3x3( 
        const double row0[3], 
        const double row1[3], 
        const double row2[3],
        double d0, 
        double d1, 
        double d2,
        double* x_addr, 
        double* y_addr, 
        double* z_addr,
        double* pivot_ratio
        );

/* 
Description:
  Use Gauss-Jordan elimination with full pivoting to solve 
  a system of 4 linear equations and 4 unknowns(x,y,z,w)

        x*row0[0] + y*row0[1] + z*row0[2] + w*row0[3] = d0
        x*row1[0] + y*row1[1] + z*row1[2] + w*row1[3] = d1
        x*row2[0] + y*row2[1] + z*row2[2] + w*row2[3] = d2
        x*row3[0] + y*row3[1] + z*row3[2] + w*row3[2] = d3

Parameters:
    row0 - [in] first row of 4x4 matrix
    row1 - [in] second row of 4x4 matrix
    row2 - [in] third row of 4x4 matrix
    row3 - [in] forth row of 4x4 matrix
    d0 - [in] 
    d1 - [in] 
    d2 - [in] 
    d3 - [in] (d0,d1,d2,d3) right hand column of system
    x_addr - [in] first unknown
    y_addr - [in] second unknown
    z_addr - [in] third unknown
    w_addr - [in] forth unknown
    pivot_ratio - [out] if not NULL, the pivot ration is 
         returned here.  If the pivot ratio is "small",
         then the matrix may be singular or ill 
         conditioned. You should test the results 
         before you use them.  "Small" depends on the
         precision of the input coefficients and the
         use of the solution.  If you can't figure out
         what "small" means in your case, then you
         must check the solution before you use it.

Returns:
    The rank of the 4x4 matrix (0,1,2,3, or 4)
    If ON_Solve4x4() is successful (returns 4), then
    the solution is returned in 
    (*x_addr, *y_addr, *z_addr, *w_addr)
    and *pivot_ratio = min(|pivots|)/max(|pivots|).
    If the return code is < 4, then, it a solution exists,
    on is returned.  However YOU MUST CHECK THE SOLUTION
    IF THE RETURN CODE IS < 4.

See Also:
  ON_Solve2x2
  ON_Solve3x2
  ON_Solve3x3
*/
ON_DECL
int
ON_Solve4x4(
          const double row0[4], 
          const double row1[4], 
          const double row2[4],  
          const double row3[4],
          double d0, 
          double d1, 
          double d2, 
          double d3,
          double* x_addr, 
          double* y_addr, 
          double* z_addr, 
          double* w_addr,
          double* pivot_ratio
          );

// return FALSE if determinant is (nearly) singular
ON_DECL
BOOL ON_EvJacobian( 
        double, // ds o ds
        double, // ds o dt
        double, // dt o dt
        double* // jacobian = determinant ( ds_o_ds ds_o_dt / ds_o_dt dt_o_dt )
        );

/*
Description:
  Finds scalars x and y so that the component of V in the plane
  of A and B is x*A + y*B.
Parameters:
  V - [in]
  A - [in] nonzero and not parallel to B
  B - [in] nonzero and not parallel to A
  x - [out]
  y - [out]
Returns:
  The rank of the problem.  
    2 = A and B are nonzero and not parallel
    1 = A and B are (nearly) parallel
    0 = A and B are (nearly) zero
See Also:
  ON_Solve2x2
*/
ON_DECL
int ON_DecomposeVector(
        const ON_3dVector& V,
        const ON_3dVector& A,
        const ON_3dVector& B,
        double* x, double* y
        );


/*
Description:
   Evaluate partial derivatives of surface unit normal
Parameters:
  ds - [in]
  dt - [in] surface first partial derivatives
  dss - [in]
  dst - [in]
  dtt - [in] surface second partial derivatives
  ns - [out]
  nt - [out] First partial derivatives of surface unit normal
             (If the Jacobian is degenerate, ns and nt are set to zero.)
Returns:
  TRUE if Jacobian is nondegenerate
  FALSE if Jacobian is degenerate
*/
ON_DECL
BOOL ON_EvNormalPartials(
        const ON_3dVector& ds,
        const ON_3dVector& dt,
        const ON_3dVector& dss,
        const ON_3dVector& dst,
        const ON_3dVector& dtt,
        ON_3dVector& ns,
        ON_3dVector& nt
        );

ON_DECL
BOOL 
ON_Pullback3dVector( // use to pull 3d vector back to surface parameter space
      const ON_3dVector&,   // 3d vector
      double,              // signed distance from vector location to closet point on surface
                                    // < 0 if point is below with respect to Du x Dv
      const ON_3dVector&,     // ds      surface first partials
      const ON_3dVector&,     // dt
      const ON_3dVector&,     // dss     surface 2nd partials
      const ON_3dVector&,     // dst     (used only when dist != 0)
      const ON_3dVector&,     // dtt
      ON_2dVector&            // pullback
      );

ON_DECL
BOOL 
ON_GetParameterTolerance(
        double,   // t0      domain
        double,   // t1 
        double,   // t       parameter in domain
        double*,  // tminus  parameter tolerance (tminus, tplus) returned here
        double*   // tplus
        );


ON_DECL
BOOL ON_EvNormal(
        int, // limit_dir 0=default,1=from quadrant I, 2 = from quadrant II, ...
        const ON_3dVector&, const ON_3dVector&, // first partials (Du,Dv)
        const ON_3dVector&, const ON_3dVector&, const ON_3dVector&, // optional second partials (Duu, Duv, Dvv)
        ON_3dVector& // unit normal returned here
        );

// returns FALSE if first derivtive is zero
ON_DECL
BOOL ON_EvCurvature(
        const ON_3dVector&, // first derivative
        const ON_3dVector&, // second derivative
        ON_3dVector&,       // Unit tangent returned here
        ON_3dVector&        // Curvature returned here
        );

ON_DECL
BOOL ON_EvPrincipalCurvatures( 
        const ON_3dVector&, // Ds,
        const ON_3dVector&, // Dt,
        const ON_3dVector&, // Dss,
        const ON_3dVector&, // Dst,
        const ON_3dVector&, // Dtt,
        const ON_3dVector&, // N,   // unit normal to surface (use ON_EvNormal())
        double*, // gauss,  // = Gaussian curvature = kappa1*kappa2
        double*, // mean,   // = mean curvature = (kappa1+kappa2)/2
        double*, // kappa1, // = largest principal curvature value (may be negative)
        double*, // kappa2, // = smallest principal curvature value (may be negative)
        ON_3dVector&, // K1,     // kappa1 unit principal curvature direction
        ON_3dVector&  // K2      // kappa2 unit principal curvature direction
                        // output K1,K2,N is right handed frame
        );

ON_DECL
ON_3dVector ON_NormalCurvature( 
        const ON_3dVector&, // surface 1rst partial (Ds)
        const ON_3dVector&, // surface 1rst partial (Dt)
        const ON_3dVector&, // surface 1rst partial (Dss)
        const ON_3dVector&, // surface 1rst partial (Dst)
        const ON_3dVector&, // surface 1rst partial (Dtt)
        const ON_3dVector&, // surface unit normal
        const ON_3dVector&  // unit tangent direction
        );

/*
Description:
  Test curve continuity from derivative values.
Parameters:
  c - [in] type of continuity to test for. Read ON::continuity
           comments for details.
  Pa - [in] point on curve A.
  D1a - [in] first derviative of curve A.
  D1a - [in] second derviative of curve A.
  Pb - [in] point on curve B.
  D1b - [in] first derviative of curve B.
  D1b - [in] second derviative of curve B.
  point_tolerance - [in] if the distance between two points is
      greater than point_tolerance, then the curve is not C0.
  d1_tolerance - [in] if the difference between two first derivatives is
      greater than d1_tolerance, then the curve is not C1.
  d2_tolerance - [in] if the difference between two second derivatives is
      greater than d2_tolerance, then the curve is not C2.
  cos_angle_tolerance - [in] default = cos(1 degree) Used only when
      c is ON::G1_continuous or ON::G2_continuous.  If the cosine
      of the angle between two tangent vectors 
      is <= cos_angle_tolerance, then a G1 discontinuity is reported.
  curvature_tolerance - [in] (default = ON_SQRT_EPSILON) Used only when
      c is ON::G2_continuous.  If K0 and K1 are curvatures evaluated
      from above and below and |K0 - K1| > curvature_tolerance,
      then a curvature discontinuity is reported.
Returns:
  TRUE if the curve has at least the c type continuity at 
  the parameter t.
*/
ON_DECL
BOOL ON_IsContinuous(
  ON::continuity c,
  ON_3dPoint Pa,
  ON_3dVector D1a,
  ON_3dVector D2a,
  ON_3dPoint Pb,
  ON_3dVector D1b,
  ON_3dVector D2b,
  double point_tolerance=ON_ZERO_TOLERANCE,
  double d1_tolerance=ON_ZERO_TOLERANCE,
  double d2_tolerance=ON_ZERO_TOLERANCE,
  double cos_angle_tolerance=0.99984769515639123915701155881391,
  double curvature_tolerance=ON_SQRT_EPSILON
  );


ON_DECL
int ON_Compare2dex( const ON_2dex* a, const ON_2dex* b);

ON_DECL
int ON_Compare3dex( const ON_3dex* a, const ON_3dex* b);

// sort an index array 
ON_DECL
void ON_Sort( 
        ON::sort_algorithm,          // ON::heap_sort or ON::quick_sort, 
        int*,                        // index[] array of count integers, 
        const void*,                 // data buffer
        size_t,                      // count 
        size_t,                      // sizeof_element width,
        int (*)(const void*,const void*) // int compar(const void*,const void*)
        );

// sort an index array 
ON_DECL
void ON_Sort( 
        ON::sort_algorithm,          // ON::heap_sort or ON::quick_sort, 
        int*,                        // index[] array of count integers, 
        const void*,                 // data buffer
        size_t,                      // count 
        size_t,                      // sizeof_element width,
        int (*)(const void*,const void*,void*), // int compar(const void* a,const void* b, void* ptr)
        void* // ptr
        );

// heap sort analogue to qsort()
ON_DECL
void ON_hsort( 
        void*,                                // data buffer
        size_t,                               // number of elements
        size_t,                               // sizeof_element width,
        int (*)(const void*,const void*)      // int compar(const void*,const void*)
        );

// heap sort array of doubles in place
ON_DECL
void ON_SortDoubleArray( 
        ON::sort_algorithm, // ON::heap_sort or ON::quick_sort, 
        double*,  // array of doubles
        size_t    // length of array
        );

ON_DECL
void ON_SortIntArray(
        ON::sort_algorithm, // ON::heap_sort or ON::quick_sort, 
        int*,     // array of doubles
        size_t    // length of array
        );

ON_DECL
void ON_SortUnsignedIntArray(
        ON::sort_algorithm, // ON::heap_sort or ON::quick_sort, 
        unsigned int*,      // array of doubles
        size_t              // length of array
        );

ON_DECL
void ON_SortStringArray(
        ON::sort_algorithm, // ON::heap_sort or ON::quick_sort, 
        char**,   // array of doubles
        size_t    // length of array
        );

ON_DECL
const int* ON_BinarySearchIntArray( 
          int key, 
          const int* base, 
          size_t nel
          );

ON_DECL
const unsigned int* ON_BinarySearchUnsignedIntArray( 
          unsigned int key, 
          const unsigned int* base, 
          size_t nel
          );

ON_DECL
const double* ON_BinarySearchDoubleArray( 
          double key, 
          const double* base, 
          size_t nel
          );

ON_DECL
const ON_2dex* ON_BinarySearch2dexArray( 
          int key_i, 
          const ON_2dex* base, 
          size_t nel
          );

// These simple intersectors are fast and detect transverse intersections.
// If the intersection is not a simple transverse case, then they
// return FALSE and you will have to use one of the slower but fancier
// models.

// returns closest points between the two infinite lines
ON_DECL
bool ON_Intersect( 
          const ON_Line&, 
          const ON_Line&, 
          double*, // parameter on first line
          double*  // parameter on second line
          );

// Returns FALSE unless intersection is a single point
// If returned parameter is < 0 or > 1, then the line
// segment between line.m_point[0] and line.m_point[1]
// does not intersect the plane
ON_DECL
bool ON_Intersect( 
          const ON_Line&, 
          const ON_Plane&, 
          double* // parameter on line
          );

ON_DECL
bool ON_Intersect( 
        const ON_Plane&, 
        const ON_Plane&, 
        ON_Line& // intersection line is returned here
        );

ON_DECL
bool ON_Intersect( 
        const ON_Plane&, 
        const ON_Plane&, 
        const ON_Plane&,
        ON_3dPoint& // intersection point is returned here
        );

// returns 0 = no intersections, 
// 1 = intersection = single point, 
// 2 = intersection = circle
// If 0 is returned, returned circle has radius=0
// and center = point on sphere closest to plane.
// If 1 is returned, intersection is a single
// point and returned circle has radius=0
// and center = intersection point on sphere.
ON_DECL
int ON_Intersect( 
                 const ON_Plane&, const ON_Sphere&, ON_Circle&
                  );

// Intersects an infinte line and sphere and returns 
// 0 = no intersections, 
// 1 = one intersection, 
// 2 = 2 intersections
// If 0 is returned, first point is point 
// on line closest to sphere and 2nd point is the point
// on the sphere closest to the line.
// If 1 is returned, first point is obtained by evaluating
// the line and the second point is obtained by evaluating
// the sphere.
ON_DECL
int ON_Intersect(                  
        const ON_Line&, 
        const ON_Sphere&,
        ON_3dPoint&, 
        ON_3dPoint& // intersection point(s) returned here
        );


// Intersects an infinte line and cylinder and returns 
// 0 = no intersections, 
// 1 = one intersection, 
// 2 = 2 intersections
// 3 = line lies on cylinder
//
// If 0 is returned, first point is point 
// on line closest to cylinder and 2nd point is the point
// on the cylinder closest to the line.
// If 1 is returned, first point is obtained by evaluating
// the line and the second point is obtained by evaluating
// the cylinder.
//
// The value of cylinder.IsFinite() determines if the
// intersection is performed on the finite or infinite cylinder.
ON_DECL
int ON_Intersect( 
      const ON_Line&, // [in]
      const ON_Cylinder&, // [in]
      ON_3dPoint&, // [out] first intersection point
      ON_3dPoint& // [out] second intersection point
      );

// Description:
//   Intersect an infinte line and circle.
// Parameters:
//   line - [in]
//   circle - [in]
//   line_t0 - [out] line parameter of first intersection point
//   circle_point0 - [out] first intersection point on circle
//   line_t1 - [out] line parameter of second intersection point
//   circle_point1 - [out] second intersection point on circle
// Returns:
//   0     No intersection
//   1     One intersection at line.PointAt(*line_t0)
//   2     Two intersections at line.PointAt(*line_t0)
//         and line.PointAt(*line_t1).
ON_DECL
int ON_Intersect( 
                  const ON_Line& line, 
                  const ON_Circle& circle,
                  double* line_t0,
                  ON_3dPoint& circle_point0,
                  double* line_t1,
                  ON_3dPoint& circle_point1
                  );

// Description:
//   Intersect a infinte line and arc.
// Parameters:
//   line - [in]
//   arc - [in]
//   line_t0 - [out] line parameter of first intersection point
//   arc_point0 - [out] first intersection point on arc
//   line_t1 - [out] line parameter of second intersection point
//   arc_point1 - [out] second intersection point on arc
// Returns:
//   0     No intersection
//   1     One intersection at line.PointAt(*line_t0)
//   2     Two intersections at line.PointAt(*line_t0)
//         and line.PointAt(*line_t1).
ON_DECL
int ON_Intersect( 
                  const ON_Line& line, 
                  const ON_Arc& arc,
                  double* line_t0,
                  ON_3dPoint& arc_point0,
                  double* line_t1,
                  ON_3dPoint& arc_point1
                  );

// returns 0 = no, 1 = yes, 2 = points are coincident and on line
ON_DECL
int ON_ArePointsOnLine(
        int, // dimension of points
        int, // is_rat = TRUE if homogeneous rational
        int, // count = number of points
        int, // stride ( >= is_rat?(dim+1) :dim)
        const double*, // point array
        const ON_BoundingBox&, // if needed, use ON_GetBoundingBox(dim,is_rat,count,stride,point)
        const ON_Line&,
        double         // tolerance (if 0.0, a tolerance based on bounding box size is used)
        );

// returns 0 = no, 1 = yes, 2 = points are coincident and on line
ON_DECL
int ON_ArePointsOnPlane(
        int, // dimension of points
        int, // is_rat = TRUE if homogeneous rational
        int, // count = number of points
        int, // stride ( >= is_rat?(dim+1) :dim)
        const double*, // point array
        const ON_BoundingBox&, // if needed, use ON_GetBoundingBox(dim,is_rat,count,stride,point)
        const ON_Plane&,
        double         // tolerance (if 0.0, a tolerance based on bounding box size is used)
        );

/*
Description:
  Use the quotient rule to compute derivatives of a one parameter
  rational function F(t) = X(t)/W(t), where W is a scalar
  and F and X are vectors of dimension dim.
Parameters:
  dim - [in]
  der_count - [in] number of derivative (>=0)
  v_stride - [in] (>= dim+1)
  v - [in/out]
    v[] is an array of length (der_count+1)*v_stride.
    The input v[] array contains  derivatives of the numerator and
    denominator	functions in the order (X, W), (Xt, Wt), (Xtt, Wtt), ...
    In general, the (dim+1) coordinates of the d-th derivative 
    are in (v[n],...,v[n+dim]) where n = d*v_stride.
    In the output v[] array the derivatives of X are replaced with
    the derivatives of F and the derivatives of W are divided by
    w = v[dim].
Returns:
  True if input is valid; i.e., v[dim] != 0.
See Also:
  ON_EvaluateQuotientRule2
  ON_EvaluateQuotientRule3
*/
ON_DECL
bool ON_EvaluateQuotientRule( 
          int dim, 
          int der_count,
          int v_stride, 
          double *v 
          );

/*
Description:
  Use the quotient rule to compute partial derivatives of a two parameter
  rational function F(s,t) = X(s,t)/W(s,t), where W is a scalar
  and F and X are vectors of dimension dim.
Parameters:
  dim - [in]
  der_count - [in] number of derivative (>=0)
  v_stride - [in] (>= dim+1)
  v - [in/out]
    v[] is an array of length (der_count+2)*(der_count+1)*v_stride.
    The input array contains derivatives of the numerator and denominator
		functions in the order X, W, Xs, Ws, Xt, Wt, Xss, Wss, Xst, Wst, Xtt, Wtt, ...
    In general, the (i,j)-th derivatives are in the (dim+1) entries of v[]
		v[k], ..., answer[k+dim], where	k = ((i+j)*(i+j+1)/2 + j)*v_stride.
    In the output v[] array the derivatives of X are replaced with
    the derivatives of F and the derivatives of W are divided by
    w = v[dim].
Returns:
  True if input is valid; i.e., v[dim] != 0.
See Also:
  ON_EvaluateQuotientRule
  ON_EvaluateQuotientRule3
*/
ON_DECL
bool ON_EvaluateQuotientRule2( 
          int dim, 
          int der_count, 
          int v_stride, 
          double *v 
          );

/*
Description:
  Use the quotient rule to compute partial derivatives of a 3 parameter
  rational function F(r,s,t) = X(r,s,t)/W(r,s,t), where W is a scalar
  and F and X are vectors of dimension dim.
Parameters:
  dim - [in]
  der_count - [in] number of derivative (>=0)
  v_stride - [in] (>= dim+1)
  v - [in/out]
    v[] is an array of length 
    v_stride*(der_count+1)*(der_count+2)*(der_count+3)/6.
    The input v[] array contains  derivatives of the numerator and
    denominator	functions in the order (X, W), (Xr, Wr), (Xs, Ws),
    (Xt, Wt), (Xrr, Wrr), (Xrs, Wrs), (Xrt, Wrt), (Xss, Wss), 
    (Xst, Wst), (Xtt, Wtt), ...
    In general, the (dim+1) coordinates of the derivative 
    (Dr^i Ds^j Dt^k, i+j+k=d) are at v[n], ..., v[n+dim] where 
    n = v_stride*( d*(d+1)*(d+2)/6  +  (d-i)*(d-i+1)/2  +  k ).
    In the output v[] array the derivatives of X are replaced with
    the derivatives of F and the derivatives of W are divided by
    w = v[dim].
Returns:
  True if input is valid; i.e., v[dim] != 0.
See Also:
  ON_EvaluateQuotientRule
  ON_EvaluateQuotientRule2
*/
ON_DECL
bool ON_EvaluateQuotientRule3( 
          int dim, 
          int der_count, 
          int v_stride,
          double *v 
          );

ON_DECL
bool ON_GetPolylineLength(
        int,           // dimension of points
        BOOL,          // bIsRational TRUE if points are homogeneous rational
        int,           // number of points
        int,           // stride between points
        const double*, // points
        double*        // length returned here
        );


/*
Description:
  Find the index of the point in the point_list that is closest to P.
Parameters:
  point_count - [in]
  point_list - [in]
  P - [in]
  closest_point_index - [out]
Returns:
  True if successful and *closest_point_index is set.
  False if input is not valid, in which case *closest_point_index
  is undefined.
*/
ON_DECL
bool ON_GetClosestPointInPointList( 
          int point_count,
          const ON_3dPoint* point_list,
          ON_3dPoint P,
          int* closest_point_index
          );

/*
Description:
  Test math library functions.
Parameters:
  function_index - [in]  Determines which math library function is called.

           1:    z = x+y
           2:    z = x-y
           3:    z = x*y
           4:    z = x/y
           5:    z = fabs(x)
           6:    z = exp(x)
           7:    z = log(x)
           8:    z = logb(x)
           9:    z = log10(x)
          10:    z = pow(x,y)
          11:    z = sqrt(x)
          12:    z = sin(x)
          13:    z = cos(x)
          14:    z = tan(x)
          15:    z = sinh(x)
          16:    z = cosh(x)
          17:    z = tanh(x)
          18:    z = asin(x)
          19:    z = acos(x)
          20:    z = atan(x)
          21:    z = atan2(y,x)
          22:    z = fmod(x,y)
          23:    z = modf(x,&y)
          24:    z = frexp(x,&y)

  double x - [in]
  double y - [in]
Returns:
  Returns the "z" value listed in the function_index parameter
  description.
Remarks:
  This function is used to test the results of class floating
  point functions.  It is primarily used to see what happens
  when opennurbs is used as a DLL and illegal operations are
  performed.
*/
ON_DECL
double ON_TestMathFunction( 
        int function_index, 
        double x, 
        double y 
        );

ON_DECL double ON_Max(double a, double b);
ON_DECL float ON_Max(float a, float b);
ON_DECL int ON_Max(int a, int b);

ON_DECL double ON_Min(double a, double b);
ON_DECL float ON_Min(float a, float b);
ON_DECL int ON_Min(int a, int b);

ON_DECL int ON_Round(double x);
#endif
