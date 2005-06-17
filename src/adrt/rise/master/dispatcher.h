#ifndef _RISE_DISPATCHER_H
#define _RISE_DISPATCHER_H

#include "cdb.h"

extern	void	rise_dispatcher_init();
extern	void	rise_dispatcher_free();
extern	void	rise_dispatcher_generate(common_db_t *db, void *data, int data_len);


#endif
