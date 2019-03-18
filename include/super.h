#ifndef _SUPER_H
#define _SUPER_H

#include "tx.h"
#include "device.h"
#include "block.h"

struct dsuper_block {
  uint64_t inode_freemap_start;
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

void testfs_make_super_block(struct device *dev);
void testfs_make_inode_freemap(void *arg);
void testfs_make_block_freemap(void *arg);
void testfs_make_csum_table(void *arg);
void testfs_make_inode_blocks(void *arg);

enum testfs_init_super_block_state {
  INIT_SUPER_BLOCK_INIT = 0,
  INIT_INOCDE_FREEMAP,
  INIT_BLOCK_FREEMAP,
  INIT_CSUM_TABLE,
  INIT_SUPER_BLOCK_DONE,
};

struct testfs_init_super_block_context {
  enum testfs_init_super_block_state state;
  struct device *dev;
  int corrupt;
  struct super_block **sbp;
  void *cb_arg;
  void (*cb)(void *);
};

int testfs_init_super_block(void *arg);
void testfs_write_super_block(struct super_block *sb, block_write_cb cb, void *arg);
void testfs_close_super_block(struct super_block *sb);

enum testfs_flush_super_block_state {
  WRITE_SUPER_BLOCK = 0,
  WRITE_INODE_FREEMAP,
  WRITE_BLOCK_FREEMAP,
  WRITE_SUPER_BLOCK_DONE
};

struct testfs_flush_super_block_context {
  struct super_block *sb;
  void *cb_arg;
  void (*cb)(void *);
  enum testfs_flush_super_block_state  state;
};
void testfs_flush_super_block(void *arg);

int testfs_get_inode_freemap(struct super_block *sb);
void testfs_put_inode_freemap(struct super_block *sb, int inode_nr);

int testfs_alloc_block(struct super_block *sb, char *block);
int testfs_free_block(struct super_block *sb, int block_nr);

#endif /* _SUPER_H */
