#include <sys/time.h>

#include "bench.h"
#include "logging.h"
#include "async.h"
#include "dir.h"
#include "block.h"
#include "csum.h"
#include "inode_alternate.h"

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

#define FILENAME_LENGTH 5

static char *read_file(const char *file, size_t *size) {
  FILE *f = fopen(file, "rb");
  if (f == NULL) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  *size = ftell(f);
  fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
  char *string = malloc(*size + 1);
  fread(string, 1, *size, f);
  fclose(f);
  string[*size] = '\0';
  return string;
}

static void benchmark_set_up(
  struct filesystem *fs,
  struct context *c,
  char filenames[][FILENAME_LENGTH],
  size_t num_files
) {
  cmd_mkfs(fs->sb, c);
  for (size_t i = 0; i < num_files; i++) {
    testfs_create_file_or_dir(fs->sb, c->cur_dir, I_FILE, filenames[i]);
  }
}

static void benchmark_sync_writes(
  struct filesystem *fs,
  struct inode *dir,
  char filenames[][FILENAME_LENGTH],
  size_t num_files,
  char *content,
  int size
) {
  struct inode *file_inodes[num_files];
  FOR(
    num_files,
    file_inodes[i] =
      testfs_get_inode(fs->sb, testfs_dir_name_to_inode_nr(dir, filenames[i]))
  );

  testfs_tx_start(fs->sb, TX_WRITE);
  FOR(
    num_files, testfs_write_data_alternate(file_inodes[i], 0, content, size));
  testfs_bulk_sync_inode(file_inodes, num_files);
  testfs_flush_block_freemap(fs->sb);
  testfs_flush_csum(fs->sb);
  testfs_tx_commit(fs->sb, TX_WRITE);

  FOR(num_files, testfs_put_inode(file_inodes[i]));
}

static void benchmark_async_writes(
  struct filesystem *fs,
  struct inode *dir,
  char filenames[][FILENAME_LENGTH],
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
      testfs_get_inode(fs->sb, testfs_dir_name_to_inode_nr(dir, filenames[i]))
  );

  testfs_tx_start(fs->sb, TX_WRITE);
  FOR(
    num_files,
    testfs_write_data_alternate_async(file_inodes[i], &f, 0, content, size)
  );
  testfs_bulk_sync_inode_async(file_inodes, num_files, &f);
  testfs_flush_block_freemap_async(fs->sb, &f);
  testfs_flush_csum_async(fs->sb, &f);
  spin_wait(&f);
  testfs_tx_commit(fs->sb, TX_WRITE);

  FOR(num_files, testfs_put_inode(file_inodes[i]));
}

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

static void print_digest(
  char *benchmark_name,
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

  printf("===== %s =====\n", benchmark_name);
  printf("Number of trials: %d\n", num_trials);
  printf("Async Speedup:    %.2f\n", speedup);
  printf(
    "Sync:             min: %lld us  max: %lld us  avg: %.2f us\n",
    min_sync_us,
    max_sync_us,
    average_sync_us
  );
  printf(
    "Async:            min: %lld us  max: %lld us  avg: %.2f us\n",
    min_async_us,
    max_async_us,
    average_async_us
  );
  printf("\n");
}

/**
 * Benchmarks the end-to-end data write path of the file system.
 *
 * This is a microbenchmark meant to test the effects of using asynchronous
 * writes when writing data to files.
 *
 * Arguments:
 * cmd[2]: string - Name of the input data file
 * cmd[3]: int    - The number of trials to run
 * cmd[4]: int    - The number of files to create
 */
static int benchmark_e2e_write(struct filesystem *fs, struct context *c) {
  if (c->nargs < 4) {
    return -EINVAL;
  }

  size_t size;
  char *content = read_file(c->cmd[2], &size);
  int num_trials = strtol(c->cmd[3], NULL, 10);
  size_t num_files = strtol(c->cmd[4], NULL, 10);

  if (content == NULL) {
    printf("Error: Unable to open file %s\n", c->cmd[2]);
    return -EINVAL;
  }

  char filenames[num_files][FILENAME_LENGTH];
  for (size_t i = 0; i < num_files; i++) {
    sprintf(filenames[i], "%lu", i);
  }

  long long results_sync_us[num_trials];
  long long results_async_us[num_trials];

  for (int trial = 0; trial < num_trials; trial++) {
    benchmark_set_up(fs, c, filenames, num_files);
    MEASURE_USEC(
      results_sync_us[trial],
      benchmark_sync_writes(
        fs, c->cur_dir, filenames, num_files, content, size)
    );

    benchmark_set_up(fs, c, filenames, num_files);
    MEASURE_USEC(
      results_async_us[trial],
      benchmark_async_writes(
        fs, c->cur_dir, filenames, num_files, content, size)
    );
  }

  free(content);
  print_digest("e2e_write", results_sync_us, results_async_us, num_trials);

  return 0;
}

/**
 * Benchmarks sequential reads of blocks from the underlying device.
 *
 * Arguments:
 * cmd[2]: Number of trials
 * cmd[3]: Number of blocks
 */
static int benchmark_raw_seq_read(struct filesystem *fs, struct context *c) {
  if (c->nargs < 3) {
    return -EINVAL;
  }

  int num_trials = strtol(c->cmd[2], NULL, 10);
  int num_blocks = strtol(c->cmd[3], NULL, 10);

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
  print_digest("raw_seq_read", results_sync_us, results_async_us, num_trials);

  return 0;
}

/**
 * Benchmarks sequential writes of blocks from the underlying device.
 *
 * Arguments:
 * cmd[2]: Number of trials
 * cmd[3]: Number of blocks
 */
static int benchmark_raw_seq_write(struct filesystem *fs, struct context *c) {
  if (c->nargs < 3) {
    return -EINVAL;
  }

  int num_trials = strtol(c->cmd[2], NULL, 10);
  int num_blocks = strtol(c->cmd[3], NULL, 10);

  long long results_sync_us[num_trials];
  long long results_async_us[num_trials];

  for (int trial = 0; trial < num_trials; trial++) {
    MEASURE_USEC(results_sync_us[trial], benchmark_raw_write(fs, num_blocks));

    MEASURE_USEC(
      results_async_us[trial], benchmark_raw_write_async(fs, num_blocks));
  }

  print_digest("raw_seq_write", results_sync_us, results_async_us, num_trials);

  return 0;
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
    return benchmark_e2e_write(fs, c);

  } else if (strcmp(c->cmd[1], "raw_seq_read") == 0) {
    return benchmark_raw_seq_read(fs, c);

  } else if (strcmp(c->cmd[1], "raw_seq_write") == 0) {
    return benchmark_raw_seq_write(fs, c);

  } else {
    printf("Unknown benchmark: '%s'\n", c->cmd[1]);
    return -EINVAL;
  }
}
