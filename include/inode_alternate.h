#ifndef __INODE_ALTERNATE_H__
#define __INODE_ALTERNATE_H__

#include "inode.h"

#define RETURN_IF_NEG(expr) ({   \
  int ret;                       \
  if ((ret = (expr)) < 0) {      \
    return ret;                  \
  }                              \
})

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
 * Flushes a list of inodes to the underlying device asynchronously.  *
 * NOTE: This function will modify the order of the inodes in the list that is
 *       passed in.
 */
void testfs_bulk_sync_inode_async(
    struct inode *inodes[], size_t num_inodes, struct future *f);

/**
 * Writes data to the file represented by the given inode synchronously
 * (alternate implementation).
 *
 * When this function returns, the writes to the underlying device may not have
 * completed. However this function guarantees that, after returning, it is
 * safe to issue additional write requests.
 *
 * To wait for the writes to complete, the caller should wait on the provided
 * future.
 */
int testfs_write_data_alternate(
    struct inode *in, int start, char *buf, const int size);

/**
 * Flushes a list of inodes to the underlying device synchronously.
 *
 * NOTE: This function will modify the order of the inodes in the list that is
 *       passed in.
 */
void testfs_bulk_sync_inode(struct inode *inodes[], size_t num_inodes);


// NOTE: The functions below are helper functions used between our sync and
//       async write path implementations

void testfs_ensure_indirect_loaded(struct inode *in);
int testfs_inode_log_to_phy(struct inode *in, int log_block_nr);
int testfs_allocate_block_alternate(struct inode *in, int log_block_nr);
int inode_compare(const void *p1, const void *p2);

#endif
