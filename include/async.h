#ifndef __ASYNC_H__
#define __ASYNC_H__

#include <stdlib.h>

#define NUM_REACTORS 3

#define MAIN_REACTOR 0
#define METADATA_REACTOR 1
#define DATA_REACTOR 2

struct future {
  // NOTE: counts[i] should only be modified by reactor i's thread
  volatile size_t counts[NUM_REACTORS];
  size_t expected_counts[NUM_REACTORS];
};

void send_request(uint32_t lcore, void (*fn)(void *), void *arg);
void spin_wait(struct future *f);
void future_init(struct future *f);

#endif
