#ifndef _BLOCK_H
#define _BLOCK_H

#include <stdbool.h>
#include "device.h"

struct request {
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *io_channel;
  char *buf;
  size_t start;
  size_t nr;
  void *arg;
  bool read;
  bool free;
  sem_t *sem;
};


void write_blocks(struct super_block *sb, char *blocks, int start, int nr);
void write_blocks_async(struct super_block *sb, uint32_t reactor_id, char *blocks, int start, int nr);
void read_blocks(struct super_block *sb, char *blocks, int start, int nr);
void read_blocks_async(struct super_block *sb, uint32_t reactor_id, char *blocks, int start, int nr);
void zero_blocks(struct super_block *sb, int start, int nr);
struct request *generate_request(struct bdev_context *context, struct spdk_io_channel *io_channel, bool is_read, size_t start, size_t nr, char* blocks);
void readwrite(void *arg);
#endif /* _BLOCK_H */
