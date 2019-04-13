#ifndef _BLOCK_H
#define _BLOCK_H

#include <stdbool.h>
#include "device.h"
#include "async.h"

void write_blocks(struct super_block *sb, char *blocks, int start, int nr);
void write_blocks_async(
  struct super_block *sb,
  uint32_t reactor_id,
  struct future *f,
  char *blocks,
  int start,
  int nr
);

void read_blocks(struct super_block *sb, char *blocks, int start, int nr);
void read_blocks_async(
  struct super_block *sb,
  uint32_t reactor_id,
  struct future *f,
  char *blocks,
  int start,
  int nr
);

void zero_blocks(struct super_block *sb, int start, int nr);

#endif /* _BLOCK_H */
