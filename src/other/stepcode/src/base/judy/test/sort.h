#ifndef SORT_H
#define SORT_H

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
//  the second argument.  Integer mappings are denoted by calling
//  judy_open with the Integer depth of the Judy Trie as the second
//  argument.

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "judy.h"

#ifdef linux
#  include <fcntl.h>
#  include <unistd.h>
#  include <errno.h>
#  include <sys/mman.h>
#  include <sys/times.h>
#else
#  include <windows.h>
#  include <io.h>
#endif

#include <time.h>

#define PRIuint "u"


//these are initialized in penny.c
extern unsigned int PennyRecs;
extern unsigned int PennyLine;
extern unsigned int PennyKey;
extern unsigned int PennyOff;
extern unsigned long long PennyMerge;

typedef struct {
    void * buff;       // record pointer in input file map
    void * next;       // duplicate chain
} PennySort;

#endif //SORT_H