#include <sys/time.h>

#include "bench.h"
#include "logging.h"
#include "async.h"
#include "dir.h"

#define FOR(limit, expr) {for (size_t i = 0; i < (limit); i++) { (expr); }}

#define MEASURE_USEC(out, expr) { \
  struct timeval t0, t1; \
  gettimeofday(&t0, NULL); \
  (expr); \
  gettimeofday(&t1, NULL); \
  (out) = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_usec - t0.tv_usec); \
}

#define M_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define M_MAX(X, Y) ((X) > (Y) ? (X) : (Y))

static void benchmark_set_up(
  struct super_block *sb,
  struct context *c,
  char **filenames,
  size_t num_files
) {
  cmd_mkfs(sb, c);
  for (size_t i = 0; i < num_files; i++) {
    testfs_create_file_or_dir(sb, c->cur_dir, I_FILE, filenames[i]);
  }
}

static void benchmark_sync_writes(
  struct super_block *sb,
  struct inode *dir,
  char **filenames,
  size_t num_files,
  char *content,
  int size
) {
  struct inode *file_inodes[num_files];
  FOR(
    num_files,
    file_inodes[i] =
      testfs_get_inode(sb, testfs_dir_name_to_inode_nr(dir, filenames[i]))
  );

  testfs_tx_start(sb, TX_WRITE);
  FOR(num_files, testfs_write_data(file_inodes[i], 0, content, size));
  FOR(num_files, testfs_sync_inode(file_inodes[i]));
  testfs_tx_commit(sb, TX_WRITE);

  FOR(num_files, testfs_put_inode(file_inodes[i]));
}

static void benchmark_async_writes(
  struct super_block *sb,
  struct inode *dir,
  char **filenames,
  size_t num_files,
  char *content,
  int size
) {
  struct future f;
  future_init(&f);

  struct inode *file_inodes[num_files];
  FOR(
    num_files,
    file_inodes[i] =
      testfs_get_inode(sb, testfs_dir_name_to_inode_nr(dir, filenames[i]))
  );

  testfs_tx_start(sb, TX_WRITE);
  FOR(
    num_files, testfs_write_data_async(file_inodes[i], &f, 0, content, size));
  FOR(num_files, testfs_sync_inode(file_inodes[i]));
  spin_wait(&f);
  testfs_tx_commit(sb, TX_WRITE);

  FOR(num_files, testfs_put_inode(file_inodes[i]));
}

static void print_digest(
  long long *results_sync_us,
  long long *results_async_us,
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

  double speedup = average_sync_us / average_async_us;

  LOG("Number of trials: %d\n", num_trials);
  LOG("Speedup:          %.2f\n", speedup);
  LOG(
    "Sync. Writes:     min: %lld  max: %lld  avg: %.2f\n",
    min_sync_us,
    max_sync_us,
    average_sync_us
  );
  LOG(
    "Async. Writes:    min: %lld  max: %lld  avg: %.2f\n",
    min_async_us,
    max_async_us,
    average_async_us
  );
  LOG("----------\n");
}

/**
 * Benchmarks the data write path of the file system.
 *
 * This is a microbenchmark meant to test the effects of using asynchronous
 * writes when writing data to files.
 *
 * Arguments:
 * cmd[1]: int - The number of trials to run
 * cmd[2]: int - The number of files to create
 */
int cmd_benchmark(struct super_block *sb, struct context *c) {
  if (c->nargs < 3) {
    return -EINVAL;
  }

  int num_trials = strtol(c->cmd[1], NULL, 10);
  size_t num_files = strtol(c->cmd[2], NULL, 10);

  char filenames[num_files][5];
  for (size_t i = 0; i < num_files; i++) {
    sprintf(filenames[i], "%lu", i);
  }

  // TODO: Get dummy data for benchmarking
  char *content = "hello world!";
  int size = strlen(content) + 1;

  long long results_sync_us[num_trials];
  long long results_async_us[num_trials];

  for (int trial = 0; trial < num_trials; trial++) {
    benchmark_set_up(sb, c, filenames, num_files);
    MEASURE_USEC(
      results_sync_us[trial],
      benchmark_sync_writes(
        sb, c->cur_dir, filenames, num_files, content, size)
    );

    benchmark_set_up(sb, c, filenames, num_files);
    MEASURE_USEC(
      results_async_us[trial],
      benchmark_async_writes(
        sb, c->cur_dir, filenames, num_files, content, size)
    );
  }

  print_digest(results_sync_us, results_async_us, num_trials);
}
