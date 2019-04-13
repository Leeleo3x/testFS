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
#include "block.h"
#include "logging.h"

#include "spdk/event.h"

static char zero[BLOCK_SIZE] = {0};

struct rw_request {
  struct spdk_bdev_desc *bdev_desc;
  struct spdk_io_channel *io_channel;

  char *buf;
  size_t start;
  size_t nr;

  uint32_t reactor_id;
  struct future *f;
};

struct r_request {
  struct rw_request common;
  char *destination;
};

static void reactor_read_complete(
    struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
  struct r_request *req = cb_arg;
  spdk_bdev_free_io(bdev_io);
  // NOTE: It's important that this memcpy occurs before we increment the counter
  memcpy(req->destination, req->common.buf, req->common.nr * BLOCK_SIZE);
  __sync_synchronize();
  req->common.f->counts[req->common.reactor_id] += 1;
  spdk_dma_free(req->common.buf);
  free(req);
}

static void reactor_read(void *arg) {
  struct r_request *req = arg;
  spdk_bdev_read_blocks(
    req->common.bdev_desc,
    req->common.io_channel,
    req->common.buf,
    req->common.start,
    req->common.nr,
    reactor_read_complete,
    req
  );
}

static void reactor_write_complete(
    struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
  struct rw_request *req = cb_arg;
  spdk_bdev_free_io(bdev_io);
  req->f->counts[req->reactor_id] += 1;
  spdk_dma_free(req->buf);
  free(req);
}

static void reactor_write(void *arg) {
  struct rw_request *req = arg;
  spdk_bdev_write_blocks(
    req->bdev_desc,
    req->io_channel,
    req->buf,
    req->start,
    req->nr,
    reactor_write_complete,
    req
  );
}

void fill_request_common(
  struct rw_request *request,
  struct super_block *sb,
  uint32_t reactor_id,
  struct future *f,
  size_t start,
  size_t nr
) {
  request->bdev_desc = sb->fs->bdev_ctx.bdev_desc;
  request->io_channel = sb->fs->reactors[reactor_id].io_channel;

  request->start = start;
  request->nr = nr;
  request->buf = spdk_dma_zmalloc(nr * BLOCK_SIZE, sb->fs->bdev_ctx.buf_align, NULL);
  if (!request->buf) {
    LOG("spdk_dma_zmalloc() failed!\n");
  }

  request->reactor_id = reactor_id;
  request->f = f;
}

void read_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  struct future f;
  future_init(&f);
  // NOTE: We use the data reactor by default for synchronous reads. We cannot
  //       use the main reactor because this function is called on the main
  //       reactor.
  read_blocks_async(sb, DATA_REACTOR, &f, blocks, start, nr);
  spin_wait(&f);
}

void read_blocks_async(
  struct super_block *sb,
  uint32_t reactor_id,
  struct future *f,
  char *blocks,
  int start,
  int nr
) {
  struct r_request *request = malloc(sizeof(struct r_request));
  fill_request_common(&(request->common), sb, reactor_id, f, start, nr);
  request->destination = blocks;
  f->expected_counts[reactor_id] += 1;
  send_request(sb->fs->reactors[reactor_id].lcore, reactor_read, request);
}

void write_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  struct future f;
  future_init(&f);
  // NOTE: We use the data reactor by default for synchronous writes. We cannot
  //       use the main reactor because this function is called on the main
  //       reactor.
  write_blocks_async(sb, DATA_REACTOR, &f, blocks, start, nr);
  spin_wait(&f);
}

void write_blocks_async(
  struct super_block *sb,
  uint32_t reactor_id,
  struct future *f,
  char *blocks,
  int start,
  int nr
) {
  struct rw_request *request = malloc(sizeof(struct rw_request));
  fill_request_common(request, sb, reactor_id, f, start, nr);
  memcpy(request->buf, blocks, nr * BLOCK_SIZE);
  f->expected_counts[reactor_id] += 1;
  send_request(sb->fs->reactors[reactor_id].lcore, reactor_write, request);
}

void zero_blocks(struct super_block *sb, int start, int nr) {
  int i;

  for (i = 0; i < nr; i++) {
    write_blocks(sb, zero, start + i, 1);
  }
}
