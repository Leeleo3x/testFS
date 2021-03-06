#include "spdk/event.h"

#include "async.h"
#include "logging.h"

static void
__call_fn(void *arg1, void *arg2)
{
  void (*fn)(void *);

  fn = (void (*)(void *))arg1;
  fn(arg2);
}

void send_request(uint32_t lcore, void (*fn)(void *), void *arg) {
  struct spdk_event *event;
  event = spdk_event_allocate(lcore, __call_fn, (void *)fn, arg);
  spdk_event_call(event);
}

void spin_wait(struct future *f) {
  for (size_t i = 0; i < NUM_REACTORS; i++) {
    while (f->counts[i] != f->expected_counts[i]) {}
  }
}

void future_init(struct future *f) {
  for (size_t i = 0; i < NUM_REACTORS; i++) {
    f->counts[i] = 0;
    f->expected_counts[i] = 0;
  }
}
