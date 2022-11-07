#ifndef _SOCKS5_H_
#define _SOCKS5_H_

#include "selector.h"

void socksv5_passive_accept(TSelectorKey* key);
unsigned socksv5_handle_read(TSelectorKey* key);
unsigned socksv5_handle_write(TSelectorKey* key);
void socksv5_handle_close(const unsigned int, TSelectorKey* key);

#endif