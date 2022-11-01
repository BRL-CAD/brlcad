/* ppmdraw.h - header file for simple drawing routines in libppm
**
** Simple, yes, and also fairly slow if the truth be told; but also very
** flexible and powerful.
**
** The two basic concepts are the drawproc and clientdata.  All the drawing
** routines take a drawproc that does the actual drawing.  A drawproc draws
** a single point, and it looks like this:
*/

#include "pm_config.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif


typedef struct {
    int x;
    int y;
} ppmd_point;

static __inline__ ppmd_point
ppmd_makePoint(int const x,
               int const y) {

    ppmd_point p;

    p.x = x;
    p.y = y;

    return p;
}

void
ppmd_validateCoord(int const c);

void
ppmd_validatePoint(ppmd_point const p);

typedef enum {
    PPMD_PATHLEG_LINE
} ppmd_pathlegtype;

struct ppmd_linelegparms {
    ppmd_point end;
};

typedef struct {
/*----------------------------------------------------------------------------
   A leg of a ppmd_path.
-----------------------------------------------------------------------------*/
    ppmd_pathlegtype type;
    union {
        struct ppmd_linelegparms linelegparms;
    } u;
} ppmd_pathleg;

typedef struct {
/*----------------------------------------------------------------------------
   A closed path
-----------------------------------------------------------------------------*/
    unsigned int version;
        /* Must be zero.  For future expansion. */
    ppmd_point   begPoint;  
    unsigned int legCount;
        /* Number of legs in the path; i.e. size of 'legs' array */
    size_t       legSize;
        /* Size of storage occupied by one ppmd_pathleg.  I.e.
           sizeof(ppmd_pathleg).  Used for
           binary backward compatibility between callers and libppmd
           as the definition of ppmd_pathleg changes.
        */
    ppmd_pathleg * legs;
} ppmd_path;



typedef void ppmd_drawprocp(pixel **, unsigned int, unsigned int,
                            pixval, ppmd_point, const void *);
typedef void ppmd_drawproc(pixel **, int, int, pixval, int, int, const void *);

ppmd_drawprocp ppmd_point_drawprocp;
ppmd_drawproc ppmd_point_drawproc;

/*
** So, you call a drawing routine, e.g. ppmd_line(), and pass it a drawproc;
** it calls the drawproc for each point it wants to draw.  Why so complicated?
** Because you can define your own drawprocs to do more interesting things than
** simply draw the point.  For example, you could make one that calls back into
** another drawing routine, say ppmd_circle() to draw a circle at each point
** of a line.
**
** Slow?  Well sure, we're talking results here, not realtime.  You can do
** tricks with this arrangement that you couldn't even think of before.
** Still, to speed things up for the 90% case you can use this:
*/
#define PPMD_NULLDRAWPROC NULL
/*
** Just like ppmd_point_drawproc() it simply draws the point, but it's done
** inline, and clipping is assumed to be handled at a higher level.
**
** Now, what about clientdata.  Well, it's an arbitrary pointer, and can
** mean something different to each different drawproc.  For the above two
** drawprocs, clientdata should be a pointer to a pixel holding the color
** to be drawn.  Other drawprocs can use it to point to something else,
** e.g. some structure to be modified, or they can ignore it.
*/


/* Outline drawing routines.  Lines, splines, circles, etc. */

int 
ppmd_setlinetype(int const type);

#define PPMD_LINETYPE_NORMAL 0
#define PPMD_LINETYPE_NODIAGS 1
/* If you set NODIAGS, all pixels drawn by ppmd_line() will be 4-connected
** instead of 8-connected; in other words, no diagonals.  This is useful
** for some applications, for example when you draw many parallel lines
** and you want them to fit together without gaps.
*/

int
ppmd_setlineclip(int const clip);

#define ppmd_setlineclipping(x)     ppmd_setlineclip(x)
/* Normally, ppmd_line() clips to the edges of the pixmap.  You can use this
** routine to disable the clipping, for example if you are using a drawproc
** that wants to do its own clipping.
*/

void
ppmd_linep(pixel **       const pixels, 
           int            const cols, 
           int            const rows, 
           pixval         const maxval, 
           ppmd_point     const p0,
           ppmd_point     const p1,
           ppmd_drawprocp       drawProc,
           const void *   const clientdata);

    /* Draws a line from p0 to p1.  */

void
ppmd_line(pixel**       const pixels, 
          int           const cols, 
          int           const rows, 
          pixval        const maxval, 
          int           const x0, 
          int           const y0, 
          int           const x1, 
          int           const y1, 
          ppmd_drawproc       drawproc,
          const void *  const clientdata);
    /* Draws a line from (x0, y0) to (x1, y1). */

void
ppmd_spline3p(pixel **       const pixels, 
              int            const cols, 
              int            const rows, 
              pixval         const maxval, 
              ppmd_point     const p0,
              ppmd_point     const p1,
              ppmd_point     const p2,
              ppmd_drawprocp       drawProc,
              const void *   const clientdata);

    /* Draws a three-point spline from p0 to p2, with p1
       as the control point.  All drawing is done via ppmd_linep(),
       so the routines that control it control ppmd_spline3p() as well. 
    */

void
ppmd_spline3(pixel **      const pixels, 
             int           const cols, 
             int           const rows, 
             pixval        const maxval, 
             int           const x0, 
             int           const y0, 
             int           const x1, 
             int           const y1, 
             int           const x2, 
             int           const y2, 
             ppmd_drawproc       drawproc,
             const void *  const clientdata);

void
ppmd_polysplinep(pixel **       const pixels, 
                 unsigned int   const cols, 
                 unsigned int   const rows, 
                 pixval         const maxval, 
                 ppmd_point     const p0,
                 unsigned int   const nc,
                 ppmd_point *   const c,
                 ppmd_point     const p1,
                 ppmd_drawprocp       drawProc,
                 const void *   const clientdata);

    /* Draws a bunch of splines end to end.  p0 and p1 are the initial and
       final points, and the c[] are the intermediate control points.  nc is
       the number of these control points.
    */

void
ppmd_polyspline(pixel **     const pixels, 
                int          const cols, 
                int          const rows, 
                pixval       const maxval, 
                int          const x0, 
                int          const y0, 
                int          const nc, 
                int *        const xc, 
                int *        const yc, 
                int          const x1, 
                int          const y1, 
                ppmd_drawproc      drawProc,
                const void * const clientdata);

void
ppmd_spline4p(pixel **       const pixels, 
              unsigned int   const cols, 
              unsigned int   const rows, 
              pixval         const maxval, 
              ppmd_point     const endPt0,
              ppmd_point     const endPt1,
              ppmd_point     const ctlPt0,
              ppmd_point     const ctlPt1,
              ppmd_drawprocp       drawproc,
              const void *   const clientdata);

void
ppmd_circlep(pixel **       const pixels, 
             unsigned int   const cols, 
             unsigned int   const rows, 
             pixval         const maxval, 
             ppmd_point     const center,
             unsigned int   const radius, 
             ppmd_drawprocp       drawProc,
             const void *   const clientData);
    /* Draws a circle centered at 'center' with radius 'radius' */

void
ppmd_circle(pixel **     const pixels, 
            int          const cols, 
            int          const rows, 
            pixval       const maxval, 
            int          const cx, 
            int          const cy, 
            int          const radius, 
            ppmd_drawproc      drawProc,
            const void * const clientdata);


/* Simple filling routines.  */

void
ppmd_filledrectangle(pixel **      const pixels, 
                     int           const cols, 
                     int           const rows, 
                     pixval        const maxval, 
                     int           const x, 
                     int           const y, 
                     int           const width, 
                     int           const height, 
                     ppmd_drawproc       drawproc,
                     const void *  const clientdata );
    /* Fills in the rectangle [x, y, width, height]. */


void
ppmd_fill_path(pixel **      const pixels, 
               int           const cols, 
               int           const rows, 
               pixval        const maxval,
               ppmd_path *   const pathP,
               pixel         const color);
    /* Fills in a closed path.  Not much different from ppmd_fill(),
       but with a different interface.
    */


/* Arbitrary filling routines.  With these you can fill any outline that
** you can draw with the outline routines.
*/

struct fillobj;

struct fillobj *
ppmd_fill_create(void);
    /* Returns a blank fillhandle. */

void
ppmd_fill_destroy(struct fillobj * fillObjP);

/* For backward compatibility only: */
char *
ppmd_fill_init(void);

void
ppmd_fill_drawprocp(pixel **     const pixels, 
                    unsigned int const cols, 
                    unsigned int const rows, 
                    pixval       const maxval, 
                    ppmd_point   const p,
                    const void * const clientdata);
    /* Use this drawproc to trace the outline you want filled.  Use
       the fillhandle as the clientdata.
    */

void
ppmd_fill_drawproc(pixel **     const pixels, 
                   int          const cols, 
                   int          const rows, 
                   pixval       const maxval, 
                   int          const x, 
                   int          const y, 
                   const void * const clientdata);

void
ppmd_fill(pixel **         const pixels, 
          int              const cols, 
          int              const rows, 
          pixval           const maxval, 
          struct fillobj * const fh,
          ppmd_drawproc          drawProc,
          const void *     const clientdata);

/* Once you've traced the outline, give the fillhandle to this routine to
** do the actual drawing.  As usual, it takes a drawproc and clientdata;
** you could define drawprocs to do stipple fills and such.
*/

/* Text drawing routines. */

void
ppmd_textp(pixel**        const pixels, 
           int            const cols, 
           int            const rows, 
           pixval         const maxval, 
           ppmd_point     const pos,
           int            const height, 
           int            const angle, 
           const char *   const sArg, 
           ppmd_drawprocp       drawProc,
           const void *   const clientdata);

void
ppmd_text(pixel**       const pixels, 
          int           const cols, 
          int           const rows, 
          pixval        const maxval, 
          int           const xpos, 
          int           const ypos, 
          int           const height, 
          int           const angle, 
          const char *  const sArg, 
          ppmd_drawproc       drawProc,
          const void *  const clientdata);
/* Draws the null-terminated string 's' left justified at the point
   ('x', 'y').  The text will be 'height' pixels high and will be aligned on a
   baseline inclined 'angle' degrees with the X axis.  The supplied
   drawproc and clientdata are passed to ppmd_line() which performs the
   actual drawing.
*/

void
ppmd_text_box(int          const height, 
              int          const angle, 
              const char * const s, 
              int *        const leftP, 
              int *        const topP, 
              int *        const rightP, 
              int *        const bottomP);
/* Calculates the extents box for text drawn by ppm_text with the given
   string, size, and orientation.  Most extent box calculations should use
   an angle specification of zero to calculate the unrotated box enclosing
   the text.  If you need the extents of rotated text, however, you can
   call ppmd_text_box with a nonzero angle.
*/

#ifdef __cplusplus
}
#endif
