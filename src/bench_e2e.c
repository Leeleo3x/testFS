#include "bench.h"

#include <stdlib.h>
#include <stdio.h>
#include "dir.h"
#include "block.h"
#include "csum.h"
#include "inode_alternate.h"

#define FILENAME_LENGTH 5
#define FOR(limit, expr) ({for (size_t i = 0; i < (limit); i++) { (expr); }})

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
int subcmd_benchmark_e2e_write(struct filesystem *fs, struct context *c) {
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

  struct bench_digest digest;
  benchmark_e2e_write(fs, c, &digest, content, size, num_trials, num_files);
  free(content);
  print_digest("e2e_write", &digest);

  return 0;
}

void benchmark_e2e_write(
  struct filesystem *fs,
  struct context *c,
  struct bench_digest *digest,
  char *content,
  size_t size,
  int num_trials,
  size_t num_files
) {
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

  populate_digest(digest, results_sync_us, results_async_us, num_trials);
}
