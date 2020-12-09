
#define SC_MEMMGR_CC

#include <sc_cf.h>
#include "sc_memmgr.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <set>

#ifdef SC_MEMMGR_ENABLE_CHECKS

/**
    sc_memmgr_error definition
*/
class sc_memmgr_error {
    private:
        std::string _srcfile;
        unsigned int _srcline;
        unsigned int _occurences;
    public:
        sc_memmgr_error( const std::string & file, const unsigned int line );
        sc_memmgr_error( const sc_memmgr_error & rhs );
        ~sc_memmgr_error( void );

        bool operator<( const sc_memmgr_error & rhs ) const;

        std::string getsrcfile( void ) const;
        unsigned int getsrcline( void ) const;
        unsigned int getoccurences( void ) const;
};

typedef std::set<sc_memmgr_error>              sc_memmgr_errors;
typedef std::set<sc_memmgr_error>::iterator    sc_memmgr_error_iterator;

/**
    sc_memmgr_record definition
*/
class sc_memmgr_record {
    private:
        void * _addr;
        size_t _size;
        std::string _srcfile;
        unsigned int _srcline;
    public:
        sc_memmgr_record( void * addr, size_t size, const char * file, const unsigned int line );
        sc_memmgr_record( void * addr );
        sc_memmgr_record( const sc_memmgr_record & rhs );
        ~sc_memmgr_record( void );

        bool operator<( const sc_memmgr_record & rhs ) const;

        void * getaddr( void ) const;
        size_t getsize( void ) const;
        std::string getsrcfile( void ) const;
        unsigned int getsrcline( void ) const;
};

typedef std::set<sc_memmgr_record>           sc_memmgr_records;
typedef std::set<sc_memmgr_record>::iterator sc_memmgr_record_iterator;

#endif /* SC_MEMMGR_ENABLE_CHECKS */

/**
    sc_memmgr definition
*/
class sc_memmgr {
    private:
#ifdef SC_MEMMGR_ENABLE_CHECKS
        bool _record_insert_busy, _record_erase_busy;
        // memory allocation/reallocation records, inserted at allocation, erased at deallocation.
        sc_memmgr_records _records;
        // memory stats
        unsigned int _allocated;            // amount of memory allocated simultaniously
        unsigned int _maximum_allocated;    // maximum amount of memory allocated simultaniously
        unsigned int _allocated_total;      // total amount of memory allocated in bytes
        unsigned int _reallocated_total;    // total amount of memory reallocated in bytes
        unsigned int _deallocated_total;    // total amount of memory deallocated in bytes
#endif /* SC_MEMMGR_ENABLE_CHECKS */
    protected:
    public:
        sc_memmgr( void );
        ~sc_memmgr( void );

        void * allocate( size_t size, const char * file, const int line );
        void * reallocate( void * addr, size_t size, const char * file, const int line );
        void   deallocate( void * addr, const char * file, const int line );
};

/**
    sc_memmgr instance.
    There should be one static instance of memmgr.
    This instance is automatically destroyed when application exits, so this allows us to add
    memory leak detection in it's destructor.
*/
sc_memmgr memmgr;

/**
    c memory functions implementation
*/
extern "C" {

    void * sc_malloc_fn( unsigned int size, const char * file, const int line ) {
        return memmgr.allocate( size, file, line );
    }

    void * sc_calloc_fn( unsigned int count, unsigned int size, const char * file, const int line ) {
        return memmgr.allocate( count * size, file, line );
    }

    void * sc_realloc_fn( void * addr, unsigned int size, const char * file, const int line ) {
        return memmgr.reallocate( addr, size, file, line );
    }

    void sc_free_fn( void * addr ) {
        memmgr.deallocate( addr, "", 0 );
    }

}

/**
    c++ memory operators implementation
*/
void * sc_operator_new( size_t size, const char * file, const int line ) {
    return memmgr.allocate( size, file, line );
}

void sc_operator_delete( void * addr, const char * file, const int line ) {
    memmgr.deallocate( addr, file, line );
}

void sc_operator_delete( void * addr ) {
    memmgr.deallocate( addr, "", 0 );
}

/**
    sc_memmgr implementation
*/
sc_memmgr::sc_memmgr( void ) {
#ifdef SC_MEMMGR_ENABLE_CHECKS
    _record_insert_busy = false;
    _record_erase_busy = false;

    _allocated = 0;
    _maximum_allocated = 0;
    _allocated_total = 0;
    _reallocated_total = 0;
    _deallocated_total = 0;
#endif /* SC_MEMMGR_ENABLE_CHECKS */
}

/**
    Destructor:
        sc_memmgr::~sc_memmgr(void)
    Description:
        The sc_memmgr destructor is used to check for memory leaks when enabled.
        All records still present when sc_memmgr instance is destroyed can be considered as
        memory leaks.
*/
sc_memmgr::~sc_memmgr( void ) {
#ifdef SC_MEMMGR_ENABLE_CHECKS
    sc_memmgr_record_iterator irecord;
    sc_memmgr_errors errors;
    sc_memmgr_error_iterator ierror;

    // Check if total allocated equals total deallocated
    if( _allocated_total != _deallocated_total ) {
        // todo: generate warning for possible memory leaks, enable full memory leak checking
        fprintf( stderr, "sc_memmgr warning: Possible memory leaks detected (%d of %d bytes)\n", _allocated_total - _deallocated_total, _allocated_total );
    }

    // Compact leaks into an error list to prevent same leak being reported multiple times.
    _record_insert_busy = true;
    _record_erase_busy = true;
    for( irecord = _records.begin();
            irecord != _records.end();
            irecord ++ ) {
        sc_memmgr_error error( irecord->getsrcfile(), irecord->getsrcline() );
        ierror = errors.find( error );
        if( ierror == errors.end() ) {
            errors.insert( error );
        }
        //else
        //    ierror->occurences ++;
    }
    _record_insert_busy = false;
    _record_erase_busy = false;

    // Loop through memory leaks to generate/buffer errors
    for( ierror = errors.begin();
            ierror != errors.end();
            ierror ++ ) {
        // todo: generate error for memory leak
        fprintf( stderr, "sc_memmgr warning: Possible memory leak in %s line %d\n", ierror->getsrcfile().c_str(), ierror->getsrcline() );
    }

    // Clear remaining records
    _record_erase_busy = true;
    _records.clear();
    errors.clear();
    _record_erase_busy = false;
#endif /* SC_MEMMGR_ENABLE_CHECKS */
}

void * sc_memmgr::allocate( size_t size, const char * file, const int line ) {
    void * addr;

    // Allocate
    addr = malloc( size );
    if( addr == NULL ) {
        // todo: error allocation failed
        fprintf( stderr, "sc_memmgr error: Memory allocation failed in %s line %d\n", file, line );
    }

    // Some stl implementations (for example debian gcc) use the new operator to construct
    // new elements when inserting sc_memmgr_record. When this our new operator gets used
    // for this operation, this would result in an infinite loop. This is fixed by the
    // _record_insert_busy flag.
#ifdef SC_MEMMGR_ENABLE_CHECKS
    if( !_record_insert_busy ) {
        // Store record for this allocation
        _record_insert_busy = true;
        _records.insert( sc_memmgr_record( addr, size, file, line ) );
        _record_insert_busy = false;

        // Update stats
        _allocated += size;
        if( _allocated > _maximum_allocated ) {
            _maximum_allocated = _allocated;
        }
        _allocated_total += size;
    }
#endif /* SC_MEMMGR_ENABLE_CHECKS */

    return addr;
}

void * sc_memmgr::reallocate( void * addr, size_t size, const char * file, const int line ) {
#ifdef SC_MEMMGR_ENABLE_CHECKS
    sc_memmgr_record_iterator record;

    if( !_record_insert_busy ) {
        // Find record of previous allocation/reallocation
        record = _records.find( sc_memmgr_record( addr ) );
        if( record == _records.end() ) {
            // todo: error reallocating memory not allocated?
            fprintf( stderr, "sc_memmgr warning: Reallocation of not allocated memory at %s line %d\n", file, line );
        } else {
            // Update stats
            _allocated -= record->getsize();
            _deallocated_total += record->getsize();

            // Erase previous allocation/reallocation
            _record_erase_busy = true;
            _records.erase( record );
            _record_erase_busy = false;
        }
    }
#endif /* SC_MEMMGR_ENABLE_CHECKS */

    // Reallocate
    addr = realloc( addr, size );
    if( addr == NULL ) {
        // todo: error reallocation failed
        fprintf( stderr, "sc_memmgr error: Reallocation failed at %s line %d\n", file, line );
    }

#ifdef SC_MEMMGR_ENABLE_CHECKS
    if( !_record_insert_busy ) {
        // Store record for this reallocation
        _record_insert_busy = true;
        _records.insert( sc_memmgr_record( addr, size, file, line ) );
        _record_insert_busy = false;

        // Update stats
        _allocated += size;
        if( _allocated > _maximum_allocated ) {
            _maximum_allocated = _allocated;
        }
        _allocated_total += size;
        _reallocated_total += size;
    }
#endif /* SC_MEMMGR_ENABLE_CHECKS */

    return addr;
}

void sc_memmgr::deallocate( void * addr, const char * file, const int line ) {
#ifdef SC_MEMMGR_ENABLE_CHECKS
    sc_memmgr_record_iterator record;

    if( !_record_erase_busy ) {
        // Find record of previous allocation/reallocation
        record = _records.find( sc_memmgr_record( addr ) );
        if( record == _records.end() ) {
            // todo: error free called for not allocated memory?
            fprintf( stderr, "sc_memmgr warning: Deallocate of not allocated memory at %s line %d\n", file, line );
        } else {
            // Update stats
            _allocated -= record->getsize();
            _deallocated_total += record->getsize();

            // Erase record
            _record_erase_busy = true;
            _records.erase( record );
            _record_erase_busy = false;
        }
    }
#else
    (void) file; // quell unused param warnings
    (void) line;
#endif /* SC_MEMMGR_ENABLE_CHECKS */

    // Deallocate
    free( addr );
}

#ifdef SC_MEMMGR_ENABLE_CHECKS
/**
    sc_memmgr_error implementation
*/
sc_memmgr_error::sc_memmgr_error( const std::string & file, const unsigned int line ) {
    _srcfile = file;
    _srcline = line;
    _occurences = 1;
}

sc_memmgr_error::sc_memmgr_error( const sc_memmgr_error & rhs ) {
    _srcfile = rhs._srcfile;
    _srcline = rhs._srcline;
    _occurences = rhs._occurences;
}

sc_memmgr_error::~sc_memmgr_error( void ) {
}

bool sc_memmgr_error::operator<( const sc_memmgr_error & rhs ) const {
    if( _srcfile == rhs._srcfile ) {
        return _srcline < rhs._srcline;
    }
    return _srcfile < rhs._srcfile;
}

std::string sc_memmgr_error::getsrcfile( void ) const {
    return _srcfile;
}

unsigned int sc_memmgr_error::getsrcline( void ) const {
    return _srcline;
}

unsigned int sc_memmgr_error::getoccurences( void ) const {
    return _occurences;
}

/**
    sc_memmgr_record implementation
*/
sc_memmgr_record::sc_memmgr_record( void * addr, size_t size, const char * file, const unsigned int line ) {
    _addr = addr;
    _size = size;
    _srcfile = file;
    _srcline = line;
}

sc_memmgr_record::sc_memmgr_record( void * addr ) {
    _addr = addr;
    _size = 0;
    _srcfile = "";
    _srcline = -1;
}

sc_memmgr_record::sc_memmgr_record( const sc_memmgr_record & rhs ) {
    _addr = rhs._addr;
    _size = rhs._size;
    _srcfile = rhs._srcfile;
    _srcline = rhs._srcline;
}

sc_memmgr_record::~sc_memmgr_record( void ) {
}

bool sc_memmgr_record::operator<( const sc_memmgr_record & rhs ) const {
    return _addr < rhs._addr;
}

void * sc_memmgr_record::getaddr( void ) const {
    return _addr;
}

size_t sc_memmgr_record::getsize( void ) const {
    return _size;
}

std::string sc_memmgr_record::getsrcfile( void ) const {
    return _srcfile;
}

unsigned int sc_memmgr_record::getsrcline( void ) const {
    return _srcline;
}

#endif /* SC_MEMMGR_ENABLE_CHECKS */
