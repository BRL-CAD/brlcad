/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */

/*  compgif.c */
/*
 *
 * GIF Image compression - LZW algorithm implemented with Trie type
 *                         structure.
 *                         Written by Bailey Brown, Jr.
 *                         last change May 24, 1990
 *                         file: compgif.c
 *
 *  You may use or modify this code as you wish, as long as you mention
 *  my name in your documentation.
 *
 *                  - Bailey Brown, Jr.
 *
 */

#include "rle_config.h"
#include <stdio.h>
#include "rletogif.h"

#define MAXIMUMCODE 4095   /* 2**maximum_code_size */
#define BLOCKSIZE 256   /* max block byte count + 1 */
#define NULLPREFIX -1


typedef struct str_table_entry {
        int code;
        int prefix;
        int suffix;
}  strTableEntry;

typedef struct str_table_node {
        strTableEntry entry;
    struct str_table_node *left;
    struct str_table_node *right;
	struct str_table_node *children;
} strTableNode, *strTableNodePtr, **strTable;

int pack_bits();


/*
 ********************************************************************
 * compgif() recieves pointers to an input function and an output    *
 * stream, and the code size as parameters and outputs successive   *
 * blocks of LZW compressed gif data.  The calling routine should   *
 * have aready written the GIF file header out to the output file.  *
 * It assumes that there will be no more than 8 bits/pixel and that *
 * each data item comes from successive bytes returned by infun.    *
 ********************************************************************
 */
int compgif(code_size, os, infun)
int code_size; /* i.e. for 8 bits/pixel code_size = 9; */
FILE* os;/* output stream should be open in write-binary mode */
ifunptr infun; /* input function should return consecutive pixel values */
{
    strTable heap; /* our very own memory manager */
    int heap_index;
    int clear_code, end_code, cur_code;
    int i, found, num_colors, prefix, compress_size;
    int cur_char, end_of_data, bits_per_pix;
    strTableNodePtr cur_node;
    strTable root;	  /* root of string table for LZW compression */
                                 /* is an array of 2**bits_per_pix pointers
				    to atomic nodes */
    heap_index = 0;
    heap = (strTable)malloc(sizeof(strTableNodePtr)*MAXIMUMCODE);
    if (heap == NULL) error("can't allocate heap");
    for (i=0; i < MAXIMUMCODE; i++) {
	heap[i] = (strTableNodePtr)malloc(sizeof(strTableNode));
        if (heap[i] == NULL) error("can't allocate heap");
    }
    bits_per_pix = code_size - 1;
    compress_size = code_size;
    num_colors = 1<<(bits_per_pix);
    clear_code = num_colors;
    end_code = clear_code + 1;
    cur_code = end_code + 1;
    prefix = NULLPREFIX;
    root = (strTable)malloc(sizeof(strTableNodePtr)*num_colors);
    if (!root) error("memory allocation failure (root)");
    for(i=0; i<num_colors; i++) {
	root[i] = heap[heap_index++];
        root[i]->entry.code = i;
        root[i]->entry.prefix = NULLPREFIX;
        root[i]->entry.suffix = i;
	root[i]->left = NULL;
	root[i]->right = NULL;
        root[i]->children = NULL;
    }
    /* initialize  output block */
    pack_bits(compress_size, -1, os);
    pack_bits(compress_size, clear_code, os);
    end_of_data = 0;
    if((cur_char = GIFNextPixel(infun)) == EOF) error("premature end of data");
    while (!end_of_data) {
        prefix = cur_char;
        cur_node = root[prefix];
        found = 1;
        if((cur_char = GIFNextPixel(infun)) == EOF) {
            end_of_data = 1; break;
        }
        while(cur_node->children && found) {
            cur_node = cur_node->children;
            while(cur_node->entry.suffix != cur_char) {
                if (cur_char < cur_node->entry.suffix) {
                    if (cur_node->left) cur_node = cur_node->left;
                    else {
                        cur_node->left = heap[heap_index++];
                        cur_node = cur_node->left;
                        found = 0; break;
                    }
                }
                else {
                    if (cur_node->right) cur_node = cur_node->right;
                    else {
                        cur_node->right = heap[heap_index++];
                        cur_node = cur_node->right;
                        found = 0; break;
                    }
                }
            }
            if (found) {
                prefix = cur_node->entry.code;
                if((cur_char = GIFNextPixel(infun)) == EOF) {
                    end_of_data = 1; break;
                }
            }
        }
        if (end_of_data) break;
        if (found) {
            cur_node->children = heap[heap_index++];
            cur_node = cur_node->children;
        }
        cur_node->children = NULL;
        cur_node->left = NULL;
        cur_node->right = NULL;
        cur_node->entry.code = cur_code;
        cur_node->entry.prefix = prefix;
        cur_node->entry.suffix = cur_char;
        pack_bits(compress_size, prefix, os);
        if (cur_code > ((1<<(compress_size))-1))
            compress_size++;
        if (cur_code < MAXIMUMCODE) {
            cur_code++;
        }
        else {
            heap_index = num_colors;  /* reinitialize string table */
            for (i=0; i < num_colors; i++ ) root[i]->children = NULL;
            pack_bits(compress_size, clear_code, os);
            compress_size = bits_per_pix + 1;
            cur_code = end_code + 1;
        }
    }
    pack_bits(compress_size, prefix, os);
    pack_bits(compress_size, end_code, os);
    pack_bits(compress_size, -1, os);
    for (i=0; i < MAXIMUMCODE; i++) free(heap[i]);
    free(heap);
    free(root);
    return (1);
}



/*
 ************************************************************************
 * pack_bits() packs the bits of the codes generated by gifenc() into   *
 * a 1..256 byte output block.  The first byte of the block is the      *
 * number 0..255 of data bytes in the block.  To flush or initialize    *
 * the block, pass a negative argument.                                 *
 ************************************************************************
 */
int pack_bits(compress_size, prefix, os)
int compress_size;
int prefix;
FILE* os;
{
    static int cur_bit = 8;
    static unsigned char block[BLOCKSIZE] = { 0 };
    int i, left_over_bits;

    /* if we are about to excede the bounds of block or if the flush
       code (code_bis < 0) we output the block */
    if((cur_bit + compress_size > (BLOCKSIZE-1)*8) || (prefix < 0)) {
        /* handle case of data overlapping blocks */
        if ((left_over_bits = (((cur_bit>>3) +
                ((cur_bit & 7) != 0))<<3) - cur_bit) != 0) {
            for (i=0; i < left_over_bits; i++) {
                if(prefix & (1<<i)) block[cur_bit>>3] |= (char)(1<<(cur_bit & 7));
                /* note n>>3 == n/8 and n & 7 == n % 8 */
                cur_bit++;
            }
        }
        compress_size -= left_over_bits;
        prefix = prefix>>left_over_bits;
        block[0] =  (unsigned char)((cur_bit>>3) - 1);
        if (block[0]) fwrite(block,1,block[0]+1,os);
        for(i=0; i < BLOCKSIZE; i++) block[i] = 0;
        cur_bit = 8;
    }
    if (prefix >= 0) {
        for (i=0; i < compress_size; i++) {
	    if(prefix & (1<<i)) block[cur_bit>>3] |=
		(unsigned char)(1<<(cur_bit & 7));
            /* note n>>3 == n/8 and n & 7 == n % 8 */
            cur_bit++;
        }
    }
    return (1);
}

/* end of compgif.c */
