
#define SCL_MEMMGR_CC
#include "scl_memmgr.h"
#if __APPLE__
# include <malloc/malloc.h>
#else
# include <malloc.h>
#endif
#include <stdio.h>

#include <string>
#include <set>

#ifdef SCL_MEMMGR_ENABLE_CHECKS

/**
    scl_memmgr_error definition
*/
class scl_memmgr_error {
    private:
        std::string _srcfile;
        unsigned int _srcline;
        unsigned int _occurences;
    public:
        scl_memmgr_error( const std::string &file, const unsigned int line );
        scl_memmgr_error( const scl_memmgr_error &rhs );
        ~scl_memmgr_error( void );

        bool operator<( const scl_memmgr_error &rhs ) const;

        std::string getsrcfile( void ) const;
        unsigned int getsrcline( void ) const;
        unsigned int getoccurences( void ) const;
};

typedef std::set<scl_memmgr_error>              scl_memmgr_errors;
typedef std::set<scl_memmgr_error>::iterator    scl_memmgr_error_iterator;

/**
    scl_memmgr_record definition
*/
class scl_memmgr_record {
    private:
        void * _addr;
        size_t _size;
        std::string _srcfile;
        unsigned int _srcline;
    public:
        scl_memmgr_record( void * addr, size_t size, const char *file, const unsigned int line );
        scl_memmgr_record( void * addr );
        scl_memmgr_record( const scl_memmgr_record &rhs );
        ~scl_memmgr_record( void );

        bool operator<( const scl_memmgr_record &rhs ) const;

        void * getaddr( void ) const;
        size_t getsize( void ) const;
        std::string getsrcfile( void ) const;
        unsigned int getsrcline( void ) const;
};

typedef std::set<scl_memmgr_record>           scl_memmgr_records;
typedef std::set<scl_memmgr_record>::iterator scl_memmgr_record_iterator;

#endif /* SCL_MEMMGR_ENABLE_CHECKS */

/**
    scl_memmgr definition
*/
class scl_memmgr {
    private:
        #ifdef SCL_MEMMGR_ENABLE_CHECKS
        bool _record_insert_busy, _record_erase_busy;
        // memory allocation/reallocation records, inserted at allocation, erased at deallocation.
        scl_memmgr_records _records;
        // memory stats
        unsigned int _allocated;            // amount of memory allocated simultaniously
        unsigned int _maximum_allocated;    // maximum amount of memory allocated simultaniously
        unsigned int _allocated_total;      // total amount of memory allocated in bytes
        unsigned int _reallocated_total;    // total amount of memory reallocated in bytes
        unsigned int _deallocated_total;    // total amount of memory deallocated in bytes
        #endif /* SCL_MEMMGR_ENABLE_CHECKS */
    protected:
    public:
        scl_memmgr(void);
        ~scl_memmgr(void);

        void * allocate( size_t size, const char *file, const int line );
        void * reallocate( void * addr, size_t size, const char *file, const int line );
        void   deallocate( void * addr, const char *file, const int line );
};

/**
    scl_memmgr instance.
    There should be one static instance of memmgr.
    This instance is automatically destroyed when application exits, so this allows us to add
    memory leak detection in it's destructor.
*/
scl_memmgr memmgr;

/**
    c memory functions implementation
*/
extern "C" {

    void * scl_malloc_fn( unsigned int size, const char * file, const int line ) {
        return memmgr.allocate(size, file, line);
    }

    void * scl_calloc_fn( unsigned int count, unsigned int size, const char * file, const int line ) {
        return memmgr.allocate(count * size, file, line);
    }

    void * scl_realloc_fn( void * addr, unsigned int size, const char * file, const int line ) {
        return memmgr.reallocate(addr, size, file, line);
    }

    void scl_free_fn( void * addr ) {
        memmgr.deallocate(addr, "", 0);
    }

}

/**
    c++ memory operators implementation
*/
void * scl_operator_new( size_t size, const char * file, const int line ) {
    return memmgr.allocate(size, file, line);
}

void scl_operator_delete( void * addr, const char * file, const int line ) {
    memmgr.deallocate(addr, file, line);
}

void scl_operator_delete( void * addr ) {
    memmgr.deallocate(addr, "", 0);
}

/**
    scl_memmgr implementation
*/
scl_memmgr::scl_memmgr(void) {
    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    _record_insert_busy = false;
    _record_erase_busy = false;

    _allocated = 0;
    _maximum_allocated = 0;
    _allocated_total = 0;
    _reallocated_total = 0;
    _deallocated_total = 0;
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */
}

/**
    Destructor:
        scl_memmgr::~scl_memmgr(void)
    Description:
        The scl_memmgr destructor is used to check for memory leaks when enabled.
        All records still present when scl_memmgr instance is destroyed can be considered as 
        memory leaks.
*/
scl_memmgr::~scl_memmgr(void) {
    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    scl_memmgr_record_iterator irecord;
    scl_memmgr_errors errors;
    scl_memmgr_error_iterator ierror;

    // Check if total allocated equals total deallocated
    if (_allocated_total != _deallocated_total) {
        // todo: generate warning for possible memory leaks, enable full memory leak checking
        printf("scl_memmgr warning: Possible memory leaks detected\n");
    }

    // Compact leaks into an error list to prevent same leak being reported multiple times.
    _record_insert_busy = true;
    _record_erase_busy = true;
    for (irecord = _records.begin();
         irecord != _records.end();
         irecord ++) {
        scl_memmgr_error error(irecord->getsrcfile(), irecord->getsrcline());
        ierror = errors.find(error);
        if (ierror == errors.end())
            errors.insert(error);
        //else
        //    ierror->occurences ++;
    }
    _record_insert_busy = false;
    _record_erase_busy = false;

    // Loop through memory leaks to generate/buffer errors
    for (ierror = errors.begin();
         ierror != errors.end();
         ierror ++) {
        // todo: generate error for memory leak
        printf("scl_memmgr warning: Possible memory leak in %s line %d\n", ierror->getsrcfile().c_str(), ierror->getsrcline());
    }

    // Clear remaining records
    _record_erase_busy = true;
    _records.clear();
    errors.clear();
    _record_erase_busy = false;
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */
}

void * scl_memmgr::allocate( size_t size, const char *file, const int line ) {
    void * addr;

    // Allocate
    addr = malloc(size);
    if (addr == NULL) {
        // todo: error allocation failed
        printf("scl_memmgr error: Memory allocation failed in %s line %d\n", file, line);
    }

    // Some stl implementations (for example debian gcc) use the new operator to construct
    // new elements when inserting scl_memmgr_record. When this our new operator gets used
    // for this operation, this would result in an infinite loop. This is fixed by the
    // _record_insert_busy flag.
    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    if (!_record_insert_busy)
    {    
        // Store record for this allocation
        _record_insert_busy = true;
        _records.insert(scl_memmgr_record(addr, size, file, line));
        _record_insert_busy = false;
    
        // Update stats
        _allocated += size;
        if (_allocated > _maximum_allocated)
            _maximum_allocated = _allocated;
        _allocated_total += size;
    }
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */

    return addr;
}

void * scl_memmgr::reallocate( void * addr, size_t size, const char *file, const int line ) {
    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    scl_memmgr_record_iterator record;

    if (!_record_insert_busy)
    {
        // Find record of previous allocation/reallocation
        record = _records.find(scl_memmgr_record(addr));
        if (record == _records.end()) {
            // todo: error reallocating memory not allocated?
            printf("scl_memmgr warning: Reallocation of not allocated memory at %s line %d\n", file, line);
        }
        else {
            // Update stats
            _allocated -= record->getsize();
            _deallocated_total += record->getsize();
        
            // Erase previous allocation/reallocation
            _record_erase_busy = true;
            _records.erase(record);
            _record_erase_busy = false;
        }
    }
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */

    // Reallocate
    addr = realloc(addr, size);
    if (addr == NULL) {
        // todo: error reallocation failed
        printf("scl_memmgr error: Reallocation failed at %s line %d\n", file, line);
    }

    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    if (!_record_insert_busy)
    {
        // Store record for this reallocation
        _record_insert_busy = true;
        _records.insert(scl_memmgr_record(addr, size, file, line));
        _record_insert_busy = false;

        // Update stats
        _allocated += size;
        if (_allocated > _maximum_allocated)
            _maximum_allocated = _allocated;
        _allocated_total += size;
        _reallocated_total += size;
    }
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */

    return addr;
}

void scl_memmgr::deallocate( void * addr, const char *file, const int line ) {
    #ifdef SCL_MEMMGR_ENABLE_CHECKS
    scl_memmgr_record_iterator record;

    if (!_record_erase_busy)
    {
        // Find record of previous allocation/reallocation
        record = _records.find(scl_memmgr_record(addr));
        if (record == _records.end()) {
            // todo: error free called for not allocated memory?
            printf("scl_memmgr warning: Deallocate of not allocated memory at %s line %d\n", file, line);
        }
        else {
            // Update stats
            _allocated -= record->getsize();
            _deallocated_total += record->getsize();

            // Erase record
            _record_erase_busy = true;
            _records.erase(record);
            _record_erase_busy = false;
        }
    }
    #endif /* SCL_MEMMGR_ENABLE_CHECKS */

    // Deallocate
    free(addr);
}

#ifdef SCL_MEMMGR_ENABLE_CHECKS
/**
    scl_memmgr_error implementation    
*/
scl_memmgr_error::scl_memmgr_error( const std::string &file, const unsigned int line ) {
    _srcfile = file;
    _srcline = line;
    _occurences = 1;
}

scl_memmgr_error::scl_memmgr_error( const scl_memmgr_error &rhs ) {
    _srcfile = rhs._srcfile;
    _srcline = rhs._srcline;
    _occurences = rhs._occurences;
}

scl_memmgr_error::~scl_memmgr_error( void ) {
}

bool scl_memmgr_error::operator<( const scl_memmgr_error &rhs ) const {
    if (_srcfile == rhs._srcfile)
        return _srcline < rhs._srcline;
    return _srcfile < rhs._srcfile;
}

std::string scl_memmgr_error::getsrcfile( void ) const {
    return _srcfile;
}

unsigned int scl_memmgr_error::getsrcline( void ) const {
    return _srcline;
}

unsigned int scl_memmgr_error::getoccurences( void ) const {
    return _occurences;
}

/**
    scl_memmgr_record implementation    
*/
scl_memmgr_record::scl_memmgr_record( void * addr, size_t size, const char *file, const unsigned int line ) {
    _addr = addr;
    _size = size;
    _srcfile = file;
    _srcline = line;
}

scl_memmgr_record::scl_memmgr_record( void * addr ) {
    _addr = addr;
    _size = 0;
    _srcfile = "";
    _srcline = -1;
}

scl_memmgr_record::scl_memmgr_record( const scl_memmgr_record &rhs ) {
    _addr = rhs._addr;
    _size = rhs._size;
    _srcfile = rhs._srcfile;
    _srcline = rhs._srcline;
}

scl_memmgr_record::~scl_memmgr_record( void ) {
}

bool scl_memmgr_record::operator<( const scl_memmgr_record &rhs ) const {
    return _addr < rhs._addr;
}

void * scl_memmgr_record::getaddr( void ) const {
    return _addr;
}

size_t scl_memmgr_record::getsize( void ) const {
    return _size;
}

std::string scl_memmgr_record::getsrcfile( void ) const {
    return _srcfile;
}

unsigned int scl_memmgr_record::getsrcline( void ) const {
    return _srcline;
}

#endif /* SCL_MEMMGR_ENABLE_CHECKS */
