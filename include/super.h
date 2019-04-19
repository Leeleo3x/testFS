#ifndef _SUPER_H
#define _SUPER_H

#include <stdbool.h>

#include "tx.h"
#include "device.h"
#include "async.h"
#include "testfs.h"

struct dsuper_block {
  int inode_freemap_start;
  int block_freemap_start;
  int csum_table_start;
  int inode_blocks_start;
  int data_blocks_start;
  int modification_time;
};

struct super_block {
  struct dsuper_block sb;
  struct bitmap *inode_freemap;
  struct bitmap *block_freemap;
  tx_type tx_in_progress;
  struct filesystem *fs;

  int *csum_table;
  bool csum_block_dirty[CSUM_TABLE_SIZE];
};

void testfs_make_super_block(struct filesystem *dev);
void testfs_make_inode_freemap(struct super_block *sb);
void testfs_make_block_freemap(struct super_block *sb);
void testfs_make_csum_table(struct super_block *sb);
void testfs_make_inode_blocks(struct super_block *sb);

int testfs_init_super_block(struct filesystem *fs, int corrupt);
void testfs_write_super_block(struct super_block *sb);
void testfs_close_super_block(struct super_block *sb);
void testfs_flush_super_block(struct super_block *sb);

int testfs_get_inode_freemap(struct super_block *sb);
void testfs_put_inode_freemap(struct super_block *sb, int inode_nr);

int testfs_alloc_block(struct super_block *sb, char *block);
int testfs_free_block(struct super_block *sb, int block_nr);

int testfs_alloc_block_async(
  struct super_block *sb, struct future *f, char *block);

/**
 * Allocates a block in the in-memory freemap. Caller is responsible for
 * ensuring that the freemap is eventually flushed to the underlying device.
 */
int testfs_alloc_block_alternate(struct super_block *sb);

/**
 * Writes the in-memory freemap to the underlying device.
 */
void testfs_flush_block_freemap_async(
  struct super_block *sb, struct future *f);

#endif /* _SUPER_H */
