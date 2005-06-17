#ifndef _TIENET_DEFINE_H
#define _TIENET_DEFINE_H


#define	TN_COMPRESSION		1		/* 0 = off, 1 = on.  Compress the result buffer */

#define	TN_MASTER_PORT		1980
#define	TN_SLAVE_PORT		1981

#define	TN_OP_PREP		0x0010
#define	TN_OP_REQWORK		0x0011
#define	TN_OP_SENDWORK		0x0012
#define	TN_OP_RESULT		0x0013
#define	TN_OP_COMPLETE		0x0014
#define	TN_OP_SHUTDOWN		0x0015
#define TN_OP_OKAY		0x0016
#define	TN_OP_MESSAGE		0x0017

#endif
