/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
 */
#ifndef INCL_TRIE
#define INCL_TRIE

#ifdef FUNC_NULL
#undef FUNC_NULL
#endif

#define FUNC_NULL	((Func *) NULL)
#define TRIE_NULL	((Trie *) NULL)

/* Datum for trie leaves.  */
typedef void Func();

/* Trie tree node.  */
typedef union trie Trie;
union trie
        { 
        struct  /* Internal nodes: datum is current letter. */
                {
                int t_char;   /* Current letter.  */
                Trie *t_altr; /* Alternate letter node link.  */
                Trie *t_next; /* Next letter node link.  */
                }
        n;
        struct  /* Leaf nodes: datum is function ptr.  */
                {
                Func *t_func; /* Function pointer.  */
                Trie *t_altr; /* Alternate letter node link.  */
                Trie *t_next; /* Next letter node link.  */
                }
        l;
        };
#define NewTrie( p ) \
		if( ((p) = (Trie *) malloc( sizeof(Trie) )) == TRIE_NULL )\
			{\
			Malloc_Bomb(sizeof(Trie));\
			return	TRIE_NULL;\
			}
extern Trie *cmd_trie;
#endif /* INCL_TRIE */
