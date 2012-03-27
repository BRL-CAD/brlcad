#ifndef SCL_MEMMGR_H
#define SCL_MEMMGR_H

#include <scl_export.h>

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
#include <cstddef>
extern "C" {
#endif /* __cplusplus */

    SCL_BASE_EXPORT void * scl_malloc_fn( unsigned int size, const char * file, const int line );
    SCL_BASE_EXPORT void * scl_calloc_fn( unsigned int count, unsigned int size, const char * file, const int line );
    SCL_BASE_EXPORT void * scl_realloc_fn( void * addr, unsigned int size, const char * file, const int line );
    SCL_BASE_EXPORT void   scl_free_fn( void * addr );

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

SCL_BASE_EXPORT void * scl_operator_new( size_t size, const char * file, const int line );
SCL_BASE_EXPORT void   scl_operator_delete( void * addr, const char * file, const int line );
SCL_BASE_EXPORT void   scl_operator_delete( void * addr );

#endif /* __cplusplus */

#ifndef SCL_MEMMGR_CC

#define scl_malloc(size)            scl_malloc_fn(size, __FILE__, __LINE__)
#define scl_calloc(count, size)     scl_calloc_fn(count, size, __FILE__, __LINE__)
#define scl_realloc(addr, size)     scl_realloc_fn(addr, size, __FILE__, __LINE__)
#define scl_free(addr)              scl_free_fn(addr)

#ifdef  __cplusplus

#include <new>

inline void * operator new( size_t size, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    return scl_operator_new( size, file, line );
}

inline void * operator new[]( size_t size, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    return scl_operator_new( size, file, line );
}

inline void operator delete( void * addr, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    scl_operator_delete( addr, file, line );
}

inline void operator delete[]( void * addr, const char * file, const int line ) THROW_STD_BAD_ALLOC {
    scl_operator_delete( addr, file, line );
}

inline void operator delete( void * addr ) THROW_EMPTY {
    scl_operator_delete( addr );
}

inline void operator delete[]( void * addr ) THROW_EMPTY {
    scl_operator_delete( addr );
}

#define new new(__FILE__, __LINE__)

#endif /* __cplusplus */

#endif /* SCL_MEMMGR_CC */

#endif /* SCL_MEMMGR_H */
