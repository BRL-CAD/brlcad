/* This library module contains "ppmdraw" routines for drawing paths.

   By Bryan Henderson San Jose CA 06.05.24.
   Contributed to the public domain.

   I actually wrote this before I knew ppmd_fill() already existed.
   ppmd_fill() is more general.  But path.c is probably faster and is
   better code, so I'm keeping it for now.
*/

#include <assert.h>

#include "pm_c_util.h"
#include "mallocvar.h"
#include "ppm.h"
#include "ppmdfont.h"
#include "ppmdraw.h"



/* This is the algorithm we use to fill a path.

   A path is a set of points that form a closed figure.  The last point
   and first point are identical.  But the path may cross itself other
   places too.  Our job is to color the interior of that figure.

   We do it with horizontal lines.  We follow the path around from
   start to finish, visiting every point in it.  We remember in a
   stack the points we've been to.  As long as we keep going in the
   same vertical direction, that stack grows.  When we turn around and
   visit a row that we've been to before, we drop a horizontal line of
   fill color from where we are now to where we were the last time we
   visited that row, and then remove that entry from the stack.  Note
   that because we go one point at a time, the entry on the stack for
   the row we're at now will always be on the top of stack.
   
   Note that the points on the stack always have consecutive row
   numbers, monotonically increasing or decreasing, whichever is the
   direction we started out in.

   This goes on, with the stack alternately growing and shrinking as
   the path turns to head up and down, until we get back to the row
   where we started.  At this point, the stack becomes empty following
   the algorithm in the previous paragraph.  If the path crosses over
   and keeps going, we just start filling the stack again, with the
   entries now going in the opposite direction from before -- e.g.
   if the path started out heading downward, the points in the stack
   mononotically increased in row number; after crossing back over,
   the points in the stack monotonically decrease in row number.


   It's probably more efficient to use vertical lines than horizontal
   ones when the image is tall and narrow.  But we're not that
   sophisticated.
*/


/* NOTE NOTE NOTE

   In all the path logic below, we call the direction of increasing row
   number "up" because we think of the raster as standard Cartesian
   plane.  So visualize the image as upside down in the first quadrant
   of the Cartesian plane -- the real top left corner of the image is at
   the origin.
*/



static bool
pointEqual(ppmd_point const comparator,
           ppmd_point const comparand) {

    return comparator.x == comparand.x && comparator.y == comparand.y;
}



static int
vertDisp(ppmd_point const begPoint,
         ppmd_point const endPoint) {
/*----------------------------------------------------------------------------
   Return the vertical displacement of 'endPoint' with respect to
   'begPoint' -- How much higher 'endPoint' is than 'begPoint'.
-----------------------------------------------------------------------------*/
    return endPoint.y - begPoint.y;
}



static int
horzDisp(ppmd_point const begPoint,
         ppmd_point const endPoint) {
/*----------------------------------------------------------------------------
   Return the horizontal displacement of 'endPoint' with respect to
   'begPoint' -- How much further right 'endPoint' is than 'begPoint'.
-----------------------------------------------------------------------------*/
    return endPoint.x - begPoint.x;
}



static bool
isOnLineSeg(ppmd_point const here,
            ppmd_point const begPoint,
            ppmd_point const endPoint) {

    return
        here.y >= MIN(begPoint.y, endPoint.y) &&
        here.y <= MAX(begPoint.y, endPoint.y) &&
        here.x >= MIN(begPoint.x, endPoint.x) &&
        here.x <= MAX(begPoint.x, endPoint.x);
}    



static double
lineSlope(ppmd_point const begPoint,
          ppmd_point const endPoint) {
/*----------------------------------------------------------------------------
   Return the slope of the line that begins at 'begPoint' and ends at
   'endPoint'.

   Positive slope means as row number increases, column number increases.
-----------------------------------------------------------------------------*/
    return (double)vertDisp(begPoint, endPoint) / horzDisp(begPoint, endPoint);
}



typedef struct {
/*----------------------------------------------------------------------------
   A complicated structure for tracking the state of a the filling
   algorithm.  See description of algorithm above.
-----------------------------------------------------------------------------*/
    ppmd_point * stack;
        /* An array which holds the fundamental stack.  The bottom of the
           stack is element 0.  It grows consecutively up.
        */
    unsigned int topOfStack;
        /* Index in stack[] of the top of the entry which will hold next
           _next_ element pushed onto the stack.
        */
    unsigned int stackSize;
        /* Number of elements in stack[] -- i.e. maximum height of stack */
    int step;
        /* -1 or 1.  -1 means each new point pushed on the stack is one
           unit below the previous one; 1 means each new point pushed on
           the stack is one unit above the previous one.
        */
} fillStack;



static void
createStack(fillStack ** const stackPP) {
/*----------------------------------------------------------------------------
   Create an empty fill stack.
-----------------------------------------------------------------------------*/
    fillStack * stackP;

    MALLOCVAR_NOFAIL(stackP);

    stackP->stackSize = 1024;

    MALLOCARRAY(stackP->stack, stackP->stackSize);

    if (stackP->stack == NULL)
        pm_error("Could not allocate memory for a fill stack of %u points",
                 stackP->stackSize);

    stackP->topOfStack = 0;

    stackP->step = +1;  /* arbitrary choice */

    *stackPP = stackP;
}



static void
destroyStack(fillStack * const stackP) {

    free(stackP->stack);

    free(stackP);
}



static ppmd_point
topOfStack(fillStack * const stackP) {

    assert(stackP->topOfStack > 0);

    return stackP->stack[stackP->topOfStack-1];
}



static bool
stackIsEmpty(fillStack * const stackP) {

    return stackP->topOfStack == 0;
}



static bool
inStackDirection(fillStack * const stackP,
                 ppmd_point  const point) {
/*----------------------------------------------------------------------------
   Return true iff the point 'point' is is a step in the current stack
   direction from the point on the top of the stack.  I.e. we could push
   this point onto the stack without violating the monotonic property.
-----------------------------------------------------------------------------*/
    if (stackIsEmpty(stackP))
        return true;
    else
        return topOfStack(stackP).y + stackP->step == point.y;
}



static bool
againstStackDirection(fillStack * const stackP,
                      ppmd_point  const point) {
/*----------------------------------------------------------------------------
   Return true iff the point 'point' is a step in the other direction
   form the current stack direction.
-----------------------------------------------------------------------------*/
    if (stackIsEmpty(stackP))
        return false;
    else
        return topOfStack(stackP).y - stackP->step == point.y;
}



static bool
isLateralFromTopOfStack(fillStack * const stackP,
                        ppmd_point  const point) {
/*----------------------------------------------------------------------------
   Return true iff the point 'point' is laterally across from the point
   on the top of the stack.
-----------------------------------------------------------------------------*/
    if (stackIsEmpty(stackP))
        return false;
    else
        return point.y == topOfStack(stackP).y;
}



static void
pushStack(fillStack * const stackP,
          ppmd_point  const newPoint) {

    assert(inStackDirection(stackP, newPoint));

    if (stackP->topOfStack >= stackP->stackSize) {
        stackP->stackSize *= 2;

        REALLOCARRAY(stackP->stack, stackP->stackSize);

        if (stackP->stack == NULL)
            pm_error("Could not allocate memory for a fill stack of %u points",
                     stackP->stackSize);
    }
    assert(stackP->topOfStack < stackP->stackSize);

    stackP->stack[stackP->topOfStack++] = newPoint;
pm_message("pushed (%u, %u) at %u", newPoint.x, newPoint.y, stackP->topOfStack-1);
}



static ppmd_point
popStack(fillStack * const stackP) {

    ppmd_point retval;

    assert(stackP->topOfStack < stackP->stackSize);

    retval = stackP->stack[--stackP->topOfStack];
pm_message("popped (%u, %u) at %u", retval.x, retval.y, stackP->topOfStack);
    return retval;
}



static void
replaceTopOfStack(fillStack * const stackP,
                  ppmd_point  const point) {

    assert(stackP->topOfStack > 0);

    stackP->stack[stackP->topOfStack-1] = point;
}



static void
reverseStackDirection(fillStack * const stackP) {

    stackP->step *= -1;
}



static void
drawFillLine(ppmd_point const begPoint,
             ppmd_point const endPoint,
             pixel **   const pixels,
             pixel      const color) {

    unsigned int leftCol, rghtCol;
    unsigned int col;
    unsigned int row;

    /* Fill lines are always horizontal */

    assert(begPoint.y == endPoint.y);

pm_message("filling from (%u, %u) to (%u, %u)", begPoint.x, begPoint.y, endPoint.x, endPoint.y);
    row = begPoint.y;

    if (begPoint.x <= endPoint.x) {
        leftCol = begPoint.x;
        rghtCol = endPoint.x;
    } else {
        leftCol = endPoint.x;
        rghtCol = begPoint.x;
    }

    for (col = leftCol; col <= rghtCol; ++col)
        pixels[row][col] = color;
}



static void
fillPoint(fillStack * const stackP,
          ppmd_point  const point,
          pixel **    const pixels,
          pixel       const color) {
/*----------------------------------------------------------------------------
   Follow the outline of the figure to the point 'point', which is adjacent
   to the point most recently added.

   Fill the image in 'pixels' with color 'color' and update *stackP as
   required.
-----------------------------------------------------------------------------*/
pm_message("filling point (%u, %u)", point.x, point.y);
    if (inStackDirection(stackP, point)) {
        pushStack(stackP, point);
        pixels[point.y][point.x] = color;
    } else {
        if (againstStackDirection(stackP, point))
            popStack(stackP);
        
        if (stackIsEmpty(stackP)) {
            reverseStackDirection(stackP);
            pushStack(stackP, point);
        } else {
            assert(isLateralFromTopOfStack(stackP, point));
            
            drawFillLine(topOfStack(stackP), point, pixels, color);
            replaceTopOfStack(stackP, point);
        }
    }
}



static void
fillLeg(ppmd_point  const begPoint,
        ppmd_point  const endPoint,
        fillStack * const stackP,
        pixel **    const pixels,
        pixel       const color) {
/*----------------------------------------------------------------------------
   Follow the leg which is a straight line segment from 'begPoint'
   through 'endPoint', filling the raster 'pixels' with color 'color',
   according to *stackP as we go.

   We update *stackP accordingly.

   A leg starts where the leg before it ends, so we skip the first point
   in the line segment.
-----------------------------------------------------------------------------*/
    assert(!stackIsEmpty(stackP));

    if (endPoint.y == begPoint.y)
        /* Line is horizontal; We need just the end point. */
        fillPoint(stackP, endPoint, pixels, color);
    else {
        double const invSlope = 1/lineSlope(begPoint, endPoint);
        int const step = endPoint.y > begPoint.y ? +1 : -1;

        ppmd_point here;

        here = begPoint;
    
        while (here.y != endPoint.y) {
            here.y += step;
            here.x = ROUNDU(begPoint.x + vertDisp(begPoint, here) * invSlope);

            assert(isOnLineSeg(here, begPoint, endPoint));

            fillPoint(stackP, here, pixels, color);
        }
    }
}



void
ppmd_fill_path(pixel **      const pixels, 
               int           const cols, 
               int           const rows, 
               pixval        const maxval,
               ppmd_path *   const pathP,
               pixel         const color) {
/*----------------------------------------------------------------------------
   Draw a path which defines a closed figure (or multiple closed figures)
   and fill it in.

   *pathP describes the path.  'color' is the color with which to fill.

   'pixels' is the canvas on which to draw the figure; its dimensions are
   'cols' x 'rows'.

   'maxval' is the maxval for 'color' and for 'pixels'.

   Fail (abort the program) if the path does not end on the same point at
   which it began.
-----------------------------------------------------------------------------*/
    ppmd_point prevVertex;
    fillStack * stackP;
    unsigned int legNumber;

    createStack(&stackP);

    prevVertex = pathP->begPoint;
    pushStack(stackP, pathP->begPoint);

    for (legNumber = 0; legNumber < pathP->legCount; ++legNumber) {
        ppmd_pathleg * const legP = &pathP->legs[legNumber];
        ppmd_point const nextVertex = legP->u.linelegparms.end;

        if (prevVertex.y >= rows || nextVertex.y >= rows)
            pm_error("Path extends below the image.");
        if (prevVertex.x >= cols || nextVertex.x >= cols)
            pm_error("Path extends off the image to the right.");

        fillLeg(prevVertex, nextVertex, stackP, pixels, color);

        prevVertex = nextVertex;
    }
    if (!pointEqual(prevVertex, pathP->begPoint))
        pm_error("Failed to fill a path -- the path is not closed "
                 "(i.e. it doesn't end up at the same point where it began)");

    assert(pointEqual(popStack(stackP), pathP->begPoint));
    assert(stackIsEmpty(stackP));

    destroyStack(stackP);
}
