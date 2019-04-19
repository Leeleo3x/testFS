#ifndef _INODE_H
#define _INODE_H

#include "bitmap.h"
#include "list.h"
#include "super.h"
#include "async.h"

typedef enum { I_NONE, I_FILE, I_DIR } inode_type;

#define NR_DIRECT_BLOCKS 4
#define NR_INDIRECT_BLOCKS (BLOCK_SIZE / sizeof(int))

// dinode - inode maintained on disk

struct dinode {
  inode_type i_type;                /* 0x00 */
  int i_size;                       /* 0x04 */
  int i_mod_time;                   /* 0x08 */
  int i_block_nr[NR_DIRECT_BLOCKS]; /* 0x0C */
  int i_indirect;                   /* 0x1C */
};

#define INODES_PER_BLOCK (BLOCK_SIZE / (sizeof(struct dinode)))

void inode_hash_init(void);
void inode_hash_destroy(void);
struct inode *testfs_get_inode(struct super_block *sb, int inode_nr);
void testfs_sync_inode(struct inode *in);
void testfs_sync_inode_async(struct inode *in, struct future *f);
void testfs_put_inode(struct inode *in);
int testfs_inode_get_size(struct inode *in);
inode_type testfs_inode_get_type(struct inode *in);
int testfs_inode_get_nr(struct inode *in);
struct super_block *testfs_inode_get_sb(struct inode *in);
int testfs_create_inode(struct super_block *sb, inode_type type,
                        struct inode **inp);
void testfs_remove_inode(struct inode *in);
int testfs_read_data(struct inode *in, int start, char *buf, const int size);
void testfs_truncate_data(struct inode *in, const int size);
int testfs_check_inode(struct super_block *sb, struct bitmap *b_freemap,
                       struct inode *in);

/**
 * Writes data to the file represented by the given inode synchronously.
 *
 * When this function returns, the data has been written to the underlying
 * device.
 */
int testfs_write_data(struct inode *in, int start, char *name, const int size);

/**
 * Writes data to the file represented by the given inode asynchronously
 * (alternate implementation).
 *
 * When this function returns, the writes to the underlying device may not have
 * completed. However this function guarantees that, after returning, it is
 * safe to issue additional write requests.
 *
 * To wait for the writes to complete, the caller should wait on the provided
 * future.
 */
int testfs_write_data_alternate_async(
    struct inode *in, struct future *f, int start, char *buf, const int size);

/**
 * Synchronizes a list of inodes.
 *
 * NOTE: This function will modify the order of the inodes in the list that is
 *       passed in.
 */
void testfs_bulk_sync_inode_async(
    struct inode *inodes[], size_t num_inodes, struct future *f);

#endif /* _INODE_H */
