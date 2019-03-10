#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"

#include "device.h"
#include "device_bdev.h"

struct ctrlr_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct ctrlr_entry *next;
  char name[1024];
};

static struct ctrlr_entry *g_controllers = NULL;
static struct ns_entry *g_namespaces = NULL;

static struct bdev_context *init_bdev_context(struct spdk_bdev *pre_bdev) {
  struct bdev_context *context = malloc(sizeof(struct bdev_context));
  if (pre_bdev == NULL) {
    context->bdev = spdk_bdev_first();
  } else {
    context->bdev = spdk_bdev_next(pre_bdev);
  }
  if (context->bdev == NULL) {
    SPDK_ERRLOG("Could not get bdev\n");
    goto err;
  }
  context->bdev_name = spdk_bdev_get_name(context->bdev);
  if (spdk_bdev_open(context->bdev, true, NULL, NULL, &context->bdev_desc)) {
    SPDK_ERRLOG("Could not open bdev: %s\n", context->bdev_name);
    goto err;
  }
  context->bdev_io_channel = spdk_bdev_get_io_channel(context->bdev_desc);
  if (context->bdev_io_channel == NULL) {
    SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
    spdk_bdev_close(context->bdev_desc);
    goto err;
  }
  SPDK_NOTICELOG("Bdev: %s init finished\n", context->bdev_name);
  return context;
err:
  free(context);
  spdk_app_stop(-1);
  return NULL;
}

static void start(void *arg1, void *arg2) {
  struct ns_entry entry;
  struct spdk_bdev *pre_bdev = NULL;
  for (int i = 0; i < NumberOfLuns; i++) {
    entry.contexts[i] =  init_bdev_context(pre_bdev);
    pre_bdev = entry.contexts[i]->bdev;
  }
}

struct device *dev_init(const char *f) {
  int rc;
  struct spdk_app_opts opts = {};
  spdk_app_opts_init(&opts);
  opts.name = "hello_world";
  opts.config_file = "config.conf";
  spdk_app_start(&opts, start, NULL);
  return NULL;
}

void dflush(struct device *dev) {}

void dclose(struct device *dev) {}