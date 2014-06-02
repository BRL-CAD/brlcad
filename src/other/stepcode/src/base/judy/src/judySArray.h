#ifndef JUDYSARRAY_H
#define JUDYSARRAY_H

/****************************************************************************//**
* \file judySArray.h C++ wrapper for judy array implementation
*
*  A judyS array maps strings to corresponding memory cells, each containing
*  a JudyValue. Each cell must be set to a non-zero value by the caller.
*
*    Author: Mark Pictor. Public domain.
*
********************************************************************************/

#include "judy.h"
#include "assert.h"
#include <string.h>

template< typename JudyValue >
struct judysKVpair {
    unsigned char * key;
    JudyValue value;
};
template< typename JudyValue >
class judySArray {
    protected:
        Judy * _judyarray;
        unsigned int _maxKeyLen;
        JudyValue * _lastSlot;
        unsigned char * _buff;
        bool _success;
    public:
        typedef judysKVpair< JudyValue > pair;
        judySArray( unsigned int maxKeyLen ): _maxKeyLen( maxKeyLen ), _success( true ) {
            _judyarray = judy_open( _maxKeyLen, 0 );
            _buff = new unsigned char[_maxKeyLen];
            assert( sizeof( JudyValue ) == sizeof( this ) && "JudyValue *must* be the same size as a pointer!" );
        }

        explicit judySArray( const judySArray< JudyValue > & other ): _maxKeyLen( other._maxKeyLen ), _success( other._success ) {
            _judyarray = judy_clone( other._judyarray );
            _buff = new unsigned char[_maxKeyLen];
            strncpy( _buff, other._buff, _maxKeyLen );
            _buff[ _maxKeyLen ] = '\0'; //ensure that _buff is null-terminated, since strncpy won't necessarily do so
            find( _buff ); //set _lastSlot
        }

        ~judySArray() {
            judy_close( _judyarray );
            delete[] _buff;
        }

        void clear() {
            _buff[0] = '\0';
            while( 0 != ( _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * ) _buff, 0 ) ) ) {
                judy_del( _judyarray );
            }
        }

        JudyValue getLastValue() {
            assert( _lastSlot );
            return &_lastSlot;
        }

        void setLastValue( JudyValue value ) {
            assert( _lastSlot );
            &_lastSlot = value;
        }

        bool success() {
            return _success;
        }
        //TODO
        // allocate data memory within judy array for external use.
        // void *judy_data (Judy *judy, unsigned int amt);

        /// insert or overwrite value for key
        bool insert( const char * key, JudyValue value, unsigned int keyLen = 0 ) {
            assert( value != 0 );
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( JudyValue * ) judy_cell( _judyarray, ( const unsigned char * )key, keyLen );
            if( _lastSlot ) {
                *_lastSlot = value;
                _success = true;
            } else {
                _success = false;
            }
            return _success;
        }

        /// retrieve the cell pointer greater than or equal to given key
        /// NOTE what about an atOrBefore function?
        const pair atOrAfter( const char * key, unsigned int keyLen = 0 ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * )key, keyLen );
            return mostRecentPair();
        }

        /// retrieve the cell pointer, or return NULL for a given key.
        JudyValue find( const char * key, unsigned int keyLen = 0 ) {
            if( keyLen == 0 ) {
                keyLen = strlen( key );
            } else {
                assert( keyLen == strlen( key ) );
            }
            assert( keyLen <= _maxKeyLen );
            _lastSlot = ( JudyValue * ) judy_slot( _judyarray, ( const unsigned char * ) key, keyLen );
            if( _lastSlot ) {
                _success = true;
                return *_lastSlot;
            } else {
                _success = false;
                return 0;
            }
        }

        /// retrieve the key-value pair for the most recent judy query.
        inline const pair mostRecentPair() {
            pair kv;
            judy_key( _judyarray, _buff, _maxKeyLen );
            if( _lastSlot ) {
                kv.value = *_lastSlot;
                _success = true;
            } else {
                kv.value = ( JudyValue ) 0;
                _success = false;
            }
            kv.key = _buff;
            return kv;
        }

        /// retrieve the first key-value pair in the array
        const pair & begin() {
            _buff[0] = '\0';
            _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * ) _buff, 0 );
            return mostRecentPair();
        }

        /// retrieve the last key-value pair in the array
        const pair & end() {
            _lastSlot = ( JudyValue * ) judy_end( _judyarray );
            return mostRecentPair();
        }

        /// retrieve the key-value pair for the next key in the array.
        const pair & next() {
            _lastSlot = ( JudyValue * ) judy_nxt( _judyarray );
            return mostRecentPair();
        }

        /// retrieve the key-value pair for the prev key in the array.
        const pair & previous() {
            _lastSlot = ( JudyValue * ) judy_prv( _judyarray );
            return mostRecentPair();
        }

        /** delete a key-value pair. If the array is not empty,
         * getLastValue() will return the entry before the one that was deleted
         * \sa isEmpty()
         */
        bool removeEntry( const char * key ) {
            if( judy_slot( _judyarray, ( const unsigned char * )key, strlen( key ) ) ) {
                _lastSlot = ( JudyValue * ) judy_del( _judyarray );
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
#endif //JUDYSARRAY_H
