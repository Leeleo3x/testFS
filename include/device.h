#ifndef _DEVICE_H
#define _DEVICE_H

#include <semaphore.h>
#include <stdint.h>



#define NUM_OF_LUNS 7
#define INODE_LUN 0
#define DATA_LUN 1
#define SYNC_LUN 2

struct filesystem {
  struct super_block *sb;
  struct bdev_context *contexts[NUM_OF_LUNS];
};

struct bdev_context {
  struct spdk_bdev *bdev;
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *io_channel;
  struct filesystem *fs;
  sem_t sem;
  const char *bdev_name;
  struct spdk_bdev_io_wait_entry *bdev_io_wait;
  size_t buf_align;
  int counter;
};


typedef void (* device_init_cb)(struct filesystem *fs);

void send_request(uint32_t lcore, void (*fn)(void *), void *arg);
void dev_init(const char *file, device_init_cb cb);
void dev_stop(struct filesystem *);
void wait_context(struct bdev_context*);

#endif