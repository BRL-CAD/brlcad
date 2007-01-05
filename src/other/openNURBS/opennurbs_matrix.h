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

#if !defined(ON_MATRIX_INC_)
#define ON_MATRIX_INC_

class ON_Xform;

class ON_CLASS ON_Matrix
{
public:
  ON_Matrix();
  ON_Matrix( 
     int, // number of rows
     int  // number of columns
     );
  ON_Matrix( // see ON_Matrix::Create(int,int,int,int) for details
     int, // first valid row index
     int, // last valid row index
     int, // first valid column index
     int  // last valid column index
     );
  ON_Matrix( const ON_Xform& );
  ON_Matrix( const ON_Matrix& );
  virtual ~ON_Matrix();
  void EmergencyDestroy(); // call if memory pool used matrix by becomes invalid

  // ON_Matrix[i][j] = value at row i and column j
  //           0 <= i < RowCount()
  //           0 <= j < ColCount()
  double* operator[](int);
  const double* operator[](int) const;

  ON_Matrix& operator=(const ON_Matrix&);
  ON_Matrix& operator=(const ON_Xform&);

  bool IsValid() const;
  int IsSquare() const; // returns 0 for no and m_row_count (= m_col_count) for yes
  int RowCount() const;
  int ColCount() const;
  int MinCount() const; // smallest of row and column count
  int MaxCount() const; // largest of row and column count

  void RowScale(int,double); 
  void ColScale(int,double);
  void RowOp(int,double,int);
  void ColOp(int,double,int);

  bool Create(
     int, // number of rows
     int  // number of columns
     );

  bool Create( // E.g., Create(1,5,1,7) creates a 5x7 sized matrix that with
               // "top" row = m[1][1],...,m[1][7] and "bottom" row
               // = m[5][1],...,m[5][7].  The result of Create(0,m,0,n) is
               // identical to the result of Create(m+1,n+1).
     int, // first valid row index
     int, // last valid row index
     int, // first valid column index
     int  // last valid column index
     );

  void Destroy();

  void Zero();

  void SetDiagonal(double); // sets diagonal value and zeros off diagonal values
  void SetDiagonal(const double*); // sets diagonal values and zeros off diagonal values
  void SetDiagonal(int, const double*); // sets size to count x count and diagonal values and zeros off diagonal values
  void SetDiagonal(const ON_SimpleArray<double>&); // sets size to length X lengthdiagonal values and zeros off diagonal values

  bool Transpose();

  bool SwapRows( int, int ); // ints are row indices to swap
  bool SwapCols( int, int ); // ints are col indices to swap
  bool Invert( 
          double // zero tolerance
          );

  // safe arithmetic
  bool Multiply( const ON_Matrix&, const ON_Matrix& );
  bool Add( const ON_Matrix&, const ON_Matrix& );
  bool Scale( double );


  // Description:
  //   Row reduce a matrix to calculate rank and determinant.
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) zero tolerance for pivot test
  //       If a the absolute value of a pivot is <= zero_tolerance,
  //       then the pivoit is assumed to be zero.
  //   determinant - [out] value of determinant is returned here.
  //   pivot - [out] value of the smallest pivot is returned here
  // Returns:
  //   Rank of the matrix.
  // Remarks:
  //   The matrix itself is row reduced so that the result is
  //   an upper triangular matrix with 1's on the diagonal.
  int RowReduce( // returns rank
    double,  // zero_tolerance
    double&, // determinant
    double&  // pivot
    ); 

  // Description:
  //   Row reduce a matrix as the first step in solving M*X=B where
  //   B is a column of values.
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) zero tolerance for pivot test
  //       If a the absolute value of a pivot is <= zero_tolerance,
  //       then the pivoit is assumed to be zero.
  //   B - [in/out] an array of m_row_count values that is row reduced
  //       with the matrix.
  //   determinant - [out] value of determinant is returned here.
  //   pivot - [out] If not NULL, then the value of the smallest 
  //       pivot is returned here
  // Returns:
  //   Rank of the matrix.
  // Remarks:
  //   The matrix itself is row reduced so that the result is
  //   an upper triangular matrix with 1's on the diagonal.
  // Example:
  //   Solve M*X=B;
  //   double B[m] = ...;
  //   double B[n] = ...;
  //   ON_Matrix M(m,n) = ...;
  //   M.RowReduce(ON_ZERO_TOLERANCE,B); // modifies M and B
  //   M.BackSolve(m,B,X); // solution is in X
  // See Also: 
  //   ON_Matrix::BackSolve
  int RowReduce(
    double,        // zero_tolerance
    double*,       // B
    double* = NULL // pivot
    ); 

  // Description:
  //   Row reduce a matrix as the first step in solving M*X=B where
  //   B is a column of 3d points
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) zero tolerance for pivot test
  //       If a the absolute value of a pivot is <= zero_tolerance,
  //       then the pivoit is assumed to be zero.
  //   B - [in/out] an array of m_row_count 3d points that is 
  //       row reduced with the matrix.
  //   determinant - [out] value of determinant is returned here.
  //   pivot - [out] If not NULL, then the value of the smallest 
  //       pivot is returned here
  // Returns:
  //   Rank of the matrix.
  // Remarks:
  //   The matrix itself is row reduced so that the result is
  //   an upper triangular matrix with 1's on the diagonal.
  // See Also: 
  //   ON_Matrix::BackSolve
  int RowReduce(
    double,        // zero_tolerance
    ON_3dPoint*,   // B
    double* = NULL // pivot
    ); 

  // Description:
  //   Row reduce a matrix as the first step in solving M*X=B where
  //   B is a column arbitrary dimension points.
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) zero tolerance for pivot test
  //       If a the absolute value of a pivot is <= zero_tolerance,
  //       then the pivoit is assumed to be zero.
  //   pt_dim - [in] dimension of points
  //   pt_stride - [in] stride between points (>=pt_dim)
  //   pt - [in/out] array of m_row_count*pt_stride values.
  //        The i-th point is
  //        (pt[i*pt_stride],...,pt[i*pt_stride+pt_dim-1]).
  //        This array of points is row reduced along with the 
  //        matrix.
  //   pivot - [out] If not NULL, then the value of the smallest 
  //       pivot is returned here
  // Returns:
  //   Rank of the matrix.
  // Remarks:
  //   The matrix itself is row reduced so that the result is
  //   an upper triangular matrix with 1's on the diagonal.
  // See Also: 
  //   ON_Matrix::BackSolve
  int RowReduce( // returns rank
    double,      // zero_tolerance
    int,         // pt_dim
    int,         // pt_stride
    double*,     // pt
    double* = NULL // pivot
    ); 

  // Description:
  //   Solve M*X=B where M is upper triangular with a unit diagonal and
  //   B is a column of values.
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) used to test for "zero" values in B
  //       in under determined systems of equations.
  //   Bsize - [in] (>=m_row_count) length of B.  The values in
  //       B[m_row_count],...,B[Bsize-1] are tested to make sure they are
  //       "zero".
  //   B - [in] array of length Bsize.
  //   X - [out] array of length m_col_count.  Solutions returned here.
  // Remarks:
  //   Actual values M[i][j] with i <= j are ignored. 
  //   M[i][i] is assumed to be one and M[i][j] i<j is assumed to be zero.
  //   For square M, B and X can point to the same memory.
  // See Also:
  //   ON_Matrix::RowReduce
  bool BackSolve(
    double,        // zero_tolerance
    int,           // Bsize
    const double*, // B
    double*        // X
      ) const;

  // Description:
  //   Solve M*X=B where M is upper triangular with a unit diagonal and
  //   B is a column of 3d points.
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) used to test for "zero" values in B
  //       in under determined systems of equations.
  //   Bsize - [in] (>=m_row_count) length of B.  The values in
  //       B[m_row_count],...,B[Bsize-1] are tested to make sure they are
  //       "zero".
  //   B - [in] array of length Bsize.
  //   X - [out] array of length m_col_count.  Solutions returned here.
  // Remarks:
  //   Actual values M[i][j] with i <= j are ignored. 
  //   M[i][i] is assumed to be one and M[i][j] i<j is assumed to be zero.
  //   For square M, B and X can point to the same memory.
  // See Also:
  //   ON_Matrix::RowReduce
  bool BackSolve(
    double,            // zero_tolerance
    int,               // Bsize
    const ON_3dPoint*, // B
    ON_3dPoint*        // X
      ) const;

  // Description:
  //   Solve M*X=B where M is upper triangular with a unit diagonal and
  //   B is a column of points
  // Parameters:
  //   zero_tolerance - [in] (>=0.0) used to test for "zero" values in B
  //       in under determined systems of equations.
  //   pt_dim - [in] dimension of points
  //   Bsize - [in] (>=m_row_count) number of points in B[].  The points
  //       correspoinding to indices m_row_count, ..., (Bsize-1)
  //       are tested to make sure they are "zero".
  //   Bpt_stride - [in] stride between B points (>=pt_dim)
  //   Bpt - [in/out] array of m_row_count*Bpt_stride values.
  //        The i-th B point is
  //        (Bpt[i*Bpt_stride],...,Bpt[i*Bpt_stride+pt_dim-1]).
  //   Xpt_stride - [in] stride between X points (>=pt_dim)
  //   Xpt - [out] array of m_col_count*Xpt_stride values.
  //        The i-th X point is
  //        (Xpt[i*Xpt_stride],...,Xpt[i*Xpt_stride+pt_dim-1]).
  // Remarks:
  //   Actual values M[i][j] with i <= j are ignored. 
  //   M[i][i] is assumed to be one and M[i][j] i<j is assumed to be zero.
  //   For square M, B and X can point to the same memory.
  // See Also:
  //   ON_Matrix::RowReduce
  bool BackSolve(
    double,       // zero_tolerance
    int,          // pt_dim
    int,          // Bsize
    int,          // Bpt_stride
    const double*,// Bpt
    int,          // Xpt_stride
    double*       // Xpt
      ) const;

  bool IsRowOrthoganal() const;
  bool IsRowOrthoNormal() const;

  bool IsColOrthoganal() const;
  bool IsColOrthoNormal() const;

  double** m; // m[i][j] = value at row i and column j
              //           0 <= i < RowCount()
              //           0 <= j < ColCount()
private:
  int m_row_count;
  int m_col_count;
  ON_SimpleArray<double*> m_row; // m_M[i][j] = row i and column j

  // coefficient memory
  // 8 July 2003 - changed coefficient memory handling to use
  //               multiple chunks on large matrices.
	void* m_reserved1;
	int   m_reserved2;
	int   m_reserved3;
  void* m_cmem;
  //ON_SimpleArray<double> m_a; // array of at least doubles
};

/*
Description:
  Perform simple row reduction on a matrix.  If A is square, positive
  definite, and really really nice, then the returned B is the inverse
  of A.  If A is not positive definite and really really nice, then it
  is probably a waste of time to call this function.
Parameters:
  row_count - [in]
  col_count - [in]
  zero_pivot - [in]
    absolute values <= zero_pivot are considered to be zero
  A - [in/out]
    A row_count X col_count matrix.  Input is the matrix to be
    row reduced.  The calculation destroys A, so output A is garbage.
  B - [out]
    A a row_count X row_count matrix. That records the row reduction.
  pivots - [out]
    minimum and maximum absolute values of pivots.
Returns:
  Rank of A.  If the returned value < min(row_count,col_count),
  then a zero pivot was encountered.
  If C = input value of A, then B*C = (I,*)
*/
ON_DECL
int ON_RowReduce( 
          int row_count, 
          int col_count,
          double zero_pivot,
          double** A, 
          double** B, 
          double pivots[2] 
          );

#endif
