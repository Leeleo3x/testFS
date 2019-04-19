#include "inode_alternate.h"
#include "block.h"
#include "csum.h"

// This file contains additional inode functions used for the alternate write
// path implementation. This was done to keep the write path implementations
// separate, since they both contain a non-trivial amount of code.

// NOTE: This file contains the synchronous functions only.

static int testfs_file_write_block(
    struct inode *in, int log_block_nr, char *buf) {
  int phy_block_nr = testfs_inode_log_to_phy(in, log_block_nr);
  if (phy_block_nr == 0) {
    phy_block_nr = testfs_allocate_block_alternate(in, log_block_nr);
  }
  if (phy_block_nr < 0) {
    // Some error occurred
    return phy_block_nr;
  }

  write_blocks(in->sb, buf, phy_block_nr, 1);
  testfs_set_csum(
    in->sb, phy_block_nr, testfs_calculate_csum(buf, BLOCK_SIZE));
  return 0;
}

static void testfs_file_read_block(
    struct inode *in, int log_block_nr, char *buf) {
  int phy_block_nr = testfs_inode_log_to_phy(in, log_block_nr);
  if (phy_block_nr > 0) {
    read_blocks(in->sb, buf, phy_block_nr, 1);
  } else {
    memset(buf, 0, BLOCK_SIZE);
  }
}

int testfs_write_data_alternate(
    struct inode *in, int start, char *buf, const int size) {
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
  char head[BLOCK_SIZE], tail[BLOCK_SIZE];
  if (has_head || has_tail) {
    if (has_head) {
      testfs_file_read_block(in, log_block_start, head);
      log_contig_start += 1;
    }
    if (has_tail) {
      testfs_file_read_block(in, log_block_end, tail);
      log_contig_end -= 1;
    }
  }

  // 4. Initiate all the other writes
  int buf_offset = first_block_offset;
  for (int log_block_nr = log_contig_start;
        log_block_nr <= log_contig_end; log_block_nr++) {
    RETURN_IF_NEG(testfs_file_write_block(in, log_block_nr, buf + buf_offset));
    buf_offset += BLOCK_SIZE;
  }

  // 5. Write the head & tail
  if (has_head || has_tail) {
    if (has_head) {
      memcpy(head + first_block_offset, buf, BLOCK_SIZE - first_block_offset);
      RETURN_IF_NEG(testfs_file_write_block(in, log_block_start, head));
    }
    if (has_tail) {
      memcpy(tail, buf + (size - tail_size), tail_size);
      RETURN_IF_NEG(testfs_file_write_block(in, log_block_end, tail));
    }
  }

  in->in.i_size = MAX(in->in.i_size, start + size);
  in->i_flags |= I_FLAGS_DIRTY;
  return 0;
}

void testfs_bulk_sync_inode(struct inode *inodes[], size_t num_inodes) {
  if (num_inodes == 0) {
    return;
  }

  struct super_block *sb = inodes[0]->sb;

  // 1. Flush any indirect blocks
  for (size_t i = 0; i < num_inodes; i++) {
    if (!(inodes[i]->i_flags & I_FLAGS_INDIRECT_DIRTY)) {
      continue;
    }
    write_blocks(
      sb,
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

  for (size_t i = 0; i < num_inodes; i++) {
    int block_nr =
      sb->sb.inode_blocks_start + testfs_inode_to_block_nr(inodes[i]);

    // Flush the block we've been building so far if we reach a new block and
    // then load the next inode block
    if (block_nr != cur_block_nr) {
      if (cur_block_nr != -1) {
        write_blocks(sb, block, cur_block_nr, 1);
      }
      read_blocks(sb, block, block_nr, 1);
      cur_block_nr = block_nr;
    }

    // Copy the updated dinode into the block
    int offset = testfs_inode_to_block_offset(inodes[i]);
    memcpy(block + offset, &(inodes[i]->in), sizeof(struct dinode));
    inodes[i]->i_flags &= ~(I_FLAGS_DIRTY);
  }

  // Flush the last block
  write_blocks(sb, block, cur_block_nr, 1);
}
