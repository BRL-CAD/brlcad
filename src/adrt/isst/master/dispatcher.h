#ifndef _ISST_DISPATCHER_H
#define _ISST_DISPATCHER_H

#include "cdb.h"

extern	void	isst_dispatcher_init();
extern	void	isst_dispatcher_free();
extern	void	isst_dispatcher_generate(common_db_t *db, void *data, int data_len);


#endif
