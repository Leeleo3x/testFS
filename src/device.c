#include <device.h>
#include "spdk/thread.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/util.h"

#include "async.h"
#include "device.h"
#include "logging.h"

struct init_completed_context {
  struct filesystem *fs;
  device_init_cb app_start;
  size_t outstanding_requests;
};

struct reactor_init_context {
  struct filesystem *fs;
  struct reactor_context *reactor;
  struct init_completed_context *completed_ctx;
};

static void init_bdev(struct filesystem *fs) {
  fs->bdev_ctx.bdev = spdk_bdev_first();
  if (fs->bdev_ctx.bdev == NULL) {
    SPDK_ERRLOG("Could not get bdev\n");
    spdk_app_stop(-1);
  }

  LOG("BLOCK_SIZE %d\n", spdk_bdev_get_block_size(fs->bdev_ctx.bdev));
  fs->bdev_ctx.bdev_name = spdk_bdev_get_name(fs->bdev_ctx.bdev);

  fs->bdev_ctx.bdev_desc = NULL;
  if (spdk_bdev_open(fs->bdev_ctx.bdev, true, NULL, NULL, &(fs->bdev_ctx.bdev_desc))) {
    SPDK_ERRLOG("Could not open bdev: %s\n", fs->bdev_ctx.bdev_name);
    spdk_app_stop(-1);
  }

  fs->bdev_ctx.buf_align = spdk_bdev_get_buf_align(fs->bdev_ctx.bdev);
  SPDK_NOTICELOG("Bdev: %s init finished\n", fs->bdev_ctx.bdev_name);
}

static void init_reactors_complete(void *arg) {
  struct init_completed_context *ctx = arg;
  ctx->outstanding_requests -= 1;
  if (ctx->outstanding_requests != 0) {
    return;
  }

  LOG("START FINISHED\n");
  // NOTE: This function currently already runs on the main reactor, but this
  //       will ensure the REPL runs on a clean stack.
  send_request(ctx->fs->reactors[MAIN_REACTOR].lcore, ctx->app_start, ctx->fs);
  free(ctx);
}

static void acquire_io_channels(void *arg) {
  struct reactor_init_context *ctx = arg;
  ctx->reactor->io_channel = spdk_bdev_get_io_channel(ctx->fs->bdev_ctx.bdev_desc);
  send_request(ctx->fs->reactors[MAIN_REACTOR].lcore, init_reactors_complete, ctx->completed_ctx);
  free(ctx);
}

static void init_reactors(struct filesystem *fs, struct init_completed_context *completed_ctx) {
  struct reactor_init_context *ctx[NUM_REACTORS];

  for (size_t i = 0; i < NUM_REACTORS; i++) {
    // NOTE: We use the reactor context index to assign the lcore ID, but we
    //       can change the mapping if needed
    fs->reactors[i].lcore = i;

    ctx[i] = malloc(sizeof(struct reactor_init_context));
    ctx[i]->fs = fs;
    ctx[i]->reactor = &(fs->reactors[i]);
    ctx[i]->completed_ctx = completed_ctx;
    send_request(fs->reactors[MAIN_REACTOR].lcore, acquire_io_channels, ctx[i]);
  }
}

static void start(void *arg1, void *arg2) {
  struct filesystem *fs = malloc(sizeof(struct filesystem));
  struct init_completed_context *completed_ctx = malloc(sizeof(struct init_completed_context));
  completed_ctx->fs = fs;
  completed_ctx->app_start = (device_init_cb) arg1;
  completed_ctx->outstanding_requests = NUM_REACTORS;
  init_bdev(fs);
  init_reactors(fs, completed_ctx);
}

void dev_stop(struct filesystem *fs) {
  for (int i = 0; i < NUM_REACTORS; i++) {
    spdk_put_io_channel(fs->reactors[i].io_channel);
  }
  spdk_bdev_close(fs->bdev_ctx.bdev_desc);
  spdk_app_stop(0);
}

void dev_init(const char *f, device_init_cb cb) {
  int rc;
  struct spdk_app_opts opts = {};
  spdk_app_opts_init(&opts);
  opts.name = "hello_world";
  opts.config_file = "config.conf";
  opts.reactor_mask = "0x7";
  spdk_app_start(&opts, start, cb);
}

void wait_context(struct bdev_context *context) {
  while (context->counter) {
    context->counter--;
    sem_wait(&context->sem);
  }
}
