/*
** Author:      James Painter
** Purpose:     Allocate memory for an N-d array.
**
** Copyright (c), 1988 GRAIL, University of Washington
**
** $Revision$
** $Date$
** $Locker$
** $Log$
** Revision 1.3  2006/04/05 19:21:56  brlcad
** more header/warning cleanup from Intel compiler warnings on Altix
**
** Revision 1.2  2006/03/24 19:36:13  brlcad
** quell compiler warnings
**
** Revision 1.1  2005/01/24 05:12:41  brlcad
** update of urt from 3.0 to 3.1b1; this includes keeping directory structure and moving tools to src/other/URToolkit/tools
**
** Revision 3.0.1.1  1992/04/30  14:10:24  spencer
** patch3: remove lint
**
** Revision 3.0  1992/01/23  16:24:19  spencer
** Initial revision.
**
 * Revision 1.1  89/02/08  12:48:33  jamie
 * Initial revision
 * 
*/

/* 
** Synopsis:
**   extern char *mallocNd();
**   'SomeDataType' *...*A;
**
**   A = ('SomeDataType' *...*) mallocNd( nDim, Dims, sizeof('SomeDataType'))
**     where 'SomeDataType' is the data type of the array elements

**           *...* is a string of nDim *'s.
**           nDim is the number of dimensions (unsigned int)
**           Dims[] contains the size of each dimension (left-to-right)
**                  (unsigned ints)
**
**   Reference values in A as though it was a compile-time 'nDim'-d array
**    e.g. A[i][j][k], for a 3-d array.
**
**   Free by calling free(A);
**
**   More information and an example follow.
*/

/*
 **  MallocNd allocates memory for an N-d array.  The array is stored
 **  contiguously (for linear access) but may be referenced through an 
 **  indirection table which is returned.  If not enough memory is available
 **  NULL is returned.
 **
 **   E.g.  if you want a 10x20x30  3-d array of doubles then:
 **
 **       static unsigned int Dims[3] = { 10, 20, 30 };
 **       double ***A = (double ***) (mallocNd( 3, Dims, sizeof(double) ));
 **
 **   Elements of A can be referenced as A[i][j][k] as you would for an
 **   ordinary 3-d array.  The base address of A can be found by:
 **    &(A[0][0][0]) or alternatively, just A[0][0].  Note that A itself is
 **   actually the address of the (double) indirection table; A[i] is 
 **   the address of a single indirection table; and  A[i][j] is the address 
 **   of one row of data.
 **
 **   Note that there is some space overhead for the indirection table.
 **   It is minimized if the dimensions are non-decreasing.  I.e.
 **   Dims[0] <= Dims[1] <= Dims[2] ...
 **
 **   There is a simple but complete example below (look for #ifdef TEST)
 */

/* Imports */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* Forward declarations */
char *BuildIndirectionTable();


/*  ------------------------------------------------------------------------ */
/*
**  A simple test case and example of mallocNd use.
*/

#ifdef TEST
main()
{
  int i, j, k, l;
  static unsigned Dims[5] = { 1, 2, 3, 4, 5 };
  double *****A, *B, val;
  extern char *mallocNd();

  /* A will be a 5-d */
  A = (double *****) mallocNd( 5, Dims, sizeof(double) );
  if (A == NULL)
    {
      fprintf( stderr, "No memory!");
      abort();
    }

  /* Fill with values so that the i'th dimension index is reflected in the 
  ** i'th base 10 digit of the value.
  */
  for(i=0; i<2; i++)
    for(j=0; j<3; j++)
      for(k=0; k<4; k++)
	for(l=0; l<5; l++) {
	  val = l + 10*(k + 10*(j + 10*i));
	  A[0][i][j][k][l] = val;
	}

  

  /* Print the array in linear order (tests that the array is really
  ** contiguous).
  */
  B = &(A[0][0][0][0][0]);
  for(i=0; i<120; i++) {
    printf( "%6.0f ", B[i] );
    if ((i %10) == 9) putchar('\n');
  }
}

#endif
/*  ------------------------------------------------------------------------ */


char *mallocNd( nDim, Dims, sizeofelt )
     unsigned int nDim;		/* The number of dimensions */
     unsigned int Dims[];	/* The size of each dimension (left-to-right) */
     unsigned int sizeofelt;	/* The number of bytes for each element */
{
  int i;
  char *ptr, *tableBase, *arrayBase, *nextFreeArray, *nextFreeTable;
  unsigned int arrayBytesNeeded, tableBytesNeeded, dimProduct, slop;
  
  /* Count up the space needed for the indirection tables.
   **  It's sizeof(pointer) * Dim[0] * ( 1 + Dim[1] * ( 1 + Dim[2] ... ))
   */
  tableBytesNeeded = 0;
  dimProduct  = 1;
  for(i=0; i<nDim-1;  i++)
    {
      dimProduct *= Dims[i];
      tableBytesNeeded += dimProduct*sizeof(char *);
    }
  
  /* Add in extra space to force alignment of the array */
  slop = (tableBytesNeeded % sizeofelt);
  if (slop != 0) 
    slop = sizeofelt - slop;

  /*  Add in the space for the array itself */
  dimProduct *= Dims[i];
  arrayBytesNeeded = dimProduct*sizeofelt;
  
  /* Get the memory for the whole thing */
  ptr = malloc( tableBytesNeeded + slop + arrayBytesNeeded );
  if (ptr == NULL) return (char *) NULL;
  
  /* Set up the base address of the actual array and the indirection table */
  tableBase = ptr;
  arrayBase = ptr + slop + tableBytesNeeded;
  
  /* Set up the next free pointers for the table space and the array space */
  nextFreeTable = tableBase;
  nextFreeArray = arrayBase;
  
  /* Build the indirection tables recursively */
  (void) BuildIndirectionTable( nDim, Dims,  (char ***)&nextFreeTable, 
			       &nextFreeArray, sizeofelt );

  /* Do a sanity check to be sure we haven't run off the end of the allocated
  ** memory.  Not strictly needed but here for robustness and debugging.
  */
  if (nextFreeTable != arrayBase - slop  ||
      nextFreeArray != ptr + tableBytesNeeded + slop + arrayBytesNeeded)
    {
      fprintf( stderr, "Error in mallocNd!  Memory overwrite." );
      abort();
    }
  return(char *) tableBase;
}



char * BuildIndirectionTable( nDim, Dims, nextFreeTable, nextFreeArray, 
			     sizeofelt )
     unsigned nDim;
     unsigned Dims[];
     char ***nextFreeTable, **nextFreeArray;
     unsigned sizeofelt;
{
  int i;
  char *ReturnValue, **table;
  
  if (nDim  == 1) { /* Base case, indirection table unneeded */
    ReturnValue =  *nextFreeArray;
    *nextFreeArray += sizeofelt * Dims[0];
  } else {	
    /* Allocate the 1'st level indirection table and fill it in 
    **  calling ourselves recursively for the others.
     */
    table =  *nextFreeTable;
    *nextFreeTable += Dims[0];
    for(i=0; i<Dims[0]; i++)
      {
	table[i] = BuildIndirectionTable( nDim-1, Dims+1, nextFreeTable, 
					 nextFreeArray, sizeofelt );
      }
    ReturnValue = (char *) table;
  }
  return ReturnValue;
}


/* 
**  Special case for 2-d arrays
*/
char *
malloc_2d(n,m,sizeofelt)
int n, m, sizeofelt;
{
  unsigned Dims[2];

  Dims[0] = n;
  Dims[1] = m;
  return mallocNd(2,Dims,sizeofelt);
}
