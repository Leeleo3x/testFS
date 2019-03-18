#include <unistd.h>
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/nvme.h"
#include "spdk/stdinc.h"
#include "spdk/thread.h"

#include "common.h"
#include "device_bdev.h"
#include "super.h"
#include "testfs.h"

#include <semaphore.h>
#include "spdk/event.h"

static char zero[BLOCK_SIZE] = {0};
sem_t sem;

struct request {
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *io_channel;
  char *buf;
  int start;
  int nr;
  void *arg;
  bool read;
};

static void complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
  printf("complete\n");
  struct hello_world_sequence *sequence = cb_arg;
  spdk_bdev_free_io(bdev_io);
  sem_post(&sem);
}

static void
__call_fn(void *arg1, void *arg2)
{
  printf("CALL_FN\n");
  void (*fn)(void *);

  fn = (void (*)(void *))arg1;
  fn(arg2);
}

static void
__send_request(void (*fn)(void *), void *arg)
{
  printf("SEND_REQ\n");
  struct spdk_event *event;

  event = spdk_event_allocate(0, __call_fn, (void *)fn, arg);
  spdk_event_call(event);
}

static void __readwrite(void *arg) {
  struct request *req = arg;
  printf("READ_WRITE\n");
  if (req->read) {
    spdk_bdev_read_blocks(req->bdev_desc, req->io_channel,
                          req->buf, req->start, req->nr, complete, req->arg);
  }
  else {
    spdk_bdev_write_blocks(req->bdev_desc, req->io_channel,
                           req->buf, req->start, req->nr, complete, req->arg);
  }
}

void read_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  printf("Read\n");
  struct ns_entry *entry = sb->dev->raw;
  struct hello_world_sequence sequence;
  struct bdev_context *ctx = entry->contexts[0];
  struct spdk_fs_channel *channel = spdk_io_channel_get_ctx(ctx->bdev_io_channel);

  uint32_t buf_align = spdk_bdev_get_buf_align(ctx->bdev);
  sequence.buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, buf_align, NULL);
  struct request req;
  req.bdev_desc = ctx->bdev_desc;
  req.io_channel = ctx->bdev_io_channel;
  req.buf = sequence.buf;
  req.start = start;
  req.nr = nr;
  req.arg = &sequence;
  req.read = true;
  __send_request(__readwrite, &req);
  sem_wait(&sem);
  memcpy(blocks, sequence.buf, nr * BLOCK_SIZE);
  spdk_dma_free(sequence.buf);
}

void write_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  printf("Write\n");
  struct ns_entry *entry = sb->dev->raw;
  struct hello_world_sequence sequence;
  struct bdev_context *ctx = entry->contexts[0];

  uint32_t buf_align = spdk_bdev_get_buf_align(ctx->bdev);
  uint32_t bsize = spdk_bdev_get_block_size(ctx->bdev);
  printf("%d\n", bsize);
  printf("%d\n", nr);
  printf("%d\n", start);
  sequence.buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, buf_align, NULL);
  memcpy(sequence.buf, blocks, nr * BLOCK_SIZE);
  struct request req;
  req.bdev_desc = ctx->bdev_desc;
  req.io_channel = ctx->bdev_io_channel;
  req.buf = sequence.buf;
  req.start = start;
  req.nr = nr;
  req.arg = &sequence;
  req.read = false;
  __send_request(__readwrite, &req);
  sem_wait(&sem);
}

void zero_blocks(struct super_block *sb, int start, int nr) {
//  struct ns_entry *entry = sb->dev->raw;
//  struct hello_world_sequence sequence;
//  struct bdev_context *ctx = entry->contexts[0];
//  sem_init(&sem, 0, 0);
//  spdk_bdev_write_zeroes_blocks(ctx->bdev_desc, ctx->bdev_io_channel, start, nr,
//                                complete, &sequence);
//  sem_wait(&sem);
  int i;

  for (i = 0; i < nr; i++) {
    write_blocks(sb, zero, start + i, 1);
  }
}
