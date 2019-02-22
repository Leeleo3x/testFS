#include "spdk/stdinc.h"
#include "spdk/env.h"
#include "spdk/nvme.h"

#include "testfs.h"
#include "device_nvme.h"
#include "super.h"
#include "common.h"

static char zero[BLOCK_SIZE] = {0};


static void complete(void *arg, const struct spdk_nvme_cpl *completion) {
  struct hello_world_sequence *sequence = arg;
  sequence->is_completed = 1;
}

void read_blocks(struct super_block *sb, char *blocks, int start, int nr) {
  struct ns_entry *entry= sb->dev->raw;
  struct hello_world_sequence sequence;

  sequence.buf = spdk_zmalloc(nr * BLOCK_SIZE, 0, NULL, SPDK_ENV_SOCKET_ID_ANY,
                               SPDK_MALLOC_DMA);
  int rc = spdk_nvme_ns_cmd_read(entry->ns, entry->qpair, sequence.buf,
                                 start, /* LBA start */
                                 nr,    /* number of LBAs */
                                 complete, &sequence, 0);
  if (rc != 0) {
    fprintf(stderr, "starting read I/O failed\n");
    exit(1);
  }
  sequence.is_completed = 0;
  sequence.ns_entry = entry;
  while (!sequence.is_completed) {
    spdk_nvme_qpair_process_completions(entry->qpair, 0);
  }
  memcpy(blocks, sequence.buf, nr * BLOCK_SIZE);
  spdk_free(sequence.buf);
  // spdk_nvme_ctrlr_free_io_qpair(entry->qpair);
}

void write_blocks(struct super_block *sb, char *blocks, int start,
                         int nr) {
  struct ns_entry *entry = sb->dev->raw;
  struct hello_world_sequence sequence;
  sequence.using_cmb_io = 1;
  sequence.buf =
      spdk_nvme_ctrlr_alloc_cmb_io_buffer(entry->ctrlr, nr * BLOCK_SIZE);
  if (sequence.buf == NULL) {
    sequence.using_cmb_io = 0;
    sequence.buf = spdk_zmalloc(nr * BLOCK_SIZE, 0, NULL,
                                SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
  }
  if (sequence.buf == NULL) {
    EXIT("malloc");
    return;
  }
  sequence.is_completed = 0;
  sequence.ns_entry = entry;

  memcpy(sequence.buf, blocks, nr * BLOCK_SIZE);
  int rc = spdk_nvme_ns_cmd_write(entry->ns, entry->qpair, sequence.buf,
                                  start, /* LBA start */
                                  nr,    /* number of LBAs */
                                  complete, &sequence, 0);
  if (rc != 0) {
    fprintf(stderr, "starting write I/O failed\n");
    exit(1);
  }
  while (!sequence.is_completed) {
    spdk_nvme_qpair_process_completions(entry->qpair, 0);
  }
  if (sequence.using_cmb_io) {
    spdk_nvme_ctrlr_free_cmb_io_buffer(entry->ctrlr, sequence.buf, nr * BLOCK_SIZE);
  } else {
    spdk_free(sequence.buf);
  }
}

void zero_blocks(struct super_block *sb, int start, int nr) {
  int i;

  for (i = 0; i < nr; i++) {
    write_blocks(sb, zero, start + i, 1);
  }
}
