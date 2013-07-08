//  Judy arrays 13 DEC 2012 (judy64n.c from http://code.google.com/p/judyarray/ )
//  This code is public domain.

//  Author Karl Malbrain, malbrain AT yahoo.com
//  with assistance from Jan Weiss.
//  modifications (and any bugs) by Mark Pictor, mpictor at gmail

//  Simplified judy arrays for strings and integers
//  Adapted from the ideas of Douglas Baskins of HP.

//  Map a set of keys to corresponding memory cells (uints).
//  Each cell must be set to a non-zero value by the caller.

//  STANDALONE is defined to compile into a string sorter.

//  String mappings are denoted by calling judy_open with zero as
//  the second argument.

#ifndef STANDALONE
#  error must define STANDALONE while compiling this file and judy64.c
#endif

#include "judy.h"
#include "sort.h"

unsigned int MaxMem = 0;

//    usage:
//    a.out [in-file] [out-file] [keysize] [recordlen] [keyoffset] [mergerecs]
//    where keysize is 10 to indicate pennysort files

//    ASKITIS compilation:
//    g++ -O3 -fpermissive -fomit-frame-pointer -w -D STANDALONE -D ASKITIS -o judy64n judy64n.c
//    ./judy64n [input-file-to-build-judy] e.g. distinct_1 or skew1_1

//    note:  the judy array is an in-memory data structure. As such, make sure you
//    have enough memory to hold the entire input file + data structure, otherwise
//    you'll have to break the input file into smaller pieces and load them in
//    on-by-one.

//    Also, the file to search judy is hardcoded to skew1_1.

int main( int argc, char ** argv ) {
    unsigned char buff[1024];
    JudySlot max = 0;
    JudySlot * cell;
    FILE * in, *out;
    void * judy;
    unsigned int len;
    unsigned int idx;
#ifdef ASKITIS
    char * askitis;
    int prev, off;
    float insert_real_time = 0.0;
    float search_real_time = 0.0;
    int size;
#if !defined(_WIN32)
    timer start, stop;
#else
    time_t start[1], stop[1];
#endif
#endif

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

    if( argc > 6 ) {
        PennyRecs = atoi( argv[6] );
    }

    if( argc > 5 ) {
        PennyOff = atoi( argv[5] );
    }

    if( argc > 4 ) {
        PennyLine = atoi( argv[4] );
    }

    PennyMerge = ( unsigned long long )PennyLine * PennyRecs;

    if( argc > 3 ) {
        PennyKey = atoi( argv[3] );
        sort( in, argv[2] );
        return merge( out, argv[2] );
    }

#ifdef ASKITIS
    judy = judy_open( 1024, 0 );

//    build judy array
    size = lseek( fileno( in ), 0L, 2 );
    askitis = malloc( size );
    lseek( fileno( in ), 0L, 0 );
    read( fileno( in ), askitis, size );
    prev = 0;
//    naskitis.com:
//    Start the timer.

#if !defined(_WIN32)
    gettimeofday( &start, NULL );
#else
    time( start );
#endif

    for( off = 0; off < size; off++ )
        if( askitis[off] == '\n' ) {
            *( judy_cell( judy, askitis + prev, off - prev ) ) += 1;   // count instances of string
            prev = off + 1;
        }
//    naskitis.com:
//    Stop the timer and do some math to compute the time required to insert the strings into the judy array.

#if !defined(_WIN32)
    gettimeofday( &stop, NULL );

    insert_real_time = 1000.0 * ( stop.tv_sec - start.tv_sec ) + 0.001 * ( stop.tv_usec - start.tv_usec );
    insert_real_time = insert_real_time / 1000.0;
#else
    time( stop );
    insert_real_time = *stop - *start;
#endif

//    naskitis.com:
//    Free the input buffer used to store the first file.  We must do this before we get the process size below.
    free( askitis );
    fprintf( stderr, "JudyArray@Karl_Malbrain\nDASKITIS option enabled\n-------------------------------\n%-20s %.2f MB\n%-20s %.2f sec\n",
             "Judy Array size:", MaxMem / 1000000., "Time to insert:", insert_real_time );
    fprintf( stderr, "%-20s %d\n", "Words:", Words );
    fprintf( stderr, "%-20s %d\n", "Inserts:", Inserts );
    fprintf( stderr, "%-20s %d\n", "Found:", Found );

    Words = 0;
    Inserts = 0;
    Found = 0;

//    search judy array
    if( in = freopen( "skew1_1", "rb", in ) ) {
        size = lseek( fileno( in ), 0L, 2 );
    } else {
        exit( 0 );
    }
    askitis = malloc( size );
    lseek( fileno( in ), 0L, 0 );
    read( fileno( in ), askitis, size );
    prev = 0;

#if !defined(_WIN32)
    gettimeofday( &start, NULL );
#else
    time( start );
#endif

    for( off = 0; off < size; off++ )
        if( askitis[off] == '\n' ) {
            *judy_cell( judy, askitis + prev, off - prev ) += 1;
            prev = off + 1;
        }
//    naskitis.com:
//    Stop the timer and do some math to compute the time required to search the judy array.

#if !defined(_WIN32)
    gettimeofday( &stop, NULL );
    search_real_time = 1000.0 * ( stop.tv_sec - start.tv_sec ) + 0.001
                       * ( stop.tv_usec - start.tv_usec );
    search_real_time = search_real_time / 1000.0;
#else
    time( stop );
    search_real_time = *stop - *start;
#endif

//    naskitis.com:
//    To do: report a count on the number of strings found.

    fprintf( stderr, "\n%-20s %.2f MB\n%-20s %.2f sec\n",
             "Judy Array size:", MaxMem / 1000000., "Time to search:", search_real_time );
    fprintf( stderr, "%-20s %d\n", "Words:", Words );
    fprintf( stderr, "%-20s %d\n", "Inserts:", Inserts );
    fprintf( stderr, "%-20s %d\n", "Found:", Found );
    exit( 0 );
#endif

    judy = judy_open( 1024, 0 );

    while( fgets( ( char * )buff, sizeof( buff ), in ) ) {
        if( len = strlen( ( const char * )buff ) ) {
            buff[--len] = 0;    // remove LF
        }
        *( judy_cell( judy, buff, len ) ) += 1;     // count instances of string
        max++;
    }

    fprintf( stderr, "%" PRIuint " memory used\n", MaxMem );

    cell = judy_strt( judy, NULL, 0 );

    if( cell ) do {
            len = judy_key( judy, buff, sizeof( buff ) );
            for( idx = 0; idx < *cell; idx++ ) {       // spit out duplicates
                fwrite( buff, len, 1, out );
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

