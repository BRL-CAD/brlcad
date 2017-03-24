/*\
 * $Id$
 *
 * bitio.c - bitstream I/O
 *
 * Works for (sizeof(unsigned long)-1)*8 bits.
 *
 * Copyright (C) 1992 by David W. Sanderson.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  This software is provided "as is"
 * without express or implied warranty.
 *
 * $Log$
 * Revision 1.5  1992/11/24  19:36:46  dws
 * Added copyright.
 *
 * Revision 1.4  1992/11/17  03:37:50  dws
 * updated comment
 *
 * Revision 1.3  1992/11/10  23:15:16  dws
 * Removed superfluous code.
 *
 * Revision 1.2  1992/11/10  23:11:22  dws
 * Generalized to handle more than one bitstream at once.
 *
 * Revision 1.1  1992/11/10  18:33:21  dws
 * Initial revision
 *
\*/

#include <string.h>

#include "bitio.h"

struct bitstream
{
    FILE *
        f;      /* bytestream */
    unsigned long
        bitbuf;     /* bit buffer */
    int
        nbitbuf;    /* number of bits in 'bitbuf' */
    char
        mode;
};

#define Mask(n)     ((1<<(n))-1)

#define BitPut(b,ul,n)  ((b)->bitbuf = (((b)->bitbuf<<(n))  \
                    |((ul)&Mask(n))),   \
            (b)->nbitbuf += (n))

#define BitGet(b,n) (((b)->bitbuf>>((b)->nbitbuf-=(n))) & Mask(n))

/*
 * pm_bitinit() - allocate and return a struct bitstream * for the
 * given FILE*.
 *
 * mode must be one of "r" or "w", according to whether you will be
 * reading from or writing to the struct bitstream *.
 *
 * Returns 0 on error.
 */

struct bitstream *
pm_bitinit(FILE * const f, const char * const mode) {

    struct bitstream * ans;

    if (!f || !mode)
        ans = NULL;
    else if (strcmp(mode, "r") != 0 && strcmp(mode, "w") != 0)
        ans = NULL;
    else {
        ans = (struct bitstream *)calloc(1, sizeof(struct bitstream));
        if (ans != NULL) {
            ans->f = f;
            ans->mode = *mode;
        }
    }
    return ans;
}

/*
 * pm_bitfini() - deallocate the given struct bitstream *.
 *
 * You must call this after you are done with the struct bitstream *.
 * 
 * It may flush some bits left in the buffer.
 *
 * Returns the number of bytes written, -1 on error.
 */

int
pm_bitfini(b)
    struct bitstream *b;
{
    int     nbyte = 0;

    if(!b)
        return -1;

    /* flush the output */
    if(b->mode == 'w')
    {
        /* flush the bits */
        if (b->nbitbuf < 0 || b->nbitbuf >= 8)
        {
            /* pm_bitwrite() didn't work */
            return -1;
        }

        /*
         * If we get to here, nbitbuf is 0..7
         */
        if(b->nbitbuf)
        {
            char    c;

            BitPut(b, 0, (long)8-(b->nbitbuf));
            c = (char) BitGet(b, (long)8);
            if(putc(c, b->f) == EOF)
            {
                return -1;
            }
            nbyte++;
        }
    }

    free(b);
    return nbyte;
}

/*
 * pm_bitread() - read the next nbits into *val from the given file.
 * 
 * The last pm_bitread() must be followed by a call to pm_bitfini().
 * 
 * Returns the number of bytes read, -1 on error.
 */

int 
pm_bitread(b, nbits, val)
    struct bitstream *b;
    unsigned long   nbits;
    unsigned long  *val;
{
    int     nbyte = 0;
    int     c;

    if(!b)
        return -1;

    while (b->nbitbuf < nbits)
    {
        if((c = getc(b->f)) == EOF)
        {
            return -1;
        }
        nbyte++;

        BitPut(b, c, (long) 8);
    }

    *val = BitGet(b, nbits);
    return nbyte;
}

/*
 * pm_bitwrite() - write the low nbits of val to the given file.
 * 
 * The last pm_bitwrite() must be followed by a call to pm_bitfini().
 * 
 * Returns the number of bytes written, -1 on error.
 */

int
pm_bitwrite(b, nbits, val)
    struct bitstream *b;
    unsigned long   nbits;
    unsigned long   val;
{
    int     nbyte = 0;
    char        c;

    if(!b)
        return -1;

    BitPut(b, val, nbits);

    while (b->nbitbuf >= 8)
    {
        c = (char) BitGet(b, (long)8);

        if(putc(c, b->f) == EOF)
        {
            return -1;
        }
        nbyte++;
    }

    return nbyte;
}
