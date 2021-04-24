/*----------------------------------------------------------------------------
   These are declarations for use with the Portable Arbitrary Map (PAM)
   format and the Netpbm library functions specific to them.
-----------------------------------------------------------------------------*/

#ifndef PAM_H
#define PAM_H

#include "pm.h"
#include "pnm.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

typedef unsigned long sample;
    /* Regardless of the capacity of "unsigned long", a sample is always
       less than 1 << 16.  This is essential for some code to avoid
       arithmetic overflows.
    */

struct pam {
/* This structure describes an open PAM image file.  It consists
   entirely of information that belongs in the header of a PAM image
   and filesystem information.  It does not contain any state
   information about the processing of that image.  

   This is not considered to be an opaque object.  The user of Netbpm
   libraries is free to access and set any of these fields whenever
   appropriate.  The structure exists to make coding of function calls
   easy.
*/

    /* 'size' and 'len' are necessary in order to provide forward and
       backward compatibility between library functions and calling programs
       as this structure grows.
       */
    unsigned int size;   
        /* The storage size of this entire structure, in bytes */
    unsigned int len;    
        /* The length, in bytes, of the information in this structure.
           The information starts in the first byte and is contiguous.  
           This cannot be greater than 'size'
           */
    FILE * file;
    int format;
        /* The format code of the raw image.  This is PAM_FORMAT
           unless the PAM image is really a view of a PBM, PGM, or PPM
           image.  Then it's PBM_FORMAT, RPBM_FORMAT, etc.
           */
    unsigned int plainformat;
        /* Logical: On output, use the plain version of the format type
           indicated by 'format'.  Otherwise, use the raw version.
           (i.e., on output, the plainness information in 'format' is
           irrelevant).  Input functions set this to FALSE, for the
           convenience of programs that copy an input pam structure for
           use with output.

           Before Netpbm 10.32, this was rather different.  It simply
           described for convenience the plainness of the format indicated
           by 'format'.
        */
    int height;  /* Height of image in rows */
    int width;   
        /* Width of image in number of columns (tuples per row) */
    unsigned int depth;   
        /* Depth of image (number of samples in each tuple). */
    sample maxval;  /* Maximum defined value for a sample */
    unsigned int bytes_per_sample;  
        /* Number of bytes used to represent each sample in the image file.
           Note that this is strictly a function of 'maxval'.  It is in a
           a separate member for computational speed.
        */
    char tuple_type[256];
        /* The tuple type string from the image header.  If the PAM image
           is really a view of a PBM, PGM, or PPM image, the value is
           PAM_PBM_TUPLETYPE, PAM_PGM_TUPLETYPE, or PAM_PPM_TUPLETYPE,
           respectively.
        */
    unsigned int allocation_depth;
        /* The number of samples for which memory is allocated for any
           'tuple' type associated with this PAM structure.  This must
           be at least as great as 'depth'.  Only the first 'depth' of
           the samples of a tuple are meaningful.

           The purpose of this is to make it possible for a program to
           change the type of a tuple to one with more or fewer
           planes.  

           0 means the allocation depth is the same as the image depth.
        */
    const char ** comment_p;
        /* Pointer to a pointer to a NUL-terminated ASCII string of
           comments.  When reading an image, this contains the
           comments from the image's PAM header; when writing, the
           image gets these as comments, right after the magic number
           line.  The individual comments are delimited by newlines
           and are in the same order as in the PAM header.

           On output, NULL means no comments.

           On input, libnetpbm mallocs storage for the comments and placed
           the pointer at *comment_p.  Caller must free it.  NULL means
           libnetpbm does not return comments and does not allocate any
           storage.
        */
};

#define PAM_HAVE_ALLOCATION_DEPTH 1
#define PAM_HAVE_COMMENT_P 1

/* PAM_STRUCT_SIZE(x) tells you how big a struct pam is up through the 
   member named x.  This is useful in conjunction with the 'len' value
   to determine which fields are present in the structure.
*/

/* Some compilers are really vigilant and recognize it as an error
   to cast a 64 bit address to a 32 bit type.  Hence the roundabout
   casting in PAM_MEMBER_OFFSET.
*/
#define PAM_MEMBER_OFFSET(mbrname) \
  ((size_t)(unsigned long)(char*)&((struct pam *)0)->mbrname)
#define PAM_MEMBER_SIZE(mbrname) \
  sizeof(((struct pam *)0)->mbrname)
#define PAM_STRUCT_SIZE(mbrname) \
  (PAM_MEMBER_OFFSET(mbrname) + PAM_MEMBER_SIZE(mbrname))

#define PAM_BLACK 0
#define PAM_BW_WHITE 1

#define PAM_PBM_TUPLETYPE "BLACKANDWHITE"
#define PAM_PGM_TUPLETYPE "GRAYSCALE"
#define PAM_PPM_TUPLETYPE "RGB"

#define PAM_PBM_BLACK PAM_BLACK
#define PAM_PBM_WHITE PAM_BW_WHITE
    /* These are values of samples in a PAM image that represents a black
       and white bitmap image.  They are the values of black and white,
       respectively.  For example, if you use pnm_readpamrow() to read a
       row from a PBM file, the black pixels get returned as 
       PAM_PBM_BLACK.
    */

#define PAM_RED_PLANE 0
#define PAM_GRN_PLANE 1
#define PAM_BLU_PLANE 2
    /* These are plane numbers for the 3 planes of a PAM image that
       represents an RGB image (tuple type is "RGB").  So
       if 'pixel' is a tuple returned by pnmreadpamrow(), then
       pixel[PAM_GRN_PLANE] is the value of the green sample in that
       pixel.
       */
#define PAM_TRN_PLANE 3
    /* A PAM with "RGB_ALPHA" tuple type has this 4th plane
       for transparency.  0 = transparent, maxval = opaque.
    */
#define PAM_GRAY_TRN_PLANE 1
    /* For a "GRAYSCALE" tuple type, this is the transparency plane */

typedef sample *tuple;  
    /* A tuple in a PAM.  This is an array such that tuple[i-1] is the
       ith sample (element) in the tuple.  It's dimension is the depth
       of the image (see pam.depth above).
    */

#define PAM_OVERALL_MAXVAL 65535

/* Note: xv uses the same "P7" signature for its thumbnail images (it
   started using it years before PAM and unbeknownst to the designer
   of PAM).  But these images are still easily distinguishable from
   PAMs 
*/
#define PAM_MAGIC1 'P'
#define PAM_MAGIC2 '7'
#define PAM_FORMAT (PAM_MAGIC1 * 256 + PAM_MAGIC2)
#define PAM_TYPE PAM_FORMAT

/* Macro for turning a format number into a type number. */

#define PAM_FORMAT_TYPE(f) ((f) == PAM_FORMAT ? PAM_TYPE : PPM_FORMAT_TYPE(f))

struct pamtuples {
    struct pam * pamP;
    tuple ***    tuplesP;
};


typedef float * pnm_transformMap;
    /* This is an array of transform maps.  transform[N] is the 
       array that is the map for Plane N.
   
       Transform maps define a transformation between PAM sample value
       to normalized libnetpbm "samplen" value, i.e. what you get back
       from pnm_readpamrown() or pass to pnm_writepamrown().
       Typically, it's a gamma transfer function generated by
       pnm_creategammatransform() or pnm_createungammatransform().

       NULL for any transform means just plain normalization -- divide
       the PAM sample value by the maxval to get the samplen, multiply
       samplen by the maxval and round to get PAM sample value.

       NULL for map table, or 'transform' member not present (pam
       structure is too small to contain it) means ALL transforms
       are plain normalization.

       Each transform map is an array indexed by a PAM sample
       value, containing 'float' values.  So it must have 'maxval'
       entries.  The sample -> samplen tranformation is just the
       obvious table lookup.  The samplen -> sample transformation is
       more complicated -- if the samplen value is between map[N]
       and map[N+1], then the sample value is N.  And only transforms
       where map[N+1] > map[N] are allowed.  
    */

/* Declarations of library functions. */

/* We don't have a specific PAM function for init and nextimage, because
   one can simply use pnm_init() and pnm_nextimage() from pnm.h.
*/

unsigned int
pnm_bytespersample(sample const maxval);

int
pnm_tupleequal(const struct pam * const pamP, 
               tuple              const comparand, 
               tuple              const comparator);

void
pnm_assigntuple(const struct pam * const pamP,
                tuple              const dest,
                tuple              const source);

static __inline__ sample
pnm_scalesample(sample const source, 
                sample const oldmaxval, 
                sample const newmaxval) {

    if (oldmaxval == newmaxval)
        /* Fast path for common case */
        return source;
    else 
        return (source * newmaxval + (oldmaxval/2)) / oldmaxval;
}



void
pnm_scaletuple(const struct pam * const pamP,
               tuple              const dest,
               tuple              const source, 
               sample             const newmaxval);

void 
pnm_scaletuplerow(const struct pam * const pamP,
                  tuple *            const destRow,
                  tuple *            const sourceRow,
                  sample             const newMaxval);

void 
pnm_maketuplergb(const struct pam * const pamP,
                 tuple              const tuple);

void 
pnm_makerowrgb(const struct pam * const pamP,
               tuple *            const tuplerow);

void 
pnm_makearrayrgb(const struct pam * const pamP,
                 tuple **           const tuples);

void
pnm_getopacity(const struct pam * const pamP,
               int *              const haveOpacityP,
               unsigned int *     const opacityPlaneP);

void
pnm_createBlackTuple(const struct pam * const pamP, tuple * const blackTupleP);

void
createBlackTuple(const struct pam * const pamP, tuple * const blackTupleP);


tuple
pnm_allocpamtuple(const struct pam * const pamP);

#define pnm_freepamtuple(tuple) pm_freerow((char*) tuple)

tuple *
pnm_allocpamrow(const struct pam * const pamP);

#define pnm_freepamrow(tuplerow) pm_freerow((char*) tuplerow)

tuple **
pnm_allocpamarray(const struct pam * const pamP);

void
pnm_freepamarray(tuple ** const tuplearray, const struct pam * const pamP);

void 
pnm_setminallocationdepth(struct pam * const pamP,
                          unsigned int const allocationDepth);

void
pnm_setpamrow(const struct pam * const pam, 
              tuple *            const tuplerow, 
              sample             const value);

unsigned char *
pnm_allocrowimage(const struct pam * const pamP);

void
pnm_freerowimage(unsigned char * const rowimage);

void 
pnm_readpaminit(FILE *       const file, 
                struct pam * const pamP, 
                int          const size);

void 
pnm_readpamrow(const struct pam * const pamP, tuple* const tuplerow);

tuple ** 
pnm_readpam(FILE *       const file, 
            struct pam * const pamP, 
            int          const size);

void 
pnm_writepaminit(struct pam * const pamP);

void
pnm_formatpamrow(const struct pam * const pamP,
                 const tuple *      const tuplerow,
                 unsigned char *    const outbuf,
                 unsigned int *     const rowSizeP);

void 
pnm_writepamrow(const struct pam * const pamP, const tuple * const tuplerow);

void
pnm_writepamrowmult(const struct pam * const pamP, 
                    const tuple *      const tuplerow,
                    unsigned int       const rptcnt);

void 
pnm_writepam(struct pam * const pamP, tuple ** const tuplearray);

void
pnm_checkpam(const struct pam *   const pamP, 
             enum pm_check_type   const checkType,
             enum pm_check_code * const retvalP);

/*----------------------------------------------------------------------------
   Facilities for working with maxval-normalized samples.  Such samples
   are floating point quantities in the range 0..1.

   This is just a working format; there is no Netpbm image format that
   has normalized samples.
-----------------------------------------------------------------------------*/
typedef float samplen;

typedef samplen *tuplen;
    /* Same as 'tuple', except using normalized samples. */

tuplen *
pnm_allocpamrown(const struct pam * const pamP);

#define pnm_freepamrown(tuplenrow) pm_freerow((char*) tuplenrow)

tuplen *
pnm_allocpamrown(const struct pam * const pamP);

void 
pnm_readpamrown(const struct pam * const pamP, 
                tuplen *           const tuplenrow);

void 
pnm_writepamrown(const struct pam * const pamP, 
                 const tuplen *     const tuplenrow);

tuplen **
pnm_allocpamarrayn(const struct pam * const pamP);

void
pnm_freepamarrayn(tuplen **          const tuplenarray, 
                  const struct pam * const pamP);

tuplen** 
pnm_readpamn(FILE *       const file, 
             struct pam * const pamP, 
             int          const size);

void 
pnm_writepamn(struct pam * const pamP, 
              tuplen **    const tuplenarray);


void
pnm_normalizetuple(struct pam * const pamP,
                   tuple        const tuple,
                   tuplen       const tuplen);

void
pnm_unnormalizetuple(struct pam * const pamP,
                     tuplen       const tuplen,
                     tuple        const tuple);

void
pnm_normalizeRow(struct pam *             const pamP,
                 const tuple *            const tuplerow,
                 const pnm_transformMap * const transform,
                 tuplen *                 const tuplenrow);

void
pnm_unnormalizeRow(struct pam *             const pamP,
                   const tuplen *           const tuplenrow,
                   const pnm_transformMap * const transform,
                   tuple *                  const tuplerow);

/*----------------------------------------------------------------------------
   Facilities for working with visual images in particular
-----------------------------------------------------------------------------*/


void
pnm_gammarown(struct pam * const pamP,
              tuplen *     const row);

void
pnm_ungammarown(struct pam * const pamP,
                tuplen *     const row);

void
pnm_applyopacityrown(struct pam * const pamP,
                     tuplen *     const tuplenrow);

void
pnm_unapplyopacityrown(struct pam * const pamP,
                       tuplen *     const tuplenrow);

pnm_transformMap *
pnm_creategammatransform(const struct pam * const pamP);

void
pnm_freegammatransform(const pnm_transformMap * const transform,
                       const struct pam *       const pamP);

pnm_transformMap *
pnm_createungammatransform(const struct pam * const pamP);

#define pnm_freeungammatransform pnm_freegammatransform;

tuple
pnm_parsecolor(const char * const colorname,
               sample       const maxval);

extern double 
pnm_lumin_factor[3];

void
pnm_YCbCrtuple(const tuple tuple, 
               double * const YP, double * const CbP, double * const CrP);

void 
pnm_YCbCr_to_rgbtuple(const struct pam * const pamP,
                      tuple              const tuple,
                      double             const Y,
                      double             const Cb, 
                      double             const Cr,
                      int *              const overflowP);

#define pnm_rgbtupleisgray(tuple) \
    ((tuple)[PAM_RED_PLANE] == (tuple)[PAM_GRN_PLANE] && \
     (tuple)[PAM_RED_PLANE] == (tuple)[PAM_BLU_PLANE])

/*----------------------------------------------------------------------------
   These are meant for passing to pm_system() as Standard Input feeder
   and Standard Output accepter.
-----------------------------------------------------------------------------*/

void
pm_feed_from_pamtuples(int    const pipeToFeedFd,
                       void * const feederParm);

void
pm_accept_to_pamtuples(int    const pipeToSuckFd,
                       void * const accepterParm);

#ifdef __cplusplus
}
#endif
#endif
