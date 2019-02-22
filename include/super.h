#ifndef _SUPER_H
#define _SUPER_H

#include "tx.h"
#include "device.h"

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
  struct device *dev;
  struct bitmap *inode_freemap;
  struct bitmap *block_freemap;
  tx_type tx_in_progress;

  // TODO: add your code here
  int *csum_table;
};

struct super_block *testfs_make_super_block(struct device *dev);
void testfs_make_inode_freemap(struct super_block *sb);
void testfs_make_block_freemap(struct super_block *sb);
void testfs_make_csum_table(struct super_block *sb);
void testfs_make_inode_blocks(struct super_block *sb);

int testfs_init_super_block(struct device *dev, int corrupt,
                            struct super_block **sbp);
void testfs_write_super_block(struct super_block *sb);
void testfs_close_super_block(struct super_block *sb);
void testfs_flush_super_block(struct super_block *sb);

int testfs_get_inode_freemap(struct super_block *sb);
void testfs_put_inode_freemap(struct super_block *sb, int inode_nr);

int testfs_alloc_block(struct super_block *sb, char *block);
int testfs_free_block(struct super_block *sb, int block_nr);

#endif /* _SUPER_H */
