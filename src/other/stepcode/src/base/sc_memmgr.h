#ifndef SC_MEMMGR_H
#define SC_MEMMGR_H

#include <sc_export.h>
#include <stdlib.h>

#if defined(SC_MEMMGR_ENABLE_CHECKS)

/**
    Platform specific defines
*/
#if defined(__MSVC__) || defined(__BORLAND__)
#define THROW_STD_BAD_ALLOC
#define THROW_EMPTY
#else
#define THROW_STD_BAD_ALLOC     throw (std::bad_alloc)
#define THROW_EMPTY             throw ()
#endif

#ifdef  __cplusplus
#include <stddef.h>
extern "C" {
#endif /* __cplusplus */

    SC_BASE_EXPORT void * sc_malloc_fn( unsigned int size, const char * file, const int line );
    SC_BASE_EXPORT void * sc_calloc_fn( unsigned int count, unsigned int size, const char * file, const int line );
    SC_BASE_EXPORT void * sc_realloc_fn( void * addr, unsigned int size, const char * file, const int line );
    SC_BASE_EXPORT void   sc_free_fn( void * addr );

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

SC_BASE_EXPORT void * sc_operator_new( size_t size, const char * file, const int line );
SC_BASE_EXPORT void   sc_operator_delete( void * addr, const char * file, const int line );
SC_BASE_EXPORT void   sc_operator_delete( void * addr );

#endif /* __cplusplus */

#ifndef SC_MEMMGR_CC

#define sc_malloc(size)            sc_malloc_fn(size, __FILE__, __LINE__)
#define sc_calloc(count, size)     sc_calloc_fn(count, size, __FILE__, __LINE__)
#define sc_realloc(addr, size)     sc_realloc_fn(addr, size, __FILE__, __LINE__)
#define sc_free(addr)              sc_free_fn(addr)

#ifdef  __cplusplus

#include <new>

inline void * operator new( size_t size, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    return sc_operator_new( size, file, line );
}

inline void * operator new[]( size_t size, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    return sc_operator_new( size, file, line );
}

inline void operator delete( void * addr, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    sc_operator_delete( addr, file, line );
}

inline void operator delete[]( void * addr, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    sc_operator_delete( addr, file, line );
}

inline void operator delete( void * addr ) THROW_EMPTY {
    sc_operator_delete( addr );
}

inline void operator delete[]( void * addr ) THROW_EMPTY {
    sc_operator_delete( addr );
}

#define new new(__FILE__, __LINE__)

#endif /* __cplusplus */

#endif /* SC_MEMMGR_CC */

#else
#define sc_malloc(size)            malloc(size)
#define sc_calloc(count, size)     calloc(count, size)
#define sc_realloc(addr, size)     realloc(addr, size)
#define sc_free(addr)              free(addr)
#endif /* SC_MEMMGR_ENABLE_CHECKS */

#endif /* SC_MEMMGR_H */
