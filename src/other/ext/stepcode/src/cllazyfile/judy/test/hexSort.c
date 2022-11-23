//  Judy arrays 13 DEC 2012 (judy64n.c from http://code.google.com/p/judyarray/ )
//  This code is public domain.

//  Author Karl Malbrain, malbrain AT yahoo.com
//  with assistance from Jan Weiss.
//  modifications (and any bugs) by Mark Pictor, mpictor at gmail

//  Simplified judy arrays for strings and integers
//  Adapted from the ideas of Douglas Baskins of HP.

//  Map a set of keys to corresponding memory cells (uints).
//  Each cell must be set to a non-zero value by the caller.


//  Integer mappings are denoted by calling judy_open with the
//  Integer depth of the Judy Trie as the second argument.

#ifndef STANDALONE
#  error must define STANDALONE while compiling this file and judy64.c
#endif

#include "judy.h"
#include "sort.h"

unsigned int MaxMem = 0;
/**
   usage:
   a.out [in-file] [out-file]
   where all lines of in-file are 32 chars, hexadecimal
   On linux, a 1M-line file can be created with the following:
   hexdump -v -e '2/8 "%08x"' -e '"\n"' /dev/urandom |head -n 1000000 >in-file
*/
int main( int argc, char ** argv ) {
    unsigned char buff[1024];
    JudySlot max = 0;
    JudySlot * cell;
    FILE * in, *out;
    void * judy;
    unsigned int len;
    unsigned int idx;

    if( argc > 1 ) {
        in = fopen( argv[1], "rb" );
    } else {
        in = stdin;
    }

    if( argc > 2 ) {
        out = fopen( argv[2], "wb" );
    } else {
        out = stdout;
    }

    setvbuf( out, NULL, _IOFBF, 4096 * 1024 );

    if( !in ) {
        fprintf( stderr, "unable to open input file\n" );
    }

    if( !out ) {
        fprintf( stderr, "unable to open output file\n" );
    }

    PennyMerge = ( unsigned long long )PennyLine * PennyRecs;

    judy = judy_open( 1024, 16 / JUDY_key_size );

    while( fgets( ( char * )buff, sizeof( buff ), in ) ) {
        judyvalue key[16 / JUDY_key_size];
        if( len = strlen( ( const char * )buff ) ) {
            buff[--len] = 0;    // remove LF
        }
#if JUDY_key_size == 4
        key[3] = strtoul( buff + 24, NULL, 16 );
        buff[24] = 0;
        key[2] = strtoul( buff + 16, NULL, 16 );
        buff[16] = 0;
        key[1] = strtoul( buff + 8, NULL, 16 );
        buff[8] = 0;
        key[0] = strtoul( buff, NULL, 16 );
#else
        key[1] = strtoull( buff + 16, NULL, 16 );
        buff[16] = 0;
        key[0] = strtoull( buff, NULL, 16 );
#endif
        *( judy_cell( judy, ( void * )key, 0 ) ) += 1;   // count instances of string
        max++;
    }

    fprintf( stderr, "%" PRIuint " memory used\n", MaxMem );

    cell = judy_strt( judy, NULL, 0 );

    if( cell ) do {
            judyvalue key[16 / JUDY_key_size];
            len = judy_key( judy, ( void * )key, 0 );
            for( idx = 0; idx < *cell; idx++ ) {       // spit out duplicates
#if JUDY_key_size == 4
                fprintf( out, "%.8X", key[0] );
                fprintf( out, "%.8X", key[1] );
                fprintf( out, "%.8X", key[2] );
                fprintf( out, "%.8X", key[3] );
#else
                fprintf( out, "%.16llX", key[0] );
                fprintf( out, "%.16llX", key[1] );
#endif
                fputc( '\n', out );
            }
        } while( cell = judy_nxt( judy ) );

#if 0
    // test deletion all the way to an empty tree

    if( cell = judy_prv( judy ) )
        do {
            max -= *cell;
        } while( cell = judy_del( judy ) );

    assert( max == 0 );
#endif
    judy_close( judy );
    return 0;
}

