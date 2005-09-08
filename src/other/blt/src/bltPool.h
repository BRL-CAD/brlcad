#ifndef BLT_POOL_H
#define BLT_POOL_H

typedef struct Blt_PoolChainStruct {
   struct Blt_PoolChainStruct *nextPtr;
} Blt_PoolChain;

#define BLT_STRING_ITEMS		0
#define BLT_FIXED_SIZE_ITEMS		1
#define BLT_VARIABLE_SIZE_ITEMS		2

typedef struct Blt_PoolStruct *Blt_Pool;

typedef void *(Blt_PoolAllocProc) _ANSI_ARGS_((Blt_Pool pool, size_t size));
typedef void (Blt_PoolFreeProc) _ANSI_ARGS_((Blt_Pool pool, void *item));

struct Blt_PoolStruct {
    Blt_PoolChain *headPtr;	/* Chain of malloc'ed chunks. */
    Blt_PoolChain *freePtr; 	/* List of deleted items. This is only used
				 * for fixed size items. */
    size_t poolSize;		/* Log2 of # of items in the current block. */
    size_t itemSize;		/* Size of an item. */
    size_t bytesLeft;		/* # of bytes left in the current chunk. */
    size_t waste;
    
    Blt_PoolAllocProc *allocProc;
    Blt_PoolFreeProc *freeProc;
};

EXTERN Blt_Pool Blt_PoolCreate _ANSI_ARGS_((int type));
EXTERN void Blt_PoolDestroy _ANSI_ARGS_((Blt_Pool pool));

#define Blt_PoolAllocItem(poolPtr, n) (*((poolPtr)->allocProc))(poolPtr, n)
#define Blt_PoolFreeItem(poolPtr, item) (*((poolPtr)->freeProc))(poolPtr, item)

#endif /* BLT_POOL_H */
