#include <device.h>
#include "spdk/thread.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/util.h"

#include "device.h"


static void
__call_fn(void *arg1, void *arg2)
{
  printf("CALL_FN\n");
  void (*fn)(void *);

  fn = (void (*)(void *))arg1;
  fn(arg2);
}

void send_request(uint32_t lcore, void (*fn)(void *), void *arg) {
  printf("SEND_REQ\n");
  struct spdk_event *event;

  event = spdk_event_allocate(lcore, __call_fn, (void *)fn, arg);
  spdk_event_call(event);
}

struct testfs_init_context {
  sem_t sem;
  uint32_t lcore;
  struct filesystem *fs;
};


static void init_bdev_context(void *arg) {
  struct testfs_init_context *c = arg;
  struct bdev_context *context = malloc(sizeof(struct bdev_context));
  context->bdev = spdk_bdev_first();
  printf("BLOCK_SIZE %d\n", spdk_bdev_get_block_size(context->bdev));
  if (context->bdev == NULL) {
    SPDK_ERRLOG("Could not get bdev\n");
    goto err;
  }
  context->bdev_name = spdk_bdev_get_name(context->bdev);
  if (spdk_bdev_open(context->bdev, true, NULL, NULL, &context->bdev_desc)) {
    SPDK_ERRLOG("Could not open bdev: %s\n", context->bdev_name);
    goto err;
  }
  context->buf_align = spdk_bdev_get_buf_align(context->bdev);
  context->io_channel = spdk_bdev_get_io_channel(context->bdev_desc);
  context->counter = 0;
  context->fs = c->fs;
  sem_init(&context->sem, 0, 0);

  SPDK_NOTICELOG("Bdev: %s init finished\n", context->bdev_name);
  c->fs->contexts[c->lcore] = context;
  sem_post(&c->sem);
  return;
err:
  free(context);
  spdk_app_stop(-1);
  sem_post(&c->sem);
}

static void start(void *arg1, void *arg2) {
  struct filesystem *fs = malloc(sizeof(struct filesystem));
  for (int i = 1; i <= NUM_OF_LUNS; i++) {
    struct testfs_init_context *context = malloc(sizeof(struct testfs_init_context));
    sem_init(&context->sem, 0, 0);
    context->lcore = i - 1;
    context->fs = fs;
    send_request(i, init_bdev_context, context);
    sem_wait(&context->sem);
    free(context);
  }
  printf("START FINISHED\n");
  device_init_cb cb = (device_init_cb)arg1;
  send_request(0, (void *)cb, fs);
}


void dev_stop(struct filesystem *fs) {
  for (int i = 0; i < NUM_OF_LUNS; i++) {
    spdk_put_io_channel(fs->contexts[i]->io_channel);
    spdk_bdev_close(fs->contexts[i]->bdev_desc);
    free(fs->contexts[i]);
  }
  spdk_app_stop(0);
}

void dev_init(const char *f, device_init_cb cb) {
  int rc;
  struct spdk_app_opts opts = {};
  spdk_app_opts_init(&opts);
  opts.name = "hello_world";
  opts.config_file = "config.conf";
  opts.reactor_mask = "0xff";
  spdk_app_start(&opts, start, cb);
}

void wait_context(struct bdev_context *context) {
  while (context->counter) {
	context->counter--;
	sem_wait(&context->sem);
  }
}

void wait_all_contexts(struct filesystem *fs) {
  for (int i = 0; i < NUM_OF_LUNS; i++) {
    struct bdev_context *context = fs->contexts[i];
	while (context->counter) {
	  context->counter--;
	  sem_wait(&context->sem);
	}

  }
}
