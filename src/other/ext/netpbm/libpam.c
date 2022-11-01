/*=============================================================================
                                  libpam.c
===============================================================================
   These are the library functions, which belong in the libnetpbm library,
   that deal with the PAM (Portable Arbitrary Format) image format.


   This file was originally written by Bryan Henderson and is contributed
   to the public domain by him and subsequent authors.
=============================================================================*/
/* See libpm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  
#ifndef __APPLE__
#define _BSD_SOURCE 1      /* Make sure strdup() is in string.h */
#define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */
#endif

#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>

#include <math.h>

#include "pm_c_util.h"
#include "mallocvar.h"


#include "pam.h"
#include "ppm.h"
#include "libpbm.h"
#include "libpgm.h"
#include "libppm.h"
#include "colorname.h"
#include "fileio.h"

#include "libpam.h"



static unsigned int
allocationDepth(const struct pam * const pamP) {

    unsigned int retval;

    if (pamP->len >= PAM_STRUCT_SIZE(allocation_depth)) {
        if (pamP->allocation_depth == 0)
            retval = pamP->depth;
        else {
            if (pamP->depth > pamP->allocation_depth)
                pm_error("'allocationDepth' (%u) is smaller than 'depth' (%u)",
                         pamP->allocation_depth, pamP->depth);
            retval = pamP->allocation_depth;
        }
    } else
        retval = pamP->depth;
    return retval;
}



static const char **
pamCommentP(const struct pam * const pamP) {

    const char ** retval;

    if (pamP->len >= PAM_STRUCT_SIZE(comment_p))
        retval = pamP->comment_p;
    else
        retval = NULL;

    return retval;
}



static void
validateComputableSize(struct pam * const pamP) {
/*----------------------------------------------------------------------------
   Validate that the dimensions of the image are such that it can be
   processed in typical ways on this machine without worrying about
   overflows.  Note that in C, arithmetic is always modulus arithmetic,
   so if your values are too big, the result is not what you expect.
   That failed expectation can be disastrous if you use it to allocate
   memory.

   It is very normal to allocate space for a tuplerow, so we make sure
   the size of a tuple row, in bytes, can be represented by an 'int'.

   Another common operation is adding 1 or 2 to the highest row, column,
   or plane number in the image, so we make sure that's possible.
-----------------------------------------------------------------------------*/
    if (pamP->width == 0)
        pm_error("Width is zero.  Image must be at least one pixel wide");
    else if (pamP->height == 0)
        pm_error("Height is zero.  Image must be at least one pixel high");
    else {
        unsigned int const depth = allocationDepth(pamP);

        if (depth > INT_MAX/sizeof(sample))
            pm_error("image depth (%u) too large to be processed", depth);
        else if (depth * sizeof(sample) > INT_MAX/pamP->width)
            pm_error("image width and depth (%u, %u) too large "
                     "to be processed.", pamP->width, depth);
        else if (pamP->width * (depth * sizeof(sample)) >
                 INT_MAX - depth * sizeof(tuple *))
            pm_error("image width and depth (%u, %u) too large "
                     "to be processed.", pamP->width, depth);
        
        if (depth > INT_MAX - 2)
            pm_error("image depth (%u) too large to be processed", depth);
        if (pamP->width > INT_MAX - 2)
            pm_error("image width (%u) too large to be processed",
                     pamP->width);
        if (pamP->height > INT_MAX - 2)
            pm_error("image height (%u) too large to be processed",
                     pamP->height);
    }
}



tuple
pnm_allocpamtuple(const struct pam * const pamP) {

    tuple retval;

    retval = malloc(allocationDepth(pamP) * sizeof(retval[0]));

    if (retval == NULL)
        pm_error("Out of memory allocating %u-plane tuple", 
                 allocationDepth(pamP));

    return retval;
}



int
pnm_tupleequal(const struct pam * const pamP, 
               tuple              const comparand, 
               tuple              const comparator) {

    unsigned int plane;
    bool equal;

    equal = TRUE;  /* initial value */
    for (plane = 0; plane < pamP->depth; ++plane) 
        if (comparand[plane] != comparator[plane])
            equal = FALSE;

    return equal;
}




void
pnm_assigntuple(const struct pam * const pamP,
                tuple              const dest,
                tuple              const source) {

    unsigned int plane;
    for (plane = 0; plane < pamP->depth; ++plane) {
        dest[plane] = source[plane];
    }
}



static void
scaleTuple(const struct pam * const pamP,
           tuple              const dest,
           tuple              const source, 
           sample             const newmaxval) {

    unsigned int plane;
    for (plane = 0; plane < pamP->depth; ++plane) 
        dest[plane] = pnm_scalesample(source[plane], pamP->maxval, newmaxval);
}



void
pnm_scaletuple(const struct pam * const pamP,
               tuple              const dest,
               tuple              const source, 
               sample             const newmaxval) {

    scaleTuple(pamP, dest, source, newmaxval);
}



void
pnm_createBlackTuple(const struct pam * const pamP, 
                     tuple *            const blackTupleP) {
/*----------------------------------------------------------------------------
   Create a "black" tuple.  By that we mean a tuple all of whose elements
   are zero.  If it's an RGB, grayscale, or b&w pixel, that means it's black.
-----------------------------------------------------------------------------*/
    unsigned int i;

    *blackTupleP = pnm_allocpamtuple(pamP);

    for (i = 0; i < pamP->depth; ++i) 
        (*blackTupleP)[i] = 0;
}



static tuple *
allocPamRow(const struct pam * const pamP) {
/*----------------------------------------------------------------------------
   We assume that the dimensions of the image are such that arithmetic
   overflow will not occur in our calculations.  NOTE: pnm_readpaminit()
   ensures this assumption is valid.
-----------------------------------------------------------------------------*/
    /* The tuple row data structure starts with pointers to the tuples,
       immediately followed by the tuples themselves.
    */

    unsigned int const bytesPerTuple = allocationDepth(pamP) * sizeof(sample);
    tuple * tuplerow;

    tuplerow = malloc(pamP->width * (sizeof(tuple *) + bytesPerTuple));
                      
    if (tuplerow != NULL) {
        /* Now we initialize the pointers to the individual tuples
           to make this a regulation C two dimensional array.  
        */
        char * p;
        unsigned int col;
        
        p = (char*) (tuplerow + pamP->width);  /* location of Tuple 0 */
        for (col = 0; col < pamP->width; ++col) {
                tuplerow[col] = (tuple) p;
                p += bytesPerTuple;
        }
    }
    return tuplerow;
}



tuple *
pnm_allocpamrow(const struct pam * const pamP) {


    tuple * const tuplerow = allocPamRow(pamP);

    if (tuplerow == NULL)
        pm_error("Out of memory allocating space for a tuple row of "
                 "%d tuples by %d samples per tuple by %u bytes per sample.",
                 pamP->width, allocationDepth(pamP), (unsigned)sizeof(sample));

    return tuplerow;
}



static unsigned int 
rowimagesize(const struct pam * const pamP) {

    /* If repeatedly calculating this turns out to be a significant
       performance problem, we could keep this in struct pam like
       bytes_per_sample.
    */

    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE)
        return pbm_packed_bytes(pamP->width);
    else 
        return (pamP->width * pamP->bytes_per_sample * pamP->depth);
}



unsigned char *
pnm_allocrowimage(const struct pam * const pamP) {

    unsigned int const rowsize = rowimagesize(pamP);
    unsigned int const overrunSpaceNeeded = 8;
        /* This is the number of extra bytes of space libnetpbm needs to have
           at the end of the buffer so it can use fast, lazy algorithms.
        */
    unsigned int const size = rowsize + overrunSpaceNeeded;

    unsigned char * retval;

    retval = malloc(size);

    if (retval == NULL)
        pm_error("Unable to allocate %u bytes for a row image buffer",
                 size);

    return retval;
}



void
pnm_freerowimage(unsigned char * const rowimage) {
    free(rowimage);
}



void 
pnm_scaletuplerow(const struct pam * const pamP,
                  tuple *            const destRow,
                  tuple *            const sourceRow,
                  sample             const newMaxval) {

    if (pamP->maxval == newMaxval) {
        /* Fast path for common case: no scaling needed */
        if (destRow != sourceRow) {
            /* Fast path for common case: it's already what it needs to be */
            unsigned int col;
            for (col = 0; col < pamP->width; ++col)
                pnm_assigntuple(pamP, destRow[col], sourceRow[col]);
        }
    } else {        
        unsigned int col;
        for (col = 0; col < pamP->width; ++col) 
            scaleTuple(pamP, destRow[col], sourceRow[col], newMaxval);
    }
}

 

tuple **
pnm_allocpamarray(const struct pam * const pamP) {
    
    tuple **tuplearray;

    /* If the speed of this is ever an issue, it might be sped up a little
       by allocating one large chunk.
    */
    
    MALLOCARRAY(tuplearray, pamP->height);
    if (tuplearray == NULL) 
        pm_error("Out of memory allocating the row pointer section of "
                 "a %u row array", pamP->height);
    else {
        int row;
        bool outOfMemory;

        outOfMemory = FALSE;
        for (row = 0; row < pamP->height && !outOfMemory; ++row) {
            tuplearray[row] = allocPamRow(pamP);
            if (tuplearray[row] == NULL) {
                unsigned int freerow;
                outOfMemory = TRUE;
                
                for (freerow = 0; freerow < row; ++freerow)
                    pnm_freepamrow(tuplearray[row]);
            }
        }
        if (outOfMemory) {
            free(tuplearray);
            
            pm_error("Out of memory allocating the %u rows %u columns wide by "
                     "%u planes deep", pamP->height, pamP->width,
                     allocationDepth(pamP));
        }
    }
    return tuplearray;
}



void
pnm_freepamarray(tuple ** const tuplearray, const struct pam * const pamP) {

    int row;
    for (row = 0; row < pamP->height; row++)
        pnm_freepamrow(tuplearray[row]);

    free(tuplearray);
}



void 
pnm_setminallocationdepth(struct pam * const pamP,
                          unsigned int const allocationDepth) {

    if (pamP->len < PAM_STRUCT_SIZE(allocation_depth))
        pm_error("Can't set minimum allocation depth in pam structure, "
                 "because the structure is only %u bytes long, and to "
                 "have an allocation_depth field, it must bea at least %u",
                 pamP->len, (unsigned)PAM_STRUCT_SIZE(allocation_depth));

    pamP->allocation_depth = MAX(allocationDepth, pamP->depth);
        
    validateComputableSize(pamP);
}



void
pnm_setpamrow(const struct pam * const pamP,
              tuple *            const tuplerow, 
              sample             const value) {

    int col;
    for (col = 0; col < pamP->width; ++col) {
        int plane;
        for (plane = 0; plane < pamP->depth; ++plane) 
            tuplerow[col][plane] = value;
    }
}




#define MAX_LABEL_LENGTH 8
#define MAX_VALUE_LENGTH 255

static void
parseHeaderLine(const char buffer[],
                char label[MAX_LABEL_LENGTH+1], 
                char value[MAX_VALUE_LENGTH+1]) {
    
    int buffer_curs;

    buffer_curs = 0;
    /* Skip initial white space */
    while (isspace(buffer[buffer_curs])) buffer_curs++;

    {
        /* Read off label, put as much as will fit into label[] */
        int label_curs;
        label_curs = 0;
        while (!isspace(buffer[buffer_curs]) && buffer[buffer_curs] != '\0') {
            if (label_curs < MAX_LABEL_LENGTH) 
                label[label_curs++] = buffer[buffer_curs];
            buffer_curs++;
        }
        label[label_curs] = '\0';  /* null terminate it */
    }    

    /* Skip white space between label and value */
    while (isspace(buffer[buffer_curs])) buffer_curs++;

    /* copy value into value[] */
    strncpy(value, buffer+buffer_curs, MAX_VALUE_LENGTH+1);

    {
        /* Remove trailing white space from value[] */
        int value_curs;
        value_curs = strlen(value)-1;
        while (value_curs >= 0 && isspace(value[value_curs])) 
            value[value_curs--] = '\0';
    }
}



struct headerSeen {
/*----------------------------------------------------------------------------
   This structure tells what we've seen so far in our progress through the
   PAM header
-----------------------------------------------------------------------------*/
    bool width;
    bool height;
    bool depth;
    bool maxval;
    bool endhdr;
};



static void
parseHeaderUint(const char *   const valueString,
                unsigned int * const valueNumP,
                const char *   const name) {
/*----------------------------------------------------------------------------
   Interpret 'valueString' as the number in a header such as
   "WIDTH 200".

   'name' is the header name ("WIDTH" in the example).
-----------------------------------------------------------------------------*/

    if (strlen(valueString) == 0)
        pm_error("Missing value for %s in PAM file header.", name);
    else {
        char * endptr;
        long int valueNum;
        errno = 0;  /* Clear errno so we can detect strtol() failure */
        valueNum = strtol(valueString, &endptr, 10);
        if (errno != 0)
            pm_error("Too-large value for %s in "
                     "PAM file header: '%s'", name, valueString);
        else if (*endptr != '\0') 
            pm_error("Non-numeric value for %s in "
                     "PAM file header: '%s'", name, valueString);
        else if (valueNum < 0) 
            pm_error("Negative value for %s in "
                     "PAM file header: '%s'", name, valueString);
        else if ((unsigned int)valueNum != valueNum)
            pm_error("Ridiculously large value for %s in "
                     "PAM file header: %lu", name, valueNum);
        else
            *valueNumP = (unsigned int)valueNum;
    }
}



static void
parseHeaderInt(const char * const valueString,
               int *        const valueNumP,
               const char * const name) {
/*----------------------------------------------------------------------------
  This is not what it seems.  It is the same thing as
  parseHeaderUint, except that the type of the value it returns is
  "int" instead of "unsigned int".  But that doesn't mean the value can
  be negative.  We throw an error is it is not positive.
-----------------------------------------------------------------------------*/
    unsigned int valueNum;

    parseHeaderUint(valueString, &valueNum, name);

    if ((int)valueNum != valueNum)
        pm_error("Ridiculously large value for %s in "
                 "PAM file header: %u", name, valueNum);
    else
        *valueNumP = (int)valueNum;
}



static void
processHeaderLine(char                const buffer[],
                  struct pam *        const pamP,
                  struct headerSeen * const headerSeenP) {
/*----------------------------------------------------------------------------
   Process a line from the PAM header.  The line is buffer[], and it is not
   a comment or blank.

   Put the value that the line defines in *pamP (unless it's ENDHDR).

   Update *headerSeenP with whatever we see.
-----------------------------------------------------------------------------*/
    char label[MAX_LABEL_LENGTH+1];
    char value[MAX_VALUE_LENGTH+1];

    parseHeaderLine(buffer, label, value);

    if (!strcmp(label, "ENDHDR"))
        headerSeenP->endhdr = TRUE;
    else if (!strcmp(label, "WIDTH")) {
        parseHeaderInt(value, &pamP->width, label);
        headerSeenP->width = TRUE;
    } else if (!strcmp(label, "HEIGHT")) {
        parseHeaderInt(value, &pamP->height, label);
        headerSeenP->height = TRUE;
    } else if (!strcmp(label, "DEPTH")) {
        parseHeaderUint(value, &pamP->depth, label);
        headerSeenP->depth = TRUE;
    } else if (!strcmp(label, "MAXVAL")) {
        unsigned int maxval;
        parseHeaderUint(value, &maxval, label);
        if (maxval >= (1<<16))
            pm_error("Maxval too large: %u.  Max is 65535", maxval);
        pamP->maxval = maxval;
        headerSeenP->maxval = TRUE;
    } else if (!strcmp(label, "TUPLTYPE")) {
        if (strlen(value) == 0)
            pm_error("TUPLTYPE header does not have any tuple type text");
        else {
            size_t const oldLen = strlen(pamP->tuple_type);
            if (oldLen + strlen(value) + 1 > sizeof(pamP->tuple_type)-1)
                pm_error("TUPLTYPE value too long in PAM header");
            if (oldLen == 0)
                strcpy(pamP->tuple_type, value);
            else {
                strcat(pamP->tuple_type, " ");
                strcat(pamP->tuple_type, value);
            }
            pamP->tuple_type[sizeof(pamP->tuple_type)-1] = '\0';
        }
    } else 
        pm_error("Unrecognized header line type: '%s'.  "
                 "Possible missing ENDHDR line?", label);
}



static void
appendComment(char **      const commentsP,
              const char * const commentHeader) {

    const char * const commentLine = &commentHeader[1];

    size_t commentLen = strlen(*commentsP) + strlen(commentLine) + 1;

    assert(commentHeader[0] == '#');

    REALLOCARRAY(*commentsP, commentLen);

    if (*commentsP == NULL)
        pm_error("Couldn't get storage for %u characters of comments from "
                 "the PAM header", (unsigned)commentLen);

    strcat(*commentsP, commentLine);
}



static void
disposeOfComments(const struct pam * const pamP,
                  const char *       const comments) {

    const char ** const retP = pamCommentP(pamP);

    if (retP)
        *retP = comments;
    else
        free((void*)comments);
}

/* per https://stackoverflow.com/questions/3981510/getline-check-if-line-is-whitespace */
int all_whitespace(const char *buf)
{
  const char *lb = buf;
  while (*lb != '\0') {
    if (!isspace((unsigned char)*lb)) return 0;
    lb++;
  }
  return 1;
}

static void
readpaminitrest(struct pam * const pamP) {
/*----------------------------------------------------------------------------
   Read the rest of the PAM header (after the first line -- the magic
   number line).  Fill in all the information in *pamP.
-----------------------------------------------------------------------------*/
    struct headerSeen headerSeen;
    char * comments;

    headerSeen.width  = FALSE;
    headerSeen.height = FALSE;
    headerSeen.depth  = FALSE;
    headerSeen.maxval = FALSE;
    headerSeen.endhdr = FALSE;

    pamP->tuple_type[0] = '\0';

    comments = strdup("");

    { 
        int c;
        /* Read off rest of 1st line -- probably just the newline after the 
           magic number 
        */
        while ((c = getc(pamP->file)) != -1 && c != '\n');
    }    

    while (!headerSeen.endhdr) {
        char buffer[256];
        char * rc;
        rc = fgets(buffer, sizeof(buffer), pamP->file);
        if (rc == NULL)
            pm_error("EOF or error reading file while trying to read the "
                     "PAM header");
        else {
            buffer[256-1-1] = '\n';  /* In case fgets() truncated */
            if (buffer[0] == '#')
                appendComment(&comments, buffer);
            else if (all_whitespace(buffer));
                /* Ignore it; it's a blank line */
            else 
                processHeaderLine(buffer, pamP, &headerSeen);
        }
    }

    disposeOfComments(pamP, comments);

    if (!headerSeen.height)
        pm_error("No HEIGHT header line in PAM header");
    if (!headerSeen.width)
        pm_error("No WIDTH header line in PAM header");
    if (!headerSeen.depth)
        pm_error("No DEPTH header line in PAM header");
    if (!headerSeen.maxval)
        pm_error("No MAXVAL header line in PAM header");

    if (pamP->height == 0) 
        pm_error("HEIGHT value is zero in PAM header");
    if (pamP->width == 0) 
        pm_error("WIDTH value is zero in PAM header");
    if (pamP->depth == 0) 
        pm_error("DEPTH value is zero in PAM header");
    if (pamP->maxval == 0) 
        pm_error("MAXVAL value is zero in PAM header");
    if (pamP->maxval > PAM_OVERALL_MAXVAL)
        pm_error("MAXVAL value (%lu) in PAM header is greater than %u",
                 pamP->maxval, PAM_OVERALL_MAXVAL);
}



void
pnm_readpaminitrestaspnm(FILE * const fileP, 
                         int *  const colsP, 
                         int *  const rowsP, 
                         gray * const maxvalP,
                         int *  const formatP) {
/*----------------------------------------------------------------------------
   Read the rest of the PAM header (after the first line) and return
   information as if it were PPM or PGM.

   Die if it isn't a PAM of the sort we can treat as PPM or PGM.
-----------------------------------------------------------------------------*/
    struct pam pam;

    pam.size   = sizeof(struct pam);
    pam.file   = fileP;
    pam.len    = PAM_STRUCT_SIZE(tuple_type);
    pam.format = PAM_FORMAT;

    readpaminitrest(&pam);


    /* A PAM raster of depth 1 is identical to a PGM raster.  A PAM
       raster of depth 3 is identical to PPM raster.  So
       ppm_readppmrow() will be able to read the PAM raster as long as
       the format it thinks it is (PGM or PPM) corresponds to the PAM
       depth.  Similar for pgm_readpgmrow().
    */
    switch (pam.depth) {
    case 3:
        *formatP = RPPM_FORMAT;
        break;
    case 1:
        *formatP = RPGM_FORMAT;
        break;
    default:
        pm_error("Cannot treat PAM image as PPM or PGM, "
                 "because its depth (%u) "
                 "is not 1 or 3.", pam.depth);
    }

    *colsP   = pam.width;
    *rowsP   = pam.height;
    *maxvalP = (gray)pam.maxval;
}


                
unsigned int
pnm_bytespersample(sample const maxval) {
/*----------------------------------------------------------------------------
   Return the number of bytes per sample in the PAM raster of a PAM image
   with maxval 'maxval'.  It's defined to be the minimum number of bytes
   needed for that maxval, i.e. 1 for maxval < 256, 2 otherwise.
-----------------------------------------------------------------------------*/

    /* The PAM format requires maxval to be greater than zero and less than
       1<<16, but since that is a largely arbitrary restriction, we don't want
       to rely on it.
    */

    unsigned int i;
    sample a;

    for (i = 0, a = maxval; i <= sizeof(maxval); ++i) {
        if (a == 0)
            return i;
        a >>= 8;
    }
    return 0;  /* silence compiler warning */
}



static void
validateMinDepth(const struct pam * const pamP,
                 unsigned int       const minDepth) {

    if (pamP->depth < minDepth)
        pm_error("Depth %u is insufficient for tuple type '%s'.  "
                 "Minimum depth is %u",
                 pamP->depth, pamP->tuple_type, minDepth);
}



static void
interpretTupleType(struct pam * const pamP) {
/*----------------------------------------------------------------------------
   Fill in redundant convenience fields in *pamP with information the
   pamP->tuple_type value implies:

     visual
     colorDepth
     haveOpacity
     opacityPlane

   Validate the tuple type against the depth and maxval as well.
-----------------------------------------------------------------------------*/
    const char * const tupleType =
        pamP->len >= PAM_STRUCT_SIZE(tuple_type) ? pamP->tuple_type : "";

    bool         visual;
    unsigned int colorDepth;
    bool         haveOpacity;
    unsigned int opacityPlane;

    assert(pamP->depth > 0);

    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE: {
        if (!strcmp(tupleType, "BLACKANDWHITE")) {
            visual = true;
            colorDepth = 1;
            haveOpacity = false;
            if (pamP->maxval != 1)
                pm_error("maxval %u is not consistent with tuple type "
                         "BLACKANDWHITE (should be 1)",
                         (unsigned)pamP->maxval);
        } else if (!strcmp(tupleType, "GRAYSCALE")) {
            visual = true;
            colorDepth = 1;
            haveOpacity = false;
        } else if (!strcmp(tupleType, "GRAYSCALE_ALPHA")) {
            visual = true;
            colorDepth = 1;
            haveOpacity = true;
            opacityPlane = PAM_GRAY_TRN_PLANE;
            validateMinDepth(pamP, 2);
        } else if (!strcmp(tupleType, "RGB")) {
            visual = true;
            colorDepth = 3;
            haveOpacity = false;
            validateMinDepth(pamP, 3);
        } else if (!strcmp(tupleType, "RGB_ALPHA")) {
            visual = true;
            colorDepth = 3;
            haveOpacity = true;
            opacityPlane = PAM_TRN_PLANE;
            validateMinDepth(pamP, 4);
        } else {
            visual = false;
        }
    } break;
    case PPM_TYPE:
        visual = true;
        colorDepth = 3;
        haveOpacity = false;
        assert(pamP->depth == 3);
        break;
    case PGM_TYPE:
        visual = true;
        colorDepth = 1;
        haveOpacity = false;
        break;
    case PBM_TYPE:
        visual = true;
        colorDepth = 1;
        haveOpacity = false;
        break;
    default:
        assert(false);
    }
    if (pamP->size >= PAM_STRUCT_SIZE(visual))
        pamP->visual = visual;
    if (pamP->size >= PAM_STRUCT_SIZE(color_depth))
        pamP->color_depth = colorDepth;
    if (pamP->size >= PAM_STRUCT_SIZE(have_opacity))
        pamP->have_opacity = haveOpacity;
    if (pamP->size >= PAM_STRUCT_SIZE(opacity_plane))
        pamP->opacity_plane = opacityPlane;
}



void 
pnm_readpaminit(FILE *       const file, 
                struct pam * const pamP, 
                int          const size) {

    if (size < PAM_STRUCT_SIZE(tuple_type)) 
        pm_error("pam object passed to pnm_readpaminit() is too small.  "
                 "It must be large "
                 "enough to hold at least up to the "
                 "'tuple_type' member, but according "
                 "to the 'size' argument, it is only %d bytes long.", 
                 size);

    pamP->size = size;
    pamP->file = file;
    pamP->len = MIN(pamP->size, sizeof(struct pam));

    if (size >= PAM_STRUCT_SIZE(allocation_depth))
        pamP->allocation_depth = 0;

    /* Get magic number. */
    pamP->format = pm_readmagicnumber(file);

    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE: 
        readpaminitrest(pamP);
        break;
    case PPM_TYPE: {
        pixval maxval;
        ppm_readppminitrest(pamP->file, &pamP->width, &pamP->height, &maxval);
        pamP->maxval = (sample) maxval;
        pamP->depth = 3;
        strcpy(pamP->tuple_type, PAM_PPM_TUPLETYPE);
        if (pamCommentP(pamP))
            *pamP->comment_p = strdup("");
    } break;

    case PGM_TYPE: {
        gray maxval;
        pgm_readpgminitrest(pamP->file, &pamP->width, &pamP->height, &maxval);
        pamP->maxval = (sample) maxval;
        pamP->depth = 1;
        strcpy(pamP->tuple_type, PAM_PGM_TUPLETYPE);
        if (pamCommentP(pamP))
            *pamP->comment_p = strdup("");
    } break;

    case PBM_TYPE:
        pbm_readpbminitrest(pamP->file, &pamP->width,&pamP->height);
        pamP->maxval = (sample) 1;
        pamP->depth = 1;
        strcpy(pamP->tuple_type, PAM_PBM_TUPLETYPE);
        if (pamCommentP(pamP))
            *pamP->comment_p = strdup("");
        break;
        
    default:
        pm_error("bad magic number 0x%x - not a PAM, PPM, PGM, or PBM file",
                 pamP->format);
    }
    
    pamP->bytes_per_sample = pnm_bytespersample(pamP->maxval);
    pamP->plainformat = FALSE;
        /* See below for complex explanation of why this is FALSE. */

    interpretTupleType(pamP);

    validateComputableSize(pamP);
}



static void
writeComments(const struct pam * const pamP) {
/*----------------------------------------------------------------------------
   Write comments for a PAM header, insofar as *pamP specifies comments.
-----------------------------------------------------------------------------*/
    const char ** const commentP = pamCommentP(pamP);

    if (commentP) {
        const char * const comment = *commentP;

        const char * p;
        bool startOfLine;

        for (p = &comment[0], startOfLine = TRUE; *p; ++p) {
            if (startOfLine)
                fputc('#', pamP->file);
                
            fputc(*p, pamP->file);

            if (*p == '\n')
                startOfLine = TRUE;
            else
                startOfLine = FALSE;
        }
        if (!startOfLine)
            fputc('\n', pamP->file);
    }
}



/* About the 'plainformat' member on image input operations:

   'plainformat' is meaningless when reading an image, but we always
   set it FALSE anyway.

   That's because it is common for a program to copy a pam structure
   used for input as a pam structure for output, and just update the
   few fields it cares about -- mainly 'file'.  We want a program like
   that to write a raw format image, and 'plainformat' in an output
   pam structure is what determines whether it is raw or plain.  So we
   set it false here so that it is false in the copied output pam
   structure.
   
   Before 10.32, we set 'plainformat' according to the
   plainness of the input image, and thought it was a good
   idea for a program that reads a plain format input image to
   write a plain format output image.  But that is not what
   the older libnetpbm facilities (e.g. pnm_writepnm()) do, so
   for compatibility, the pam facility shouldn't either.  This
   came to light as we converted programs from the pnm/pbm/ppm
   facilities to pam.
*/

void 
pnm_writepaminit(struct pam * const pamP) {

    const char * tupleType;

    if (pamP->size < pamP->len)
        pm_error("pam object passed to pnm_writepaminit() is smaller "
                 "(%u bytes, according to its 'size' element) "
                 "than the amount of data in it "
                 "(%u bytes, according to its 'len' element).",
                 pamP->size, pamP->len);

    if (pamP->size < PAM_STRUCT_SIZE(bytes_per_sample))
        pm_error("pam object passed to pnm_writepaminit() is too small.  "
                 "It must be large "
                 "enough to hold at least up through the "
                 "'bytes_per_sample' member, but according "
                 "to its 'size' member, it is only %u bytes long.", 
                 pamP->size);
    if (pamP->len < PAM_STRUCT_SIZE(maxval))
        pm_error("pam object must contain members at least through 'maxval', "
                 "but according to the 'len' member, it is only %u bytes "
                 "long.", pamP->len);

    if (pamP->maxval > PAM_OVERALL_MAXVAL)
        pm_error("maxval (%lu) passed to pnm_writepaminit() "
                 "is greater than %u", pamP->maxval, PAM_OVERALL_MAXVAL);

    if (pamP->len < PAM_STRUCT_SIZE(tuple_type)) {
        tupleType = "";
        if (pamP->size >= PAM_STRUCT_SIZE(tuple_type))
            pamP->tuple_type[0] = '\0';
    } else
        tupleType = pamP->tuple_type;

    pamP->bytes_per_sample = pnm_bytespersample(pamP->maxval);

    if (pamP->size >= PAM_STRUCT_SIZE(comment_p) &&
        pamP->len < PAM_STRUCT_SIZE(comment_p))
        pamP->comment_p = NULL;

    if (pamP->size >= PAM_STRUCT_SIZE(allocation_depth) &&
        pamP->len < PAM_STRUCT_SIZE(allocation_depth))
        pamP->allocation_depth = 0;

    interpretTupleType(pamP);

    pamP->len = MIN(pamP->size, PAM_STRUCT_SIZE(opacity_plane));
    
    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE:
        /* See explanation below of why we ignore 'pm_plain_output' here. */
        fprintf(pamP->file, "P7\n");
        writeComments(pamP);
        fprintf(pamP->file, "WIDTH %u\n",   (unsigned)pamP->width);
        fprintf(pamP->file, "HEIGHT %u\n",  (unsigned)pamP->height);
        fprintf(pamP->file, "DEPTH %u\n",   pamP->depth);
        fprintf(pamP->file, "MAXVAL %lu\n", pamP->maxval);
        if (!all_whitespace(tupleType))
            fprintf(pamP->file, "TUPLTYPE %s\n", pamP->tuple_type);
        fprintf(pamP->file, "ENDHDR\n");
        break;
    case PPM_TYPE:
        /* The depth must be exact, because pnm_writepamrow() is controlled
           by it, without regard to format.
        */
        if (pamP->depth != 3)
            pm_error("pnm_writepaminit() got PPM format, but depth = %d "
                     "instead of 3, as required for PPM.", 
                     pamP->depth);
        if (pamP->maxval > PPM_OVERALLMAXVAL) 
            pm_error("pnm_writepaminit() got PPM format, but maxval = %ld, "
                     "which exceeds the maximum allowed for PPM: %d", 
                     pamP->maxval, PPM_OVERALLMAXVAL);
        ppm_writeppminit(pamP->file, pamP->width, pamP->height, 
                         (pixval) pamP->maxval, pamP->plainformat);
        break;

    case PGM_TYPE:
        if (pamP->depth != 1)
            pm_error("pnm_writepaminit() got PGM format, but depth = %d "
                     "instead of 1, as required for PGM.",
                     pamP->depth);
        if (pamP->maxval > PGM_OVERALLMAXVAL)
            pm_error("pnm_writepaminit() got PGM format, but maxval = %ld, "
                     "which exceeds the maximum allowed for PGM: %d", 
                     pamP->maxval, PGM_OVERALLMAXVAL);
        pgm_writepgminit(pamP->file, pamP->width, pamP->height, 
                         (gray) pamP->maxval, pamP->plainformat);
        break;

    case PBM_TYPE:
        if (pamP->depth != 1)
            pm_error("pnm_writepaminit() got PBM format, but depth = %d "
                     "instead of 1, as required for PBM.",
                     pamP->depth);
        if (pamP->maxval != 1) 
            pm_error("pnm_writepaminit() got PBM format, but maxval = %ld "
                     "instead of 1, as required for PBM.", pamP->maxval);
        pbm_writepbminit(pamP->file, pamP->width, pamP->height, 
                         pamP->plainformat);
        break;

    default:
        pm_error("Invalid format passed to pnm_writepaminit(): %d",
                 pamP->format);
    }
}



/* EFFECT OF -plain WHEN WRITING PAM FORMAT:

   Before Netpbm 10.63 (June 2013), pnm_writepaminit() did a pm_error() here
   if 'pm_plain_output' was set (i.e. the user said -plain).  But this isn't
   really logical, because -plain is a global option for the program and here
   we are just writing one image.  As a global option, -plain must be defined
   to have effect where it makes sense and have no effect where it doesn't.
   Note that a program that generates GIF just ignores -plain.  Note also that
   a program could conceivably generate both a PPM image and a PAM image.

   Note also how we handle the other a user can request plain format: the
   'plainformat' member of the PAM struct.  In the case of PAM, we ignore that
   member.
*/



void
pnm_checkpam(const struct pam *   const pamP, 
             enum pm_check_type   const checkType, 
             enum pm_check_code * const retvalP) {

    if (checkType != PM_CHECK_BASIC) {
        if (retvalP) *retvalP = PM_CHECK_UNKNOWN_TYPE;
    } else switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE: {
        pm_filepos const need_raster_size = 
            pamP->width * pamP->height * pamP->depth * pamP->bytes_per_sample;
        pm_check(pamP->file, checkType, need_raster_size, retvalP);
    }
        break;
    case PPM_TYPE:
        pgm_check(pamP->file, checkType, pamP->format, 
                  pamP->width, pamP->height, pamP->maxval, retvalP);
        break;
    case PGM_TYPE:
        pgm_check(pamP->file, checkType, pamP->format, 
                  pamP->width, pamP->height, pamP->maxval, retvalP);
        break;
    case PBM_TYPE:
        pbm_check(pamP->file, checkType, pamP->format, 
                  pamP->width, pamP->height, retvalP);
        break;
    default:
        if (retvalP) *retvalP = PM_CHECK_UNCHECKABLE;
    }
}



void 
pnm_maketuplergb(const struct pam * const pamP,
                 tuple              const tuple) {

    if (allocationDepth(pamP) < 3)
        pm_error("allocation depth %u passed to pnm_maketuplergb().  "
                 "Must be at least 3.", allocationDepth(pamP));

    if (pamP->depth < 3)
        tuple[2] = tuple[1] = tuple[0];
}



void 
pnm_makerowrgb(const struct pam * const pamP,
               tuple *            const tuplerow) {
    
    if (pamP->depth < 3) {
        unsigned int col;

        if (allocationDepth(pamP) < 3)
            pm_error("allocation depth %u passed to pnm_makerowrgb().  "
                     "Must be at least 3.", allocationDepth(pamP));
        
        if (strncmp(pamP->tuple_type, "RGB", 3) != 0) {
            for (col = 0; col < pamP->width; ++col) {
                tuple const thisTuple = tuplerow[col];
                thisTuple[2] = thisTuple[1] = thisTuple[0];
            }
        }
    }
}



void 
pnm_makearrayrgb(const struct pam * const pamP,
                 tuple **           const tuples) {

    if (pamP->depth < 3) {
        unsigned int row;
        if (allocationDepth(pamP) < 3)
            pm_error("allocation depth %u passed to pnm_makearrayrgb().  "
                     "Must be at least 3.", allocationDepth(pamP));
        
        for (row = 0; row < pamP->height; ++row) {
            tuple * const tuplerow = tuples[row];
            unsigned int col;
            for (col = 0; col < pamP->width; ++col) {
                tuple const thisTuple = tuplerow[col];
                thisTuple[2] = thisTuple[1] = thisTuple[0];
            }
        }
    }
}



void 
pnm_makerowrgba(const struct pam * const pamP,
                tuple *            const tuplerow) {
/*----------------------------------------------------------------------------
   Make the tuples 'tuplerow' the RGBA equivalent of what they are now,
   which is described by *pamP.

   This means afterward, *pamP no longer correctly describes these tuples;
   Caller must be sure to update *pamP it or not use it anymore.

   We fail if Caller did not supply enough allocated space in 'tuplerow' for
   the extra planes (tuple allocation depth).
-----------------------------------------------------------------------------*/
    if (pamP->len < PAM_STRUCT_SIZE(opacity_plane)) {
        pm_message("struct pam length %u is too small for pnm_makerowrgba().  "
                   "This function requires struct pam fields through "
                   "'opacity_plane'", pamP->len);
        abort();
    } else {
        if (!pamP->visual)
            pm_error("Non-visual tuples given to pnm_addopacityrow()");
        
        if (pamP->color_depth >= 3 && pamP->have_opacity) {
            /* It's already in RGBA format.  Leave it alone. */
        } else {
            unsigned int col;

            if (allocationDepth(pamP) < 4)
                pm_error("allocation depth %u passed to pnm_makerowrgba().  "
                         "Must be at least 4.", allocationDepth(pamP));
        
            for (col = 0; col < pamP->width; ++col) {
                tuple const thisTuple = tuplerow[col];
                thisTuple[PAM_TRN_PLANE] = 
                    pamP->have_opacity ? thisTuple[pamP->opacity_plane] :
                    pamP->maxval;

                assert(PAM_RED_PLANE == 0);
                thisTuple[PAM_BLU_PLANE] = thisTuple[0];
                thisTuple[PAM_GRN_PLANE] = thisTuple[0];
            }
        }
    }
}



void 
pnm_addopacityrow(const struct pam * const pamP,
                  tuple *            const tuplerow) {
/*----------------------------------------------------------------------------
   Add an opacity plane to the tuples in 'tuplerow', if one isn't already
   there.

   This means afterward, *pamP no longer correctly describes these tuples;
   Caller must be sure to update *pamP it or not use it anymore.

   We fail if Caller did not supply enough allocated space in 'tuplerow' for
   the extra plane (tuple allocation depth).
-----------------------------------------------------------------------------*/
    if (pamP->len < PAM_STRUCT_SIZE(opacity_plane)) {
        pm_message("struct pam length %u is too small for pnm_makerowrgba().  "
                   "This function requires struct pam fields through "
                   "'opacity_plane'", pamP->len);
        abort();
    } else {
        if (!pamP->visual)
            pm_error("Non-visual tuples given to pnm_addopacityrow()");
        
        if (pamP->have_opacity) {
            /* It already has opacity.  Leave it alone. */
        } else {
            unsigned int const opacityPlane = pamP->color_depth;

            unsigned int col;

            if (allocationDepth(pamP) < opacityPlane + 1)
                pm_error("allocation depth %u passed to pnm_addopacityrow().  "
                         "Must be at least %u.",
                         allocationDepth(pamP), opacityPlane + 1);
        
            for (col = 0; col < pamP->width; ++col)
                tuplerow[col][opacityPlane] = pamP->maxval;
        }
    }
}



void
pnm_getopacity(const struct pam * const pamP,
               int *              const haveOpacityP,
               unsigned int *     const opacityPlaneP) {

    /* Usage note: this is obsolete since we added 'have_opacity', etc.
       to struct pam.
    */
    if (!strcmp(pamP->tuple_type, "RGB_ALPHA")) {
        *haveOpacityP = TRUE;
        *opacityPlaneP = PAM_TRN_PLANE;
    } else if (!strcmp(pamP->tuple_type, "GRAYSCALE_ALPHA")) {
        *haveOpacityP = TRUE;
        *opacityPlaneP = PAM_GRAY_TRN_PLANE;
    } else
        *haveOpacityP = FALSE;
}



tuple
pnm_backgroundtuple(struct pam *  const pamP,
                    tuple      ** const tuples) {
/*--------------------------------------------------------------------
  This function was copied from libpnm3.c's pnm_backgroundxel() and
  modified to use tuples instead of xels.
----------------------------------------------------------------------*/
    tuple tuplePtr, bgtuple, ul, ur, ll, lr;

    /* Guess a good background value. */
    ul = tuples[0][0];
    ur = tuples[0][pamP->width-1];
    ll = tuples[pamP->height-1][0];
    lr = tuples[pamP->height-1][pamP->width-1];
    bgtuple = NULL;

    /* We first recognize three corners equal.  If not, we look for any
       two.  If not, we just average all four.
    */
    if (pnm_tupleequal(pamP, ul, ur) && pnm_tupleequal(pamP, ur, ll))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ul, ur) &&
             pnm_tupleequal(pamP, ur, lr))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ul, ll) &&
             pnm_tupleequal(pamP, ll, lr))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ur, ll) &&
             pnm_tupleequal(pamP, ll, lr))
        tuplePtr = ur;
    else if (pnm_tupleequal(pamP, ul, ur))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ul, ll))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ul, lr))
        tuplePtr = ul;
    else if (pnm_tupleequal(pamP, ur, ll))
        tuplePtr = ur;
    else if (pnm_tupleequal(pamP, ur, lr))
        tuplePtr = ur;
    else if (pnm_tupleequal(pamP, ll, lr))
        tuplePtr = ll;
    else {
        /* Reimplement libpnm3.c's mean4() but for tuples. */
        unsigned int plane;
        bgtuple = pnm_allocpamtuple(pamP);
        for (plane = 0; plane < pamP->depth; ++plane)
          bgtuple[plane] = (ul[plane] + ur[plane] + ll[plane] + lr[plane]) / 4;
    }
    if (!bgtuple) {
        unsigned int plane;
        bgtuple = pnm_allocpamtuple(pamP);
        for (plane = 0; plane < pamP->depth; ++plane)
          bgtuple[plane] = tuplePtr[plane];
    }

    return bgtuple;
}



/*=============================================================================
   pm_system() Standard Input feeder and Standard Output accepter functions.   
=============================================================================*/

void
pm_feed_from_pamtuples(int    const pipeToFeedFd,
                       void * const feederParm) {

    struct pamtuples * const inputTuplesP = feederParm;

    struct pam outpam;

    outpam = *inputTuplesP->pamP;
    outpam.file = fdopen(pipeToFeedFd, "w");

    /* The following signals (and normally kills) the process with
       SIGPIPE if the pipe does not take all the data.
    */
    pnm_writepam(&outpam, *inputTuplesP->tuplesP);

    pm_close(outpam.file);
}



void
pm_accept_to_pamtuples(int    const pipeToSuckFd,
                       void * const accepterParm ) {

    struct pamtuples * const outputTuplesP = accepterParm;

    struct pam * const inpamP = outputTuplesP->pamP;

    *outputTuplesP->tuplesP =
        pnm_readpam(fdopen(pipeToSuckFd, "r"), inpamP,
                    PAM_STRUCT_SIZE(tuple_type));

    pm_close(inpamP->file);
}
