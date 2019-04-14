#ifndef _DEVICE_H
#define _DEVICE_H

#include <stdint.h>
#include "async.h"

struct bdev_context {
  struct spdk_bdev *bdev;
  struct spdk_bdev_desc *bdev_desc;
  const char *bdev_name;
  size_t buf_align;
};

struct reactor_context {
  uint32_t lcore;
  struct spdk_io_channel *io_channel;
};

struct filesystem {
  struct super_block *sb;
  struct bdev_context bdev_ctx;
  struct reactor_context reactors[NUM_REACTORS];
};

typedef void (* device_init_cb)(struct filesystem *fs);

void dev_init(const char *file, device_init_cb cb);
void dev_stop(struct filesystem *);

#endif
