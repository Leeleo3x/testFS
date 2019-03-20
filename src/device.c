#include <device.h>
#include "spdk/thread.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/util.h"

#include "device.h"

static struct bdev_context *init_bdev_context(struct spdk_bdev *pre_bdev) {
  struct bdev_context *context = malloc(sizeof(struct bdev_context));
  if (pre_bdev == NULL) {
    context->bdev = spdk_bdev_first();
  } else {
    context->bdev = spdk_bdev_next(pre_bdev);
  }
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
  sem_init(&context->sem, 0, 0);

  SPDK_NOTICELOG("Bdev: %s init finished\n", context->bdev_name);
  return context;
err:
  free(context);
  spdk_app_stop(-1);
  return NULL;
}

static void start(void *arg1, void *arg2) {
  struct filesystem *fs = malloc(sizeof(struct filesystem));

  device_init_cb cb = (device_init_cb)arg1;
  struct spdk_bdev *pre_bdev = NULL;
  for (int i = 0; i < NUM_OF_LUNS; i++) {
    fs->contexts[i] = init_bdev_context(pre_bdev);
    pre_bdev = fs->contexts[i]->bdev;
  }
  struct spdk_event *event = spdk_event_allocate(1, cb, fs, NULL);
  spdk_event_call(event);
}


void dev_stop(struct filesystem *fs) {
  for (int i = 0; i < NUM_OF_LUNS; i++) {
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
  opts.reactor_mask = "0x3";
  spdk_app_start(&opts, start, cb);
}

