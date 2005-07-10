#ifndef _IGVT_COMPNET_H
#define _IGVT_COMPNET_H

#define IGVT_COMPNET_PORT 1983

void igvt_compnet_connect(char *host, int port);
void igvt_compnet_update(char *string, char status);
void igvt_compnet_reset();

#endif
