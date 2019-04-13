#ifndef __ASYNC_H__
#define __ASYNC_H__

void send_request(uint32_t lcore, void (*fn)(void *), void *arg);

#endif
