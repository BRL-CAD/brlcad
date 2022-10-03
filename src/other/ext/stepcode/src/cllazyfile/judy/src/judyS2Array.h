#ifndef JUDYS2ARRAY_H
#define JUDYS2ARRAY_H

/****************************************************************************//**
* \file judyS2Array.h C++ wrapper for judy array implementation
*
*  A judyS2 array maps strings to multiple JudyValue's, similar to
* std::multimap. Internally, this is a judyL array of std::vector< JudyValue >.
*
*    Author: Mark Pictor. Public domain.
*
********************************************************************************/

#include "judy.h"
#include "assert.h"
#include <string.h>
#include <iterator>
#include <vector>

template< typename JudyValue >
struct judys2KVpair {
    unsigned char * key;
    JudyValue value;
};

/** A judyS2 array maps a set of strings to multiple JudyValue's, similar to std::multimap.
 * Internally, this is a judyS array of std::vector< JudyValue >.
 *  \param JudyValue the type of the value, i.e. int, pointer-to-object, etc.
 */
template< typename JudyValue >
class judyS2Array {
    public:
        typedef std::vector< JudyValue > vector;
        typedef const vector cvector;
        typedef judys2KVpair< vector * > pair;
        typedef judys2KVpair< cvector * > cpair;
    protected:
        Judy * _judyarray;
        unsigned int _maxKeyLen;
        vector ** _lastSlot;
        unsigned char * _buff;
        bool _success;
        cpair kv;
    public:
        judyS2Array( unsigned int maxKeyLen ): _maxKeyLen( maxKeyLen ), _lastSlot( 0 ), _success( true ) {
            _judyarray = judy_open( _maxKeyLen, 0 );
            _buff = new unsigned char[_maxKeyLen];
            assert( sizeof( JudyValue ) == sizeof( this ) && "JudyValue *must* be the same size as a pointer!" );
        }

        explicit judyS2Array( const judyS2Array< JudyValue > & other ): _maxKeyLen( other._maxKeyLen ), _success( other._success ) {
            _judyarray = judy_clone( other._judyarray );
            _buff = new unsigned char[_maxKeyLen];
            strncpy( _buff, other._buff, _maxKeyLen );
            _buff[ _maxKeyLen ] = '\0'; //ensure that _buff is null-terminated, since strncpy won't necessarily do so
            find( _buff ); //set _lastSlot
        }

        /// calls clear, so should be safe to call at any point
        ~judyS2Array() {
            clear();
            judy_close( _judyarray );
            delete[] _buff;
        }

        /// delete all vectors and empty the array
        void clear() {
            _buff[0] = '\0';
            while( 0 != ( _lastSlot = ( vector ** ) judy_strt( _judyarray, ( const unsigned char * ) _buff, 0 ) ) ) {
                //( * _lastSlot )->~vector(); //TODO: placement new
                delete( * _lastSlot );
                judy_del( _judyarray );
            }
        }

        vector * getLastValue() {
            assert( _lastSlot );
            return &_lastSlot;
        }

        void setLastValue( vector * value ) {
            assert( _lastSlot );
            &_lastSlot = value;
        }

        bool success() {
            return _success;
        }

        /** TODO
         * test for std::vector::shrink_to_fit (C++11), use it once the array is as full as it will be
         * void freeUnused() {...}
         */

        //TODO
        // allocate data memory within judy array for external use.
        // void *judy_data (Judy *judy, unsigned int amt);

        /// insert value into the vector for key.
        bool insert( const char * key, JudyValue value, unsigned int keyLen = 0 ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( vector ** ) judy_cell( _judyarray, ( const unsigned char * )key, keyLen );
            if( _lastSlot ) {
                if( !( * _lastSlot ) ) {
                    * _lastSlot = new vector;
                    /* TODO store vectors inside judy with placement new
                    * vector * n = judy_data( _judyarray, sizeof( std::vector < JudyValue > ) );
                    * new(n) vector;
                    * *_lastSlot = n;
                    * NOTE - memory alloc'd via judy_data will not be freed until the array is freed (judy_close)
                    * also use placement new in the other insert function, below
                    */
                }
                ( * _lastSlot )->push_back( value );
                _success = true;
            } else {
                _success = false;
            }
            return _success;
        }

        /** for a given key, append to or overwrite the vector
         * this never simply re-uses the pointer to the given vector because
         * that would mean that two keys could have the same value (pointer).
         */
        bool insert( const char * key, const vector & values, unsigned int keyLen = 0, bool overwrite = false ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( vector ** ) judy_cell( _judyarray, ( const unsigned char * )key, keyLen );
            if( _lastSlot ) {
                if( !( * _lastSlot ) ) {
                    * _lastSlot = new vector;
                    /* TODO store vectors inside judy with placement new
                     * (see other insert(), above)
                     */
                } else if( overwrite ) {
                    ( * _lastSlot )->clear();
                }
                std::copy( values.begin(), values.end(), std::back_inserter< vector >( ( ** _lastSlot ) ) );
                _success = true;
            } else {
                _success = false;
            }
            return _success;
        }

        /// retrieve the cell pointer greater than or equal to given key
        /// NOTE what about an atOrBefore function?
        const cpair atOrAfter( const char * key, unsigned int keyLen = 0 ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( vector ** ) judy_strt( _judyarray, ( const unsigned char * )key, keyLen );
            return mostRecentPair();
        }

        /// retrieve the cell pointer, or return NULL for a given key.
        cvector * find( const char * key, unsigned int keyLen = 0 ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( vector ** ) judy_slot( _judyarray, ( const unsigned char * ) key, keyLen );
            if( ( _lastSlot ) && ( * _lastSlot ) ) {
                _success = true;
                return * _lastSlot;
            } else {
                _success = false;
                return 0;
            }
        }

        /// retrieve the key-value pair for the most recent judy query.
        inline const cpair & mostRecentPair() {
            judy_key( _judyarray, _buff, _maxKeyLen );
            if( _lastSlot ) {
                kv.value = *_lastSlot;
                _success = true;
            } else {
                kv.value = NULL;
                _success = false;
            }
            kv.key = _buff;
            return kv;
        }

        /// retrieve the first key-value pair in the array
        const cpair & begin() {
            _buff[0] = '\0';
            _lastSlot = ( vector ** ) judy_strt( _judyarray, ( const unsigned char * ) _buff, 0 );
            return mostRecentPair();
        }

        /// retrieve the last key-value pair in the array
        const cpair & end() {
            _lastSlot = ( vector ** ) judy_end( _judyarray );
            return mostRecentPair();
        }

        /// retrieve the key-value pair for the next key in the array.
        const cpair & next() {
            _lastSlot = ( vector ** ) judy_nxt( _judyarray );
            return mostRecentPair();
        }

        /// retrieve the key-value pair for the prev key in the array.
        const cpair & previous() {
            _lastSlot = ( vector ** ) judy_prv( _judyarray );
            return mostRecentPair();
        }

        /** delete a key-value pair. If the array is not empty,
         * getLastValue() will return the entry before the one that was deleted
         * \sa isEmpty()
         */
        bool removeEntry( const char * key ) {
            if( 0 != ( judy_slot( _judyarray, ( const unsigned char * )key, strlen( key ) ) ) ) {
                // _lastSlot->~vector(); //for use with placement new
                delete _lastSlot;
                _lastSlot = ( vector ** ) judy_del( _judyarray );
                return true;
            } else {
                return false;
            }
        }

        ///return true if the array is empty
        bool isEmpty() {
            _buff[0] = 0;
            return ( ( judy_strt( _judyarray, ( const unsigned char * ) _buff, 0 ) ) ? false : true );
        }
};
#endif //JUDYS2ARRAY_H
