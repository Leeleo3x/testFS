#ifndef __BENCH_H__
#define __BENCH_H__

#include <sys/time.h>

#include "super.h"
#include "testfs.h"

#define MEASURE_USEC(out, expr) ({                                   \
  struct timeval t0, t1;                                             \
  gettimeofday(&t0, NULL);                                           \
  (expr);                                                            \
  gettimeofday(&t1, NULL);                                           \
  (out) = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_usec - t0.tv_usec); \
})

struct bench_result {
  long long max_us, min_us;
  double avg_us;
};

struct bench_digest {
  int trials;
  struct bench_result sync;
  struct bench_result async;
};

// REPL commands
int cmd_benchmark(struct super_block *sb, struct context *c);
int subcmd_benchmark_e2e_write(struct filesystem *fs, struct context *c);
int subcmd_benchmark_raw_seq_read(struct filesystem *fs, struct context *c);
int subcmd_benchmark_raw_seq_write(struct filesystem *fs, struct context *c);

// Raw sequential read/write microbenchmarks
void benchmark_raw_seq_read(
  struct filesystem *fs,
  struct bench_digest *digest,
  int num_trials,
  int num_blocks
);
void benchmark_raw_seq_write(
  struct filesystem *fs,
  struct bench_digest *digest,
  int num_trials,
  int num_blocks
);

// End-to-end write path microbenchmark
void benchmark_e2e_write(
  struct filesystem *fs,
  struct context *c,
  struct bench_digest *digest,
  char *content,
  size_t size,
  int num_trials,
  size_t num_files
);

// Benchmark utilities
void populate_digest(
  struct bench_digest *digest,
  long long results_sync_us[],
  long long results_async_us[],
  int num_trials
);
void print_digest(char *benchmark_name, struct bench_digest *digest);

#endif
