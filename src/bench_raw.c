#include "bench.h"

#include <stdlib.h>
#include "async.h"
#include "block.h"

static void benchmark_raw_write(struct filesystem *fs, int num_blocks) {
  char block[BLOCK_SIZE];
  for (int i = 0; i < num_blocks; i++) {
    write_blocks(fs->sb, block, i, 1);
  }
}

static void benchmark_raw_write_async(struct filesystem *fs, int num_blocks) {
  char block[BLOCK_SIZE];
  struct future f;
  future_init(&f);
  for (int i = 0; i < num_blocks; i++) {
    write_blocks_async(fs->sb, DATA_REACTOR, &f, block, i, 1);
  }
  spin_wait(&f);
}

static void benchmark_raw_read(
    struct filesystem *fs, char *buffer, int num_blocks) {
  for (int i = 0; i < num_blocks; i++) {
    read_blocks(fs->sb, buffer + (i * BLOCK_SIZE), i, 1);
  }
}

static void benchmark_raw_read_async(
    struct filesystem *fs, char *buffer, int num_blocks) {
  struct future f;
  future_init(&f);
  for (int i = 0; i < num_blocks; i++) {
    read_blocks_async(
      fs->sb, DATA_REACTOR, &f, buffer + (i * BLOCK_SIZE), i, 1);
  }
  spin_wait(&f);
}

/**
 * Benchmarks sequential reads of blocks from the underlying device.
 *
 * Arguments:
 * cmd[2]: Number of trials
 * cmd[3]: Number of blocks
 */
int subcmd_benchmark_raw_seq_read(struct filesystem *fs, struct context *c) {
  if (c->nargs < 3) {
    return -EINVAL;
  }

  int num_trials = strtol(c->cmd[2], NULL, 10);
  int num_blocks = strtol(c->cmd[3], NULL, 10);

  struct bench_digest digest;
  benchmark_raw_seq_read(fs, &digest, num_trials, num_blocks);
  print_digest("raw_seq_read", &digest);

  return 0;
}

void benchmark_raw_seq_read(
  struct filesystem *fs,
  struct bench_digest *digest,
  int num_trials,
  int num_blocks
) {
  char *buffer = malloc(sizeof(char) * BLOCK_SIZE * num_blocks);

  long long results_sync_us[num_trials];
  long long results_async_us[num_trials];

  for (int trial = 0; trial < num_trials; trial++) {
    MEASURE_USEC(
      results_sync_us[trial], benchmark_raw_read(fs, buffer, num_blocks));

    MEASURE_USEC(
      results_async_us[trial],
      benchmark_raw_read_async(fs, buffer, num_blocks)
    );
  }

  free(buffer);
  populate_digest(digest, results_sync_us, results_async_us, num_trials);
}

/**
 * Benchmarks sequential writes of blocks from the underlying device.
 *
 * Arguments:
 * cmd[2]: Number of trials
 * cmd[3]: Number of blocks
 */
int subcmd_benchmark_raw_seq_write(struct filesystem *fs, struct context *c) {
  if (c->nargs < 3) {
    return -EINVAL;
  }

  int num_trials = strtol(c->cmd[2], NULL, 10);
  int num_blocks = strtol(c->cmd[3], NULL, 10);

  struct bench_digest digest;
  benchmark_raw_seq_write(fs, &digest, num_trials, num_blocks);
  print_digest("raw_seq_write", &digest);

  return 0;
}

void benchmark_raw_seq_write(
  struct filesystem *fs,
  struct bench_digest *digest,
  int num_trials,
  int num_blocks
) {
  long long results_sync_us[num_trials];
  long long results_async_us[num_trials];

  for (int trial = 0; trial < num_trials; trial++) {
    MEASURE_USEC(results_sync_us[trial], benchmark_raw_write(fs, num_blocks));

    MEASURE_USEC(
      results_async_us[trial], benchmark_raw_write_async(fs, num_blocks));
  }

  populate_digest(digest, results_sync_us, results_async_us, num_trials);
}
