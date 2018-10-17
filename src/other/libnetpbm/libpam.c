/*----------------------------------------------------------------------------
                                  libpam.c
------------------------------------------------------------------------------
   These are the library functions, which belong in the libnetpbm library,
   that deal with the PAM (Portable Arbitrary Format) image format.
-----------------------------------------------------------------------------*/

/* See libpm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  
#define _BSD_SOURCE 1      /* Make sure strdup() is in string.h */
#define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */

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
    unsigned int const depth = allocationDepth(pamP);

    if (depth > INT_MAX/sizeof(sample))
        pm_error("image depth (%u) too large to be processed", depth);
    else if (pamP->width > 0 && depth * sizeof(sample) > INT_MAX/pamP->width)
        pm_error("image width and depth (%u, %u) too large "
                 "to be processed.", pamP->width, depth);
    else if (pamP->width * (depth * sizeof(sample)) >
             INT_MAX - depth * sizeof(tuple *))
        pm_error("image width and depth (%u, %u) too large "
                 "to be processed.", pamP->width, depth);
    
    if (depth > INT_MAX - 2)
        pm_error("image depth (%u) too large to be processed", depth);
    if (pamP->width > INT_MAX - 2)
        pm_error("image width (%u) too large to be processed", pamP->width);
    if (pamP->height > INT_MAX - 2)
        pm_error("image height (%u) too large to be processed", pamP->height);
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



void
createBlackTuple(const struct pam * const pamP, 
                 tuple *            const blackTupleP) {

/* This is poorly named, because it lacks the "pnm" prefix.  But for some
   reason, this is how we originally named this.  So to maintain backward
   compatibility with binaries that refer to "createBlackTuple", we define
   this.  The preferred name, pnm_createBlackTuple() was new in Netpbm 10.20,
   January 2004.  We should eventually retire createBlackTuple().
*/
    pnm_createBlackTuple(pamP, blackTupleP);
}



static tuple *
allocPamRow(const struct pam * const pamP) {
/*----------------------------------------------------------------------------
   We assume that the dimensions of the image are such that arithmetic
   overflow will not occur in our calculations.  NOTE: pnm_readpaminit()
   ensures this assumption is valid.
-----------------------------------------------------------------------------*/
    /* The tuple row data structure starts with 'width' pointers to
       the tuples, immediately followed by the 'width' tuples
       themselves.  Each tuple consists of 'depth' samples.  
    */

    int const bytesPerTuple = allocationDepth(pamP) * sizeof(sample);
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
        pm_error("Out of memory allocating space for a tuple row of\n"
                 "%d tuples by %d samples per tuple by %d bytes per sample.",
                 pamP->width, allocationDepth(pamP), sizeof(sample));

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
                 pamP->len, PAM_STRUCT_SIZE(allocation_depth));

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
parse_header_line(const char buffer[], char label[MAX_LABEL_LENGTH+1], 
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
process_header_line(char                const buffer[],
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

    parse_header_line(buffer, label, value);

    if (strcmp(label, "ENDHDR") == 0)
        headerSeenP->endhdr = TRUE;
    else {
        if (strcmp(label, "WIDTH") == 0 ||
            strcmp(label, "HEIGHT") == 0 ||
            strcmp(label, "DEPTH") == 0 ||
            strcmp(label, "MAXVAL") == 0) {

            if (strlen(value) == 0)
                pm_error("Missing value for %s in PAM file header.",
                         label);
            else {
                char *endptr;
                long int numeric_value;
                errno = 0;  /* Clear errno so we can detect strtol() failure */
                numeric_value = strtol(value, &endptr, 10);
                if (errno != 0)
                    pm_error("Too-large value for %s in "
                             "PAM file header: '%s'", label, value);
                if (*endptr != '\0') 
                    pm_error("Non-numeric value for %s in "
                             "PAM file header: '%s'", label, value);
                else if (numeric_value < 0) 
                    pm_error("Negative value for %s in "
                             "PAM file header: '%s'", label, value);
            }
        }
    
        if (strcmp(label, "WIDTH") == 0) {
            pamP->width = atoi(value);
            headerSeenP->width = TRUE;
        } else if (strcmp(label, "HEIGHT") == 0) {
            pamP->height = atoi(value);
            headerSeenP->height = TRUE;
        } else if (strcmp(label, "DEPTH") == 0) {
            pamP->depth = atoi(value);
            headerSeenP->depth = TRUE;
        } else if (strcmp(label, "MAXVAL") == 0) {
            pamP->maxval = atoi(value);
            headerSeenP->maxval = TRUE;
        } else if (strcmp(label, "TUPLTYPE") == 0) {
            int len = strlen(pamP->tuple_type);
            if (len + strlen(value) + 1 > sizeof(pamP->tuple_type)-1)
                pm_error("TUPLTYPE value too long in PAM header");
            if (len == 0)
                strcpy(pamP->tuple_type, value);
            else {
                strcat(pamP->tuple_type, "\n");
                strcat(pamP->tuple_type, value);
            }
            pamP->tuple_type[sizeof(pamP->tuple_type)-1] = '\0';
        } else 
            pm_error("Unrecognized header line: '%s'.  "
                     "Possible missing ENDHDR line?", label);
    }
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
                 "the PAM header", commentLen);

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
                process_header_line(buffer, pamP, &headerSeen);
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

    pam.size        = sizeof(struct pam);
    pam.file        = fileP;
    pam.len         = PAM_STRUCT_SIZE(tuple_type);
    pam.format      = PAM_FORMAT;

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
    default: {
        pm_error("Cannot treat PAM image as PPM or PGM, "
                 "because its depth (%u) "
                 "is not 1 or 3.", pam.depth);
    }
    }

    *colsP   = pam.width;
    *rowsP   = pam.height;
    *maxvalP = (gray)pam.maxval;
}


                
unsigned int
pnm_bytespersample(sample const maxval) {

    assert(sizeof(maxval) * 8 <= 32);

    if      (maxval >>  8 == 0) return 1;
    else if (maxval >> 16 == 0) return 2;
    else if (maxval >> 24 == 0) return 3;
    else                        return 4;
}



void 
pnm_readpaminit(FILE *       const file, 
                struct pam * const pamP, 
                int          const size) {

    if (size < PAM_STRUCT_SIZE(tuple_type)) 
        pm_error("pam object passed to pnm_readpaminit() is too small.  "
                 "It must be large\n"
                 "enough to hold at least up to the "
                 "'tuple_type' member, but according\n"
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
        pm_error("bad magic number - not a PAM, PPM, PGM, or PBM file");
    }
    
    pamP->bytes_per_sample = pnm_bytespersample(pamP->maxval);
    pamP->plainformat = FALSE;
        /* See below for complex explanation of why this is FALSE. */

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
                 "(%d bytes, according to its 'size' element) "
                 "than the amount of data in it "
                 "(%d bytes, according to its 'len' element).",
                 pamP->size, pamP->len);

    if (pamP->len < PAM_STRUCT_SIZE(bytes_per_sample))
        pm_error("pam object passed to pnm_writepaminit() is too small.  "
                 "It must be large\n"
                 "enough to hold at least up through the "
                 "'bytes_per_sample' member, but according\n"
                 "to its 'len' member, it is only %d bytes long.", 
                 pamP->len);

    if (pamP->maxval > PAM_OVERALL_MAXVAL)
        pm_error("maxval (%lu) passed to pnm_writepaminit() "
                 "is greater than %u", pamP->maxval, PAM_OVERALL_MAXVAL);

    if (pamP->len < PAM_STRUCT_SIZE(tuple_type))
        tupleType = "";
    else
        tupleType = pamP->tuple_type;

    pamP->bytes_per_sample = pnm_bytespersample(pamP->maxval);
    
    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE:
        if (pm_plain_output)
            pm_error("There is no plain version of PAM.  -plain option "
                     "is not allowed");
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
pnm_getopacity(const struct pam * const pamP,
               bool *             const haveOpacityP,
               unsigned int *     const opacityPlaneP) {

    /* Design note; If use of this information proliferates, we should
       probably add it to struct pam as convenience values analogous to
       bytes_per_sample.
    */
    if (strcmp(pamP->tuple_type, "RGB_ALPHA") == 0) {
        *haveOpacityP = TRUE;
        *opacityPlaneP = PAM_TRN_PLANE;
    } else if (strcmp(pamP->tuple_type, "GRAYSCALE_ALPHA") == 0) {
        *haveOpacityP = TRUE;
        *opacityPlaneP = PAM_GRAY_TRN_PLANE;
    } else
        *haveOpacityP = FALSE;
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
