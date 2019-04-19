#include "inode.h"
#include "block.h"
#include "csum.h"

// This file contains additional inode functions used for the alternate write
// path implementation. This was done to keep the write path implementations
// separate, since they both contain a non-trivial amount of code.

// NOTE: This file contains the asynchronous functions only.

#define RETURN_IF_NEG(expr) {    \
  int ret;                       \
  if ((ret = (expr)) < 0) {      \
    return ret;                  \
  }                              \
}

static void testfs_ensure_indirect_loaded(struct inode *in) {
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
static int testfs_inode_log_to_phy(struct inode *in, int log_block_nr) {
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

static int testfs_allocate_block_alternate(
    struct inode *in, int log_block_nr) {
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

static int testfs_file_write_block_async(
    struct inode *in, struct future *f, int log_block_nr, char *buf) {
  int phy_block_nr = testfs_inode_log_to_phy(in, log_block_nr);
  if (phy_block_nr == 0) {
    phy_block_nr = testfs_allocate_block_alternate(in, log_block_nr);
  }
  if (phy_block_nr < 0) {
    // Some error occurred
    return phy_block_nr;
  }

  write_blocks_async(in->sb, DATA_REACTOR, f, buf, phy_block_nr, 1);
  testfs_set_csum(
    in->sb, phy_block_nr, testfs_calculate_csum(buf, BLOCK_SIZE));
  return 0;
}

static void testfs_file_read_block_async(
    struct inode *in, struct future *f, int log_block_nr, char *buf) {
  int phy_block_nr = testfs_inode_log_to_phy(in, log_block_nr);
  if (phy_block_nr > 0) {
    read_blocks_async(in->sb, DATA_REACTOR, f, buf, phy_block_nr, 1);
  } else {
    memset(buf, 0, BLOCK_SIZE);
  }
}

int testfs_write_data_alternate_async(
    struct inode *in, struct future *f, int start, char *buf, const int size) {
  if (size <= 0) {
    return 0;
  }

  // 1. Calculate the offsets into the first and last block, as well as the
  //    number of blocks we will overwrite
  int first_block_offset = start % BLOCK_SIZE;
  int tail_size = (size - first_block_offset) % BLOCK_SIZE;
  assert(first_block_offset >= 0);
  assert(tail_size >= 0);
  int body_size = (size - first_block_offset - tail_size);
  bool has_head = first_block_offset != 0;
  bool has_tail = tail_size != 0;

  // 2. Calculate the block range for the write
  int log_block_start = start / BLOCK_SIZE;
  int log_block_end =
    log_block_start + (body_size / BLOCK_SIZE) + (has_tail ? 1 : 0);
  if (!has_head) {
    log_block_end -= 1;
  }
  assert(log_block_start <= log_block_end);
  if (log_block_end >= NR_DIRECT_BLOCKS &&
      ((log_block_end - NR_DIRECT_BLOCKS) >= NR_INDIRECT_BLOCKS)) {
    // Abort if we cannot write the whole file
    return -EFBIG;
  }
  int log_contig_start = log_block_start;
  int log_contig_end = log_block_end;

  // 3. The head & tail of the write are special cases - we need to read the
  //    data first (if it exists)
  struct future head_tail_f;
  char head[BLOCK_SIZE], tail[BLOCK_SIZE];
  if (has_head || has_tail) {
    future_init(&head_tail_f);
    if (has_head) {
      testfs_file_read_block_async(in, &head_tail_f, log_block_start, head);
      log_contig_start += 1;
    }
    if (has_tail) {
      testfs_file_read_block_async(in, &head_tail_f, log_block_end, tail);
      log_contig_end -= 1;
    }
  }

  // 4. Initiate all the other writes
  int buf_offset = first_block_offset;
  for (int log_block_nr = log_contig_start;
        log_block_nr <= log_contig_end; log_block_nr++) {
    RETURN_IF_NEG(
      testfs_file_write_block_async(in, f, log_block_nr, buf + buf_offset));
    buf_offset += BLOCK_SIZE;
  }

  // 5. Write the head & tail
  if (has_head || has_tail) {
    spin_wait(&head_tail_f);
    if (has_head) {
      memcpy(head + first_block_offset, buf, BLOCK_SIZE - first_block_offset);
      RETURN_IF_NEG(
        testfs_file_write_block_async(in, f, log_block_start, head));
    }
    if (has_tail) {
      memcpy(tail, buf + (size - tail_size), tail_size);
      RETURN_IF_NEG(testfs_file_write_block_async(in, f, log_block_end, tail));
    }
  }

  in->in.i_size = MAX(in->in.i_size, start + size);
  in->i_flags |= I_FLAGS_DIRTY;
  return 0;
}

static int inode_compare(const void *p1, const void *p2) {
  return testfs_inode_to_block_nr(*((struct inode **)p1)) -
    testfs_inode_to_block_nr(*((struct inode **)p2));
}

void testfs_bulk_sync_inode_async(
    struct inode *inodes[], size_t num_inodes, struct future *f) {
  if (num_inodes == 0) {
    return;
  }

  struct super_block *sb = inodes[0]->sb;

  // 1. Flush any indirect blocks
  for (size_t i = 0; i < num_inodes; i++) {
    if (!(inodes[i]->i_flags & I_FLAGS_INDIRECT_DIRTY)) {
      continue;
    }
    write_blocks_async(
      sb,
      METADATA_REACTOR,
      f,
      (char *) (inodes[i]->indirect),
      inodes[i]->in.i_indirect,
      1
    );
    inodes[i]->i_flags &= ~(I_FLAGS_INDIRECT_DIRTY);
  }

  // 2. Ensure that the inodes are clustered by physical block number
  qsort(inodes, num_inodes, sizeof(struct inode *), inode_compare);

  // 3. Write the inodes block by block
  int cur_block_nr = -1;
  char block[BLOCK_SIZE];
  struct future read_f;
  future_init(&read_f);

  for (size_t i = 0; i < num_inodes; i++) {
    int block_nr =
      sb->sb.inode_blocks_start + testfs_inode_to_block_nr(inodes[i]);

    // Flush the block we've been building so far if we reach a new block and
    // then load the next inode block
    if (block_nr != cur_block_nr) {
      if (cur_block_nr != -1) {
        write_blocks_async(sb, METADATA_REACTOR, f, block, cur_block_nr, 1);
      }
      read_blocks_async(sb, METADATA_REACTOR, &read_f, block, block_nr, 1);
      spin_wait(&read_f);
      cur_block_nr = block_nr;
    }

    // Copy the updated dinode into the block
    int offset = testfs_inode_to_block_offset(inodes[i]);
    memcpy(block + offset, &(inodes[i]->in), sizeof(struct dinode));
    inodes[i]->i_flags &= ~(I_FLAGS_DIRTY);
  }

  // Flush the last block
  write_blocks_async(sb, METADATA_REACTOR, f, block, cur_block_nr, 1);
}
