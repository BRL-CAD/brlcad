#ifndef _IGVT_DISPATCHER_H
#define _IGVT_DISPATCHER_H

#include "cdb.h"

extern	void	igvt_dispatcher_init();
extern	void	igvt_dispatcher_free();
extern	void	igvt_dispatcher_generate(common_db_t *db, void *data, int data_len);


#endif
