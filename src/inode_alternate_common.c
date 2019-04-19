#include "inode_alternate.h"
#include "block.h"

void testfs_ensure_indirect_loaded(struct inode *in) {
  if (in->i_flags & I_FLAGS_INDIRECT_LOADED) {
    return;
  }
  // NOTE: We do this synchronously since in all current use cases the callers
  //       cannot proceed until the indirect block has been loaded.
  struct future f;
  future_init(&f);
  read_blocks_async(
    in->sb, METADATA_REACTOR, &f, (char *) in->indirect, in->in.i_indirect, 1);
  spin_wait(&f);
  in->i_flags |= I_FLAGS_INDIRECT_LOADED;
}

/**
 * Returns the physical block number mapped to a given logical block number for
 * a file.
 *
 * A negative return value indicates an error. A return value of 0 indicates a
 * physical block has not been mapped to the provided logical block number.
 */
int testfs_inode_log_to_phy(struct inode *in, int log_block_nr) {
  if (log_block_nr < NR_DIRECT_BLOCKS) {
    int phy_block_nr = in->in.i_block_nr[log_block_nr];
    assert(phy_block_nr >= 0);
    return in->in.i_block_nr[log_block_nr];
  }

  int indirect_log_block_nr = log_block_nr - NR_DIRECT_BLOCKS;
  if (indirect_log_block_nr >= NR_INDIRECT_BLOCKS) {
    return -EFBIG;
  }
  if (in->in.i_indirect == 0) {
    return 0;
  }

  testfs_ensure_indirect_loaded(in);
  return in->indirect[indirect_log_block_nr];
}

int testfs_allocate_block_alternate(struct inode *in, int log_block_nr) {
  int phy_block_nr = testfs_alloc_block_alternate(in->sb);
  if (phy_block_nr < 0) {
    return phy_block_nr;
  }

  in->i_flags |= I_FLAGS_DIRTY;

  if (log_block_nr < NR_DIRECT_BLOCKS) {
    in->in.i_block_nr[log_block_nr] = phy_block_nr;
    return phy_block_nr;
  }

  if (in->in.i_indirect == 0) {
    // Allocate the indirect block if one doesn't exist
    int indirect_block_nr = testfs_alloc_block_alternate(in->sb);
    if (indirect_block_nr < 0) {
      return indirect_block_nr;
    }
    memset(in->indirect, 0, sizeof(int) * NR_INDIRECT_BLOCKS);
    in->in.i_indirect = indirect_block_nr;
    in->i_flags |= I_FLAGS_INDIRECT_LOADED;

  } else {
    testfs_ensure_indirect_loaded(in);
  }

  int indirect_log_block_nr = log_block_nr - NR_DIRECT_BLOCKS;
  in->indirect[indirect_log_block_nr] = phy_block_nr;
  in->i_flags |= I_FLAGS_INDIRECT_DIRTY;
  return phy_block_nr;
}

int inode_compare(const void *p1, const void *p2) {
  return testfs_inode_to_block_nr(*((struct inode **)p1)) -
    testfs_inode_to_block_nr(*((struct inode **)p2));
}
