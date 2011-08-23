#ifndef OBJ_TOKEN_TYPE_H
#define OBJ_TOKEN_TYPE_H
const int TOKEN_STRING_LEN = 80;

typedef union YYSTYPE
{
    float real;
    int integer;
    int reference[3];
    bool toggle;
    size_t index;
    char string[TOKEN_STRING_LEN];
} YYSTYPE;
#endif
