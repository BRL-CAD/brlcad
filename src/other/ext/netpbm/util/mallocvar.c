#include <limits.h>
#include <stdlib.h>

#include "pm_c_util.h"


#include "mallocvar.h"


static void *
mallocz(size_t const size) {
/*----------------------------------------------------------------------------
   Same as malloc(), except it is legal to allocate zero bytes.
-----------------------------------------------------------------------------*/
    return malloc(MAX(1, size));
}



static void
allocarrayNoHeap(void **      const rowIndex,
                 unsigned int const rows,
                 unsigned int const cols,
                 unsigned int const elementSize,
                 bool *       const failedP) {

    unsigned int rowsDone;

    for (rowsDone = 0, *failedP = false; rowsDone < rows && !*failedP; ) {
        void * rowSpace;

        mallocProduct(&rowSpace, cols, elementSize);
        
        if (rowSpace == NULL) {
            unsigned int row;

            *failedP = true;

            /* unwind partially completed job */
            for (row = 0; row < rowsDone; ++row)
                free(rowIndex[row]);
        } else
            rowIndex[rowsDone++] = rowSpace;
    }
}



static unsigned char *
allocRowHeap(unsigned int const rows,
             unsigned int const cols,
             unsigned int const size) {
/*----------------------------------------------------------------------------
   Allocate a row heap.  That's a chunk of memory for use in a
   pm_mallocarray2 two-dimensional array to contain the rows.

   The heap must fit 'rows' rows of 'cols' columns each of elements
   'size' bytes in size.

   Return NULL if we can't get the memory.
-----------------------------------------------------------------------------*/
    unsigned char * retval;

    if (cols != 0 && rows != 0 && UINT_MAX / cols / rows < size)
        /* Too big even to request the memory ! */
        retval = NULL;
    else
        retval = mallocz(rows * cols * size);

    return retval;
}



void
pm_mallocarray2(void **      const resultP,
                unsigned int const rows,
                unsigned int const cols,
                unsigned int const elementSize)  {
/*----------------------------------------------------------------------------
   Allocate an array of 'rows' rows of 'cols' columns each, with each
   element 'size' bytes.

   We use the C multidimensional array paradigm:  The array is a row
   index (array of pointers to rows) plus an array of elements for each
   of those rows.  So a[row][col] gives you the element of the two
   dimensional array at Row 'row', Column 'col'.

   Each array element is ideally aligned to an 'elementSize' boundary.  But we
   guarantee this only for 1, 2, 4, 8, and 16, because of limitations of libc
   malloc() (which we use to allocate all the memory).

   We tack on two extra elements to the end of the row index, transparent to
   the user, for use in memory management (in particular, in destroying the
   array).  The first is a NULL pointer, so you can tell the vertical
   dimension of the array.  The second points to the row heap (see below).

   We have two ways of allocating the space: fragmented and unfragmented.  In
   both, the row index (plus the extra elements) is in one block of memory.
   In the fragmented format, each row is also in an independent memory block,
   and the row heap pointer (see above) is NULL.  In the unfragmented format,
   all the rows are in a single block of memory called the row heap and the
   row heap pointer points to that.

   We use unfragmented format if possible, but if the allocation of the
   row heap fails, we fall back to fragmented.
-----------------------------------------------------------------------------*/
    void ** rowIndex;
    bool failed;

    MALLOCARRAY(rowIndex, rows + 1 + 1);
    if (rowIndex == NULL)
        failed = true;
    else {
        unsigned char * rowheap;

        rowheap = allocRowHeap(cols, rows, elementSize);

        if (rowheap) {
            /* It's unfragmented format */

            rowIndex[rows+1] = rowheap;  /* Declare it unfragmented format */

            if (rowheap) {
                unsigned int row;
                
                for (row = 0; row < rows; ++row)
                    rowIndex[row] = &(rowheap[row * cols * elementSize]);
            }
            failed = false;
        } else {
            /* We couldn't get the whole heap in one block, so try fragmented
               format.
            */
            rowIndex[rows+1] = NULL;   /* Declare it fragmented format */
            
            allocarrayNoHeap(rowIndex, rows, cols, elementSize, &failed);
        }
        rowIndex[rows+0] = NULL;   /* mark end of rows */
    }
    if (failed)
        *resultP = NULL;
    else
        *resultP = rowIndex;
}



static unsigned int
array2RowCount(void ** const rowIndex) {
/*----------------------------------------------------------------------------
   Return the number of rows in the 2-dimensional array.
-----------------------------------------------------------------------------*/
    /* The end of the rows is marked by a null pointer where a row 
       pointer otherwise would be.
    */

    unsigned int row;

    for (row = 0; rowIndex[row]; ++row);

    return row;
}



void
pm_freearray2(void ** const rowIndex) {

    unsigned int const rows = array2RowCount(rowIndex);

    void * const rowheap = rowIndex[rows+1];

    if (rowheap != NULL)
        free(rowheap);
    else {
        /* Free each individually malloced row */
        unsigned int row;
        for (row = 0; row < rows; ++row)
            free(rowIndex[row]);
    }
    free(rowIndex);
}


