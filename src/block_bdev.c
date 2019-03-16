#include <unistd.h>
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/nvme.h"
#include "spdk/stdinc.h"

#include "common.h"
#include "device_bdev.h"
#include "super.h"
#include "testfs.h"

static char zero[BLOCK_SIZE] = {0};

static void read_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
  struct read_context *sequence = cb_arg;
  block_read_cb cb = sequence->cb;
  void *arg = sequence->arg;
  char *blocks = sequence->blocks;
  free(sequence);
  if (success) {
    cb(blocks, arg);
  }
  else {
    cb(NULL, arg);
  }
}

void read_blocks(struct super_block *sb, char *blocks, uint64_t start, size_t nr, block_read_cb cb, void *arg) {
  printf("Read\n");
  struct ns_entry *entry = sb->dev->raw;
  struct read_context *sequence = malloc(sizeof(struct read_context));
  struct bdev_context *ctx = entry->contexts[0];
  sequence->cb = cb;
  sequence->arg = arg;
  sequence->blocks = blocks;

  size_t buf_align = spdk_bdev_get_buf_align(ctx->bdev);
  sequence->buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, buf_align, NULL);
  int rc = spdk_bdev_read_blocks(ctx->bdev_desc, ctx->bdev_io_channel,
                                 sequence.buf, start, nr, read_cb, sequence);
}

void write_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  printf("Write\n");
  struct ns_entry *entry = sb->dev->raw;
  struct hello_world_sequence sequence;
  struct bdev_context *ctx = entry->contexts[0];

  size_t buf_align = spdk_bdev_get_buf_align(ctx->bdev);
  uint32_t bsize = spdk_bdev_get_block_size(ctx->bdev);
  printf("%d\n", bsize);
  printf("%d\n", nr);
  printf("%d\n", start);
  sequence.buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, buf_align, NULL);
  memcpy(sequence.buf, blocks, nr * BLOCK_SIZE);
  int rc = spdk_bdev_write_blocks(ctx->bdev_desc, ctx->bdev_io_channel,
                                  sequence.buf, start, nr, complete, &sequence);
}

void zero_blocks(struct super_block *sb, int start, int nr) {
  struct ns_entry *entry = sb->dev->raw;
  struct hello_world_sequence sequence;
  struct bdev_context *ctx = entry->contexts[0];
  spdk_bdev_write_zeroes_blocks(ctx->bdev_desc, ctx->bdev_io_channel, start, nr,
                                complete, &sequence);
}
