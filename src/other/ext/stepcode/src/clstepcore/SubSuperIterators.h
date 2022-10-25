#ifndef SUB_SUPER_ITERATORS
#define SUB_SUPER_ITERATORS

#include "ExpDict.h"
#include "ExpDict.h"
#include <queue>
#include <assert.h>

/** abstract base class for recursive breadth-first input iterators of EntityDescriptor/EntityDescLinkNode
 * NOTE: due to pure virtual functions being necessary for initialization, derived class constructor must call reset(t)
 */
class recursiveEntDescripIterator {
    protected:
        const EntityDescriptor * startEntity;
        typedef struct {
            const EntityDescriptor * ed;
            unsigned int depth; ///< for debugging; records how many lists had to be traversed to find the current node
        } queue_pair;

        std::deque< queue_pair > q;
        unsigned int position; ///< primarily used in comparisons between iterators

        ///add contents of a linked list to q
        void addLinkedList( const queue_pair qp ) {
            EntityDescLinkNode * a = listHead( qp.ed );
            queue_pair tmp;
            tmp.depth = qp.depth + 1;
            while( a != 0 ) {
                tmp.ed = nodeContent( a );
                q.push_back( tmp );
                a = ( EntityDescLinkNode * ) a->NextNode( );
            }
        }
        virtual EntityDescLinkNode * listHead( const EntityDescriptor * t ) const = 0;     ///< returns the head of something inheriting SingleLinkList
        virtual EntityDescriptor * nodeContent( const EntityDescLinkNode * n ) const = 0;  ///< returns the content of a SingleLinkNode


    public:
        recursiveEntDescripIterator( const EntityDescriptor * t = 0 ): startEntity( t ), position( 0 ) {
            //NOTE due to pure virtual functions, derived class constructor *must* call reset(t)
        }
        ~recursiveEntDescripIterator( ) {
        }

        void reset( const EntityDescriptor * t = 0 ) {
            position = 0;
            q.clear( );
            if( t ) {
                startEntity = t;
            }
            if( startEntity ) {
                queue_pair p;
                p.depth = 0;
                p.ed = startEntity;
                addLinkedList( p );
            }
        }

        const EntityDescriptor * next( ) {
            if( q.empty( ) ) {
                return ( EntityDescriptor * ) 0;
            } else {
                position++;
                queue_pair qp = q.front( );
                q.pop_front( );
                addLinkedList( qp );
                return qp.ed;
            }
        }

        const EntityDescriptor * current( ) const {
            if( q.empty( ) ) {
                return ( EntityDescriptor * ) 0;
            }
            return( q.front( ).ed );
        }

        bool hasNext( ) const {
            return( ( ( q.size( ) > 1 ) && ( q[1].ed != 0 ) ) //there is another EntityDescriptor in q
                    || ( nodeContent( listHead( q[0].ed ) ) != 0 ) ); //or, the only one in the queue has a non-empty list
        }

        bool empty( ) const {
            return q.empty( );
        }

        unsigned int pos( ) const {
            return position;
        }

        unsigned int depth( ) const {
            return q[0].depth;
        }

        const EntityDescriptor * operator *( ) const {
            return current( );
        }

        const EntityDescriptor * operator ->( ) const {
            return current( );
        }

        /// two iterators are not considered equal unless the startEntity pointers match and the positions match
        bool operator ==( const recursiveEntDescripIterator & b ) const {
            return( ( startEntity == b.startEntity ) && ( position == b.position ) );
        }

        bool operator !=( const recursiveEntDescripIterator & b ) const {
            return( ( startEntity != b.startEntity ) || ( position != b.position ) );
        }

        /// for inequality operators, return a Logical; LUnknown means that the startEntity pointers do not match
        Logical operator >( const recursiveEntDescripIterator & b ) const {
            if( startEntity != b.startEntity ) {
                return LUnknown;
            }
            if( position > b.position ) {
                return LTrue;
            } else {
                return LFalse;
            }
        }

        Logical operator <( const recursiveEntDescripIterator & b ) const {
            if( startEntity != b.startEntity ) {
                return LUnknown;
            }
            if( position < b.position ) {
                return LTrue;
            } else {
                return LFalse;
            }
        }

        Logical operator >=( const recursiveEntDescripIterator & b ) const {
            if( startEntity != b.startEntity ) {
                return LUnknown;
            }
            if( position >= b.position ) {
                return LTrue;
            } else {
                return LFalse;
            }
        }

        Logical operator <=( const recursiveEntDescripIterator & b ) const {
            if( startEntity != b.startEntity ) {
                return LUnknown;
            }
            if( position <= b.position ) {
                return LTrue;
            } else {
                return LFalse;
            }
        }

        const EntityDescriptor * operator ++( ) {
            return next( );
        }

        const EntityDescriptor * operator ++( int ) {
            const EntityDescriptor * c = current( );
            next( );
            return c;
        }
};

/** Recursive breadth-first input iterator for supertypes
 * \sa subtypesIterator
 */
class supertypesIterator : public recursiveEntDescripIterator {
    protected:
        EntityDescLinkNode * listHead( const EntityDescriptor * t ) const {     ///< returns the head of an EntityDescriptorList
            if( !t ) {
                return 0;
            }
            return ( EntityDescLinkNode * ) t->Supertypes().GetHead();
        }
        EntityDescriptor * nodeContent( const EntityDescLinkNode * n ) const {  ///< returns the content of a EntityDescLinkNode
            if( !n ) {
                return 0;
            }
            return n->EntityDesc();
        }
    public:
        supertypesIterator( const EntityDescriptor * t = 0 ): recursiveEntDescripIterator( t ) {
            reset( t );
        }
};

/** Recursive breadth-first input iterator for subtypes
 * \sa supertypesIterator
 */
class subtypesIterator: public recursiveEntDescripIterator {
    protected:
        EntityDescLinkNode * listHead( const EntityDescriptor * t ) const {     ///< returns the head of an EntityDescriptorList
            if( !t ) {
                return 0;
            }
            return ( EntityDescLinkNode * ) t->Subtypes().GetHead();
        }
        EntityDescriptor * nodeContent( const EntityDescLinkNode * n ) const {  ///< returns the content of a EntityDescLinkNode
            if( !n ) {
                return 0;
            }
            return n->EntityDesc();
        }
    public:
        subtypesIterator( const EntityDescriptor * t = 0 ): recursiveEntDescripIterator( t ) {
            reset( t );
        }
};

#endif //SUB_SUPER_ITERATORS
