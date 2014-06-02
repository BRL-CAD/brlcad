#ifndef JUDY64_H
#define JUDY64_H
//  Judy arrays 13 DEC 2012 (judy64n.c from http://code.google.com/p/judyarray/ )
//  This code is public domain.

//  Author Karl Malbrain, malbrain AT yahoo.com
//  with assistance from Jan Weiss.
//  modifications (and any bugs) by Mark Pictor, mpictor at gmail

//  Simplified judy arrays for strings and integers
//  Adapted from the ideas of Douglas Baskins of HP.

//  Map a set of keys to corresponding memory cells (uints).
//  Each cell must be set to a non-zero value by the caller.

//  String mappings are denoted by calling judy_open with zero as
//  the second argument.  Integer mappings are denoted by calling
//  judy_open with the Integer depth of the Judy Trie as the second
//  argument.

//  functions:
//  judy_open:  open a new judy array returning a judy object.
//  judy_close: close an open judy array, freeing all memory.
//  judy_clone: clone an open judy array, duplicating the stack.
//  judy_data:  allocate data memory within judy array for external use.
//  judy_cell:  insert a string into the judy array, return cell pointer.
//  judy_strt:  retrieve the cell pointer greater than or equal to given key
//  judy_slot:  retrieve the cell pointer, or return NULL for a given key.
//  judy_key:   retrieve the string value for the most recent judy query.
//  judy_end:   retrieve the cell pointer for the last string in the array.
//  judy_nxt:   retrieve the cell pointer for the next string in the array.
//  judy_prv:   retrieve the cell pointer for the prev string in the array.
//  judy_del:   delete the key and cell for the current stack entry.

/* Import/Export flags for base. */
#ifndef SC_BASE_EXPORT
# if defined(SC_BASE_DLL_EXPORTS) && defined(SC_BASE_DLL_IMPORTS)
#  error "Only SC_BASE_DLL_EXPORTS or SC_BASE_DLL_IMPORTS can be defined, not both."
# elif defined(SC_BASE_DLL_EXPORTS)
#  define SC_BASE_EXPORT __declspec(dllexport)
# elif defined(SC_BASE_DLL_IMPORTS)
#  define SC_BASE_EXPORT __declspec(dllimport)
# else
#  define SC_BASE_EXPORT
# endif
#endif

#if defined(__LP64__)      || \
    defined(__x86_64__)    || \
    defined(__amd64__)     || \
    defined(_WIN64)        || \
    defined(__sparc64__)   || \
    defined(__arch64__)    || \
    defined(__powerpc64__) || \
    defined (__s390x__)
//    defines for 64 bit
typedef unsigned long long judyvalue;
typedef unsigned long long JudySlot;
#define JUDY_key_mask (0x07)
#define JUDY_key_size 8
#define JUDY_slot_size 8
#define JUDY_span_bytes (3 * JUDY_key_size)
#define JUDY_span_equiv JUDY_2
#define JUDY_radix_equiv JUDY_8

#define PRIjudyvalue    "llu"

#else
//    defines for 32 bit
typedef unsigned int judyvalue;
typedef unsigned int JudySlot;
#define JUDY_key_mask (0x03)
#define JUDY_key_size 4
#define JUDY_slot_size 4
#define JUDY_span_bytes (7 * JUDY_key_size)
#define JUDY_span_equiv JUDY_4
#define JUDY_radix_equiv JUDY_8

#define PRIjudyvalue    "u"

#endif

#define JUDY_mask (~(JudySlot)0x07)

//    define the alignment factor for judy nodes and allocations
//    to enable this feature, set to 64

#define JUDY_cache_line 64     // minimum size is 8 bytes
// can be calculated using http://stackoverflow.com/a/4049562/382458 - but that would limit optimization!
// 10x 1M key hexsort, line size 64: 9.949s; size 8: 10.018s --> 1% improvement for 64; however, this may be dwarfed by the sort code

#define JUDY_seg    65536

enum JUDY_types {
    JUDY_radix        = 0,    // inner and outer radix fan-out
    JUDY_1            = 1,    // linear list nodes of designated count
    JUDY_2            = 2,
    JUDY_4            = 3,
    JUDY_8            = 4,
    JUDY_16           = 5,
    JUDY_32           = 6,
#ifdef ASKITIS
    JUDY_64           = 7
#else
    JUDY_span         = 7     // up to 28 tail bytes of key contiguously stored
#endif
};

typedef struct {
    void * seg;               // next used allocator
    unsigned int next;        // next available offset
} JudySeg;

typedef struct {
    JudySlot next;            // judy object
    unsigned int off;         // offset within key
    int slot;                 // slot within object
} JudyStack;

typedef struct {
    JudySlot root[1];         // root of judy array
    void ** reuse[8];         // reuse judy blocks
    JudySeg * seg;            // current judy allocator
    unsigned int level;       // current height of stack
    unsigned int max;         // max height of stack
    unsigned int depth;       // number of Integers in a key, or zero for string keys
    JudyStack stack[1];       // current cursor
} Judy;

#ifdef ASKITIS
int Words = 0;
int Inserts = 0;
int Found = 0;
#  if JUDY_key_size < 8
#    define JUDY_max    JUDY_16
#  else
#    define JUDY_max    JUDY_64
#  endif
#else
#  define JUDY_max    JUDY_32
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /// open a new judy array returning a judy object.
    SC_BASE_EXPORT Judy * judy_open( unsigned int max, unsigned int depth );

    /// close an open judy array, freeing all memory.
    SC_BASE_EXPORT void judy_close( Judy * judy );

    /// clone an open judy array, duplicating the stack.
    SC_BASE_EXPORT Judy * judy_clone( Judy * judy );

    /// allocate data memory within judy array for external use.
    SC_BASE_EXPORT void * judy_data( Judy * judy, unsigned int amt );

    /// insert a key into the judy array, return cell pointer.
    SC_BASE_EXPORT JudySlot * judy_cell( Judy * judy, const unsigned char * buff, unsigned int max );

    /// retrieve the cell pointer greater than or equal to given key
    SC_BASE_EXPORT JudySlot * judy_strt( Judy * judy, const unsigned char * buff, unsigned int max );

    /// retrieve the cell pointer, or return NULL for a given key.
    SC_BASE_EXPORT JudySlot * judy_slot( Judy * judy, const unsigned char * buff, unsigned int max );

    /// retrieve the string value for the most recent judy query.
    SC_BASE_EXPORT unsigned int judy_key( Judy * judy, unsigned char * buff, unsigned int max );

    /// retrieve the cell pointer for the last string in the array.
    SC_BASE_EXPORT JudySlot * judy_end( Judy * judy );

    /// retrieve the cell pointer for the next string in the array.
    SC_BASE_EXPORT JudySlot * judy_nxt( Judy * judy );

    /// retrieve the cell pointer for the prev string in the array.
    SC_BASE_EXPORT JudySlot * judy_prv( Judy * judy );

    /// delete the key and cell for the current stack entry.
    SC_BASE_EXPORT JudySlot * judy_del( Judy * judy );

#ifdef __cplusplus
}
#endif

#endif //JUDY64_H
