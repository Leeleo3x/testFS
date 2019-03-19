#include <unistd.h>
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/nvme.h"
#include "spdk/stdinc.h"
#include "spdk/thread.h"

#include "common.h"
#include "super.h"
#include "testfs.h"
#include "device.h"

#include <semaphore.h>
#include "spdk/event.h"

static char zero[BLOCK_SIZE] = {0};

struct request {
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *io_channel;
  char *buf;
  size_t start;
  size_t nr;
  void *arg;
  bool read;
  sem_t *sem;
};

static void complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
  printf("complete\n");
  struct request *request = cb_arg;
  spdk_bdev_free_io(bdev_io);
  sem_post(request->sem);
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
  struct spdk_io_channel *io_channel = spdk_bdev_get_io_channel(req->bdev_desc);
  if (req->read) {
    spdk_bdev_read_blocks(req->bdev_desc, io_channel,
                          req->buf, req->start, req->nr, complete, req);
  }
  else {
    spdk_bdev_write_blocks(req->bdev_desc, io_channel,
                           req->buf, req->start, req->nr, complete, req);
  }
}

struct request *generate_request(struct bdev_context *context, bool is_read, size_t start, size_t nr, char* blocks) {
  printf("Generate\n");
  struct request *request= malloc(sizeof(struct request));
  request->bdev_desc = context->bdev_desc;
  request->read = is_read;
  request->start = start;
  request->nr = nr;
  request->buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, context->buf_align, NULL);
  request->sem = &context->sem;
  if (!is_read) {
    memcpy(request->buf, blocks, nr * BLOCK_SIZE);
  }
  return request;
}


void free_request(struct request *request) {
  spdk_dma_free(request->buf);
  free(request);
}

void read_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  printf("Read\n");
  struct bdev_context *context = sb->fs->contexts[0];
  printf("FILESYSTEM: %p\n", sb->fs);
  struct request *request = generate_request(context, true, start, nr, NULL);
  __send_request(__readwrite, request);
  sem_wait(request->sem);
  memcpy(blocks, request->buf, nr * BLOCK_SIZE);
  free_request(request);
}

void write_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  printf("Write\n");
  struct bdev_context *context = sb->fs->contexts[0];
  printf("FILESYSTEM: %p\n", sb->fs);
  struct request *request = generate_request(context, false, start, nr, blocks);
  __send_request(__readwrite, request);
  sem_wait(request->sem);
  free_request(request);
}

void zero_blocks(struct super_block *sb, int start, int nr) {
  int i;

  for (i = 0; i < nr; i++) {
    write_blocks(sb, zero, start + i, 1);
  }
}
