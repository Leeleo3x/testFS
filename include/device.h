#ifndef _DEVICE_H
#define _DEVICE_H

#include <semaphore.h>
#include <stdint.h>



#define NUM_OF_LUNS 1
#define SUPER_BLOCK_LOC 0


struct filesystem {
  struct super_block *sb;
  struct bdev_context *contexts[NUM_OF_LUNS];
};

struct bdev_context {
  struct spdk_bdev *bdev;
  struct spdk_bdev_desc *bdev_desc;
  sem_t sem;
  const char *bdev_name;
  struct spdk_bdev_io_wait_entry *bdev_io_wait;
  size_t buf_align;
};

typedef void (* device_init_cb)(void *arg1, void *arg2);

void dev_init(const char *file, device_init_cb cb);
void dev_stop(struct filesystem *);

#endif