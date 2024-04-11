#ifndef JUDYLARRAY_H
#define JUDYLARRAY_H

/****************************************************************************//**
* \file judyLArray.h C++ wrapper for judyL array implementation
*
* A judyL array maps JudyKey's to corresponding memory cells, each containing
* a JudyValue. Each cell must be set to a non-zero value by the caller.
*
*    Author: Mark Pictor. Public domain.
*
********************************************************************************/

#include "judy.h"
#include "assert.h"

template< typename JudyKey, typename JudyValue >
struct judylKVpair {
    JudyKey key;
    JudyValue value;
};

/** A judyL array maps JudyKey's to corresponding memory cells, each containing
 * a JudyValue. Each cell must be set to a non-zero value by the caller.
 *
 * Both template parameters must be the same size as a void*
 *  \param JudyKey the type of the key, i.e. uint64_t, pointer-to-object, etc
 *  \param JudyValue the type of the value
 */
template< typename JudyKey, typename JudyValue >
class judyLArray {
    public:
        typedef judylKVpair< JudyKey, JudyValue > pair;
    protected:
        Judy * _judyarray;
        unsigned int _maxLevels, _depth;
        JudyValue * _lastSlot;
        JudyKey _buff[1];
        bool _success;
        pair _kv;
    public:
        judyLArray(): _maxLevels( sizeof( JudyKey ) ), _depth( 1 ), _lastSlot( 0 ), _success( true ) {
            assert( sizeof( JudyKey ) == JUDY_key_size && "JudyKey *must* be the same size as a pointer!" );
            assert( sizeof( JudyValue ) == JUDY_key_size && "JudyValue *must* be the same size as a pointer!" );
            _judyarray = judy_open( _maxLevels, _depth );
            _buff[0] = 0;
        }

        explicit judyLArray( const judyLArray< JudyKey, JudyValue > & other ): _maxLevels( other._maxLevels ),
            _depth( other._depth ), _success( other._success ) {
            _judyarray = judy_clone( other._judyarray );
            _buff[0] = other._buff[0];
            find( *_buff ); //set _lastSlot
        }

        ~judyLArray() {
            judy_close( _judyarray );
        }

        void clear( bool deleteContents = false ) {
            JudyKey key = 0;
            while( 0 != ( _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * ) &key, 0 ) ) ) {
                if( deleteContents ) {
                    delete *_lastSlot;
                }
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
        bool insert( JudyKey key, JudyValue value ) {
            assert( value != 0 );
            _lastSlot = ( JudyValue * ) judy_cell( _judyarray, ( const unsigned char * ) &key, _depth * JUDY_key_size );
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
        const pair atOrAfter( JudyKey key ) {
            _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * ) &key, _depth * JUDY_key_size );
            return mostRecentPair();
        }

        /// retrieve the cell pointer, or return NULL for a given key.
        JudyValue find( JudyKey key ) {
            _lastSlot = ( JudyValue * ) judy_slot( _judyarray, ( const unsigned char * ) &key, _depth * JUDY_key_size );
            if( _lastSlot ) {
                _success = true;
                return *_lastSlot;
            } else {
                _success = false;
                return 0;
            }
        }

        /// retrieve the key-value pair for the most recent judy query.
        inline const pair & mostRecentPair() {
            judy_key( _judyarray, ( unsigned char * ) _buff, _depth * JUDY_key_size );
            if( _lastSlot ) {
                _kv.value = *_lastSlot;
                _success = true;
            } else {
                _kv.value = ( JudyValue ) 0;
                _success = false;
            }
            _kv.key = _buff[0];
            return _kv;
        }

        /// retrieve the first key-value pair in the array
        const pair & begin() {
            JudyKey key = 0;
            _lastSlot = ( JudyValue * ) judy_strt( _judyarray, ( const unsigned char * ) &key, 0 );
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
        bool removeEntry( JudyKey * key ) {
            if( judy_slot( _judyarray, key, _depth * JUDY_key_size ) ) {
                _lastSlot = ( JudyValue * ) judy_del( _judyarray );
                return true;
            } else {
                return false;
            }
        }

        /// true if the array is empty
        bool isEmpty() {
            JudyKey key = 0;
            return ( ( judy_strt( _judyarray, ( const unsigned char * ) &key, _depth * JUDY_key_size ) ) ? false : true );
        }
};
#endif //JUDYLARRAY_H
