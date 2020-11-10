#ifndef SDAIMODEL_CONTENTS_LIST_H
#define SDAIMODEL_CONTENTS_LIST_H 1

#include <sc_export.h>

class SC_DAI_EXPORT SDAI_Model_contents__list {
    public:
        SDAI_Model_contents__list( int = 16 );
        ~SDAI_Model_contents__list();

        SDAI_Model_contents_ptr retrieve( int index );
        int is_empty();

        SDAI_Model_contents_ptr & operator[]( int index );

        void Insert( SDAI_Model_contents_ptr, int index );
        void Append( SDAI_Model_contents_ptr );
        void Remove( int index );
        int Index( SDAI_Model_contents_ptr );

        void Clear();
        int Count();

    private:
        void Check( int index );
    private:
        SDAI_Model_contents_ptr * _buf;
        int _bufsize;
        int _count;
};

typedef SDAI_Model_contents__list *
SDAI_Model_contents__list_ptr;
typedef SDAI_Model_contents__list_ptr
SDAI_Model_contents__list_var;

#endif
