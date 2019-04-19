#include "bench.h"

#define M_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define M_MAX(X, Y) ((X) > (Y) ? (X) : (Y))

void populate_digest(
  struct bench_digest *digest,
  long long results_sync_us[],
  long long results_async_us[],
  int num_trials
) {
  double average_sync_us = 0.;
  double average_async_us = 0.;
  long long max_sync_us = results_sync_us[0];
  long long min_sync_us = results_sync_us[0];
  long long max_async_us = results_async_us[0];
  long long min_async_us = results_async_us[0];
  double num_trials_d = (double) num_trials;

  for (int i = 0; i < num_trials; i++) {
    average_sync_us += results_sync_us[i] / num_trials_d;
    average_async_us += results_async_us[i] / num_trials_d;
    max_sync_us = M_MAX(max_sync_us, results_sync_us[i]);
    min_sync_us = M_MIN(min_sync_us, results_sync_us[i]);
    max_async_us = M_MAX(max_async_us, results_async_us[i]);
    min_async_us = M_MIN(min_async_us, results_async_us[i]);
  }

  digest->trials = num_trials;
  digest->sync.max_us = max_sync_us;
  digest->sync.min_us = min_sync_us;
  digest->sync.avg_us = average_sync_us;
  digest->async.max_us = max_async_us;
  digest->async.min_us = min_async_us;
  digest->async.avg_us = average_async_us;
}

void print_digest(
  char *benchmark_name,
  struct bench_digest *digest
) {
  double speedup = digest->sync.avg_us / digest->async.avg_us;

  printf("===== %s =====\n", benchmark_name);
  printf("Number of trials: %d\n", digest->trials);
  printf("Async Speedup:    %.2f\n", speedup);
  printf(
    "Sync:             min: %lld us  max: %lld us  avg: %.2f us\n",
    digest->sync.min_us,
    digest->sync.max_us,
    digest->sync.avg_us
  );
  printf(
    "Async:            min: %lld us  max: %lld us  avg: %.2f us\n",
    digest->async.min_us,
    digest->async.max_us,
    digest->async.avg_us
  );
  printf("\n");
}

/**
 * Run microbenchmarks on the file system.
 *
 * Arguments:
 * cmd[1]: string - Benchmark name
 */
int cmd_benchmark(struct super_block *sb, struct context *c) {
  if (c->nargs < 1) {
    return -EINVAL;
  }

  struct filesystem *fs = sb->fs;

  if (strcmp(c->cmd[1], "e2e_write") == 0) {
    return subcmd_benchmark_e2e_write(fs, c);

  } else if (strcmp(c->cmd[1], "raw_seq_read") == 0) {
    return subcmd_benchmark_raw_seq_read(fs, c);

  } else if (strcmp(c->cmd[1], "raw_seq_write") == 0) {
    return subcmd_benchmark_raw_seq_write(fs, c);

  } else {
    printf("Unknown benchmark: '%s'\n", c->cmd[1]);
    return -EINVAL;
  }
}
